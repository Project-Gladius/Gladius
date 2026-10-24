#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Scripts/Time/RealTimeClockSubsystem.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Scripts/Backend/GameManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Serialization/Csv/CsvParser.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UBattleScheduleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Clock = Collection.InitializeDependency<URealTimeClockSubsystem>();
    PreviousTime = Clock->GetLocalTime();
    PreviousWorld = GetGameInstance()->GetWorld();
    ReloadSchedule();
    Clock->OnTimeSample.AddUObject(this, &UBattleScheduleSubsystem::HandleTime);
}

void UBattleScheduleSubsystem::Deinitialize()
{
    if (Clock.IsValid()) Clock->OnTimeSample.RemoveAll(this);
    Super::Deinitialize();
}

bool UBattleScheduleSubsystem::ParseSchedule(const FString& Csv, TArray<int32>& OutMinutes, FString& OutError)
{
    OutError.Reset();
    TArray<int32> Parsed;
    FCsvParser Parser(Csv);
    const auto& Rows = Parser.GetRows();
    bool bHeader = false;
    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
    {
        const auto& Row = Rows[RowIndex];
        if (Row.IsEmpty()) continue;
        FString Value = FString(Row[0]).TrimStartAndEnd();
        Value.RemoveFromStart(TEXT("\xFEFF"));
        if (Row.Num() == 1 && Value.IsEmpty()) continue;
        if (!bHeader)
        {
            if (Row.Num() != 1 || Value != TEXT("StartTime"))
            {
                OutError = TEXT("Expected a single StartTime header.");
                return false;
            }
            bHeader = true;
            continue;
        }
        if (Row.Num() != 1 || Value.Len() != 5 || Value[2] != TCHAR(':')
            || Value[0] < '0' || Value[0] > '9' || Value[1] < '0' || Value[1] > '9'
            || Value[3] < '0' || Value[3] > '9' || Value[4] < '0' || Value[4] > '9')
        {
            OutError = FString::Printf(TEXT("Row %d: expected HH:MM (00:00..23:59)."), RowIndex + 1);
            return false;
        }
        const int32 Hour = FCString::Atoi(*Value.Left(2));
        const int32 Minute = FCString::Atoi(*Value.Right(2));
        if (Hour > 23 || Minute > 59)
        {
            OutError = FString::Printf(TEXT("Row %d: time out of range."), RowIndex + 1);
            return false;
        }
        Parsed.AddUnique(Hour * 60 + Minute);
    }
    if (!bHeader)
    {
        OutError = TEXT("Missing StartTime header.");
        return false;
    }
    Parsed.Sort();
    OutMinutes = MoveTemp(Parsed);
    return true;
}

bool UBattleScheduleSubsystem::ReloadSchedule()
{
    FString File = GetDefault<URealTimeSettings>()->ScheduleFile;
    if (FPaths::IsRelative(File)) File = FPaths::Combine(FPaths::ProjectContentDir(), File);
    FString Csv, Error;
    TArray<int32> Parsed;
    if (!FFileHelper::LoadFileToString(Csv, *File))
    {
        UE_LOG(LogTemp, Warning, TEXT("BattleSchedule: could not read %s; keeping previous schedule."), *File);
        return false;
    }
    if (!ParseSchedule(Csv, Parsed, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("BattleSchedule: %s (%s); keeping previous schedule."), *Error, *File);
        return false;
    }
    StartMinutes = MoveTemp(Parsed);
    UE_LOG(LogTemp, Log, TEXT("BattleSchedule: loaded %d daily times from %s"), StartMinutes.Num(), *File);
    return true;
}

bool UBattleScheduleSubsystem::ConsumeDue(const FDateTime& Previous, const FDateTime& Now,
    const TArray<int32>& Minutes, TSet<int64>& Consumed)
{
    const double Elapsed = (Now - Previous).GetTotalSeconds();
    if (Elapsed <= 0.0 || Elapsed > 60.0) return false;
    bool bDue = false;
    // Include the previous date for midnight crossings.
    for (FDateTime Day = Previous.GetDate(); Day <= Now.GetDate(); Day += FTimespan::FromDays(1))
    {
        for (int32 Minute : Minutes)
        {
            const FDateTime Slot = Day + FTimespan::FromMinutes(Minute);
            if (Slot > Previous && Slot <= Now && !Consumed.Contains(Slot.GetTicks()))
            {
                Consumed.Add(Slot.GetTicks());
                bDue = true;
            }
        }
    }
    return bDue;
}

void UBattleScheduleSubsystem::HandleTime(const FDateTime& Now)
{
    UWorld* World = GetGameInstance()->GetWorld();
    const bool bSameWorld = PreviousWorld.Get() == World;
    const bool bDue = ConsumeDue(PreviousTime, Now, StartMinutes, ConsumedSlots);
    PreviousTime = Now;
    PreviousWorld = World;
    const URealTimeSettings* Settings = GetDefault<URealTimeSettings>();
    // Consume even outside the temple, preventing late starts on returning from combat.
    if (!bDue || !Settings->bEnableBattleSchedule || !bSameWorld || !World || !World->HasBegunPlay()
        || !World->IsGameWorld() || World->GetNetMode() == NM_Client
        || !URealTimeSettings::MatchesMap(World, Settings->TempleMap)) return;
    AGameManager* Manager = nullptr;
    for (TActorIterator<AGameManager> It(World); It; ++It) { Manager = *It; break; }
    for (TActorIterator<ATempleCycleController> It(World); It; ++It)
    {
        // Older maps may not yet have their combat destination wired.
        if (!Manager && !Settings->BattleMap.IsNull()) Manager = World->SpawnActor<AGameManager>();
        if (Manager && Manager->CombatLevel.IsNull()) Manager->CombatLevel = Settings->BattleMap;
        UE_LOG(LogTemp, Log, TEXT("BattleSchedule: requesting combat at %s"), *Now.ToString());
        It->RequestCombatTravel();
        return;
    }
    // Support temple maps without the optional conversation/cycle controller.
    if (Manager)
    {
        if (Manager->CombatLevel.IsNull()) Manager->CombatLevel = Settings->BattleMap;
        if (!Manager->CombatLevel.IsNull()) Manager->TravelToCombat();
    }
    else if (!Settings->BattleMap.IsNull())
        UGameplayStatics::OpenLevelBySoftObjectPtr(World, Settings->BattleMap);
    else
        UE_LOG(LogTemp, Warning, TEXT("BattleSchedule: no combat map configured."));
}
