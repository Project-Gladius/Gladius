#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/UnitNameplate.h"
#include "Scripts/Battle/BattlePlayerNameplate.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/WidgetComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABattleUnit::ABattleUnit()
{
	//이동하는 동안에만 켠다. 서 있는 유닛 30개가 매 프레임 도는 것을 막는다
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	//SpawnActor가 액터 스케일을 1로 덮어쓰기 때문에 루트는 비워 두고,
	//크기는 그 아래 Mesh에서 준다. 블루프린트에서 Mesh 스케일을 조절할 것
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	//메시는 블루프린트에서 지정한다
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	Nameplate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(Root);

	//Root는 항상 스케일 1이므로 이 값이 곧 머리 위 높이(cm)다
	Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 200.f));

	//화면 공간이라 항상 카메라를 향하고 거리와 무관하게 같은 크기로 그려진다
	Nameplate->SetWidgetSpace(EWidgetSpace::Screen);
	Nameplate->SetDrawAtDesiredSize(true);
	Nameplate->SetUsingAbsoluteScale(true);

	//블루프린트에서 Widget Class를 바꾸면 그쪽이 우선한다
	Nameplate->SetWidgetClass(UUnitNameplate::StaticClass());
	static ConstructorHelpers::FClassFinder<UBattlePlayerNameplate> DesignerUI(TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate"));
	PlayerUIClass = DesignerUI.Class;
}

void ABattleUnit::BeginPlay()
{
	Super::BeginPlay();
	ApplyLabel();
}

void ABattleUnit::NotifyActionSelected(EBattleAction Action)
{
	SelectedAction = Action;
    if (!CanPlayEvadeRoll() && IsValid(EvadeRollMontage) && IsValid(Mesh))
    {
        if (UAnimInstance* Anim = Mesh->GetAnimInstance()) Anim->Montage_Stop(0.f, EvadeRollMontage);
    }
	OnActionSelected(Action);
}

bool ABattleUnit::CanPlayEvadeRoll() const
{
    return SelectedAction != EBattleAction::Attack1 && SelectedAction != EBattleAction::Attack2;
}

void ABattleUnit::NotifyAttack(int32 AttackIndex, bool bCrit, bool bFeverHighlight)
{
    OnAttack(AttackIndex, bCrit, bFeverHighlight);
    OnAttackStarted.Broadcast(AttackIndex);
}

void ABattleUnit::NotifyDied()
{
	bDead = true;
	OnDied();
}

void ABattleUnit::NotifyRevived()
{
	bDead = false;
	OnRevived();
}

void ABattleUnit::PlayFeverFlurry(const TArray<bool>& Crits, float FirstDelay,
	const TArray<float>& SectionSeconds)
{
	//앞 턴의 연타가 남아 있으면 버린다. 턴이 바뀌었으니 들고 있을 이유가 없다
	GetWorldTimerManager().ClearTimer(FlurryHandle);

	FlurryCrits = Crits;
	FlurrySections = SectionSeconds;
	FlurryIndex = 0;

	//섹션 길이를 모르면 언제 다음 대를 띄울지 알 수 없다. 조용히 겹쳐 재생하지 않는다
	if (FlurryCrits.Num() == 0 || FlurrySections.Num() == 0)
	{
		FlurryCrits.Reset();
		return;
	}

	//스킬 모션이 도는 동안은 기다린다. 겹치면 스킬이 안 보인다
	GetWorldTimerManager().SetTimer(FlurryHandle, this, &ABattleUnit::FireNextFeverAttack,
		FMath::Max(KINDA_SMALL_NUMBER, FirstDelay), false);
}

void ABattleUnit::FireNextFeverAttack()
{
	if (!FlurryCrits.IsValidIndex(FlurryIndex))
	{
		GetWorldTimerManager().ClearTimer(FlurryHandle);
		FlurryCrits.Reset();
		return;
	}

	const int32 Index = FlurryIndex++;
	OnFeverAttack(Index, FlurryCrits.Num(), FlurryCrits[Index]);

	//마지막 대였으면 더 걸지 않는다
	if (FlurryCrits.IsValidIndex(FlurryIndex))
	{
		//지금 재생한 섹션이 끝나는 순간이 다음 대의 시작이다. 섹션이 평타 수보다
		//적으면 앞에서부터 다시 쓴다 (3개면 1,2,3,1,2,3…)
		const float ThisSection = FlurrySections[Index % FlurrySections.Num()];

		GetWorldTimerManager().SetTimer(FlurryHandle, this, &ABattleUnit::FireNextFeverAttack,
			FMath::Max(KINDA_SMALL_NUMBER, ThisSection), false);
	}
	else
	{
		FlurryCrits.Reset();
		FlurrySections.Reset();
		FlurryIndex = 0;
	}
}

