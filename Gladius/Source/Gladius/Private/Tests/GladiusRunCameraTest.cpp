#include "Scripts/Battle/GladiusRunCamera.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Scripts/Battle/CutSceneApplier.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace
{
struct FRunCameraWorld
{
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    UWorld* World;
    ABattleManager* Manager;
    ABattleUnit* Subject;
    ABattleUnit* OtherGladius;
    ABattleUnit* Sera;
    FRunCameraWorld()
    {
        Instance->AddToRoot();
        Instance->InitializeStandalone();
        World = Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
        World->EnableActorTickAndUserCallbacks(true);
#endif
        World->InitializeActorsForPlay(FURL());
        Manager = World->SpawnActor<ABattleManager>();
        FCharacterDef G;
        G.Id = TEXT("gladius");
        G.BaseHP = 100;
        FCharacterDef S = G;
        S.Id = TEXT("sera");
        Manager->Classes = { S, G }; // 직업은 고정 인덱스가 아니라 ID로 찾는다.
        Manager->DefaultCharacterId = G.Id;
        Manager->StartBattle();
        Manager->SubmitAction(TEXT("A"), TEXT("Subject"), EBattleAction::Attack1, G.Id);
        Manager->SubmitAction(TEXT("B"), TEXT("Other"), EBattleAction::Attack2, G.Id);
        Manager->SubmitAction(TEXT("C"), TEXT("Sera"), EBattleAction::Attack1, S.Id);
        Manager->StopBattle();
        Subject = Manager->GetBattlePlayers().FindChecked(TEXT("A")).Unit;
        OtherGladius = Manager->GetBattlePlayers().FindChecked(TEXT("B")).Unit;
        Sera = Manager->GetBattlePlayers().FindChecked(TEXT("C")).Unit;
        Subject->SetActorLocation(FVector(10, 20, 0));
        OtherGladius->SetActorLocation(FVector(110, 20, 0));
        Sera->SetActorLocation(FVector(10, 220, 0));
        Manager->GetBossUnit()->SetActorLocation(FVector(1010, 20, 0));
    }
    ~FRunCameraWorld()
    {
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Instance->RemoveFromRoot();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCameraCentroidTest, "Gladius.CutSceneApplier.GladiusRun.WeightedCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRunCameraCentroidTest::RunTest(const FString& Parameters)
{
    FRunCameraWorld Context;
    FGladiusRunWeights Weights;
    FVector Center;
    TestTrue(TEXT("Default center exists"), AGladiusRunCamera::CalculateWeightedCenter(*Context.Manager, *Context.Subject, Weights, Center));
    TestTrue(TEXT("Subject excluded, equal class weights, boss zero"), Center.Equals(FVector(60, 120, 0), 0.01));
    Weights.Gladius = 0.f;
    Weights.Sera = 0.f;
    Weights.Boss = 1.f;
    TestTrue(TEXT("Boss-only center exists"), AGladiusRunCamera::CalculateWeightedCenter(*Context.Manager, *Context.Subject, Weights, Center));
    TestTrue(TEXT("Boss weight applied"), Center.Equals(Context.Manager->GetBossUnit()->GetActorLocation()));
    Weights.Boss = 0.f;
    TestFalse(TEXT("Zero total weight has no center"), AGladiusRunCamera::CalculateWeightedCenter(*Context.Manager, *Context.Subject, Weights, Center));
    TestTrue(TEXT("Zero weight fallback finite"), Center.Equals(Context.Subject->GetActorLocation()));
    Weights.Gladius = 0.25f;
    Weights.Sera = 0.75f;
    TestTrue(TEXT("Unequal weights center exists"), AGladiusRunCamera::CalculateWeightedCenter(*Context.Manager, *Context.Subject, Weights, Center));
    TestTrue(TEXT("Weighted mean, not an unnormalized sum"), Center.Equals(FVector(35, 170, 0), 0.01));
    Context.OtherGladius->Destroy();
    AGladiusRunCamera::CalculateWeightedCenter(*Context.Manager, *Context.Subject, Weights, Center);
    TestTrue(TEXT("Destroyed actors excluded"), Center.Equals(Context.Sera->GetActorLocation()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCameraSmoothingTest, "Gladius.CutSceneApplier.GladiusRun.AngularSmoothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRunCameraSmoothingTest::RunTest(const FString& Parameters)
{
    float Yaw = 0.f;
    float Velocity = 0.f;
    const float Dt = 1.f / 60.f;
    for (int32 Frame = 0; Frame < 600; ++Frame)
    {
        const float PreviousVelocity = Velocity;
        // 갑자기 반대편으로 중심이 바뀌는 경우까지 검증한다.
        const float Target = Frame < 120 ? 150.f : -90.f;
        AGladiusRunCamera::AdvanceYaw(Target, Dt, 60.f, 120.f, 4.f, Yaw, Velocity);
        TestTrue(TEXT("Speed bounded"), FMath::Abs(Velocity) <= 60.001f);
        TestTrue(TEXT("Velocity changes gradually"), FMath::Abs(Velocity - PreviousVelocity) <= 120.f * Dt + 0.001f);
    }
    TestTrue(TEXT("Eventually reaches new direction"), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, -90.f)) < 0.05f);
    Yaw = 179.f;
    Velocity = 0.f;
    AGladiusRunCamera::AdvanceYaw(-179.f, Dt, 60.f, 120.f, 4.f, Yaw, Velocity);
    TestTrue(TEXT("Shortest rotation across angle wrap"), Velocity > 0.f);
    float Yaw30 = 0.f, Velocity30 = 0.f, Yaw120 = 0.f, Velocity120 = 0.f;
    for (int32 Frame = 0; Frame < 30; ++Frame) { AGladiusRunCamera::AdvanceYaw(120.f, 1.f / 30.f, 60.f, 120.f, 4.f, Yaw30, Velocity30); }
    for (int32 Frame = 0; Frame < 120; ++Frame) { AGladiusRunCamera::AdvanceYaw(120.f, 1.f / 120.f, 60.f, 120.f, 4.f, Yaw120, Velocity120); }
    TestTrue(TEXT("Frame rate stable rotation"), FMath::Abs(Yaw30 - Yaw120) < 0.05f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCameraPlaybackTest, "Gladius.CutSceneApplier.GladiusRun.TrackingAndRestoration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRunCameraPlaybackTest::RunTest(const FString& Parameters)
{
    FRunCameraWorld Context;
    APlayerController* Controller = Context.World->SpawnActor<APlayerController>();
    AActor* GameCamera = Context.World->SpawnActor<AActor>();
    Controller->SetViewTarget(GameCamera);
    ACutSceneApplier* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Context.Manager;
    TestTrue(TEXT("Built-in GladiusRun starts without sequence asset"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultGladius, TEXT("GladiusRun")));
    AGladiusRunCamera* Camera = nullptr;
    for (TActorIterator<AGladiusRunCamera> It(Context.World); It; ++It) { if (IsValid(*It)) { Camera = *It; break; } }
    if (!TestNotNull(TEXT("Runtime tracking camera created"), Camera)) { return false; }
    TestEqual(TEXT("Tracking camera becomes view target"), Controller->GetViewTarget(), static_cast<AActor*>(Camera));
    TestTrue(TEXT("Camera tick compensates slow motion"), FMath::IsNearlyEqual(Camera->CustomTimeDilation, 10.f));
    auto CheckPose = [&]()
    {
        const FVector Focus = Context.Subject->GetActorLocation() + FVector(0, 0, Camera->FocusHeight);
        TestTrue(TEXT("Fixed distance maintained"), FMath::IsNearlyEqual(FVector::Dist(Camera->GetActorLocation(), Focus), static_cast<double>(Camera->CameraDistance), 0.01));
        TestTrue(TEXT("Subject on optical axis"), (Focus - Camera->GetActorLocation()).GetSafeNormal().Equals(Camera->GetActorForwardVector(), 0.0001));
        TestTrue(TEXT("Pitch fixed at minus twenty"), FMath::IsNearlyEqual(Camera->GetActorRotation().Pitch, -20.f, 0.001f));
    };
    CheckPose();
    Context.Subject->SetActorLocation(FVector(500, -250, 50));
    Context.Sera->SetActorLocation(FVector(-500, 1000, 0));
    Camera->Tick(1.f / 60.f);
    CheckPose();
    const float YawBeforeZeroWeights = Camera->GetActorRotation().Yaw;
    Camera->Weights.Gladius = Camera->Weights.Sera = Camera->Weights.Boss = 0.f;
    Camera->Tick(1.f / 60.f);
    CheckPose();
    TestTrue(TEXT("Zero weights do not snap to world origin"), FMath::Abs(FMath::FindDeltaAngleDegrees(YawBeforeZeroWeights, Camera->GetActorRotation().Yaw)) < 1.f);
    for (int32 Frame = 0; Frame < 600 && Applier->IsPlaying(); ++Frame) { Camera->Tick(1.f / 60.f); }
    TestTrue(TEXT("No default duration: still running after ten seconds"), Applier->IsPlaying());
    TestFalse(TEXT("Invalid replacement rejected"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultGladius, TEXT("MissingShot")));
    TestTrue(TEXT("Invalid replacement preserves existing shot"), Applier->IsPlaying());
    TestEqual(TEXT("Invalid request preserves view target"), Controller->GetViewTarget(), static_cast<AActor*>(Camera));
    FCutSceneDefinition NextShot;
    NextShot.CameraClass = AGladiusRunCamera::StaticClass();
    Applier->CutScenes.Add(TEXT("NextShot"), NextShot);
    TestTrue(TEXT("Next request replaces indefinite shot"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultSera, TEXT("NextShot")));
    TestFalse(TEXT("Previous camera destroyed"), IsValid(Camera));
    TestEqual(TEXT("Replacement has new selected subject"), Applier->GetCurrentSubject(), Context.Sera);
    TestTrue(TEXT("Previous subject time restored on replacement"), FMath::IsNearlyEqual(Context.Subject->CustomTimeDilation, 1.f));
    for (TActorIterator<AGladiusRunCamera> It(Context.World); It; ++It) { if (IsValid(*It)) { Camera = *It; break; } }
    if (!TestTrue(TEXT("Replacement camera exists"), IsValid(Camera))) { return false; }
    TestEqual(TEXT("View transferred to replacement"), Controller->GetViewTarget(), static_cast<AActor*>(Camera));
    TestTrue(TEXT("Replacement applies slow motion once, not cumulatively"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.1f));
    Camera->DurationSeconds = 0.25f;
    for (int32 Frame = 0; Frame < 20 && Applier->IsPlaying(); ++Frame) { Camera->Tick(1.f / 60.f); }
    TestFalse(TEXT("Explicit positive duration still ends automatically"), Applier->IsPlaying());
    TestTrue(TEXT("World time restored"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 1.f));
    TestTrue(TEXT("Subject time restored"), FMath::IsNearlyEqual(Context.Subject->CustomTimeDilation, 1.f));
    TestEqual(TEXT("Game camera restored"), Controller->GetViewTarget(), GameCamera);
    TestTrue(TEXT("Can replay tracking camera"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultGladius, TEXT("GladiusRun")));
    Applier->CancelCutScene();
    TestFalse(TEXT("Cancel restores tracking shot too"), Applier->IsPlaying());
    TestEqual(TEXT("Cancel restores original view target"), Controller->GetViewTarget(), GameCamera);
    return true;
}
#endif
