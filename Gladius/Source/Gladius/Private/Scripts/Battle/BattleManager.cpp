#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"

//보스와 레인 첫 줄이 떨어진 거리 (ArenaRadius 배율). 손댈 일이 없어 상수로 둔다
static constexpr float BossDistanceRatio = 0.8f;
static constexpr float LaneDistanceRatio = 0.7f;

//키보드로 조종하는 더미 유저
static const FString DebugUserId = TEXT("@testuser00");

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

	BindActionKeys();
	DrawArenaGuides();
	StartBattle();
}

void ABattleManager::StartBattle()
{
	EnemyHP = EnemyMaxHP;
	TurnIndex = 0;
	LastEnemyLane = -1;
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

		FBattlePlayer NewPlayer;
		NewPlayer.DisplayName = DisplayName;
		NewPlayer.HP = PlayerMaxHP;
		NewPlayer.SlotIndex = Slot;
		NewPlayer.Unit = GetWorld()->SpawnActor<ABattleUnit>(
			SlotLocation(NewPlayer.Lane, Slot), FRotator::ZeroRotator);
		NewPlayer.Unit->SetActorScale3D(FVector(UnitScale));
		//바로 안 불러주면 다음 판정 때까지 컴포넌트 기본 문자열이 보인다
		NewPlayer.Unit->SetLabel(DisplayName, NewPlayer.HP, PlayerMaxHP);
#if WITH_EDITOR
		NewPlayer.Unit->SetActorLabel(UserId);
#endif

		Players.Add(UserId, NewPlayer);
		Player = Players.Find(UserId);

		UE_LOG(LogTemp, Log, TEXT("[Battle] 참가: %s (자리 %d) — 현재 %d/%d명"),
			*UserId, Slot, Players.Num(), MaxPlayers);
	}

	//핸들은 바뀔 수 있으니 매번 갱신
	Player->DisplayName = DisplayName;

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

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 입력 시작 — %.1f초 / %d명 / Boss %d"),
		TurnIndex, InputSeconds, Players.Num(), EnemyHP);

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

	const EBattleLane EnemyLane = ChooseEnemyLane();
	LastEnemyLane = (int32)EnemyLane;

	int32 TotalDealt = 0;
	int32 Died = 0;
	int32 Revived = 0;

	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;

		if (P.ReviveCountdown > 0)
		{
			// ponytail: 기획상 부활 전에 '아직 시청 중인지'를 확인해야 하지만
			// 그건 YouTube API라 여기서는 항상 시청 중이라고 본다.
			// 강화 능력치 50% 손실과 슈퍼챗 즉시부활도 능력치 구현 후에 붙인다
			if (--P.ReviveCountdown == 0)
			{
				P.HP = PlayerMaxHP;
				++Revived;
			}
			continue;
		}

		P.Lane = ApplyMove(P.PendingAction, P.Lane);

		const int32 Taken = DamageToPlayer(P.PendingAction, EnemyLane == P.Lane, EnemyDamage, DefenseMultiplier);
		P.HP = FMath::Max(0, P.HP - Taken);

		if (P.PendingAction == EBattleAction::Attack1) TotalDealt += Attack1Damage;
		else if (P.PendingAction == EBattleAction::Attack2) TotalDealt += Attack2Damage;

		if (P.HP <= 0)
		{
			//자리는 반납하지 않고 들고 있는다
			P.ReviveCountdown = ReviveTurns;
			++Died;
		}
	}

	EnemyHP = FMath::Max(0, EnemyHP - TotalDealt);

	UpdateVisuals();

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 결과 — 적 공격 Lane %d / 입힌 피해 %d / 사망 %d / 부활 %d → Boss %d"),
		TurnIndex, (int32)EnemyLane, TotalDealt, Died, Revived, EnemyHP);

	LastTurnSummary = FString::Printf(TEXT("Boss attacked %s   Damage dealt %d   Down %d   Revived %d"),
		*StaticEnum<EBattleLane>()->GetNameStringByValue((int64)EnemyLane), TotalDealt, Died, Revived);

	if (EnemyHP <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] 종료 — 보스 격파"));
		StopBattle();
		return;
	}

	//결과를 보여주는 시간을 준 뒤 다음 턴으로
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::StartTurn, ResolveSeconds, false);
}

