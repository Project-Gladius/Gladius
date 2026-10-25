#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RealTimeSunSubsystem.generated.h"

class URealTimeClockSubsystem;
class UDirectionalLightComponent;
class ATemporaryMoonLight;
struct FRealTimeSunProfile;

UCLASS()
class GLADIUS_API URealTimeSunSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    static FVector CalculateSunDirection(double LocalHours, const FRealTimeSunProfile& Profile);
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
    friend struct FRealTimeTestAccess;
    friend struct FTempleLightColorTestAccess;
    TWeakObjectPtr<URealTimeClockSubsystem> Clock;
    TWeakObjectPtr<UDirectionalLightComponent> Sun;
    TWeakObjectPtr<ATemporaryMoonLight> TemporaryMoon;
    FName BoundTag;
    bool bWarnedMissingSun = false;
    void UpdateSun(const FDateTime& Now);
    void UpdateTempleLightColors(const FVector& SunDirection);
    UDirectionalLightComponent* FindSun(FName Tag) const;
};
