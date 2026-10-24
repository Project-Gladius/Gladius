#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RealTimeClockSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FRealTimeSample, const FDateTime&);

// Time source only: no lighting, CSV, map or travel responsibilities.
UCLASS()
class GLADIUS_API URealTimeClockSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category="Real Time")
    FDateTime GetLocalTime() const { return FDateTime::Now(); }

    FRealTimeSample OnTimeSample;
private:
    FTSTicker::FDelegateHandle TickerHandle;
    bool Sample(float DeltaSeconds);
};
