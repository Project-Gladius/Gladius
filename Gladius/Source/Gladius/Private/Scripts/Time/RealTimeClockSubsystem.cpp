#include "Scripts/Time/RealTimeClockSubsystem.h"

void URealTimeClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Core ticker runs independently of game-time dilation and world pause.
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &URealTimeClockSubsystem::Sample), 0.25f);
}

void URealTimeClockSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    OnTimeSample.Clear();
    Super::Deinitialize();
}

bool URealTimeClockSubsystem::Sample(float DeltaSeconds)
{
    OnTimeSample.Broadcast(GetLocalTime());
    return true;
}
