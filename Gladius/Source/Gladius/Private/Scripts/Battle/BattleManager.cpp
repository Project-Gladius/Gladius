#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

//보스와 레인 첫 줄이 떨어진 거리 (ArenaRadius 배율). 손댈 일이 없어 상수로 둔다
static constexpr float BossDistanceRatio = 0.8f;
static constexpr float LaneDistanceRatio = 0.7f;

//키보드로 조종하는 더미 유저. AddTestPlayer가 0번에 붙이는 이름과 같아야
//Q로 만들든 키로 만들든 이름표가 흔들리지 않는다
static const FString DebugUserId = TEXT("@testuser00");
static const FString DebugDisplayName = TEXT("테스터00");

ABattleManager::ABattleManager()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(RootComponent);
	StatusText->SetUsingAbsoluteRotation(true);
	//BattleUnit의 체력 표시와 같은 이유로 yaw 180
	StatusText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetWorldSize(70.f);
	StatusText->SetTextRenderColor(FColor::Yellow);
}

void ABattleManager::BeginPlay()
{
	Super::BeginPlay();

	LoadRecords();

	BindActionKeys();
	DrawArenaGuides();
	StartBattle();
}

void ABattleManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//방송이 정상 종료되는 경우의 마지막 저장
	SaveRecords();
	Super::EndPlay(EndPlayReason);
}

void ABattleManager::StartBattle()
{
	EnemyHP = EnemyMaxHP;
	TurnIndex = 0;
	SpawnBoss();
	UpdateVisuals();
	StartTurn();
}

void ABattleManager::StopBattle()
{
	Phase = EBattlePhase::Resolve;
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
}

bool ABattleManager::IsAcceptingInput() const
{
	return Phase == EBattlePhase::Input || Phase == EBattlePhase::Collect;
}

// ---------------- 좌표 ----------------

FVector ABattleManager::BossLocation() const
{
	return GetActorLocation() + FVector(ArenaRadius * BossDistanceRatio, 0.f, 0.f);
}

FVector ABattleManager::SlotLocation(EBattleLane Lane, int32 SlotIndex) const
{
	//레인 중심각. 보스에서 중심을 바라본 방향(-X)이 가운데 레인이다
	const float LaneCenter = (Lane == EBattleLane::Left) ? LaneAngle
		: (Lane == EBattleLane::Right) ? -LaneAngle : 0.f;

	//부채꼴 안을 격자로 채운다. 열은 각도로, 행은 보스로부터의 거리로 벌린다
	const int32 Cols = FMath::Max(1, SlotColumns);
	const int32 Col = SlotIndex % Cols;
	const int32 Row = SlotIndex / Cols;

	const float ColStep = LaneAngle * 0.8f / Cols;
	const float ColOffset = (Col - (Cols - 1) * 0.5f) * ColStep;
	const float Distance = ArenaRadius * (LaneDistanceRatio + Row * 0.12f);

	const FVector Dir = FRotator(0.f, 180.f + LaneCenter + ColOffset, 0.f).RotateVector(FVector::ForwardVector);
	return BossLocation() + Dir * Distance;
}

void ABattleManager::DrawArenaGuides() const
{
	const FVector Center = GetActorLocation();

	//YAxis/ZAxis를 넘겨 수평 원으로 그린다
	DrawDebugCircle(GetWorld(), Center, ArenaRadius, 48, FColor::Black, true, -1.f, 0, 5.f,
		FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);

	//레인 중심각의 절반 지점이 경계. 보스에서 투기장 테두리까지만 뻗는다
	const float HalfAngle = LaneAngle * 0.5f;
	const FVector Start = BossLocation();
	const FVector Offset = Start - Center;

	for (const float Side : { -HalfAngle, HalfAngle })
	{
		const FVector Dir = FRotator(0.f, 180.f + Side, 0.f).RotateVector(FVector::ForwardVector);

		//원과 만나는 지점까지의 거리 (레이-원 교차)
		const float B = FVector::DotProduct(Offset, Dir);
		const float Length = -B + FMath::Sqrt(FMath::Max(0.f, B * B - Offset.SizeSquared() + ArenaRadius * ArenaRadius));

		DrawDebugLine(GetWorld(), Start, Start + Dir * Length, FColor(160, 160, 160), true, -1.f, 0, 6.f);
	}
}

