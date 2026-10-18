#include "Scripts/Battle/BattleSpot.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABattleSpot::ABattleSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	//액터 위치가 곧 플레이어의 발밑이다. 메시 피벗은 허리에 있어서 그만큼
	//띄워야 하는데, 루트 자체는 띄울 수 없으니 빈 루트를 두고 그 아래에 붙인다
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Root);

	//구체 대신 실제 플레이어 메시를 세운다. 겹침과 가림을 실행 전에 보려면
	//표식이 캐릭터와 같은 크기여야 한다 (BP_BattleUnit의 GladiusMesh와 동일한 값)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Preview(
		TEXT("/Game/Asset/Battle/gladius/SM_MERGED_Gladius.SM_MERGED_Gladius"));
	if (Preview.Succeeded())
	{
		Marker->SetStaticMesh(Preview.Object);
	}

	//표식일 뿐이라 충돌도 그림자도 없고, 플레이 중에는 숨긴다
	Marker->SetRelativeLocation(FVector(0.f, 0.f, 87.1f));
	Marker->SetRelativeScale3D(FVector(0.1f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
	Marker->SetHiddenInGame(true);
}
