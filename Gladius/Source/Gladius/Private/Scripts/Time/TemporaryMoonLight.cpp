#include "Scripts/Time/TemporaryMoonLight.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ATemporaryMoonLight::ATemporaryMoonLight()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    RootComponent->SetMobility(EComponentMobility::Movable);
    Sphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("YellowMoonSphere"));
    Sphere->SetupAttachment(RootComponent);
    Sphere->SetMobility(EComponentMobility::Movable);
    Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sphere->SetGenerateOverlapEvents(false);
    Sphere->SetCanEverAffectNavigation(false);
    Sphere->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Mesh.Succeeded()) Sphere->SetStaticMesh(Mesh.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Materials/Time/M_TemporaryMoon.M_TemporaryMoon"));
    if (Material.Succeeded()) Sphere->SetMaterial(0, Material.Object);
    MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("YellowMoonLight"));
    MoonLight->SetupAttachment(RootComponent);
    MoonLight->SetMobility(EComponentMobility::Movable);
    // The sphere is the visible disc; avoid adding a second atmospheric sun disc.
    MoonLight->SetAtmosphereSunLight(false);
    MoonLight->SetIntensity(0.f);
    Tags.Add(TEXT("TemporaryMoon"));
}

void ATemporaryMoonLight::UpdateFromSun(const FVector& SunDirection, float SunIntensity, const URealTimeSettings& Settings)
{
    const FVector NormalizedSun = SunDirection.GetSafeNormal();
    const bool bNight = NormalizedSun.Z <= 0.0;
    // Hold the temporary night light at the viewer's zenith throughout the night.
    MoonDirection = bNight ? FVector::UpVector : -NormalizedSun;
    OrbitDistance = FMath::Max(Settings.TemporaryMoonDistance, 10000.f);
    // Engine sphere radius is 50 cm. Cap apparent radius to keep it a sky object.
    const float Radius = FMath::Clamp(Settings.TemporaryMoonRadius, 100.f, OrbitDistance * 0.1f);
    Sphere->SetRelativeScale3D(FVector(Radius / 50.f));
    if (!GlowMaterial) GlowMaterial = Sphere->CreateAndSetMaterialInstanceDynamic(0);
    if (GlowMaterial)
    {
        GlowMaterial->SetVectorParameterValue(TEXT("MoonColor"), Settings.TemporaryMoonColor);
        GlowMaterial->SetScalarParameterValue(TEXT("Glow"), FMath::Max(Settings.TemporaryMoonGlow, 0.f));
    }
    MoonLight->SetWorldRotation((-MoonDirection).Rotation());
    MoonLight->SetLightColor(Settings.TemporaryMoonColor);
    // Fade near the horizon and provide directional illumination across the whole map.
    const float Night = FMath::Clamp(float(-NormalizedSun.Z) * 5.f, 0.f, 1.f);
    MoonLight->SetIntensity(FMath::Max(SunIntensity, 0.f) * FMath::Max(Settings.TemporaryMoonLightScale, 0.f) * Night);
    Sphere->SetVisibility(bNight);
    UpdatePosition();
}

void ATemporaryMoonLight::UpdatePosition()
{
    FVector Center = FVector::ZeroVector;
    if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
        Center = Camera->GetCameraLocation();
    SetActorLocation(Center);
    Sphere->SetRelativeLocation(MoonDirection * OrbitDistance);
}

void ATemporaryMoonLight::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdatePosition();
}
