#include "Scripts/Battle/GladiusSkill1Camera.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/CutSceneApplier.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"

namespace
{
struct FSkillCameraWorld
{
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    UWorld* World;
    ABattleManager* Manager;
    ABattleUnit* Subject;
    FSkillCameraWorld()
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
        Manager->Classes = { G };
        Manager->DefaultCharacterId = G.Id;
        Manager->StartBattle();
        Manager->SubmitAction(TEXT("A"), TEXT("A"), EBattleAction::Attack1, G.Id);
        Manager->StopBattle();
        Subject = Manager->GetBattlePlayers().FindChecked(TEXT("A")).Unit;
        Subject->SetActorLocation(FVector(0, 0, 50));
        Manager->GetBossUnit()->SetActorLocation(FVector(1000, 0, 50));
    }
    UStaticMeshComponent* AddBody(const TCHAR* Name, float RelativeHeight)
    {
        UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Subject, FName(Name));
        Subject->AddInstanceComponent(Body);
        Body->SetupAttachment(Subject->GetRootComponent());
        Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Body->SetRelativeLocation(FVector(0, 0, RelativeHeight));
        Body->SetRelativeScale3D(FVector(1, 1, 2));
        Body->RegisterComponent();
        return Body;
    }
    ~FSkillCameraWorld()
    {
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Instance->RemoveFromRoot();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCameraGeometryTest, "Gladius.CutSceneApplier.GladiusSkill1.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillCameraGeometryTest::RunTest(const FString& Parameters)
{
    const FVector Origin(100, 200, 50);
    const FTransform Frame(FRotator::ZeroRotator, Origin);
    FTransform Pose = AGladiusSkill1Camera::CalculatePose(Frame, 500, 170, 70, 0, 1.5f, 1.1f);
    const FVector Offset = Pose.GetLocation() - Origin;
    TestTrue(TEXT("Horizontal distance is 500 cm"), FMath::IsNearlyEqual(Offset.Size2D(), 500.0, 0.001));
    TestTrue(TEXT("Camera sits seventy degrees to right of front"), FMath::IsNearlyEqual(Offset.Rotation().Yaw, 70.0, 0.001));
    TestTrue(TEXT("Initial height is 170 cm"), FMath::IsNearlyEqual(Offset.Z, 170.0, 0.001));
    TestTrue(TEXT("Initial camera is horizontal"), FMath::IsNearlyZero(Pose.Rotator().Pitch, 0.001));
    Pose = AGladiusSkill1Camera::CalculatePose(Frame, 500, 170, 70, 20, 1.5f, 1.1f);
    TestTrue(TEXT("Twenty cm duck lowers camera by thirty cm"), FMath::IsNearlyEqual(Pose.GetLocation().Z - Origin.Z, 140.0, 0.001));
    const FVector Focus = Origin + FVector(0, 0, 148);
    TestTrue(TEXT("Optical axis passes through height 148 cm"), (Focus - Pose.GetLocation()).GetSafeNormal().Equals(Pose.GetRotation().GetForwardVector(), 0.0001));
    const FTransform Rotated(FRotator(0, 90, 0), Origin);
    Pose = AGladiusSkill1Camera::CalculatePose(Rotated, 500, 170, 70, 0, 1.5f, 1.1f);
    TestTrue(TEXT("Angle follows battle coordinate frame"), FMath::IsNearlyEqual((Pose.GetLocation() - Origin).Rotation().Yaw, 160.0, 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCameraMeshFallbackTest, "Gladius.CutSceneApplier.GladiusSkill1.VisibleMeshFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillCameraMeshFallbackTest::RunTest(const FString& Parameters)
{
    FSkillCameraWorld Context;
    UStaticMeshComponent* Body = Context.AddBody(TEXT("Body"), 100.f);
    UStaticMeshComponent* Hidden = Context.AddBody(TEXT("HiddenMesh"), 1000.f);
    Hidden->SetHiddenInGame(true);
    ACutSceneApplier* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Context.Manager;
    TestTrue(TEXT("Built-in skill starts using visible static mesh"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultGladius, TEXT("GladiusSkill1")));
    AGladiusSkill1Camera* Camera = nullptr;
    for (TActorIterator<AGladiusSkill1Camera> It(Context.World); It; ++It) { if (IsValid(*It)) { Camera = *It; break; } }
    if (!TestNotNull(TEXT("Skill camera exists"), Camera)) { return false; }
    TestFalse(TEXT("Missing pelvis uses mesh top"), Camera->IsUsingPelvis());
    TestTrue(TEXT("Visible mesh top excludes hidden objects and nameplate"), FMath::IsNearlyEqual(Camera->InitialCharacterHeight, 200.f, 0.01f));
    Context.Subject->AddActorWorldOffset(FVector(0, 0, 300));
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Whole-character vertical motion is not ducking"), FMath::IsNearlyZero(Camera->Delta, 0.01f));
    Body->SetRelativeLocation(FVector(0, 0, 80));
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Visible top displacement produces twenty cm delta"), FMath::IsNearlyEqual(Camera->Delta, 20.f, 0.01f));
    TestTrue(TEXT("Camera lowers by thirty cm relative to feet"), FMath::IsNearlyEqual(Camera->GetActorLocation().Z - Context.Subject->GetActorLocation().Z, 140.0, 0.01));
    Body->SetRelativeLocation(FVector(0, 0, 110));
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Rising above initial height does not produce negative duck"), FMath::IsNearlyZero(Camera->Delta, 0.01f));
    TestTrue(TEXT("Camera returns to original height"), FMath::IsNearlyEqual(Camera->GetActorLocation().Z - Context.Subject->GetActorLocation().Z, 170.0, 0.01));
    for (int32 Index = 0; Index < 4 && Applier->IsPlaying(); ++Index) { Camera->Tick(1.f); }
    TestTrue(TEXT("Skill has no implicit duration"), Applier->IsPlaying());
    Applier->CancelCutScene();
    TestFalse(TEXT("Skill cancels normally"), Applier->IsPlaying());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCameraPelvisTest, "Gladius.CutSceneApplier.GladiusSkill1.PelvisTracking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillCameraPelvisTest::RunTest(const FString& Parameters)
{
    FSkillCameraWorld Context;
    Context.AddBody(TEXT("Body"), 200.f); // 관절이 있으면 상단 높이보다 관절을 우선한다.
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Asset/Battle/Sera/Modeling/Sera_Rigged_Final.Sera_Rigged_Final"));
    if (!TestNotNull(TEXT("Project rig for bone test exists"), Mesh)) { return false; }
    UPoseableMeshComponent* Pose = NewObject<UPoseableMeshComponent>(Context.Subject, TEXT("BoneSource"));
    Context.Subject->AddInstanceComponent(Pose);
    Pose->SetupAttachment(Context.Subject->GetRootComponent());
    Pose->SetSkinnedAssetAndUpdate(Mesh);
    Pose->RegisterComponent();
    const FName Pelvis(TEXT("jnt_pelvis01"));
    Pose->SetBoneLocationByName(Pelvis, Context.Subject->GetActorLocation() + FVector(0, 0, 100), EBoneSpaces::WorldSpace);
    Pose->RefreshBoneTransforms();
    AGladiusSkill1Camera* Camera = Context.World->SpawnActor<AGladiusSkill1Camera>();
    Camera->PelvisBoneName = Pelvis;
    TestTrue(TEXT("Rigged camera starts"), Camera->StartCutScene(Context.Manager, Context.Subject));
    TestTrue(TEXT("Pelvis preferred over mesh bounds"), Camera->IsUsingPelvis());
    TestTrue(TEXT("Initial pelvis relative height captured"), FMath::IsNearlyEqual(Camera->InitialCharacterHeight, 100.f, 0.1f));
    Pose->SetBoneLocationByName(Pelvis, Context.Subject->GetActorLocation() + FVector(0, 0, 80), EBoneSpaces::WorldSpace);
    Pose->RefreshBoneTransforms();
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Pelvis movement produces duck delta"), FMath::IsNearlyEqual(Camera->Delta, 20.f, 0.1f));
    Camera->StopCutScene();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCameraActualGladiusTest, "Gladius.CutSceneApplier.GladiusSkill1.ActualGladiusHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillCameraActualGladiusTest::RunTest(const FString& Parameters)
{
    FSkillCameraWorld Context;
    UClass* Class = LoadClass<ABattleUnit>(nullptr, TEXT("/Game/Dev/BP_BattleUnitG.BP_BattleUnitG_C"));
    if (!TestNotNull(TEXT("Actual Gladius blueprint exists"), Class)) { return false; }
    ABattleUnit* Unit = Context.World->SpawnActor<ABattleUnit>(Class);
    if (!TestNotNull(TEXT("Actual Gladius spawns"), Unit)) { return false; }
    Unit->DispatchBeginPlay();
    AGladiusSkill1Camera* Camera = Context.World->SpawnActor<AGladiusSkill1Camera>();
    TestTrue(TEXT("Actual Gladius has a measurable visible mesh"), Camera->StartCutScene(Context.Manager, Unit));
    TestTrue(TEXT("Actual Gladius height is positive"), Camera->InitialCharacterHeight > 0.f);
    AddInfo(FString::Printf(TEXT("Actual Gladius initial relative height: %.2f cm, pelvis: %s"),
        Camera->InitialCharacterHeight, Camera->IsUsingPelvis() ? TEXT("yes") : TEXT("no")));
    Camera->StopCutScene();
    return true;
}
#endif
