#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TemporaryMoonLight.generated.h"

class UStaticMeshComponent;
class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class URealTimeSettings;

// Disposable visual/light placeholder. The clock and battle scheduler know nothing about it.
UCLASS(NotBlueprintable, Transient)
class GLADIUS_API ATemporaryMoonLight : public AActor
{
    GENERATED_BODY()
public:
    ATemporaryMoonLight();
    virtual void Tick(float DeltaSeconds) override;
    void UpdateFromSun(const FVector& SunDirection, float SunIntensity, const URealTimeSettings& Settings);
private:
    friend struct FRealTimeTestAccess;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Sphere;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDirectionalLightComponent> MoonLight;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;
    FVector MoonDirection = FVector::UpVector;
    float OrbitDistance = 200000.f;
    void UpdatePosition();
};
