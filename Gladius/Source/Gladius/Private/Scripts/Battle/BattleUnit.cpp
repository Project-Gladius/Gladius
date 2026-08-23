#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/UnitNameplate.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"

ABattleUnit::ABattleUnit()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// ponytail: 임시 더미 경로 하드코딩. 정식 모델 나오면 프로퍼티로 빼기
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DummyMesh(TEXT("/Game/Asset/DUMMY_MODEL.DUMMY_MODEL"));
	if (DummyMesh.Succeeded())
	{
		Mesh->SetSkeletalMesh(DummyMesh.Object);
	}

	Nameplate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(Mesh);

	//DUMMY_MODEL이 16.8cm라 로컬 20이면 머리 위. 액터 스케일에 비례해 올라간다
	Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 20.f));

	//화면 공간이라 항상 카메라를 향하고 거리와 무관하게 같은 크기로 그려진다.
	//TextRenderComponent 때 필요했던 yaw 180 보정이 필요 없다
	Nameplate->SetWidgetSpace(EWidgetSpace::Screen);
	Nameplate->SetWidgetClass(UUnitNameplate::StaticClass());
	Nameplate->SetDrawAtDesiredSize(true);
	Nameplate->SetUsingAbsoluteScale(true);
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
