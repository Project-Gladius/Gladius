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