// ---------------- 참가 / 입력 ----------------

bool ABattleManager::ParseCommand(const FString& Text, EBattleAction& OutAction)
{
	FString T = Text.TrimStartAndEnd();
	T.RemoveFromStart(TEXT("!"));
	T = T.ToLower();

	if (T == TEXT("공격") || T == TEXT("attack") || T == TEXT("a")) { OutAction = EBattleAction::Attack1; return true; }
	if (T == TEXT("강공격") || T == TEXT("attack2") || T == TEXT("aa")) { OutAction = EBattleAction::Attack2; return true; }
	if (T == TEXT("왼쪽") || T == TEXT("left") || T == TEXT("l")) { OutAction = EBattleAction::AvoidL; return true; }
	if (T == TEXT("가운데") || T == TEXT("mid") || T == TEXT("m")) { OutAction = EBattleAction::AvoidM; return true; }
	if (T == TEXT("오른쪽") || T == TEXT("right") || T == TEXT("r")) { OutAction = EBattleAction::AvoidR; return true; }
	if (T == TEXT("방어") || T == TEXT("defend") || T == TEXT("d")) { OutAction = EBattleAction::Defense; return true; }

	return false;
}

// ---------------- 저장 ----------------

FString ABattleManager::SaveFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("PlayerData.json");
}

void ABattleManager::SaveRecords()
{
	FPlayerRecordFile File;
	Records.GenerateValueArray(File.Records);

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(File, Json))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 기록 직렬화 실패"));
		return;
	}

	//인코딩을 지정하지 않으면 한글 핸들이 섞였을 때 UTF-16으로 저장돼
	//외부 도구에서 열기 번거로워진다. 읽을 때는 자동 판별되므로 저장만 고정하면 된다
	if (!FFileHelper::SaveStringToFile(Json, *SaveFilePath(),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 기록 저장 실패: %s"), *SaveFilePath());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] 유저 기록 %d명 저장"), File.Records.Num());
}

void ABattleManager::LoadRecords()
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *SaveFilePath()))
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] 저장된 유저 기록 없음 — 새로 시작합니다"));
		return;
	}

	FPlayerRecordFile File;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &File))
	{
		//파일이 깨졌다면 덮어쓰지 않고 남겨둔다. 사람이 열어볼 수 있어야 한다
		UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 기록을 읽지 못했습니다: %s"), *SaveFilePath());
		return;
	}

	Records.Empty();
	for (const FPlayerRecord& Record : File.Records)
	{
		Records.Add(Record.UserId, Record);
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] 유저 기록 %d명 불러옴"), Records.Num());
}

int32 ABattleManager::MaxHPFor(const FPlayerRecord& Record) const
{
	return PlayerBaseHP + Record.MaxHPBonus * HPPerBonus;
}

void ABattleManager::ApplyRecordToPlayer(const FPlayerRecord& Record, FBattlePlayer& Player) const
{
	Player.MaxHP = MaxHPFor(Record);
	Player.DamageBonus = Record.DamageBonus * DamagePerBonus;
	Player.CritChance = BaseCritChance + Record.CritChanceBonus * CritChancePerBonus;
	Player.CritDamage = BaseCritDamage + Record.CritDamageBonus * CritDamagePerBonus;
}

int32 ABattleManager::AllocateSlotIndex(const TSet<int32>& Used, int32 MaxSlots)
{
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (!Used.Contains(i))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool ABattleManager::SubmitChat(const FString& UserId, const FString& DisplayName,
	const FString& Text, const FDateTime& SentAt)
{
	EBattleAction Action;
	if (!ParseCommand(Text, Action))
	{
		return false;
	}

	//실측 지연. CollectSeconds를 추측이 아니라 이 값을 보고 맞춘다.
	// ponytail: 지금은 로그만 남긴다. 지연 편차가 CollectSeconds를 넘기 시작하면
	// 그때 SentAt으로 어느 턴에 속하는지 직접 계산하도록 바꾼다
	if (SentAt != FDateTime())
	{
		const double DelaySeconds = (FDateTime::UtcNow() - SentAt).GetTotalSeconds();
		UE_LOG(LogTemp, Verbose, TEXT("[Battle] 채팅 지연 %.1f초 (%s)"), DelaySeconds, *UserId);

		if (DelaySeconds > InputSeconds + CollectSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Battle] 지연 %.1f초 — 입력+수집 시간을 넘었습니다. CollectSeconds를 늘리세요"), DelaySeconds);
		}
	}

	return SubmitAction(UserId, DisplayName, Action);
}

