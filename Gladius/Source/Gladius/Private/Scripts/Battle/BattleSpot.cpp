#include "Scripts/Battle/BattleSpot.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABattleSpot::ABattleSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	RootComponent = Marker;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Marker->SetStaticMesh(Sphere.Object);
	}

	//표식일 뿐이라 충돌도 그림자도 없고, 플레이 중에는 숨긴다
	Marker->SetRelativeScale3D(FVector(0.3f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
	Marker->SetHiddenInGame(true);
}
