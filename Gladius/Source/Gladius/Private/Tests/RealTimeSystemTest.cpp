#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "EngineUtils.h"
#include "Scripts/Time/RealTimeSunSubsystem.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Scripts/Time/RealTimeClockSubsystem.h"
#include "Scripts/Time/TemporaryMoonLight.h"
#include "Components/StaticMeshComponent.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Scripts/Temple/Goddess.h"
#include "Scripts/Backend/GameManager.h"
#include "BattleCameraTestAccess.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"

struct FRealTimeTestAccess
{
    static ATemporaryMoonLight* Moon(URealTimeSunSubsystem* S) { return S->TemporaryMoon.Get(); }
    static UStaticMeshComponent* MoonSphere(ATemporaryMoonLight* M) { return M->Sphere; }
    static UDirectionalLightComponent* MoonLight(ATemporaryMoonLight* M) { return M->MoonLight; }
    static void Prepare(UWorld* W)
    {
        W->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
        W->SetGameMode(FURL());
        W->InitializeActorsForPlay(FURL());
    }
    static void SunAt(URealTimeSunSubsystem* S, const FDateTime& Now) { S->UpdateSun(Now); }
    static void Sample(UBattleScheduleSubsystem* S, UWorld* W, const FDateTime& Previous, const FDateTime& Now)
    {
        S->PreviousTime = Previous;
        S->PreviousWorld = W;
        S->StartMinutes = {590};
        S->HandleTime(Now);
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTimeCsvTest,"Gladius.RealTime.CsvValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTimeCsvTest::RunTest(const FString& Parameters)
{
    TArray<int32> Times;
    FString Error;
    TestTrue(TEXT("Quoted values, blanks, CRLF, duplicates"), UBattleScheduleSubsystem::ParseSchedule(TEXT("StartTime\r\n\"17:50\"\r\n\r\n09:50\r\n09:50\r\n"), Times, Error));
    TestEqual(TEXT("Deduplicated"), Times.Num(), 2);
    if (Times.Num() == 2) { TestEqual(TEXT("Sorted first"), Times[0], 590); TestEqual(TEXT("Sorted last"), Times[1], 1070); }
    for (const FString& Bad : {TEXT("StartTime\n24:00"), TEXT("StartTime\n09:60"), TEXT("StartTime\n9:50"), TEXT("StartTime\n12:ab"), TEXT("StartTime\n09:50,10:50"), TEXT("09:50"), TEXT("")})
        TestFalse(*Bad, UBattleScheduleSubsystem::ParseSchedule(Bad, Times, Error));
    TestEqual(TEXT("Failure preserves previous schedule"), Times.Num(), 2);
    FString Csv;
    TestTrue(TEXT("External CSV exists"), FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("ExternalData/BattleSchedule.csv"))));
    TestTrue(TEXT("Default CSV parses"), UBattleScheduleSubsystem::ParseSchedule(Csv, Times, Error));
    TestEqual(TEXT("09:50 through 17:50 inclusive"), Times.Num(), 9);
    for (int32 I = 0; I < Times.Num(); ++I) TestEqual(TEXT("Hourly :50"), Times[I], (9+I)*60+50);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTimeBoundaryTest,"Gladius.RealTime.ScheduleBoundaries",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTimeBoundaryTest::RunTest(const FString& Parameters)
{
    const FDateTime Before(2026,10,24,9,49,59), At(2026,10,24,9,50,0), After(2026,10,24,9,50,1);
    TSet<int64> Consumed;
    const TArray<int32> Times{590};
    TestFalse(TEXT("Before time"), UBattleScheduleSubsystem::ConsumeDue(Before-FTimespan::FromSeconds(1), Before, Times, Consumed));
    TestTrue(TEXT("Crossing exact time"), UBattleScheduleSubsystem::ConsumeDue(Before, At, Times, Consumed));
    TestFalse(TEXT("Same minute no repeat"), UBattleScheduleSubsystem::ConsumeDue(At, After, Times, Consumed));
    TestFalse(TEXT("Clock rollback does not trigger"), UBattleScheduleSubsystem::ConsumeDue(After, Before, Times, Consumed));
    TestFalse(TEXT("Crossing again after rollback deduplicated"), UBattleScheduleSubsystem::ConsumeDue(Before, At, Times, Consumed));
    TestTrue(TEXT("Next day triggers again"), UBattleScheduleSubsystem::ConsumeDue(Before+FTimespan::FromDays(1), At+FTimespan::FromDays(1), Times, Consumed));
    Consumed.Reset();
    TestFalse(TEXT("Sleep/large jump no catch-up"), UBattleScheduleSubsystem::ConsumeDue(Before-FTimespan::FromHours(2), After, Times, Consumed));
    TestFalse(TEXT("Late entry no catch-up"), UBattleScheduleSubsystem::ConsumeDue(At, After, Times, Consumed));
    TestTrue(TEXT("Midnight works"), UBattleScheduleSubsystem::ConsumeDue(FDateTime(2026,10,24,23,59,59), FDateTime(2026,10,25,0,0,1), {0}, Consumed));
    TestFalse(TEXT("Missing CSV produces empty schedule"), UBattleScheduleSubsystem::ConsumeDue(Before, At, {}, Consumed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTimeSunTest,"Gladius.RealTime.SunOrbit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTimeSunTest::RunTest(const FString& Parameters)
{
    FRealTimeSunProfile P;
    P.NoonAltitude = 60; P.NoonDirection = 35;
    const FVector Noon = URealTimeSunSubsystem::CalculateSunDirection(12, P);
    TestTrue(TEXT("Noon altitude"), FMath::IsNearlyEqual(Noon.Rotation().Pitch, 60., 0.001));
    TestTrue(TEXT("Noon direction"), FMath::IsNearlyEqual(Noon.Rotation().Yaw, 35., 0.001));
    TestTrue(TEXT("Morning horizon"), FMath::IsNearlyZero(URealTimeSunSubsystem::CalculateSunDirection(6,P).Z, 0.001));
    TestTrue(TEXT("Evening horizon"), FMath::IsNearlyZero(URealTimeSunSubsystem::CalculateSunDirection(18,P).Z, 0.001));
    TestTrue(TEXT("Midnight below horizon"), URealTimeSunSubsystem::CalculateSunDirection(0,P).Z < 0);
    TestTrue(TEXT("Continuous midnight orbit"), URealTimeSunSubsystem::CalculateSunDirection(0,P).Equals(URealTimeSunSubsystem::CalculateSunDirection(24,P), 0.001));
    FRealTimeSunProfile Other; Other.NoonAltitude=25; Other.NoonDirection=150;
    TestFalse(TEXT("Independent profiles"), Noon.Equals(URealTimeSunSubsystem::CalculateSunDirection(12,Other), 0.001));
    P.NoonAltitude = 90;
    TestTrue(TEXT("Zenith is finite"), URealTimeSunSubsystem::CalculateSunDirection(12,P).Equals(FVector::UpVector,0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTimeSunWorldTest,"Gladius.RealTime.SunWorldIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTimeSunWorldTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI=NewObject<UGameInstance>(GEngine);
    GI->AddToRoot(); GI->InitializeStandalone();
    UWorld* W=GI->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
    W->EnableActorTickAndUserCallbacks(true);
#endif
    FRealTimeTestAccess::Prepare(W);
    ADirectionalLight* Actor=W->SpawnActor<ADirectionalLight>();
    UDirectionalLightComponent* Light=CastChecked<UDirectionalLightComponent>(Actor->GetLightComponent());
    URealTimeSettings* Settings=GetMutableDefault<URealTimeSettings>();
    TGuardValue<TSoftObjectPtr<UWorld>> BattleGuard(Settings->BattleMap,TSoftObjectPtr<UWorld>(W));
    TGuardValue<TSoftObjectPtr<UWorld>> TempleGuard(Settings->TempleMap,Settings->TempleMap);
    TGuardValue<bool> EnabledGuard(Settings->bEnableSun,true);
    TGuardValue<bool> MoonGuard(Settings->bEnableTemporaryMoon,true);
    TGuardValue<FRealTimeSunProfile> ProfileGuard(Settings->BattleSun,Settings->BattleSun);
    TGuardValue<FRealTimeSunProfile> TempleProfileGuard(Settings->TempleSun,Settings->TempleSun);
    Settings->BattleSun.NoonAltitude=60; Settings->BattleSun.NoonDirection=35; Settings->BattleSun.SunActorTag=NAME_None;
    Settings->TempleSun.NoonAltitude=25; Settings->TempleSun.NoonDirection=150; Settings->TempleSun.SunActorTag=NAME_None;
    W->BeginPlay();
    URealTimeSunSubsystem* S=W->GetSubsystem<URealTimeSunSubsystem>();
    const FDateTime Noon(2026,10,24,12,0,0);
    FRealTimeTestAccess::SunAt(S,Noon);
    TestTrue(TEXT("Battle sun rotates actual light"),Light->GetForwardVector().Equals(-URealTimeSunSubsystem::CalculateSunDirection(12,Settings->BattleSun),0.001));
    TestEqual(TEXT("Sun supports runtime rotation"),Light->Mobility,EComponentMobility::Movable);
    ATemporaryMoonLight* Moon=FRealTimeTestAccess::Moon(S);
    if (TestNotNull(TEXT("Temporary moon created automatically"),Moon))
    {
        auto* Globe=FRealTimeTestAccess::MoonSphere(Moon);
        auto* MoonLight=FRealTimeTestAccess::MoonLight(Moon);
        TestFalse(TEXT("Moon sphere hidden below daytime horizon"),Globe->IsVisible());
        TestEqual(TEXT("No moon illumination at noon"),MoonLight->Intensity,0.f);
        FRealTimeTestAccess::SunAt(S,FDateTime(2026,10,24,0,0,0));
        TestTrue(TEXT("Night sphere is at zenith"),Globe->GetRelativeLocation().GetSafeNormal().Equals(FVector::UpVector,0.001));
        TestTrue(TEXT("Night moon sphere is visible"),Globe->IsVisible());
        TestTrue(TEXT("Night moon illuminates entire map"),MoonLight->Intensity > 0);
        TestTrue(TEXT("Night light shines straight down"),MoonLight->GetForwardVector().Equals(-FVector::UpVector,0.001));
        for (int32 Hour : {19, 23, 1, 5})
        {
            FRealTimeTestAccess::SunAt(S,FDateTime(2026,10,24,Hour,0,0));
            TestTrue(TEXT("Sphere stays overhead throughout the night"),Globe->GetRelativeLocation().GetSafeNormal().Equals(FVector::UpVector,0.001));
            TestTrue(TEXT("Night sphere remains visible"),Globe->IsVisible());
        }
        const FLinearColor Color=MoonLight->GetLightColor();
        TestTrue(TEXT("Moon emits yellow light"),Color.R > Color.B && Color.G > Color.B);
        TestTrue(TEXT("Sphere has emissive material"),Globe->GetMaterial(0) != nullptr);
        TestFalse(TEXT("Sphere never blocks gameplay"),Globe->IsCollisionEnabled());
    }
    Settings->BattleMap=Settings->TempleMap;
    Settings->TempleMap=TSoftObjectPtr<UWorld>(W);
    FRealTimeTestAccess::SunAt(S,Noon);
    TestTrue(TEXT("Temple selects its independent profile"),Light->GetForwardVector().Equals(-URealTimeSunSubsystem::CalculateSunDirection(12,Settings->TempleSun),0.001));
    const FQuat Before=Light->GetComponentQuat();
    Settings->bEnableSun=false;
    FRealTimeTestAccess::SunAt(S,FDateTime(2026,10,24,18,0,0));
    TestTrue(TEXT("Disabled sun keeps rotation"),Light->GetComponentQuat().Equals(Before,0.001));
    TestNull(TEXT("Disabling clock lighting removes temporary moon"),FRealTimeTestAccess::Moon(S));
    Actor->Destroy();
    W->EndPlay(EEndPlayReason::Quit);
    GI->Shutdown(); GEngine->DestroyWorldContext(W); W->DestroyWorld(false); GI->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTimeFallbackTest,"Gladius.RealTime.TempleWithoutController",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTimeFallbackTest::RunTest(const FString& Parameters)
{
    // Each completed transition leaves its world; test both fallback routes in separate worlds.
    for (const bool bExistingManager : {false, true})
    {
        UGameInstance* GI=NewObject<UGameInstance>(GEngine);
        GI->AddToRoot(); GI->InitializeStandalone();
        UWorld* W=GI->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
        W->EnableActorTickAndUserCallbacks(true);
#endif
        FRealTimeTestAccess::Prepare(W);
        URealTimeSettings* Settings=GetMutableDefault<URealTimeSettings>();
        TGuardValue<TSoftObjectPtr<UWorld>> MapGuard(Settings->TempleMap,TSoftObjectPtr<UWorld>(W));
        TGuardValue<bool> EnabledGuard(Settings->bEnableBattleSchedule,true);
        TGuardValue<bool> SunGuard(Settings->bEnableSun,false);
        AGameManager* Manager=bExistingManager ? W->SpawnActor<AGameManager>() : nullptr;
        W->BeginPlay();
        UBattleScheduleSubsystem* S=GI->GetSubsystem<UBattleScheduleSubsystem>();
        FRealTimeTestAccess::Sample(S,W,FDateTime(2026,10,24,9,49,59),FDateTime(2026,10,24,9,50,0));
        FWorldContext& Context=GEngine->GetWorldContextFromWorldChecked(W);
        if (!Manager) for (TActorIterator<AGameManager> It(W);It;++It) { Manager=*It;break; }
        TestNotNull(TEXT("Bare temple obtains a routing manager"),Manager);
        TestTrue(TEXT("Travel waits while temple outro plays"),Context.TravelURL.IsEmpty());
        if (Manager)
        {
            TestEqual(TEXT("Manager receives shared battle map"),Manager->CombatLevel.ToSoftObjectPath(),Settings->BattleMap.ToSoftObjectPath());
            TestEqual(TEXT("Temple state retained during outro"),Manager->GetGameState(),EGameState::Temple);
        }
        ULevelSequencePlayer* Player=nullptr;
        for (TActorIterator<ALevelSequenceActor> It(W);It;++It) if (It->GetSequencePlayer()) { Player=It->GetSequencePlayer();break; }
        if (TestNotNull(TEXT("Temple outro player created"),Player))
        {
            // Advance the real sequence to its natural end, then run deferred travel.
            for (int32 I=0;I<3;++I) Player->Update(10.f);
            W->GetTimerManager().Tick(0.1f);
            TestTrue(TEXT("Completed outro requests battle map"),Context.TravelURL.Contains(TEXT("/Game/Levels/Main/MainBattle")));
            if (Manager) TestEqual(TEXT("Manager enters combat after outro"),Manager->GetGameState(),EGameState::Combat);
        }
        Context.TravelURL.Reset();
        W->EndPlay(EEndPlayReason::Quit);
        GI->Shutdown(); GEngine->DestroyWorldContext(W); W->DestroyWorld(false); GI->RemoveFromRoot();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTimeTravelTest,"Gladius.RealTime.TempleTravelIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTimeTravelTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->AddToRoot(); GI->InitializeStandalone();
    UWorld* W = GI->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
    W->EnableActorTickAndUserCallbacks(true);
#endif
    FRealTimeTestAccess::Prepare(W);
    W->SpawnActor<AGameManager>();
    ABattleManager* Records = W->SpawnActor<ABattleManager>();
    FBattleCameraTestAccess::RecordsOnly(Records);
    W->SpawnActor<AGoddess>();
    ATempleCycleController* Controller = W->SpawnActor<ATempleCycleController>();
    Controller->StatusWidgetClass=nullptr; Controller->ChatLogWidgetClass=nullptr;
    Controller->bAutoTravelOnCombatRequest=false;
    URealTimeSettings* Settings = GetMutableDefault<URealTimeSettings>();
    TGuardValue<TSoftObjectPtr<UWorld>> MapGuard(Settings->TempleMap, TSoftObjectPtr<UWorld>(W));
    TGuardValue<bool> EnabledGuard(Settings->bEnableBattleSchedule, true);
    TGuardValue<bool> SunGuard(Settings->bEnableSun, false);
    // This fixture deliberately omits the external chat service.
    AddExpectedError(TEXT("TempleCycleController: YouTubeChatManager"), EAutomationExpectedErrorFlags::Contains, 1);
    W->BeginPlay();
    TestTrue(TEXT("World began play"), W->HasBegunPlay());
    TestTrue(TEXT("Temple controller began play"), Controller->HasActorBegunPlay());
    UBattleScheduleSubsystem* S = GI->GetSubsystem<UBattleScheduleSubsystem>();
    TestTrue(TEXT("Auto-created clock"), GI->GetSubsystem<URealTimeClockSubsystem>() != nullptr);
    TestTrue(TEXT("Auto-created sun consumer"), W->GetSubsystem<URealTimeSunSubsystem>() != nullptr);
    AActor* Probe = W->SpawnActor<AActor>();
    Controller->OnCombatRequested.AddDynamic(Probe, &AActor::K2_DestroyActor);
    Settings->bEnableBattleSchedule=false;
    FRealTimeTestAccess::Sample(S,W,FDateTime(2026,10,24,9,49,59),FDateTime(2026,10,24,9,50,0));
    TestFalse(TEXT("Disabled schedule never travels"), Probe->IsActorBeingDestroyed());
    Settings->bEnableBattleSchedule=true;
    Settings->TempleMap=Settings->BattleMap;
    FRealTimeTestAccess::Sample(S,W,FDateTime(2026,10,25,9,49,59),FDateTime(2026,10,25,9,50,0));
    TestFalse(TEXT("Outside temple never travels"), Probe->IsActorBeingDestroyed());
    Settings->TempleMap=TSoftObjectPtr<UWorld>(W);
    FRealTimeTestAccess::Sample(S,W,FDateTime(2026,10,25,9,50,0),FDateTime(2026,10,25,9,50,1));
    TestFalse(TEXT("Returning after missed slot never travels"), Probe->IsActorBeingDestroyed());
    FRealTimeTestAccess::Sample(S,W,FDateTime(2026,10,26,9,49,59),FDateTime(2026,10,26,9,50,0));
    TestTrue(TEXT("Due slot invokes existing controller travel event"), Probe->IsActorBeingDestroyed());
    W->GetTimerManager().Tick(45.f);
    TestEqual(TEXT("Scheduled request stopped temple cycles through existing travel path"),Controller->GetCompletedCycles(),0);
    Controller->Destroy();
    FBattleCameraTestAccess::Cleanup(W);
    W->EndPlay(EEndPlayReason::Quit);
    GI->Shutdown(); GEngine->DestroyWorldContext(W); W->DestroyWorld(false); GI->RemoveFromRoot();
    return true;
}
#endif

#include "TempleLightColorTest.inl"