bool ABattleManager::SubmitAction(const FString& UserId, const FString& DisplayName, EBattleAction Action)
{
	if (!IsAcceptingInput())
	{
		return false;
	}

	FBattlePlayer* Player = Players.Find(UserId);

	if (!Player)
	{
		if (Players.Num() >= MaxPlayers)
		{
			// ponytail: 대기열은 YouTube 댓글 응답이 필요해 아직 없다. 지금은 그냥 거절
			return false;
		}

		TSet<int32> Used;
		for (const TPair<FString, FBattlePlayer>& Pair : Players)
		{
			Used.Add(Pair.Value.SlotIndex);
		}

		const int32 Slot = AllocateSlotIndex(Used, MaxPlayers);
		if (Slot == INDEX_NONE)
		{
			return false;
		}

		//저장된 기록이 있으면 능력치를 이어받고, 없으면 새로 만든다
		FPlayerRecord& Record = Records.FindOrAdd(UserId);
		Record.UserId = UserId;
		Record.DisplayName = DisplayName;

		FBattlePlayer NewPlayer;
		NewPlayer.DisplayName = DisplayName;
		ApplyRecordToPlayer(Record, NewPlayer);
		NewPlayer.HP = NewPlayer.MaxHP;
		NewPlayer.SlotIndex = Slot;
		NewPlayer.Unit = GetWorld()->SpawnActor<ABattleUnit>(
			SlotLocation(NewPlayer.Lane, Slot), FRotator::ZeroRotator);
		NewPlayer.Unit->SetActorScale3D(FVector(UnitScale));
		//바로 안 불러주면 다음 판정 때까지 컴포넌트 기본 문자열이 보인다
		NewPlayer.Unit->SetLabel(DisplayName, NewPlayer.HP, NewPlayer.MaxHP);
#if WITH_EDITOR
		NewPlayer.Unit->SetActorLabel(UserId);
#endif

		Players.Add(UserId, NewPlayer);
		Player = Players.Find(UserId);

		UE_LOG(LogTemp, Log, TEXT("[Battle] 참가: %s (자리 %d, HP %d, 피해+%d, 치명 %.0f%%/%.0f%%) — 현재 %d/%d명"),
			*UserId, Slot, NewPlayer.MaxHP, NewPlayer.DamageBonus,
			NewPlayer.CritChance * 100.f, NewPlayer.CritDamage * 100.f, Players.Num(), MaxPlayers);
	}

	//핸들은 바뀔 수 있으니 매번 갱신
	Player->DisplayName = DisplayName;
	if (FPlayerRecord* Record = Records.Find(UserId))
	{
		Record->DisplayName = DisplayName;
	}

	//사망 중이면 입력을 받지 않는다
	if (Player->ReviveCountdown > 0)
	{
		return false;
	}

	//마지막에 들어온 입력이 이김
	Player->PendingAction = Action;
	return true;
}

// ---------------- 턴 ----------------

void ABattleManager::StartTurn()
{
	++TurnIndex;
	Phase = EBattlePhase::Input;

	//보스 행동을 먼저 정해 입력 시간 내내 예고한다.
	//판정 시점에 정하면 회피가 순수 확률 찍기가 되어 선택지의 의미가 없어진다
	TelegraphedAction = ChooseBossAction();

	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		Pair.Value.PendingAction = EBattleAction::Idle;
	}

	//채팅이 붙기 전까지, 키보드로 조종하는 한 명 말고는 무작위로 움직이게 둔다
	if (bRandomizeTestPlayers)
	{
		for (TPair<FString, FBattlePlayer>& Pair : Players)
		{
			if (Pair.Key != DebugUserId && Pair.Value.ReviveCountdown == 0)
			{
				Pair.Value.PendingAction = (EBattleAction)FMath::RandRange(1, 6);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 입력 시작 — %.1f초 / %d명 / Boss %d / 예고 %s 피해 %d (물리내성 %d 마법내성 %d)"),
		TurnIndex, InputSeconds, Players.Num(), EnemyHP,
		*HitAreaLabel(TelegraphedAction.HitArea), TelegraphedAction.Damage,
		(int32)TelegraphedAction.PhysicalResist, (int32)TelegraphedAction.MagicalResist);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BeginCollect, InputSeconds, false);
}

