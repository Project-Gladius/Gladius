#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/BattleSpot.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	//TextRender는 yaw 0이면 좌우가 뒤집혀 보인다
	StatusText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetWorldSize(70.f);
	StatusText->SetTextRenderColor(FColor::Yellow);
}

void ABattleManager::BeginPlay()
{
	Super::BeginPlay();

	ApplyBalanceFiles();
	LoadRecords();
	GatherSpots();

	if (!PlayerUnitClass || !BossUnitClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Battle] PlayerUnitClass 또는 BossUnitClass가 비어 있습니다 — 메시가 없어 유닛이 보이지 않습니다"));
	}

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

	// ponytail: 플레이어 체력·피버는 아직 초기화하지 않는다 (다음 판 흐름이 정해지면 같이)
	CurrentPatternId.Reset();
	PatternStep = 0;
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
	//예고 시간의 선택도 유효하다. 판정 중에만 받지 않는다
	return Phase != EBattlePhase::Resolve;
}

// ---------------- 좌표 ----------------

FVector ABattleManager::BossLocation() const
{
	return GetActorLocation() + FVector(ArenaRadius * BossDistanceRatio, 0.f, 0.f);
}

void ABattleManager::GatherSpots()
{
	for (TArray<FVector>& Lane : Spots)
	{
		Lane.Reset();
	}

	//레벨에 놓인 것을 전부 자동으로 찾는다. 배열에 등록할 필요가 없다
	for (TActorIterator<ABattleSpot> It(GetWorld()); It; ++It)
	{
		Spots[(int32)It->Lane].Add(It->GetActorLocation());
	}

	//보스에서 가까운 순으로 채운다. 번호를 손으로 매기지 않아도 되게
	const FVector Boss = BossLocation();
	for (TArray<FVector>& Lane : Spots)
	{
		Lane.Sort([&Boss](const FVector& A, const FVector& B)
		{
			return FVector::DistSquared(A, Boss) < FVector::DistSquared(B, Boss);
		});
	}

	const int32 Total = Spots[0].Num() + Spots[1].Num() + Spots[2].Num();
	if (Total == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] BattleSpot이 없어 격자 계산으로 배치합니다"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] BattleSpot %d개 (좌 %d / 중 %d / 우 %d)"),
		Total, Spots[0].Num(), Spots[1].Num(), Spots[2].Num());

	for (int32 i = 0; i < 3; ++i)
	{
		if (Spots[i].Num() > 0 && Spots[i].Num() < MaxPlayers)
		{
			// ponytail: 스팟보다 인원이 많으면 자리가 겹친다.
			// 기획서에도 '회의/테스트 후 수정'으로 남아 있는 항목이라 지금은 경고만 남긴다
			UE_LOG(LogTemp, Warning, TEXT("[Battle] %s 구역 스팟이 %d개뿐입니다 (정원 %d) — 몰리면 겹칩니다"),
				*StaticEnum<EBattleLane>()->GetNameStringByValue(i), Spots[i].Num(), MaxPlayers);
		}
	}
}

void ABattleManager::GenerateSpotsFromGrid()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	//이미 놓인 게 있으면 손댄 배치를 덮어쓰지 않는다
	if (TActorIterator<ABattleSpot>(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] 이미 BattleSpot이 있습니다. 다시 만들려면 먼저 지우세요"));
		return;
	}

	int32 Created = 0;
	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		const FString LaneName = StaticEnum<EBattleLane>()->GetNameStringByValue(LaneIndex);

		for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
		{
			//스팟을 만드는 중이므로 스팟을 참조하지 않는 격자 계산을 직접 쓴다
			ABattleSpot* Spot = World->SpawnActor<ABattleSpot>(
				GridSlotLocation((EBattleLane)LaneIndex, Slot), FRotator::ZeroRotator);
			if (!Spot)
			{
				continue;
			}

			Spot->Lane = (EBattleLane)LaneIndex;
			++Created;

#if WITH_EDITOR
			//아웃라이너에서 구역별로 접어 볼 수 있게 정리해 둔다
			Spot->SetActorLabel(FString::Printf(TEXT("Spot_%s_%02d"), *LaneName, Slot));
			Spot->SetFolderPath(FName(*FString::Printf(TEXT("BattleSpots/%s"), *LaneName)));
#endif
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] BattleSpot %d개를 격자 위치에 만들었습니다"), Created);
}

