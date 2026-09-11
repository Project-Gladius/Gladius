#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/UnitNameplate.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"

ABattleUnit::ABattleUnit()
{
	PrimaryActorTick.bCanEverTick = false;

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