void ABattleManager::BeginCollect()
{
	//시청자 화면에서는 아직 입력 시간이다. 늦게 도착하는 채팅을 계속 받는다
	Phase = EBattlePhase::Collect;

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 수집 — %.1f초"), TurnIndex, CollectSeconds);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::ResolveTurn, CollectSeconds, false);
}

void ABattleManager::ResolveTurn()
{
	Phase = EBattlePhase::Resolve;

	//턴 시작 때 예고한 그대로 친다
	const FBossAction& Boss = TelegraphedAction;

	int32 TotalDealt = 0;
	int32 Crits = 0;
	int32 Died = 0;
	int32 Revived = 0;

	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		FPlayerRecord* Record = Records.Find(Pair.Key);

		if (P.ReviveCountdown > 0)
		{
			// ponytail: 기획상 부활 전에 '아직 시청 중인지'를 확인해야 하지만
			// 그건 YouTube API라 여기서는 항상 시청 중이라고 본다.
			// 슈퍼챗 즉시부활도 아직 없다
			if (--P.ReviveCountdown == 0)
			{
				//부활하면서 강화 능력치의 GameOverLoss 비율을 잃는다
				if (Record)
				{
					const float Keep = 1.f - GameOverLoss;
					Record->MaxHPBonus = FMath::FloorToInt(Record->MaxHPBonus * Keep);
					Record->DamageBonus = FMath::FloorToInt(Record->DamageBonus * Keep);
					Record->CritChanceBonus = FMath::FloorToInt(Record->CritChanceBonus * Keep);
					Record->CritDamageBonus = FMath::FloorToInt(Record->CritDamageBonus * Keep);
					ApplyRecordToPlayer(*Record, P);
				}

				P.HP = P.MaxHP;
				P.Fever = 0;
				++Revived;
			}
			continue;
		}

		P.Lane = ApplyMove(P.PendingAction, P.Lane);

		const bool bHit = HitAreaContains(Boss.HitArea, P.Lane);
		const int32 Taken = DamageToPlayer(P.PendingAction, bHit, Boss.Damage, DefenceDivisor);
		P.HP = FMath::Max(0, P.HP - Taken);

		const bool bAttack1 = P.PendingAction == EBattleAction::Attack1;
		const bool bAttack2 = P.PendingAction == EBattleAction::Attack2;

		if (bAttack1 || bAttack2)
		{
			const int32 BaseDamage = bAttack1 ? Attack1BaseDamage : Attack2BaseDamage;
			const EDamageType Type = bAttack1 ? Attack1Type : Attack2Type;
			const float Resist = ResistanceMultiplier(
				Type == EDamageType::Physical ? Boss.PhysicalResist : Boss.MagicalResist);

			//치명타는 플레이어마다 따로 굴린다
			const bool bCrit = FMath::FRand() < P.CritChance;
			const int32 Dealt = DamageToBoss(BaseDamage, P.DamageBonus, P.Fever, Resist, bCrit, P.CritDamage);

			TotalDealt += Dealt;
			Crits += bCrit ? 1 : 0;
			if (Record)
			{
				Record->TotalDamageDealt += Dealt;
			}

			P.Fever += FeverIncrement;
		}
		else if (P.PendingAction == EBattleAction::AvoidL
			|| P.PendingAction == EBattleAction::AvoidM
			|| P.PendingAction == EBattleAction::AvoidR)
		{
			P.Fever = FMath::Max(0, P.Fever - FeverDecrement);
		}

		if (P.HP <= 0)
		{
			//자리는 반납하지 않고 들고 있는다
			P.ReviveCountdown = ReviveTurns;
			++Died;
		}
	}

	EnemyHP = FMath::Max(0, EnemyHP - TotalDealt);

	UpdateVisuals();

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 결과 — 보스 %s(%d) / 입힌 피해 %d (치명타 %d) / 게임오버 %d / 부활 %d → Boss %d"),
		TurnIndex, *HitAreaLabel(Boss.HitArea), Boss.Damage, TotalDealt, Crits, Died, Revived, EnemyHP);

	LastTurnSummary = FString::Printf(TEXT("Boss hit %s   Damage dealt %d (%d crit)   Down %d   Revived %d"),
		*HitAreaLabel(Boss.HitArea), TotalDealt, Crits, Died, Revived);

	//기록이 바뀌는 건 참가와 강화뿐이고 둘 다 턴 안에서 끝난다.
	//주기 저장 대신 턴 경계에서만 쓰면 조절할 값도 없고 저장 시점도 분명해진다
	SaveRecords();

	if (EnemyHP <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] 종료 — 보스 격파"));
		StopBattle();
		return;
	}

	//결과를 보여주는 시간을 준 뒤 다음 턴으로
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::StartTurn, ResolveSeconds, false);
}