FVector ABattleManager::SlotLocation(EBattleLane Lane, int32 SlotIndex) const
{
	//레벨에 스팟을 놓았으면 그쪽이 우선
	const TArray<FVector>& LaneSpots = Spots[(int32)Lane];
	if (LaneSpots.Num() > 0)
	{
		return LaneSpots[SlotIndex % LaneSpots.Num()];
	}
	return GridSlotLocation(Lane, SlotIndex);
}

FVector ABattleManager::GridSlotLocation(EBattleLane Lane, int32 SlotIndex) const
{
	//레인 중심각. 보스에서 중심을 바라본 방향(-X)이 가운데 레인이다
	const float LaneCenter = (Lane == EBattleLane::Left) ? LaneAngle
		: (Lane == EBattleLane::Right) ? -LaneAngle : 0.f;

	//부채꼴 안을 격자로 채운다. 열은 각도로, 행은 보스로부터의 거리로 벌린다
	const int32 Cols = FMath::Max(1, SlotColumns);
	const int32 Col = SlotIndex % Cols;
	const int32 Row = SlotIndex / Cols;

	const float ColStep = LaneAngle * ColumnSpreadRatio / Cols;
	const float ColOffset = (Col - (Cols - 1) * 0.5f) * ColStep;
	const float Distance = ArenaRadius * (LaneDistanceRatio + Row * RowSpacingRatio);

	const FVector Dir = FRotator(0.f, 180.f + LaneCenter + ColOffset, 0.f).RotateVector(FVector::ForwardVector);
	return BossLocation() + Dir * Distance;
}

#if WITH_EDITOR
void ABattleManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	FlushPersistentDebugLines(World);
	if (!bPreviewSlots)
	{
		return;
	}

	DrawArenaGuides();

	//BattleSpot을 놓았으면 그 액터들이 곧 자리다. 격자 점을 같이 그리면
	//옮겨둔 스팟과 옛 격자가 겹쳐 보여 헷갈린다
	if (TActorIterator<ABattleSpot>(World))
	{
		return;
	}

	//자리 하나당 점 하나. 레인별로 색을 달리해 구분한다
	const FColor LaneColors[] = { FColor::Green, FColor::Yellow, FColor::Blue };
	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
		{
			DrawDebugSphere(World, GridSlotLocation((EBattleLane)LaneIndex, Slot),
				25.f, 8, LaneColors[LaneIndex], true, -1.f, 0, 2.f);
		}
	}
}
#endif

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
	Player.CritChanceBonus = Record.CritChanceBonus * CritChancePerBonus;
	Player.CritDamageBonus = Record.CritDamageBonus * CritDamagePerBonus;
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

		if (DelaySeconds > PlayerSelectCountdown + CollectSeconds)
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
		const bool bNewRecord = !Records.Contains(UserId);
		FPlayerRecord& Record = Records.FindOrAdd(UserId);
		Record.UserId = UserId;
		Record.DisplayName = DisplayName;

		//처음 참가하는 유저는 밸런스 파일의 initialLevels 단계를 들고 시작한다
		if (bNewRecord)
		{
			Record.MaxHPBonus = Balance.InitialHPLevel;
			Record.DamageBonus = Balance.InitialDamageLevel;
			Record.CritChanceBonus = Balance.InitialCritChanceLevel;
			Record.CritDamageBonus = Balance.InitialCritDamageLevel;
		}

		FBattlePlayer NewPlayer;
		NewPlayer.DisplayName = DisplayName;
		NewPlayer.Lane = Balance.InitialZone;
		NewPlayer.Fever = Balance.InitialFever;
		NewPlayer.EvadeStreak = Balance.InitialEvadeStreak;
		ApplyRecordToPlayer(Record, NewPlayer);
		NewPlayer.HP = NewPlayer.MaxHP;
		NewPlayer.SlotIndex = Slot;
		NewPlayer.Unit = GetWorld()->SpawnActor<ABattleUnit>(
			PlayerUnitClass ? PlayerUnitClass.Get() : ABattleUnit::StaticClass(),
			SlotLocation(NewPlayer.Lane, Slot), FRotator::ZeroRotator);
		//바로 안 불러주면 다음 판정 때까지 컴포넌트 기본 문자열이 보인다
		NewPlayer.Unit->SetLabel(DisplayName, NewPlayer.HP, NewPlayer.MaxHP);
#if WITH_EDITOR
		NewPlayer.Unit->SetActorLabel(UserId);