// ponytail: 균등 랜덤. 패턴/AI는 규칙 확정 후 이 함수만 교체
EBattleLane ABattleManager::ChooseEnemyLane() const
{
	return (EBattleLane)FMath::RandRange(0, 2);
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
		P.Unit->SetLabel(P.DisplayName, P.HP, PlayerMaxHP);
	}
}

void ABattleManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//기본 폰트에 한글 글리프가 없어 이 표시판은 영문만 쓴다
	const FString Header = FString::Printf(TEXT("Turn %d   Players %d/%d   Boss %d/%d"),
		TurnIndex, Players.Num(), MaxPlayers, EnemyHP, EnemyMaxHP);

	const float Remaining = GetWorldTimerManager().GetTimerRemaining(PhaseTimerHandle);

	FString Body;
	switch (Phase)
	{
	case EBattlePhase::Input:
	{
		const FBattlePlayer* Me = Players.Find(DebugUserId);
		const FString MyLine = Me
			? FString::Printf(TEXT("%s: %s  HP %d"), *DebugUserId,
				*StaticEnum<EBattleAction>()->GetNameStringByValue((int64)Me->PendingAction), Me->HP)
			: FString::Printf(TEXT("%s: not joined"), *DebugUserId);

		Body = FString::Printf(
			TEXT("CHOOSE YOUR ACTION   %.1fs\n%s\n1=Idle 2=Attack1 3=Attack2  4=AvoidL 5=AvoidM 6=AvoidR 7=Defense\nQ=add test player"),
			Remaining, *MyLine);
		break;
	}
	case EBattlePhase::Collect:
		Body = FString::Printf(TEXT("COLLECTING...   %.1fs"), Remaining);
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
			[this, i]() { SubmitAction(DebugUserId, DebugUserId, (EBattleAction)i); });
		InputComponent->KeyBindings.Emplace(MoveTemp(Binding));
	}

	FInputKeyBinding AddBinding(FInputChord(EKeys::Q), IE_Pressed);
	AddBinding.bConsumeInput = false;
	AddBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this]() { AddTestPlayer(); });
	InputComponent->KeyBindings.Emplace(MoveTemp(AddBinding));
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

int32 ABattleManager::DamageToPlayer(EBattleAction Action, bool bHit, int32 Base, float DefMul)
{
	if (!bHit)
	{
		return 0;
	}
	return Action == EBattleAction::Defense ? FMath::RoundToInt(Base * DefMul) : Base;
}

void ABattleManager::RunSelfTest()
{
	check(ApplyMove(EBattleAction::AvoidL, EBattleLane::Right) == EBattleLane::Left);
	check(ApplyMove(EBattleAction::AvoidR, EBattleLane::Left) == EBattleLane::Right);
	check(ApplyMove(EBattleAction::Attack1, EBattleLane::Right) == EBattleLane::Right);
	check(ApplyMove(EBattleAction::Idle, EBattleLane::Mid) == EBattleLane::Mid);

	check(DamageToPlayer(EBattleAction::Idle, false, 20, 0.5f) == 0);
	check(DamageToPlayer(EBattleAction::Idle, true, 20, 0.5f) == 20);
	check(DamageToPlayer(EBattleAction::Defense, true, 20, 0.5f) == 10);
	check(DamageToPlayer(EBattleAction::Defense, false, 20, 0.5f) == 0);

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

	//같은 자리 번호는 레인이 달라도 서로 다른 좌표여야 한다
	check(!SlotLocation(EBattleLane::Left, 0).Equals(SlotLocation(EBattleLane::Right, 0)));
	check(!SlotLocation(EBattleLane::Mid, 0).Equals(SlotLocation(EBattleLane::Mid, 1)));

	UE_LOG(LogTemp, Log, TEXT("[Battle] SelfTest 통과"));
}