// ponytail: 균등 랜덤 스텁. 패턴이 정해지면 이 함수만 데이터 조회로 교체하면 된다.
// 구조체와 피격 범위 9종은 이미 지원하므로 값만 채워 넣으면 동작한다
FBossAction ABattleManager::ChooseBossAction() const
{
	FBossAction Action;
	Action.Damage = BossBaseDamage;
	Action.PhysicalResist = (EResistance)FMath::RandRange(0, 2);
	Action.MagicalResist = (EResistance)FMath::RandRange(0, 2);
	Action.HitArea = ResolveHitArea(EHitArea::Random);
	Action.NoticeText = FString::Printf(TEXT("BOSS WILL HIT: %s"), *HitAreaLabel(Action.HitArea));
	return Action;
}

// ---------------- 표시 ----------------

void ABattleManager::SpawnBoss()
{
	if (BossUnit)
	{
		return;
	}

	//보스는 중심 쪽(-X)을 본다
	BossUnit = GetWorld()->SpawnActor<ABattleUnit>(BossLocation(), FRotator(0.f, 180.f, 0.f));
	BossUnit->SetActorScale3D(FVector(UnitScale * BossScale));
#if WITH_EDITOR
	BossUnit->SetActorLabel(TEXT("Boss"));
#endif
}

void ABattleManager::UpdateVisuals()
{
	if (BossUnit)
	{
		BossUnit->SetLabel(TEXT("BOSS"), EnemyHP, EnemyMaxHP);
	}

	for (const TPair<FString, FBattlePlayer>& Pair : Players)
	{
		const FBattlePlayer& P = Pair.Value;
		if (!P.Unit)
		{
			continue;
		}

		//사망 중에는 자리만 잡아두고 숨긴다
		P.Unit->SetActorHiddenInGame(P.ReviveCountdown > 0);

		//이동 보간 없이 순간이동
		P.Unit->SetActorLocation(SlotLocation(P.Lane, P.SlotIndex));
		P.Unit->SetLabel(P.DisplayName, P.HP, P.MaxHP);
	}
}

void ABattleManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//기본 폰트에 한글 글리프가 없어 이 표시판은 영문만 쓴다
	const FString Header = FString::Printf(TEXT("Turn %d   Players %d/%d   Boss %d/%d"),
		TurnIndex, Players.Num(), MaxPlayers, EnemyHP, EnemyMaxHP);

	const float Remaining = GetWorldTimerManager().GetTimerRemaining(PhaseTimerHandle);

	const FString Telegraph = FString::Printf(TEXT("%s   damage %d   resist P%.1f M%.1f"),
		*TelegraphedAction.NoticeText, TelegraphedAction.Damage,
		ResistanceMultiplier(TelegraphedAction.PhysicalResist),
		ResistanceMultiplier(TelegraphedAction.MagicalResist));

	FString Body;
	switch (Phase)
	{
	case EBattlePhase::Input:
	{
		const FBattlePlayer* Me = Players.Find(DebugUserId);
		const FString MyLine = Me
			? FString::Printf(TEXT("%s: %s   HP %d/%d   Fever %d"), *DebugUserId,
				*StaticEnum<EBattleAction>()->GetNameStringByValue((int64)Me->PendingAction),
				Me->HP, Me->MaxHP, Me->Fever)
			: FString::Printf(TEXT("%s: not joined"), *DebugUserId);

		Body = FString::Printf(
			TEXT("%s\nCHOOSE YOUR ACTION   %.1fs\n%s\n1=Idle 2=Attack1 3=Attack2  4=AvoidL 5=AvoidM 6=AvoidR 7=Defense\nQ=add test player  E=grant bonus"),
			*Telegraph, Remaining, *MyLine);
		break;
	}
	case EBattlePhase::Collect:
		Body = FString::Printf(TEXT("%s\nCOLLECTING...   %.1fs"), *Telegraph, Remaining);
		break;

	default:
		Body = LastTurnSummary.IsEmpty() ? TEXT("READY") : LastTurnSummary;
		break;
	}

	StatusText->SetText(FText::FromString(Header + TEXT("\n") + Body));
}