#endif

		Player = &Players.Add(UserId, NewPlayer);

		UE_LOG(LogTemp, Log, TEXT("[Battle] 참가: %s (자리 %d, HP %d, 피해+%d, 치명+%.0f%%/+%.0f%%) — 현재 %d/%d명"),
			*UserId, Slot, NewPlayer.MaxHP, NewPlayer.DamageBonus,
			NewPlayer.CritChanceBonus * 100.f, NewPlayer.CritDamageBonus * 100.f, Players.Num(), MaxPlayers);
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

	//서 있는 구역으로 다시 회피하는 것을 막는 설정이면 거절한다
	if (!bAllowStayEvade && IsEvade(Action) && ApplyMove(Action, Player->Lane) == Player->Lane)
	{
		return false;
	}

	//마지막에 들어온 입력이 이김
	Player->PendingAction = Action;
	Player->bActedThisTurn = true;
	return true;
}

// ---------------- 턴 ----------------

void ABattleManager::StartTurn()
{
	++TurnIndex;
	Phase = EBattlePhase::Telegraph;

	//보스 행동을 먼저 정해 예고한다.
	//판정 시점에 정하면 회피가 순수 확률 찍기가 되어 선택지의 의미가 없어진다
	TelegraphedAction = NextBossAction();

	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		Pair.Value.PendingAction = EBattleAction::Idle;
		Pair.Value.bActedThisTurn = false;
	}

	//채팅이 붙기 전까지, 키보드로 조종하는 한 명 말고는 무작위로 움직이게 둔다
	if (bRandomizeTestPlayers)
	{
		for (TPair<FString, FBattlePlayer>& Pair : Players)
		{
			if (Pair.Key != DebugUserId && Pair.Value.ReviveCountdown == 0)
			{
				Pair.Value.PendingAction = (EBattleAction)FMath::RandRange(1, 6);
				Pair.Value.bActedThisTurn = true;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d/%d 예고 — %.1f초 / %d명 / Boss %d / '%s' → %s"),
		TurnIndex, Turns, TelegraphSeconds, Players.Num(), EnemyHP,
		*TelegraphedAction.Name, *ZonesLabel(TelegraphedAction));

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BeginInput, TelegraphSeconds, false);
}

