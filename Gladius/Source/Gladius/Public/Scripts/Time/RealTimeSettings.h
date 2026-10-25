#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RealTimeSettings.generated.h"

USTRUCT(BlueprintType)
struct GLADIUS_API FRealTimeSunProfile
{
    GENERATED_BODY()

    // Elevation above the horizon at 12:00 local time.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sun", meta=(ClampMin="1", ClampMax="90", Units="deg"))
    float NoonAltitude = 60.f;

    // Direction FROM the scene TOWARDS the sun: +X=0, +Y=90.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sun", meta=(Units="deg"))
    float NoonDirection = 0.f;

    // Optional actor tag. Otherwise prefer the unique atmosphere sun at index 0.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sun")
    FName SunActorTag;
};

// One shared editor for both maps. Consumers own their respective behavior.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Real Time"))
class GLADIUS_API URealTimeSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    UPROPERTY(Config, EditAnywhere, Category="Sun")
    bool bEnableSun = true;

    UPROPERTY(Config, EditAnywhere, Category="Sun")
    FRealTimeSunProfile BattleSun;

    UPROPERTY(Config, EditAnywhere, Category="Sun")
    FRealTimeSunProfile TempleSun;

    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon")
    bool bEnableTemporaryMoon = true;

    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon")
    FLinearColor TemporaryMoonColor = FLinearColor(1.f, 0.82f, 0.25f);

    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon", meta=(ClampMin="1700", ClampMax="12000", Units="K"))
    float TemporaryMoonTemperature = 3700.f;

    // Relative to the map's existing sun intensity, so existing exposure is respected.
    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon", meta=(ClampMin="0", ClampMax="2"))
    float TemporaryMoonLightScale = 0.35f;

    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon", meta=(ClampMin="0"))
    float TemporaryMoonGlow = 8.f;

    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon", meta=(ClampMin="10000", Units="cm"))
    float TemporaryMoonDistance = 200000.f;

    UPROPERTY(Config, EditAnywhere, Category="Temporary Moon", meta=(ClampMin="100", Units="cm"))
    float TemporaryMoonRadius = 2000.f;

    UPROPERTY(Config, EditAnywhere, Category="Maps")
    TSoftObjectPtr<UWorld> BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Levels/Main/MainBattle.MainBattle")));

    UPROPERTY(Config, EditAnywhere, Category="Maps")
    TSoftObjectPtr<UWorld> TempleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Levels/Main/MainTemple.MainTemple")));

    UPROPERTY(Config, EditAnywhere, Category="Schedule")
    bool bEnableBattleSchedule = true;

    // Relative to Content; staged loose so the CSV remains editable after packaging.
    UPROPERTY(Config, EditAnywhere, Category="Schedule", meta=(RelativePath))
    FString ScheduleFile = TEXT("ExternalData/BattleSchedule.csv");

    static bool MatchesMap(const UWorld* World, const TSoftObjectPtr<UWorld>& Map);
};