// ---------------- 입력 ----------------

void ABattleManager::AddTestPlayer()
{
	const FString Id = FString::Printf(TEXT("@testuser%02d"), Players.Num());
	//표시 이름을 한글로 둬서 UMG 이름표가 UTF-8을 제대로 그리는지 눈으로 확인한다.
	//실제 핸들이 붙으면 이 줄은 사라진다
	const FString Name = FString::Printf(TEXT("테스터%02d"), Players.Num());
	SubmitAction(Id, Name, (EBattleAction)FMath::RandRange(1, 6));
}

void ABattleManager::GrantBonusToAll()
{
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FPlayerRecord& Record = Records.FindOrAdd(Pair.Key);
		Record.UserId = Pair.Key;
		++Record.MaxHPBonus;
		++Record.DamageBonus;
		++Record.CritChanceBonus;
		++Record.CritDamageBonus;

		//진행 중인 전투에도 바로 반영한다. 늘어난 최대 체력만큼 현재 체력도 올린다
		const int32 OldMax = Pair.Value.MaxHP;
		ApplyRecordToPlayer(Record, Pair.Value);
		Pair.Value.HP += Pair.Value.MaxHP - OldMax;
	}

	UpdateVisuals();
	UE_LOG(LogTemp, Log, TEXT("[Battle] 참여자 %d명 능력치 +1"), Players.Num());
}

void ABattleManager::BindActionKeys()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] PlayerController 없음 — 키 입력 비활성. Simulate 말고 Play로 실행하세요"));
		return;
	}

	EnableInput(PC);
	if (!InputComponent)
	{
		return;
	}

	//숫자 1~7 = EBattleAction 순서 그대로, DebugUserId 한 명을 조종한다
	static const FKey ActionKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven };
	for (int32 i = 0; i < UE_ARRAY_COUNT(ActionKeys); ++i)
	{
		FInputKeyBinding Binding(FInputChord(ActionKeys[i]), IE_Pressed);
		Binding.bConsumeInput = false;
		Binding.KeyDelegate.GetDelegateForManualSet().BindLambda(
			[this, i]() { SubmitAction(DebugUserId, DebugDisplayName, (EBattleAction)i); });
		InputComponent->KeyBindings.Emplace(MoveTemp(Binding));
	}

	FInputKeyBinding AddBinding(FInputChord(EKeys::Q), IE_Pressed);
	AddBinding.bConsumeInput = false;
	AddBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this]() { AddTestPlayer(); });
	InputComponent->KeyBindings.Emplace(MoveTemp(AddBinding));

	FInputKeyBinding BonusBinding(FInputChord(EKeys::E), IE_Pressed);
	BonusBinding.bConsumeInput = false;
	BonusBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this]() { GrantBonusToAll(); });
	InputComponent->KeyBindings.Emplace(MoveTemp(BonusBinding));
}

// ---------------- 순수 함수 ----------------

EBattleLane ABattleManager::ApplyMove(EBattleAction Action, EBattleLane Current)
{
	switch (Action)
	{
	case EBattleAction::AvoidL: return EBattleLane::Left;
	case EBattleAction::AvoidM: return EBattleLane::Mid;
	case EBattleAction::AvoidR: return EBattleLane::Right;
	default: return Current;
	}
}

