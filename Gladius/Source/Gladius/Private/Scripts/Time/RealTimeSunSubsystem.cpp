#include "Scripts/Time/RealTimeSunSubsystem.h"
#include "Scripts/Time/RealTimeClockSubsystem.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/Time/TemporaryMoonLight.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/Light.h"

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
    if (TemporaryMoon.IsValid()) TemporaryMoon->Destroy();
    TemporaryMoon.Reset();
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
        if (It->IsA<ATemporaryMoonLight>()) continue;
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
    if (!Settings->bEnableSun || !Settings->bEnableTemporaryMoon)
    {
        if (TemporaryMoon.IsValid()) TemporaryMoon->Destroy();
        TemporaryMoon.Reset();
    }
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
    const FVector SunDirection = CalculateSunDirection(Hours, *Profile);
    Sun->SetWorldRotation((-SunDirection).Rotation());
    if (Settings->bEnableTemporaryMoon)
    {
        if (!TemporaryMoon.IsValid())
        {
            FActorSpawnParameters Params;
            Params.ObjectFlags |= RF_Transient;
            TemporaryMoon = GetWorld()->SpawnActor<ATemporaryMoonLight>(ATemporaryMoonLight::StaticClass(), FTransform::Identity, Params);
        }
        if (TemporaryMoon.IsValid()) TemporaryMoon->UpdateFromSun(SunDirection, Sun->Intensity, *Settings);
    }
    if (URealTimeSettings::MatchesMap(GetWorld(), Settings->TempleMap))
        UpdateTempleLightColors(SunDirection);
}

void URealTimeSunSubsystem::UpdateTempleLightColors(const FVector& SunDirection)
{
    ULightComponent* Source = Sun.Get();
    if (!Source) return;
    if (SunDirection.Z <= 0.0)
    {
        // Prefer the moon managed by this subsystem; also support an authored atmosphere moon.
        UDirectionalLightComponent* Moon = TemporaryMoon.IsValid()
            ? TemporaryMoon->FindComponentByClass<UDirectionalLightComponent>() : nullptr;
        if (!Moon)
        {
            for (TActorIterator<AActor> It(GetWorld()); It; ++It)
            {
                TInlineComponentArray<UDirectionalLightComponent*> Lights;
                It->GetComponents(Lights);
                for (UDirectionalLightComponent* Light : Lights)
                    if (Light != Sun.Get() && Light->IsUsedAsAtmosphereSunLight()
                        && Light->GetAtmosphereSunLightIndex() == 1 && Light->IsVisible() && Light->Intensity > 0.f)
                        Moon = Light;
            }
        }
        if (Moon && Moon->IsVisible() && Moon->Intensity > 0.f) Source = Moon;
    }

    // Standalone light actors provide temple illumination. Candle/firefly Blueprint components
    // are intentionally excluded. Resolve each sample to support streamed/reloaded lights.
    for (TActorIterator<ALight> It(GetWorld()); It; ++It)
    {
        ULightComponent* Light = It->GetLightComponent();
        if (!Light || Light == Source || Light->IsA<UDirectionalLightComponent>()) continue;
        if (Light->Mobility != EComponentMobility::Movable)
            Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightColor(Source->GetLightColor());
        Light->SetTemperature(Source->Temperature);
        Light->SetUseTemperature(Source->bUseTemperature);
    }
}
