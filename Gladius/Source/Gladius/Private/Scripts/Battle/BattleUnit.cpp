#include "Scripts/Battle/BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
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

	HPText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HPText"));
	HPText->SetupAttachment(Mesh);

	//액터 회전·스케일을 따라가지 않게 한다.
	//따라가면 보스(yaw 180)의 글자가 뒤집히고, 유닛 스케일에 글자 크기까지 딸려 커진다
	HPText->SetUsingAbsoluteRotation(true);
	HPText->SetUsingAbsoluteScale(true);

	//DUMMY_MODEL이 16.8cm라 로컬 20이면 머리 위. 액터 스케일에 비례해 올라간다
	HPText->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	//카메라가 -X쪽에서 +X를 보는 고정 카메라라 yaw 180이면 정면으로 읽힌다
	HPText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	HPText->SetHorizontalAlignment(EHTA_Center);
	HPText->SetWorldSize(48.f);
	HPText->SetTextRenderColor(FColor::Black);
}

void ABattleUnit::SetHP(int32 Current, int32 Max)
{
	HPText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Current, Max)));
}