float ABattleManager::ResistanceMultiplier(EResistance Resistance)
{
	switch (Resistance)
	{
	case EResistance::Enduring:   return 0.5f;
	case EResistance::Vulnerable: return 1.5f;
	default:                      return 1.0f;
	}
}

EHitArea ABattleManager::ResolveHitArea(EHitArea Area)
{
	if (Area != EHitArea::Random)
	{
		return Area;
	}

	//예고를 보고 대응할 수 있어야 하므로, 무작위는 예고 전에 구체 범위로 확정한다.
	//None과 Random을 뺀 나머지 중에서 고른다
	static const EHitArea Concrete[] = {
		EHitArea::Left, EHitArea::Mid, EHitArea::Right,
		EHitArea::LeftMid, EHitArea::MidRight, EHitArea::LeftRight, EHitArea::All };

	return Concrete[FMath::RandRange(0, UE_ARRAY_COUNT(Concrete) - 1)];
}

bool ABattleManager::HitAreaContains(EHitArea Area, EBattleLane Lane)
{
	switch (Area)
	{
	case EHitArea::All:       return true;
	case EHitArea::Left:      return Lane == EBattleLane::Left;
	case EHitArea::Mid:       return Lane == EBattleLane::Mid;
	case EHitArea::Right:     return Lane == EBattleLane::Right;
	case EHitArea::LeftMid:   return Lane != EBattleLane::Right;
	case EHitArea::MidRight:  return Lane != EBattleLane::Left;
	case EHitArea::LeftRight: return Lane != EBattleLane::Mid;
	//None은 아무도 안 맞고, Random은 ResolveHitArea를 거치므로 여기 오지 않는다
	default:                  return false;
	}
}

FString ABattleManager::HitAreaLabel(EHitArea Area)
{
	switch (Area)
	{
	case EHitArea::None:      return TEXT("Nothing");
	case EHitArea::Left:      return TEXT("Left");
	case EHitArea::Mid:       return TEXT("Mid");
	case EHitArea::Right:     return TEXT("Right");
	case EHitArea::LeftMid:   return TEXT("Left+Mid");
	case EHitArea::MidRight:  return TEXT("Mid+Right");
	case EHitArea::LeftRight: return TEXT("Left+Right");
	case EHitArea::All:       return TEXT("Everywhere");
	default:                  return TEXT("Random");
	}
}

int32 ABattleManager::DamageToBoss(int32 BaseDamage, int32 DamageBonus, int32 Fever,
	float Resist, bool bCrit, float CritDamage)
{
	const float Raw = (BaseDamage + DamageBonus + Fever) * Resist * (1.f + (bCrit ? CritDamage : 0.f));
	return FMath::Max(0, FMath::RoundToInt(Raw));
}

int32 ABattleManager::DamageToPlayer(EBattleAction Action, bool bHit, int32 Base, int32 Divisor)
{
	if (!bHit)
	{
		return 0;
	}
	if (Action != EBattleAction::Defense)
	{
		return Base;
	}
	//기획서상 방어는 나눗셈 후 올림
	return FMath::DivideAndRoundUp(Base, FMath::Max(1, Divisor));
}