void ABattleManager::BeginInput()
{
	Phase = EBattlePhase::Input;

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 선택 — %.1f초"), TurnIndex, PlayerSelectCountdown);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BeginCollect, PlayerSelectCountdown, false);
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

		//게임 오버 중에는 입력할 수 없으므로 무입력으로 세지 않는다
		if (P.ReviveCountdown == 0)
		{
			P.IdleTurns = P.bActedThisTurn ? 0 : P.IdleTurns + 1;
		}

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

				//부활은 처음 참가한 것과 같은 상태로 되돌린다
				P.HP = P.MaxHP;
				P.Fever = Balance.InitialFever;
				P.EvadeStreak = Balance.InitialEvadeStreak;
				++Revived;
			}
			continue;
		}

		P.Lane = ApplyMove(P.PendingAction, P.Lane);

		const bool bAttack1 = P.PendingAction == EBattleAction::Attack1;
		const bool bAttack2 = P.PendingAction == EBattleAction::Attack2;
		const bool bEvade = IsEvade(P.PendingAction);

		//기획서 5절 순서 — 플레이어 공격을 먼저 계산한다.
		//피버는 이번 턴 공격 피해에 반영된 뒤에 갱신된다
		if (bAttack1 || bAttack2)
		{
			const FAttackDef& Attack = bAttack1 ? Attack1 : Attack2;
			//내성은 이동 후 구역 기준 (기획서 2절)
			const float Resist = ZoneResistMultiplier(Boss, P.Lane, Attack.Type);

			//치명타는 플레이어마다 따로 굴린다. 공격의 기본 치명타 + 신전 강화분 (상한 적용)
			const float Chance = FMath::Min(Attack.CritChance + P.CritChanceBonus, CritChanceCap);
			const bool bCrit = FMath::FRand() < Chance;
			const int32 Dealt = DamageToBoss(Attack.BaseDamage, P.DamageBonus, P.Fever, FeverDamageCoefficient,
				Resist, bCrit, Attack.CritDamage + P.CritDamageBonus);

			TotalDealt += Dealt;
			Crits += bCrit ? 1 : 0;
			if (Record)
			{
				Record->TotalDamageDealt += Dealt;
			}

			P.Fever += FeverIncrement;
		}

		const int32 Taken = DamageToPlayer(P.PendingAction,
			Boss.DamageByZone[(int32)P.Lane], Boss.HitCount, DefenseDamageRatio);
		P.HP = FMath::Max(0, P.HP - Taken);

		//연속 회피 횟수는 회피가 아닌 행동(무입력 포함)에서 끊긴다
		P.EvadeStreak = bEvade ? P.EvadeStreak + 1 : 0;

		if (P.PendingAction == EBattleAction::Defense)
		{
			P.Fever += DefenseFeverGain;
		}
		else if (bEvade)
		{
			P.Fever = DecayFever(P.Fever, P.EvadeStreak, EvadeStreakN,
				EvadeFeverReduction, EvadeRepeatMode, FeverRounding);
		}

		//상한은 어느 경로로 바뀌었든 똑같이 적용한다
		if (FeverCap > 0.f)
		{
			P.Fever = FMath::Min(P.Fever, FeverCap);
		}

		if (P.HP <= 0)
		{
			//자리는 반납하지 않고 들고 있는다
			P.ReviveCountdown = ReviveTurns;
			++Died;
		}
	}

	EnemyHP = FMath::Max(0, EnemyHP - TotalDealt);

	//무입력이 이어진 유저를 전장에서 뺀다. 순회 중에는 지울 수 없어 모아뒀다가 처리한다
	TArray<FString> Leaving;
	for (const TPair<FString, FBattlePlayer>& Pair : Players)
	{
		if (Pair.Value.IdleTurns >= IdleTurnsBeforeKick)
		{
			Leaving.Add(Pair.Key);
		}
	}
	for (const FString& UserId : Leaving)
	{
		RemovePlayer(UserId);
	}

	UpdateVisuals();

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 결과 — 보스 %s(%d타) / 입힌 피해 %d (치명타 %d) / 게임오버 %d / 부활 %d → Boss %d"),
		TurnIndex, *ZonesLabel(Boss), Boss.HitCount, TotalDealt, Crits, Died, Revived, EnemyHP);

	LastTurnSummary = FString::Printf(TEXT("Boss hit %s   Damage dealt %d (%d crit)   Down %d   Revived %d"),
		*ZonesLabel(Boss), TotalDealt, Crits, Died, Revived);

	//기록이 바뀌는 건 참가와 강화뿐이고 둘 다 턴 안에서 끝난다.
	//주기 저장 대신 턴 경계에서만 쓰면 조절할 값도 없고 저장 시점도 분명해진다
	SaveRecords();

	if (EnemyHP <= 0)
	{
		LastTurnSummary = FString::Printf(TEXT("VICTORY   boss defeated on turn %d"), TurnIndex);
		UE_LOG(LogTemp, Log, TEXT("[Battle] 종료 — 보스 격파 (Turn %d)"), TurnIndex);
		StopBattle();
		return;
	}

	//목표는 생존이므로, 정해진 턴을 버티면 무승부로 끝난다
	if (TurnIndex >= Turns)
	{
		LastTurnSummary = FString::Printf(TEXT("DRAW   survived %d turns   boss left %d HP"), Turns, EnemyHP);
		UE_LOG(LogTemp, Log, TEXT("[Battle] 종료 — %d턴 생존, 무승부 (Boss %d 남음)"), Turns, EnemyHP);
		StopBattle();
		return;
	}

	//결과를 보여주는 시간을 준 뒤 다음 턴으로
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::StartTurn, ResolveSeconds, false);
}

void ABattleManager::RemovePlayer(const FString& UserId)
{
	FBattlePlayer* Player = Players.Find(UserId);
	if (!Player)
	{
		return;
	}

	//능력치는 Records에 그대로 남는다. 다시 명령어를 치면 이어서 참가한다
	if (Player->Unit)
	{
		Player->Unit->Destroy();
	}

	Players.Remove(UserId);

	UE_LOG(LogTemp, Log, TEXT("[Battle] 이탈: %s (무입력 %d턴) — 현재 %d/%d명"),
		*UserId, IdleTurnsBeforeKick, Players.Num(), MaxPlayers);
}

FString ABattleManager::BalanceFolderPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BalanceDataDir);
}

