#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"

//보스와 레인이 떨어진 거리 (ArenaRadius 배율). 손댈 일이 없어 상수로 둔다
static constexpr float BossDistanceRatio = 0.8f;
static constexpr float LaneDistanceRatio = 0.7f;

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
	PlayerHP = PlayerMaxHP;
	EnemyHP = EnemyMaxHP;
	PlayerLane = EBattleLane::Mid;
	TurnIndex = 0;
	LastEnemyLane = -1;
	SpawnUnits();
	UpdateVisuals();
	StartTurn();
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

FVector ABattleManager::BossLocation() const
{
	return GetActorLocation() + FVector(ArenaRadius * BossDistanceRatio, 0.f, 0.f);
}

FVector ABattleManager::LaneLocation(EBattleLane Lane) const
{
	//보스에서 중심을 바라본 방향(-X)이 가운데 레인. 좌/우는 거기서 각도만 벌린다
	//좌우가 뒤집혀 보이면 이 부호만 바꾸면 된다
	const float Offset = (Lane == EBattleLane::Left) ? LaneAngle
		: (Lane == EBattleLane::Right) ? -LaneAngle : 0.f;

	const FVector Dir = FRotator(0.f, 180.f + Offset, 0.f).RotateVector(FVector::ForwardVector);
	return BossLocation() + Dir * (ArenaRadius * LaneDistanceRatio);
}

void ABattleManager::SpawnUnits()
{
	if (BossUnit && PlayerUnit)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	//보스는 중심 쪽(-X)을 보고, 플레이어는 보스 쪽(+X)을 본다
	BossUnit = GetWorld()->SpawnActor<ABattleUnit>(BossLocation(), FRotator(0.f, 180.f, 0.f), Params);
	BossUnit->SetActorScale3D(FVector(UnitScale * BossScale));

	PlayerUnit = GetWorld()->SpawnActor<ABattleUnit>(LaneLocation(PlayerLane), FRotator::ZeroRotator, Params);
	PlayerUnit->SetActorScale3D(FVector(UnitScale));

#if WITH_EDITOR
	BossUnit->SetActorLabel(TEXT("Boss"));
	PlayerUnit->SetActorLabel(TEXT("Player"));
#endif
}

void ABattleManager::UpdateVisuals()
{
	if (PlayerUnit)
	{
		//이동 보간 없이 순간이동
		PlayerUnit->SetActorLocation(LaneLocation(PlayerLane));
		PlayerUnit->SetHP(PlayerHP, PlayerMaxHP);
	}
	if (BossUnit)
	{
		BossUnit->SetHP(EnemyHP, EnemyMaxHP);
	}
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

	//숫자 1~7 = EBattleAction 순서 그대로
	static const FKey ActionKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven };
	for (int32 i = 0; i < UE_ARRAY_COUNT(ActionKeys); ++i)
	{
		FInputKeyBinding Binding(FInputChord(ActionKeys[i]), IE_Pressed);
		Binding.bConsumeInput = false;
		Binding.KeyDelegate.GetDelegateForManualSet().BindLambda(
			[this, i]() { SubmitAction((EBattleAction)i); });
		InputComponent->KeyBindings.Emplace(MoveTemp(Binding));
	}
}

void ABattleManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bInputOpen)
	{
		StatusText->SetText(FText::GetEmpty());
		return;
	}

	const FString Selected = StaticEnum<EBattleAction>()->GetNameStringByValue((int64)PendingAction);

	const FString BossLast = LastEnemyLane < 0
		? TEXT("-")
		: FString::Printf(TEXT("Attack %s"), *StaticEnum<EBattleLane>()->GetNameStringByValue(LastEnemyLane));

	StatusText->SetText(FText::FromString(FString::Printf(
		//기본 폰트에 한글 글리프가 없어 여기는 영문만 쓴다
		TEXT("Turn %d   %.1fs   Selected: %s\nBoss last: %s\n1=Idle 2=Attack1 3=Attack2\n4=AvoidL 5=AvoidM 6=AvoidR 7=Defense"),
		TurnIndex, GetWorldTimerManager().GetTimerRemaining(InputTimerHandle), *Selected, *BossLast)));
}

void ABattleManager::StopBattle()
{
	bInputOpen = false;
	GetWorldTimerManager().ClearTimer(InputTimerHandle);
}

void ABattleManager::StartTurn()
{
	++TurnIndex;
	PendingAction = EBattleAction::Idle;
	bInputOpen = true;

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 시작 — 입력 %.1f초 / Player %d / Enemy %d / Lane %d"),
		TurnIndex, InputSeconds, PlayerHP, EnemyHP, (int32)PlayerLane);

	GetWorldTimerManager().SetTimer(InputTimerHandle, this, &ABattleManager::ResolveTurn, InputSeconds, false);
}

bool ABattleManager::SubmitAction(EBattleAction Action)
{
	if (!bInputOpen)
	{
		return false;
	}

	//마지막에 들어온 입력이 이김. 채팅 집계는 상위에서 처리
	PendingAction = Action;
	return true;
}

void ABattleManager::ResolveTurn()
{
	bInputOpen = false;

	const EBattleAction Action = PendingAction;
	PlayerLane = ApplyMove(Action, PlayerLane);

	const EBattleLane EnemyLane = ChooseEnemyLane();
	LastEnemyLane = (int32)EnemyLane;

	const int32 Taken = DamageToPlayer(Action, EnemyLane == PlayerLane, EnemyDamage, DefenseMultiplier);

	int32 Dealt = 0;
	if (Action == EBattleAction::Attack1) Dealt = Attack1Damage;
	else if (Action == EBattleAction::Attack2) Dealt = Attack2Damage;

	PlayerHP = FMath::Max(0, PlayerHP - Taken);
	EnemyHP = FMath::Max(0, EnemyHP - Dealt);

	UpdateVisuals();

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 결과 — 행동 %d / 적 공격 Lane %d / 받음 %d / 입힘 %d → Player %d / Enemy %d"),
		TurnIndex, (int32)Action, (int32)EnemyLane, Taken, Dealt, PlayerHP, EnemyHP);

	if (PlayerHP <= 0 || EnemyHP <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] 종료 — %s 승"), PlayerHP > 0 ? TEXT("플레이어") : TEXT("적"));
		StopBattle();
		return;
	}

	StartTurn();
}

// ponytail: 균등 랜덤. 패턴/AI는 규칙 확정 후 이 함수만 교체
EBattleLane ABattleManager::ChooseEnemyLane() const
{
	return (EBattleLane)FMath::RandRange(0, 2);
}

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

	UE_LOG(LogTemp, Log, TEXT("[Battle] SelfTest 통과"));
}