void ABattleManager::RunSelfTest()
{
	check(ApplyMove(EBattleAction::AvoidL, EBattleLane::Right) == EBattleLane::Left);
	check(ApplyMove(EBattleAction::AvoidR, EBattleLane::Left) == EBattleLane::Right);
	check(ApplyMove(EBattleAction::Attack1, EBattleLane::Right) == EBattleLane::Right);
	check(ApplyMove(EBattleAction::Idle, EBattleLane::Mid) == EBattleLane::Mid);

	check(DamageToPlayer(EBattleAction::Idle, false, 20, 2) == 0);
	check(DamageToPlayer(EBattleAction::Idle, true, 20, 2) == 20);
	check(DamageToPlayer(EBattleAction::Defense, true, 20, 2) == 10);
	check(DamageToPlayer(EBattleAction::Defense, false, 20, 2) == 0);
	//홀수는 올림이어야 한다 (5 / 2 = 3)
	check(DamageToPlayer(EBattleAction::Defense, true, 5, 2) == 3);
	check(DamageToPlayer(EBattleAction::Defense, true, 1, 2) == 1);

	check(ResistanceMultiplier(EResistance::Enduring) == 0.5f);
	check(ResistanceMultiplier(EResistance::Normal) == 1.0f);
	check(ResistanceMultiplier(EResistance::Vulnerable) == 1.5f);

	//피격 범위 9종
	check(!HitAreaContains(EHitArea::None, EBattleLane::Mid));
	check(HitAreaContains(EHitArea::All, EBattleLane::Left));
	check(HitAreaContains(EHitArea::Left, EBattleLane::Left));
	check(!HitAreaContains(EHitArea::Left, EBattleLane::Mid));
	check(HitAreaContains(EHitArea::LeftMid, EBattleLane::Left));
	check(HitAreaContains(EHitArea::LeftMid, EBattleLane::Mid));
	check(!HitAreaContains(EHitArea::LeftMid, EBattleLane::Right));
	check(HitAreaContains(EHitArea::MidRight, EBattleLane::Right));
	check(!HitAreaContains(EHitArea::MidRight, EBattleLane::Left));
	check(HitAreaContains(EHitArea::LeftRight, EBattleLane::Left));
	check(HitAreaContains(EHitArea::LeftRight, EBattleLane::Right));
	check(!HitAreaContains(EHitArea::LeftRight, EBattleLane::Mid));

	//Random은 예고 전에 반드시 구체 범위로 바뀐다
	for (int32 i = 0; i < 30; ++i)
	{
		const EHitArea Resolved = ResolveHitArea(EHitArea::Random);
		check(Resolved != EHitArea::Random && Resolved != EHitArea::None);
	}
	check(ResolveHitArea(EHitArea::Left) == EHitArea::Left);

	//(기본 + 강화 + 피버) * 내성 * (1 + 치명타 피해)
	check(DamageToBoss(1, 0, 0, 1.0f, false, 0.4f) == 1);
	check(DamageToBoss(1, 2, 3, 1.0f, false, 0.4f) == 6);
	check(DamageToBoss(2, 0, 0, 0.5f, false, 0.4f) == 1);
	check(DamageToBoss(2, 0, 0, 1.5f, false, 0.4f) == 3);
	check(DamageToBoss(10, 0, 0, 1.0f, true, 0.4f) == 14);

	check(AllocateSlotIndex({}, 30) == 0);
	check(AllocateSlotIndex({ 0, 1, 3 }, 30) == 2);
	check(AllocateSlotIndex({ 0, 1, 2 }, 3) == INDEX_NONE);

	EBattleAction A;
	check(ParseCommand(TEXT("  공격 "), A) && A == EBattleAction::Attack1);
	check(ParseCommand(TEXT("!왼쪽"), A) && A == EBattleAction::AvoidL);
	check(ParseCommand(TEXT("RIGHT"), A) && A == EBattleAction::AvoidR);
	check(ParseCommand(TEXT("방어"), A) && A == EBattleAction::Defense);
	check(!ParseCommand(TEXT("안녕하세요"), A));
	check(!ParseCommand(TEXT(""), A));

	//강화 단계가 실제 수치로 반영되는지
	{
		FPlayerRecord R;
		check(MaxHPFor(R) == PlayerBaseHP);
		R.MaxHPBonus = 3;
		check(MaxHPFor(R) == PlayerBaseHP + 3 * HPPerBonus);

		//강화 수치가 전투용 스냅샷으로 옮겨지는지
		R.DamageBonus = 2;
		R.CritChanceBonus = 1;
		FBattlePlayer P;
		ApplyRecordToPlayer(R, P);
		check(P.MaxHP == PlayerBaseHP + 3 * HPPerBonus);
		check(P.DamageBonus == 2 * DamagePerBonus);
		check(FMath::IsNearlyEqual(P.CritChance, BaseCritChance + CritChancePerBonus));
	}

	//같은 자리 번호는 레인이 달라도 서로 다른 좌표여야 한다
	check(!SlotLocation(EBattleLane::Left, 0).Equals(SlotLocation(EBattleLane::Right, 0)));
	check(!SlotLocation(EBattleLane::Mid, 0).Equals(SlotLocation(EBattleLane::Mid, 1)));

	UE_LOG(LogTemp, Log, TEXT("[Battle] SelfTest 통과"));
}