void ABattleManager::ApplyBalanceFiles()
{
	if (!bUseBalanceFiles)
	{
		UE_LOG(LogTemp, Log, TEXT("[Balance] 파일을 쓰지 않습니다 — 에디터 값으로 진행합니다"));
		return;
	}

	const FString Folder = BalanceFolderPath();
	if (!Balance.LoadFromFolder(Folder))
	{
		//기획서 7절: 모르는 값을 조용히 기본값으로 대체하지 않는다.
		//전부 포기하고 에디터에 들어 있던 값으로 진행한다
		UE_LOG(LogTemp, Warning, TEXT("[Balance] %s 를 읽지 못해 에디터 값으로 진행합니다"), *Folder);
		return;
	}

	FeverIncrement = Balance.AttackFeverGain;
	DefenseFeverGain = Balance.DefenseFeverGain;
	FeverDamageCoefficient = Balance.FeverDamageCoefficient;
	FeverCap = Balance.FeverCap;
	EvadeStreakN = Balance.EvadeThreshold;
	EvadeFeverReduction = Balance.EvadeReduction;
	EvadeRepeatMode = Balance.EvadeRepeat;
	FeverRounding = Balance.FeverRounding;
	bAllowStayEvade = Balance.bAllowStayEvade;
	DefenseDamageRatio = Balance.DefenseDamageRatio;

	PlayerBaseHP = Balance.BaseHP;
	Attack1 = Balance.Attacks[0];
	Attack2 = Balance.Attacks[1];

	HPPerBonus = Balance.HPPerLevel;
	DamagePerBonus = Balance.DamagePerLevel;
	CritChancePerBonus = Balance.CritChancePerLevel;
	CritDamagePerBonus = Balance.CritDamagePerLevel;
	CritChanceCap = Balance.CritChanceCap;

	EnemyMaxHP = Balance.BossMaxHP;
	Turns = Balance.Turns;

	//에디터에서 고친 값이 조용히 사라지는 일이 없도록, 덮어쓴 결과를 남긴다
	UE_LOG(LogTemp, Log, TEXT("[Balance] 에디터 값을 파일 값으로 덮어썼습니다 — ")
		TEXT("HP %d / 공격 %d·%d / 피버 +%.0f·%.0f×%.0f / 보스 HP %d / %d턴"),
		PlayerBaseHP, Attack1.BaseDamage, Attack2.BaseDamage,
		FeverIncrement, DefenseFeverGain, FeverDamageCoefficient, EnemyMaxHP, Turns);
}

const FBossPattern* ABattleManager::ChoosePattern(int32 TurnsLeft, const FString& PreviousId) const
{
	//가중치가 있고, 연속 재선택 제한에 걸리지 않고, 남은 턴에 들어가는 것만 후보
	TArray<const FBossPattern*> Candidates;
	TArray<float> Weights;
	float Total = 0.f;

	for (const FPatternWeight& Entry : Balance.Pool)
	{
		if (Entry.Weight <= 0.f)
		{
			continue;
		}
		if (!Balance.bAllowImmediateRepeat && Entry.PatternId == PreviousId)
		{
			continue;
		}

		const FBossPattern* Pattern = Balance.FindPattern(Entry.PatternId);
		if (!Pattern || Pattern->Steps.Num() > TurnsLeft)
		{
			continue;
		}

		Candidates.Add(Pattern);
		Weights.Add(Entry.Weight);
		Total += Entry.Weight;
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	//누적 가중치 구간. pool의 배열 순서를 그대로 쓴다 (기획서 4절)
	float Pick = FMath::FRand() * Total;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Pick -= Weights[i];
		if (Pick < 0.f)
		{
			return Candidates[i];
		}
	}
	return Candidates.Last();
}

FBossAction ABattleManager::NextBossAction()
{
	if (!Balance.bLoaded)
	{
		// ponytail: 파일이 없을 때만 도는 임시 행동. 가운데를 2로 친다
		FBossAction Action;
		Action.Id = TEXT("fallback");
		Action.DamageByZone[(int32)EBattleLane::Mid] = 2;
		Action.TelegraphText = TEXT("NO BALANCE FILE");
		return Action;
	}

	const FBossPattern* Pattern = Balance.FindPattern(CurrentPatternId);

	//패턴이 없거나 끝났으면 새로 뽑는다. 실행 중에는 다시 뽑지 않는다
	if (!Pattern || PatternStep >= Pattern->Steps.Num())
	{
		//이번 턴을 포함해 남은 턴. 여기에 딱 맞게 끝나야 한다 (기획서 5절)
		const int32 TurnsLeft = Turns - TurnIndex + 1;

		//직전에 끝난 패턴은 지금 CurrentPatternId에 들어 있다
		Pattern = ChoosePattern(TurnsLeft, CurrentPatternId);
		if (!Pattern)
		{
			UE_LOG(LogTemp, Error, TEXT("[Battle] 남은 %d턴에 들어가는 패턴이 없습니다"), TurnsLeft);
			return FBossAction();
		}

		CurrentPatternId = Pattern->Id;
		PatternStep = 0;

		UE_LOG(LogTemp, Log, TEXT("[Battle] 패턴 '%s' 선택 (%d턴, 남은 %d턴)"),
			*Pattern->Name, Pattern->Steps.Num(), TurnsLeft);
	}

	const FBossAction* Action = Balance.FindAction(Pattern->Steps[PatternStep]);
	++PatternStep;

	return Action ? *Action : FBossAction();
}