void ABattleUnit::HoldMotion(float Seconds)
{
	const UWorld* World = GetWorld();
	if (!World || Seconds <= 0.f)
	{
		return;
	}

	//이미 더 길게 잡혀 있으면 그대로 둔다. 짧은 몬타주가 긴 것을 깎으면 안 된다
	MotionHoldUntil = FMath::Max(MotionHoldUntil, World->GetTimeSeconds() + Seconds);

	//미뤄 둔 이동이 있으면 늘어난 만큼 다시 기다리게 한다
	if (bHasPendingMove)
	{
		GetWorldTimerManager().SetTimer(MotionHoldHandle, this, &ABattleUnit::FlushPendingMove,
			MotionHoldUntil - World->GetTimeSeconds(), false);
	}
}

void ABattleUnit::FlushPendingMove()
{
	if (!bHasPendingMove)
	{
		return;
	}

	bHasPendingMove = false;
	MotionHoldUntil = 0.f;
	MoveTo(PendingMoveTarget, PendingMoveDuration, PendingMoveGait);
}

void ABattleUnit::MoveTo(const FVector& Target, float Duration, EMoveGait Gait)
{
	//쓰러져 있는 중이라면 다 일어난 뒤에 간다. 요청을 버리지는 않는다 —
	//버리면 자리 재배치를 놓쳐 그 자리에 영영 남는다
	if (const UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now < MotionHoldUntil)
		{
			PendingMoveTarget = Target;
			PendingMoveDuration = Duration;
			PendingMoveGait = Gait;
			bHasPendingMove = true;

			GetWorldTimerManager().SetTimer(MotionHoldHandle, this, &ABattleUnit::FlushPendingMove,
				MotionHoldUntil - Now, false);
			return;
		}
	}

	//이미 그 자리면 굳이 움직이지 않는다
	if (GetActorLocation().Equals(Target, 1.f))
	{
		return;
	}

	if (Duration <= 0.f)
	{
		SetActorLocation(Target);
		return;
	}

	LastGait = Gait;

	MoveStart = GetActorLocation();
	MoveTarget = Target;
	MoveElapsed = 0.f;
	MoveDuration = Duration;
	bMoving = true;

	SetActorTickEnabled(true);

	const float Distance = FVector::Dist(MoveStart, MoveTarget);

	//진행 방향을 보고 달린다. 짧은 자리 조정까지 방향을 틀면 제자리에서 도는 것처럼 보인다
	if (Distance >= TurnMinDistance)
	{
		TurnToward(MoveTarget);
	}

	OnMoveStarted(Duration, Distance, Gait);
}

void ABattleUnit::SetFacingTarget(const FVector& WorldLocation)
{
	bHasFaceTarget = true;
	FaceTarget = WorldLocation;

	//처음 지정할 때는 돌리는 연출 없이 바로 그쪽을 보게 한다
	if (!bMoving)
	{
		const FVector Delta = FaceTarget - GetActorLocation();
		if (!Delta.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, Delta.Rotation().Yaw, 0.f));
		}
		bTurning = false;
	}
}

void ABattleUnit::TurnToward(const FVector& WorldLocation)
{
	const FVector Delta = WorldLocation - GetActorLocation();
	if (Delta.IsNearlyZero())
	{
		return;
	}

	DesiredYaw = Delta.Rotation().Yaw;

	//이미 그 방향이면 Tick을 깨우지 않는다
	if (FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw), 0.5f))
	{
		return;
	}

	bTurning = true;
	SetActorTickEnabled(true);
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	//회전은 이동과 따로 끝난다. 둘 다 끝나야 Tick을 끈다
	if (bTurning)
	{
		const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, DesiredYaw, TurnSpeed * DeltaSeconds);
		SetActorRotation(FRotator(0.f, NewYaw, 0.f));

		if (FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(NewYaw, DesiredYaw), 0.5f))
		{
			bTurning = false;
		}
	}

	if (!bMoving)
	{
		if (!bTurning)
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	MoveElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f);

	//곡선을 꽂으면 가감속이 붙는다. 없으면 등속
	const float Progress = MoveCurve ? MoveCurve->GetFloatValue(Alpha) : Alpha;
	SetActorLocation(FMath::Lerp(MoveStart, MoveTarget, Progress));

	if (Alpha >= 1.f)
	{
		//곡선이 1에서 끝나지 않더라도 목적지에 정확히 세운다
		SetActorLocation(MoveTarget);
		bMoving = false;

		//도착했으면 다시 보스를 본다. 돌아가는 동안에는 Tick이 더 돌아야 한다
		if (bHasFaceTarget)
		{
			TurnToward(FaceTarget);
		}

		if (!bTurning)
		{
			SetActorTickEnabled(false);
		}

		OnArrived();
		OnMovementFinished.Broadcast();
	}
}

