#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BattleScheduleSubsystem.generated.h"

class URealTimeClockSubsystem;

// CSV loading, daily trigger bookkeeping and temple travel only.
UCLASS()
class GLADIUS_API UBattleScheduleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Real Time|Schedule")
    bool ReloadSchedule();

    static bool ParseSchedule(const FString& Csv, TArray<int32>& OutMinutes, FString& OutError);
    // Pure decision helper: no late catch-up after shutdown/sleep or large clock jumps.
    static bool ConsumeDue(const FDateTime& Previous, const FDateTime& Now,
        const TArray<int32>& Minutes, TSet<int64>& Consumed);
    bool AddManualStart(const FDateTime& At);
    bool CancelNext(const FDateTime& Now, FDateTime& Cancelled, FDateTime& Next);
    bool GetNextStart(const FDateTime& Now, FDateTime& Next) const;
    // Survives level travel; the first temple entry is the baseline until a battle has ended.
    FDateTime GetTempleIntervalStart(const FDateTime& Now);
    void RecordBattleEnd(const FDateTime& At) { LastBattleEnd = At; }
private:
    FDateTime LastBattleEnd;
    TArray<FDateTime> ManualStarts;
    friend struct FRealTimeTestAccess;
    friend struct FAdminScheduleTestAccess;
    TWeakObjectPtr<URealTimeClockSubsystem> Clock;
    TWeakObjectPtr<UWorld> PreviousWorld;
    FDateTime PreviousTime;
    TArray<int32> StartMinutes;
    TSet<int64> ConsumedSlots;
    void HandleTime(const FDateTime& Now);
};