// ---------------- 표시 ----------------

void ABattleManager::SpawnBoss()
{
	if (BossUnit)
	{
		return;
	}

	//보스는 중심 쪽(-X)을 본다
	BossUnit = GetWorld()->SpawnActor<ABattleUnit>(
		BossUnitClass ? BossUnitClass.Get() : ABattleUnit::StaticClass(),
		BossLocation(), FRotator(0.f, 180.f, 0.f));
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
	const FString Header = FString::Printf(TEXT("Turn %d/%d   Players %d/%d   Boss %d/%d"),
		TurnIndex, Turns, Players.Num(), MaxPlayers, EnemyHP, EnemyMaxHP);

	const float Remaining = GetWorldTimerManager().GetTimerRemaining(PhaseTimerHandle);

	//구역마다 피해와 내성이 다르므로 좌·중·우를 나란히 보여준다
	static const TCHAR* ZoneNames[] = { TEXT("L"), TEXT("M"), TEXT("R") };
	FString Zones;
	for (int32 i = 0; i < 3; ++i)
	{
		Zones += FString::Printf(TEXT("  %s dmg %d P%.1f M%.1f"),
			ZoneNames[i], TelegraphedAction.DamageByZone[i],
			ResistanceMultiplier(TelegraphedAction.ResistanceByZone[i].Physical),
			ResistanceMultiplier(TelegraphedAction.ResistanceByZone[i].Magical));
	}

	const FString Telegraph = FString::Printf(TEXT("%s   hits %s (x%d)\n%s"),
		*TelegraphedAction.TelegraphText, *ZonesLabel(TelegraphedAction),
		TelegraphedAction.HitCount, *Zones);

	FString Body;
	switch (Phase)
	{
	case EBattlePhase::Telegraph:
		Body = FString::Printf(TEXT("%s\nGET READY...   %.1fs"), *Telegraph, Remaining);
		break;

	case EBattlePhase::Input:
	{
		const FBattlePlayer* Me = Players.Find(DebugUserId);
		const FString MyLine = Me
			? FString::Printf(TEXT("%s: %s   HP %d/%d   Fever %.1f"), *DebugUserId,
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

bool ABattleManager::IsEvade(EBattleAction Action)
{
	return Action == EBattleAction::AvoidL
		|| Action == EBattleAction::AvoidM
		|| Action == EBattleAction::AvoidR;
}

float ABattleManager::ResistanceMultiplier(EResistance Resistance) const
{
	//배율도 밸런스 파일이 들고 있다 (resistanceMultipliers)
	return Balance.ResistanceMultipliers[(int32)Resistance];
}

float ABattleManager::ZoneResistMultiplier(const FBossAction& Action, EBattleLane Lane, EDamageType Type) const
{
	const FZoneResistance& Zone = Action.ResistanceByZone[(int32)Lane];
	return ResistanceMultiplier(Type == EDamageType::Physical ? Zone.Physical : Zone.Magical);
}

FString ABattleManager::ZonesLabel(const FBossAction& Action)
{
	static const TCHAR* Names[] = { TEXT("Left"), TEXT("Mid"), TEXT("Right") };

	FString Label;
	for (int32 Zone = 0; Zone < 3; ++Zone)
	{
		//피해가 0인 구역은 맞지 않는다
		if (Action.DamageByZone[Zone] <= 0)
		{
			continue;
		}

		if (!Label.IsEmpty())
		{
			Label += TEXT("+");
		}
		Label += Names[Zone];
	}

	return Label.IsEmpty() ? TEXT("Nothing") : Label;
}

int32 ABattleManager::DamageToBoss(int32 BaseDamage, int32 DamageBonus, float Fever, float FeverCoefficient,
	float Resist, bool bCrit, float CritDamage)
{
	const float Raw = (BaseDamage + DamageBonus + Fever * FeverCoefficient)
		* Resist * (1.f + (bCrit ? CritDamage : 0.f));

	//파일의 attackDamageRounding: roundHalfUp
	return FMath::Max(0, FMath::RoundToInt(Raw));
}

float ABattleManager::DecayFever(float Fever, int32 EvadeStreak, int32 StreakN,
	float Reduction, EEvadeRepeatMode Repeat, EFeverRounding Rounding)
{
	const int32 N = FMath::Max(1, StreakN);

	//기준 도달 후 매회는 N회째부터 계속, N회마다는 N의 배수일 때만
	const bool bTrigger = (Repeat == EEvadeRepeatMode::EveryAfter)
		? EvadeStreak >= N
		: (EvadeStreak > 0 && EvadeStreak % N == 0);


	if (!bTrigger)
	{
		return Fever;
	}

	//감소율이 1을 넘어도 음수가 되지 않게 배율에서 막는다
	const float Reduced = Fever * FMath::Max(0.f, 1.f - Reduction);

	switch (Rounding)
	{
	case EFeverRounding::Floor: return FMath::FloorToFloat(Reduced);
	case EFeverRounding::Round: return FMath::RoundToFloat(Reduced);
	default:                    return Reduced;
	}
}

int32 ABattleManager::DamageToPlayer(EBattleAction Action, int32 ZoneDamage, int32 HitCount, float DefenseRatio)
{
	if (ZoneDamage <= 0)
	{
		return 0;
	}

	//방어는 비율을 곱한 뒤 올림 (파일의 defenseDamageRounding: ceil)
	const int32 PerHit = Action == EBattleAction::Defense
		? FMath::CeilToInt(ZoneDamage * DefenseRatio)
		: ZoneDamage;

	return PerHit * FMath::Max(1, HitCount);
}

void ABattleManager::RunSelfTest()
{
	check(ApplyMove(EBattleAction::AvoidL, EBattleLane::Right) == EBattleLane::Left);
	check(ApplyMove(EBattleAction::AvoidR, EBattleLane::Left) == EBattleLane::Right);
	check(ApplyMove(EBattleAction::Attack1, EBattleLane::Right) == EBattleLane::Right);

	check(IsEvade(EBattleAction::AvoidL) && IsEvade(EBattleAction::AvoidM) && IsEvade(EBattleAction::AvoidR));
	check(!IsEvade(EBattleAction::Attack1) && !IsEvade(EBattleAction::Defense) && !IsEvade(EBattleAction::Idle));
	check(ApplyMove(EBattleAction::Idle, EBattleLane::Mid) == EBattleLane::Mid);

	//구역 피해 0 = 안 맞음, 방어는 비율 곱한 뒤 올림, 마지막에 타격 횟수
	check(DamageToPlayer(EBattleAction::Idle, 0, 1, 0.5f) == 0);
	check(DamageToPlayer(EBattleAction::Idle, 20, 1, 0.5f) == 20);
	check(DamageToPlayer(EBattleAction::Defense, 20, 1, 0.5f) == 10);
	check(DamageToPlayer(EBattleAction::Defense, 0, 1, 0.5f) == 0);
	//홀수는 올림이어야 한다 (5 × 0.5 = 2.5 → 3)
	check(DamageToPlayer(EBattleAction::Defense, 5, 1, 0.5f) == 3);
	check(DamageToPlayer(EBattleAction::Defense, 1, 1, 0.5f) == 1);
	//올림은 타격마다 따로 적용된다 (3 → 2씩 두 번 = 4)
	check(DamageToPlayer(EBattleAction::Defense, 3, 2, 0.5f) == 4);
	check(DamageToPlayer(EBattleAction::Idle, 3, 2, 0.5f) == 6);

	check(ResistanceMultiplier(EResistance::Enduring) == 0.5f);
	check(ResistanceMultiplier(EResistance::Normal) == 1.0f);
	check(ResistanceMultiplier(EResistance::Vulnerable) == 1.5f);

	//구역별 내성 — 오른쪽을 치는 동안 왼쪽에서 물리로 때리면 취약
	{
		FBossAction Action;
		Action.DamageByZone[(int32)EBattleLane::Right] = 2;
		Action.ResistanceByZone[(int32)EBattleLane::Left].Physical = EResistance::Vulnerable;
		Action.ResistanceByZone[(int32)EBattleLane::Mid].Physical = EResistance::Enduring;

		check(ZoneResistMultiplier(Action, EBattleLane::Left, EDamageType::Physical) == 1.5f);
		check(ZoneResistMultiplier(Action, EBattleLane::Left, EDamageType::Magical) == 1.0f);
		check(ZoneResistMultiplier(Action, EBattleLane::Mid, EDamageType::Physical) == 0.5f);
		check(ZoneResistMultiplier(Action, EBattleLane::Right, EDamageType::Physical) == 1.0f);

		check(ZonesLabel(Action) == TEXT("Right"));
		Action.DamageByZone[(int32)EBattleLane::Left] = 1;
		check(ZonesLabel(Action) == TEXT("Left+Right"));

		//피해가 하나도 없으면 아무도 맞지 않는다
		check(ZonesLabel(FBossAction()) == TEXT("Nothing"));
	}

	//(기본 + 강화 + 피버×계수) * 내성 * (1 + 치명타 피해)
	check(DamageToBoss(1, 0, 0, 1.f, 1.0f, false, 0.4f) == 1);
	check(DamageToBoss(1, 2, 3, 1.f, 1.0f, false, 0.4f) == 6);
	check(DamageToBoss(2, 0, 0, 1.f, 0.5f, false, 0.4f) == 1);
	check(DamageToBoss(2, 0, 0, 1.f, 1.5f, false, 0.4f) == 3);
	check(DamageToBoss(10, 0, 0, 1.f, 1.0f, true, 0.4f) == 14);
	//피버 계수가 0이면 피버가 피해에 영향을 주지 않는다
	check(DamageToBoss(10, 0, 5, 0.f, 1.0f, false, 0.f) == 10);
	check(DamageToBoss(10, 0, 5, 2.f, 1.0f, false, 0.f) == 20);

	// ---- 연속 회피 피버 감소 (기획서 10절 검증 기준) ----

	//F=8, N=2, 감소율 0.5, 소수 유지, 4회 연속 회피
	//  기준 도달 후 매회 → 8, 4, 2, 1
	{
		float F = 8.f;
		const float Expected[] = { 8.f, 4.f, 2.f, 1.f };
		for (int32 e = 1; e <= 4; ++e)
		{
			F = DecayFever(F, e, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::None);
			check(F == Expected[e - 1]);
		}
	}

	//  N회마다 → 8, 4, 4, 2
	{
		float F = 8.f;
		const float Expected[] = { 8.f, 4.f, 4.f, 2.f };
		for (int32 e = 1; e <= 4; ++e)
		{
			F = DecayFever(F, e, 2, 0.5f, EEvadeRepeatMode::EachN, EFeverRounding::None);
			check(F == Expected[e - 1]);
		}
	}

	//감소율 1이면 기준에 닿는 순간 0이 된다 (밸런스 파일 기본값)
	check(DecayFever(7.f, 2, 2, 1.f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Floor) == 0.f);
	//기준에 닿기 전에는 손대지 않는다
	check(DecayFever(7.f, 1, 2, 1.f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Floor) == 7.f);
	//소수 처리 — 5 × 0.5 = 2.5
	check(DecayFever(5.f, 2, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::None) == 2.5f);
	check(DecayFever(5.f, 2, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Floor) == 2.f);
	check(DecayFever(5.f, 2, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Round) == 3.f);

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
		check(FMath::IsNearlyEqual(P.CritChanceBonus, CritChancePerBonus));
	}

	//같은 자리 번호는 레인이 달라도 서로 다른 좌표여야 한다.
	//스팟 유무에 좌우되지 않도록 격자 계산을 직접 검사한다
	check(!GridSlotLocation(EBattleLane::Left, 0).Equals(GridSlotLocation(EBattleLane::Right, 0)));
	check(!GridSlotLocation(EBattleLane::Mid, 0).Equals(GridSlotLocation(EBattleLane::Mid, 1)));

	//패턴이 정확히 Turns에 맞춰 끝나는지. 난수가 끼어 있어 한 번으로는 못 잡는다.
	//밸런스 파일을 읽지 못했으면 검사할 패턴이 없으므로 건너뛴다
	//에디터에서 버튼으로 부르면 BeginPlay를 거치지 않아 아직 비어 있다.
	//수치를 매니저에 옮기지는 않고 검사용으로만 읽는다
	if (!Balance.bLoaded && bUseBalanceFiles)
	{
		Balance.LoadFromFolder(BalanceFolderPath());
	}

	if (Balance.bLoaded)
	{
		//에디터의 Turns는 아직 파일 값으로 덮이기 전일 수 있으므로 파일 쪽을 본다
		const int32 TotalTurns = Balance.Turns;

		for (int32 Trial = 0; Trial < 200; ++Trial)
		{
			int32 Left = TotalTurns;
			FString Previous;

			while (Left > 0)
			{
				const FBossPattern* Pattern = ChoosePattern(Left, Previous);
				//후보가 없으면 로딩 검증이 놓친 것이다
				check(Pattern);
				//남은 턴보다 긴 패턴은 후보가 될 수 없다
				check(Pattern->Steps.Num() <= Left);

				Left -= Pattern->Steps.Num();
				Previous = Pattern->Id;
			}

			//남기지도 넘기지도 않고 딱 맞아야 한다
			check(Left == 0);
		}

		UE_LOG(LogTemp, Log, TEXT("[Battle] 패턴 검사 통과 — %d턴을 200회 모두 정확히 채움"), TotalTurns);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] 밸런스 파일이 없어 패턴 검사를 건너뜁니다"));
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] SelfTest 통과"));
}
