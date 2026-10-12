#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/UnitNameplate.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Curves/CurveFloat.h"

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
}

void ABattleUnit::BeginPlay()
{
	Super::BeginPlay();
	ApplyLabel();
}

void ABattleUnit::MoveTo(const FVector& Target, float Duration, bool bRollThrough)
{
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

	//걷기와 달리기는 거리로 갈린다. 기준 거리는 블루프린트가 정하므로 값만 넘긴다
	OnMoveStarted(Duration, Distance, bRollThrough);
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
	}
}

void ABattleUnit::SetLabel(const FString& Name, int32 Current, int32 Max)
{
	PendingLabel = FString::Printf(TEXT("%s\n%d / %d"), *Name, Current, Max);
	ApplyLabel();
}

void ABattleUnit::ApplyLabel()
{
	if (UUnitNameplate* Widget = Cast<UUnitNameplate>(Nameplate->GetUserWidgetObject()))
	{
		Widget->SetText(PendingLabel);
	}
}
