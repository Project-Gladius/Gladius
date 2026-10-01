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

void ABattleUnit::MoveTo(const FVector& Target, float Duration)
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
	OnMoveStarted(Duration);
}

void ABattleUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMoving)
	{
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
		SetActorTickEnabled(false);
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
