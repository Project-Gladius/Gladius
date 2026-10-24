#include "Scripts/Time/RealTimeSunSubsystem.h"
#include "Scripts/Time/RealTimeClockSubsystem.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"

bool URealTimeSunSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void URealTimeSunSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (UGameInstance* GI = InWorld.GetGameInstance())
    {
        Clock = GI->GetSubsystem<URealTimeClockSubsystem>();
        if (Clock.IsValid())
        {
            Clock->OnTimeSample.AddUObject(this, &URealTimeSunSubsystem::UpdateSun);
            UpdateSun(Clock->GetLocalTime());
        }
    }
}

void URealTimeSunSubsystem::Deinitialize()
{
    if (Clock.IsValid()) Clock->OnTimeSample.RemoveAll(this);
    Sun.Reset();
    Super::Deinitialize();
}

FVector URealTimeSunSubsystem::CalculateSunDirection(double LocalHours, const FRealTimeSunProfile& Profile)
{
    const double HourAngle = (LocalHours - 12.0) * PI / 12.0;
    const double Altitude = FMath::DegreesToRadians(FMath::Clamp(double(Profile.NoonAltitude), 1.0, 90.0));
    const double Yaw = FMath::DegreesToRadians(double(Profile.NoonDirection));
    const FVector NoonHorizontal(FMath::Cos(Yaw), FMath::Sin(Yaw), 0);
    const FVector AfternoonHorizontal(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0);
    return (NoonHorizontal * (FMath::Cos(Altitude) * FMath::Cos(HourAngle))
        + AfternoonHorizontal * FMath::Sin(HourAngle)
        + FVector::UpVector * (FMath::Sin(Altitude) * FMath::Cos(HourAngle))).GetSafeNormal();
}

UDirectionalLightComponent* URealTimeSunSubsystem::FindSun(FName Tag) const
{
    TArray<UDirectionalLightComponent*> All, Atmosphere;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (!Tag.IsNone() && !It->ActorHasTag(Tag)) continue;
        TInlineComponentArray<UDirectionalLightComponent*> Lights;
        It->GetComponents(Lights);
        for (UDirectionalLightComponent* Light : Lights)
        {
            All.Add(Light);
            if (Light->IsUsedAsAtmosphereSunLight() && Light->GetAtmosphereSunLightIndex() == 0)
                Atmosphere.Add(Light);
        }
    }
    if (!Tag.IsNone()) return All.Num() == 1 ? All[0] : nullptr;
    if (Atmosphere.Num() == 1) return Atmosphere[0];
    return All.Num() == 1 ? All[0] : nullptr;
}

void URealTimeSunSubsystem::UpdateSun(const FDateTime& Now)
{
    const URealTimeSettings* Settings = GetDefault<URealTimeSettings>();
    if (!Settings->bEnableSun) return;
    const FRealTimeSunProfile* Profile = nullptr;
    if (URealTimeSettings::MatchesMap(GetWorld(), Settings->BattleMap)) Profile = &Settings->BattleSun;
    else if (URealTimeSettings::MatchesMap(GetWorld(), Settings->TempleMap)) Profile = &Settings->TempleSun;
    if (!Profile) return;
    if (!Sun.IsValid() || BoundTag != Profile->SunActorTag)
    {
        BoundTag = Profile->SunActorTag;
        Sun = FindSun(BoundTag);
        if (!Sun.IsValid())
        {
            if (!bWarnedMissingSun)
                UE_LOG(LogTemp, Warning, TEXT("RealTimeSun: no unique sun in %s; set Sun Actor Tag in Project Settings > Game > Real Time."), *GetWorld()->GetName());
            bWarnedMissingSun = true;
            return;
        }
        // Runtime-only; saved lighting assets are not changed.
        Sun->SetMobility(EComponentMobility::Movable);
    }
    const double Hours = Now.GetTimeOfDay().GetTotalHours();
    // A directional light points along its rays, opposite the visible sun direction.
    Sun->SetWorldRotation((-CalculateSunDirection(Hours, *Profile)).Rotation());
}