void ABattleUnit::SetLabel(const FString& Name, int32 Current, int32 Max)
{
	SetLabelText(FString::Printf(TEXT("%s\n%d / %d"), *Name, Current, Max));
}

void ABattleUnit::SetLabelText(const FString& Text)
{
	PendingLabel = Text;
	ApplyLabel();
}

void ABattleUnit::SetPlayerUI(const FBattlePlayerUIData& Data)
{
	UClass* DesiredClass = PlayerUIClass.Get();
	// Old unit BPs may still explicitly reference the former native-only widget.
	if (!DesiredClass || DesiredClass == UBattlePlayerNameplate::StaticClass())
		DesiredClass = GetDefault<ABattleUnit>()->PlayerUIClass.Get();
	if (!DesiredClass) return;
	if (Nameplate->GetWidgetClass() != DesiredClass)
	{
		Nameplate->SetWidgetClass(DesiredClass);
		Nameplate->SetPivot(FVector2D(0.5f, 1.f));
		Nameplate->InitWidget();
	}
	if (UBattlePlayerNameplate* Widget = Cast<UBattlePlayerNameplate>(Nameplate->GetUserWidgetObject()))
	{
		Widget->SetDisplayData(Data);
	}
}

void ABattleUnit::SetPlayerUIVisible(bool bVisible)
{
	Nameplate->SetVisibility(bVisible);
}

void ABattleUnit::UpdatePlayerUIScale(const FVector& CameraLocation)
{
	if (UBattlePlayerNameplate* Widget = Cast<UBattlePlayerNameplate>(Nameplate->GetUserWidgetObject()))
	{
		const float Distance = FVector::Dist(CameraLocation, Nameplate->GetComponentLocation());
		const float Scale = Widget->GetDistanceScale(Distance);
		Widget->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
		Widget->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void ABattleUnit::ApplyLabel()
{
	if (UUnitNameplate* Widget = Cast<UUnitNameplate>(Nameplate->GetUserWidgetObject()))
	{
		Widget->SetText(PendingLabel);
	}
}

// ===== 전투 명령 실행 함수 (C++ 기본 구현) =====

void ABattleUnit::DodgeLeft_Implementation(const FVector& RollTo, float RollSeconds, float RollDistance)
{
	OnEvadeRoll(RollSeconds, RollDistance);
	MoveTo(RollTo, RollSeconds, EMoveGait::Roll);
}

void ABattleUnit::DodgeMid_Implementation(const FVector& RollTo, float RollSeconds, float RollDistance)
{
	OnEvadeRoll(RollSeconds, RollDistance);
	MoveTo(RollTo, RollSeconds, EMoveGait::Roll);
}

void ABattleUnit::DodgeRight_Implementation(const FVector& RollTo, float RollSeconds, float RollDistance)
{
	OnEvadeRoll(RollSeconds, RollDistance);
	MoveTo(RollTo, RollSeconds, EMoveGait::Roll);
}

void ABattleUnit::CastSkill1_Implementation(bool bCrit, bool bFeverHighlight)
{
	NotifyAttack(1, bCrit, bFeverHighlight);
}

void ABattleUnit::CastSkill2_Implementation(bool bCrit, bool bFeverHighlight)
{
	NotifyAttack(2, bCrit, bFeverHighlight);
}

void ABattleUnit::Defend_Implementation()
{
	OnDefend();
	//펼치는 모션이 끝나면 밑에 깔린 '버티는 자세'가 드러나도록 상태로 둔다
	SetDefending(true);
}

void ABattleUnit::PerformDodge(EBattleAction Action, const FVector& RollTo, float RollSeconds, float RollDistance)
{
	switch (Action)
	{
	case EBattleAction::AvoidL: DodgeLeft(RollTo, RollSeconds, RollDistance); break;
	case EBattleAction::AvoidM: DodgeMid(RollTo, RollSeconds, RollDistance); break;
	case EBattleAction::AvoidR: DodgeRight(RollTo, RollSeconds, RollDistance); break;
	default: break;
	}
}

void ABattleUnit::PerformSkill(int32 AttackIndex, bool bCrit, bool bFeverHighlight)
{
	if (AttackIndex == 2)
	{
		CastSkill2(bCrit, bFeverHighlight);
	}
	else
	{
		CastSkill1(bCrit, bFeverHighlight);
	}
}
