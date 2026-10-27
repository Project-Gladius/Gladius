#include "Scripts/Battle/CutSceneApplier.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BattleCameraTestAccess.h"
#include "Scripts/Battle/CutSceneCamera.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "Camera/CameraComponent.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "MovieSceneTrack.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "SkeletalRenderPublic.h"
#include "MovieSceneSequenceTickManager.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Misc/AutomationTest.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "Materials/MaterialExpressionTextureSample.h"

namespace
{
struct FCutSceneTestWorld
{
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    UWorld* World;
    FCutSceneTestWorld()
    {
        Instance->AddToRoot();
        Instance->InitializeStandalone();
        World = Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
        World->EnableActorTickAndUserCallbacks(true);
#endif
        World->InitializeActorsForPlay(FURL());
    }
    ~FCutSceneTestWorld()
    {
        FBattleCameraTestAccess::Cleanup(World);
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Instance->RemoveFromRoot();
    }
};

bool AdvanceCurrentShot(UWorld* World, ACutSceneApplier* Applier)
{
    ULevelSequencePlayer* Player = nullptr;
    for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
    {
        if (IsValid(*It)) { Player = It->GetSequencePlayer(); break; }
    }
    if (!Player) { return false; }
    ABattleUnit* Subject = Applier->GetCurrentSubject();
    for (int32 Index = 0; Index < 5 && Applier->IsPlaying() && Applier->GetCurrentSubject() == Subject; ++Index)
    {
        Player->Update(0.05f);
        UMovieSceneSequenceTickManager* TickManager = Player->GetTickManager();
        const auto Runner = TickManager->GetRunner(FMovieSceneSequenceTickInterval());
        if (Runner) { Runner->Flush(); }
        TickManager->RunLatentActions();
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutSceneSplitSelectionTest, "Gladius.CutSceneApplier.ClassStrategySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCutSceneSplitSelectionTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    TMap<FString, FBattlePlayer> Players;
    auto Add = [&](const TCHAR* Id, int32 ClassIndex, float Fever, EBattleAction Action)
    {
        FBattlePlayer& P = Players.Add(Id);
        P.Unit = Context.World->SpawnActor<ABattleUnit>();
        P.HP = P.MaxHP = 100;
        P.ClassIndex = ClassIndex;
        P.Fever = Fever;
        P.PendingAction = Action;
        return P.Unit.Get();
    };
    Add(TEXT("GladiusA"), 1, 20, EBattleAction::Attack1);
    ABattleUnit* Gladius = Add(TEXT("GladiusB"), 1, 80, EBattleAction::Attack2);
    Add(TEXT("GladiusDefender"), 1, 1000, EBattleAction::Defense);
    ABattleUnit* Sera = Add(TEXT("SeraA"), 0, 300, EBattleAction::Attack1);
    Add(TEXT("SeraB"), 0, 300, EBattleAction::Attack2);
    Add(TEXT("DeadGladius"), 1, 2000, EBattleAction::Attack1);
    Players[TEXT("DeadGladius")].HP = 0;
    Add(TEXT("RevivingSera"), 0, 2000, EBattleAction::Attack1);
    Players[TEXT("RevivingSera")].ReviveCountdown = 1;
    TestEqual(TEXT("Gladius only picks highest fever attacking Gladius"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::DefaultGladius, FVector::ZeroVector, 1), Gladius);
    TestEqual(TEXT("Sera only picks highest fever attacking Sera, stable tie break"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::DefaultSera, FVector::ZeroVector, 0), Sera);
    TestNull(TEXT("Unresolved class does not fall back to another class"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::DefaultGladius, FVector::ZeroVector));
    Players[TEXT("GladiusA")].PendingAction = EBattleAction::Defense;
    Players[TEXT("GladiusB")].PendingAction = EBattleAction::AvoidL;
    TestNull(TEXT("No attacking Gladius means no Gladius cutscene"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::DefaultGladius, FVector::ZeroVector, 1));
    TestEqual(TEXT("Sera remains independently selectable"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::DefaultSera, FVector::ZeroVector, 0), Sera);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutSceneSplitPlaybackTest, "Gladius.CutSceneApplier.ClassStrategyPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCutSceneSplitPlaybackTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    ABattleManager* Manager = Context.World->SpawnActor<ABattleManager>();
    FCharacterDef GladiusClass;
    GladiusClass.Id = TEXT("gladius");
    GladiusClass.BaseHP = 100;
    FCharacterDef SeraClass = GladiusClass;
    SeraClass.Id = TEXT("sera");
    // 실제 직업 ID로 조회하는지 확인하기 위해 통상 순서를 뒤집는다.
    Manager->Classes = { SeraClass, GladiusClass };
    Manager->DefaultCharacterId = GladiusClass.Id;
    FBattleCameraTestAccess::Prepare(Manager);
    TestTrue(TEXT("Gladius joined"), Manager->SubmitAction(TEXT("G"), TEXT("G"), EBattleAction::Attack1, GladiusClass.Id));
    TestTrue(TEXT("Sera joined"), Manager->SubmitAction(TEXT("S"), TEXT("S"), EBattleAction::Attack2, SeraClass.Id));
    Manager->StopBattle();
    ABattleUnit* Gladius = Manager->GetBattlePlayers().FindChecked(TEXT("G")).Unit;
    ABattleUnit* Sera = Manager->GetBattlePlayers().FindChecked(TEXT("S")).Unit;
    ACutSceneApplier* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Manager;
    auto AddSequence = [&](FName Type)
    {
        ULevelSequence* Sequence = NewObject<ULevelSequence>(Applier);
        Sequence->Initialize();
        Sequence->GetMovieScene()->SetPlaybackRange(0, 24000);
        FCutSceneDefinition& Definition = Applier->CutScenes.Add(Type);
        Definition.Sequence = Sequence;
        Definition.SubjectBindingTag = Definition.BossBindingTag = NAME_None;
        return Sequence;
    };
    ULevelSequence* GladiusSequence = AddSequence(TEXT("GladiusShot"));
    ULevelSequence* SeraSequence = AddSequence(TEXT("SeraShot"));
    TestEqual(TEXT("Default query selects Gladius"), Applier->FindCharacter(), Gladius);
    TestEqual(TEXT("Sera query resolves ID rather than array position"),
        Applier->FindCharacter(ECutSceneCharacterStrategy::DefaultSera), Sera);
    TestTrue(TEXT("Gladius shot starts"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultGladius, TEXT("GladiusShot")));
    TestEqual(TEXT("Only Gladius is current"), Applier->GetCurrentSubject(), Gladius);
    for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It)
    {
        if (IsValid(*It)) { TestEqual(TEXT("Uses requested Gladius sequence"), It->GetSequence(), GladiusSequence); }
    }
    AdvanceCurrentShot(Context.World, Applier);
    TestFalse(TEXT("Finishing Gladius does not automatically play Sera"), Applier->IsPlaying());
    TestTrue(TEXT("World restored after Gladius"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 1.f));
    TestTrue(TEXT("Independent Sera shot starts"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultSera, TEXT("SeraShot")));
    TestEqual(TEXT("Only Sera is current"), Applier->GetCurrentSubject(), Sera);
    for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It)
    {
        if (IsValid(*It)) { TestEqual(TEXT("Uses requested Sera sequence"), It->GetSequence(), SeraSequence); }
    }
    AdvanceCurrentShot(Context.World, Applier);
    TestFalse(TEXT("Sera finishes independently"), Applier->IsPlaying());
    Manager->Classes.RemoveAt(1);
    TestNull(TEXT("Missing Gladius definition does not select Sera"), Applier->FindCharacter(ECutSceneCharacterStrategy::DefaultGladius));
    TestFalse(TEXT("Missing job refuses playback"), Applier->ApplyCutScene(ECutSceneCharacterStrategy::DefaultGladius, TEXT("GladiusShot")));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutSceneSelectionTest, "Gladius.CutSceneApplier.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCutSceneSelectionTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    TMap<FString, FBattlePlayer> Players;
    auto Add = [&](const TCHAR* Id, int32 HP, int32 MaxHP, float Fever, const FVector& Location)
    {
        FBattlePlayer& P = Players.Add(Id);
        P.Unit = Context.World->SpawnActor<ABattleUnit>();
        P.Unit->SetActorLocation(Location);
        P.HP = HP;
        P.MaxHP = MaxHP;
        P.Fever = Fever;
        return P.Unit.Get();
    };
    ABattleUnit* A = Add(TEXT("A"), 50, 100, 20, FVector(200, 0, 0));
    ABattleUnit* B = Add(TEXT("B"), 100, 1000, 80, FVector(100, 0, 0));
    Add(TEXT("Dead"), 0, 100, 1000, FVector::ZeroVector);
    Add(TEXT("Reviving"), 100, 100, 1000, FVector::ZeroVector);
    Players[TEXT("Reviving")].ReviveCountdown = 2;
    Players.Add(TEXT("MissingActor")).HP = 100;
    TestEqual(TEXT("Highest fever among live participants"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::HighestFever, FVector::ZeroVector), B);
    TestEqual(TEXT("Lowest HP ratio, not lowest absolute HP"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::LowestHealthRatio, FVector::ZeroVector), B);
    TestEqual(TEXT("Closest to boss"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::ClosestToBoss, FVector::ZeroVector), B);
    Players[TEXT("B")].Fever = 20;
    TestEqual(TEXT("Ties are resolved by UserId"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::HighestFever, FVector::ZeroVector), A);
    for (int32 Index = 0; Index < 30; ++Index)
    {
        ABattleUnit* Picked = ACutSceneApplier::SelectCharacter(
            Players, ECutSceneCharacterStrategy::RandomAlive, FVector::ZeroVector);
        TestTrue(TEXT("Random never picks dead/reviving/missing actors"), Picked == A || Picked == B);
    }
    Players[TEXT("B")].Unit->Destroy();
    TestEqual(TEXT("Destroyed actor excluded"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::ClosestToBoss, FVector::ZeroVector), A);
    Players[TEXT("A")].HP = 0;
    TestNull(TEXT("No live participants"), ACutSceneApplier::SelectCharacter(
        Players, ECutSceneCharacterStrategy::RandomAlive, FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutSceneOriginTest, "Gladius.CutSceneApplier.Coordinates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCutSceneOriginTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    ABattleUnit* Subject = Context.World->SpawnActor<ABattleUnit>();
    ABattleUnit* Boss = Context.World->SpawnActor<ABattleUnit>();
    Subject->SetActorLocation(FVector(100, 200, 300));
    Subject->SetActorScale3D(FVector(3));
    Boss->SetActorLocation(FVector(100, 1200, 900));
    FTransform Origin = ACutSceneApplier::MakeOrigin(*Subject, *Boss);
    TestTrue(TEXT("Origin at subject"), Origin.GetLocation().Equals(Subject->GetActorLocation()));
    TestTrue(TEXT("+X points horizontally toward boss"), Origin.TransformVectorNoScale(FVector::ForwardVector).Equals(FVector::RightVector));
    TestTrue(TEXT("Subject scale does not scale camera movement"), Origin.GetScale3D().Equals(FVector::OneVector));
    TestTrue(TEXT("Local point transformed into world"), Origin.TransformPosition(FVector(-300, 100, 150)).Equals(FVector(0, -100, 450), 0.01));
    Boss->SetActorLocation(Subject->GetActorLocation());
    Subject->SetActorRotation(FRotator(0, 45, 0));
    Origin = ACutSceneApplier::MakeOrigin(*Subject, *Boss);
    TestTrue(TEXT("Overlapping actors use subject facing"), Origin.GetRotation().Rotator().Equals(FRotator(0, 45, 0), 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutSceneLifecycleTest, "Gladius.CutSceneApplier.PlaybackAndRestoration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCutSceneLifecycleTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    ABattleManager* Manager = Context.World->SpawnActor<ABattleManager>();
    FCharacterDef Character;
    Character.Id = TEXT("test");
    Character.BaseHP = 100;
    Manager->Classes = { Character };
    FBattleCameraTestAccess::Prepare(Manager);
    if (!TestTrue(TEXT("Participant joined"), Manager->SubmitAction(TEXT("test-user"), TEXT("Test"), EBattleAction::Idle))) { return false; }
    Manager->StopBattle();
    ABattleUnit* Subject = Manager->GetBattlePlayers().FindChecked(TEXT("test-user")).Unit;
    ACutSceneApplier* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Manager;
    APlayerController* Controller = Context.World->SpawnActor<APlayerController>();
    AActor* GameCamera = Context.World->SpawnActor<AActor>();
    Controller->SetViewTarget(GameCamera);
    const auto Strategy = ECutSceneCharacterStrategy::HighestFever;
    ULevelSequence* Sequence = NewObject<ULevelSequence>(Applier);
    Sequence->Initialize();
    Sequence->GetMovieScene()->SetPlaybackRange(0, 24000);
    Sequence->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(24000, 1));
    FCutSceneDefinition& Definition = Applier->CutScenes.Add(TEXT("TestShot"));
    Definition.Sequence = Sequence;
    Definition.WorldTimeScale = 0.2f;
    Definition.SubjectBindingTag = NAME_None;
    Definition.BossBindingTag = NAME_None;
    Subject->CustomTimeDilation = 1.5f;
    UGameplayStatics::SetGlobalTimeDilation(Context.World, 0.5f);
    TestFalse(TEXT("Unknown shot fails without changing time"), Applier->ApplyCutScene(Strategy, TEXT("Unknown")));
    TestTrue(TEXT("Failure preserves time"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.5f));
    TestTrue(TEXT("Configured shot starts"), Applier->ApplyCutScene(Strategy, TEXT("TestShot")));
    TestEqual(TEXT("Selected actual manager participant"), Applier->GetCurrentSubject(), Subject);
    TestTrue(TEXT("World slowed relative to previous value"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.1f));
    TestTrue(TEXT("Subject previous speed preserved"), FMath::IsNearlyEqual(Subject->CustomTimeDilation, 7.5f));
    TestTrue(TEXT("New request replaces current shot"), Applier->ApplyCutScene(Strategy, TEXT("TestShot")));
    ACutSceneApplier* Other = Context.World->SpawnActor<ACutSceneApplier>();
    Other->BattleManager = Manager;
    Other->CutScenes = Applier->CutScenes;
    TestFalse(TEXT("Another applier cannot own world time concurrently"), Other->ApplyCutScene(Strategy, TEXT("TestShot")));
    Controller->SetViewTarget(Manager->GetBossUnit());
    Applier->CancelCutScene();
    Applier->CancelCutScene();
    TestFalse(TEXT("Cancel clears playback"), Applier->IsPlaying());
    TestTrue(TEXT("Cancel restores prior global value"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.5f));
    TestTrue(TEXT("Cancel restores prior custom value"), FMath::IsNearlyEqual(Subject->CustomTimeDilation, 1.5f));
    TestEqual(TEXT("Cancel restores game camera"), Controller->GetViewTarget(), GameCamera);

    TestTrue(TEXT("Can replay after cancel"), Applier->ApplyCutScene(Strategy, TEXT("TestShot")));
    ULevelSequencePlayer* Player = nullptr;
    for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It)
    {
        if (IsValid(*It)) { Player = It->GetSequencePlayer(); break; }
    }
    if (TestNotNull(TEXT("Runtime sequence player exists"), Player))
    {
        TestTrue(TEXT("Sequence compensated to real time"), FMath::IsNearlyEqual(Player->GetPlayRate(), 10.f));
        for (int32 Index = 0; Index < 5 && Applier->IsPlaying(); ++Index)
        {
            Player->Update(0.05f);
            // 실제 프레임 끝처럼 지연 평가와 완료 알림까지 처리한다.
            UMovieSceneSequenceTickManager* TickManager = Player->GetTickManager();
            const auto Runner = TickManager->GetRunner(FMovieSceneSequenceTickInterval());
            if (Runner) { Runner->Flush(); }
            TickManager->RunLatentActions();
        }
        TestFalse(TEXT("Natural finish cleans playback"), Applier->IsPlaying());
        TestTrue(TEXT("Natural finish restores time"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.5f));
    }
    Applier->CancelCutScene();
    TestTrue(TEXT("Start before applier destruction"), Applier->ApplyCutScene(Strategy, TEXT("TestShot")));
    Applier->DispatchBeginPlay();
    Applier->Destroy();
    TestTrue(TEXT("Applier EndPlay restores time"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.5f));
    TestTrue(TEXT("Applier EndPlay restores subject"), FMath::IsNearlyEqual(Subject->CustomTimeDilation, 1.5f));
    TestTrue(TEXT("Other applier can play after owner destruction"), Other->ApplyCutScene(Strategy, TEXT("TestShot")));
    Subject->Destroy();
    TestFalse(TEXT("Subject destruction cancels safely"), Other->IsPlaying());
    TestTrue(TEXT("Destruction restores time"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Context.World), 0.5f));
    UGameplayStatics::SetGlobalTimeDilation(Context.World, 1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeraImportedCameraTest, "Gladius.CutSceneApplier.SeraImportedAttack2",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSeraImportedCameraTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    ABattleManager* Manager = Context.World->SpawnActor<ABattleManager>();
    FCharacterDef Character;
    Character.Id = TEXT("sera"); Character.BaseHP = 5;
    Manager->Classes = {Character};
    FBattleCameraTestAccess::Prepare(Manager);
    if (!TestTrue(TEXT("Sera participant joins"), Manager->SubmitAction(TEXT("sera-camera"), TEXT("Sera"), EBattleAction::Attack2, TEXT("sera")))) return false;
    Manager->StopBattle();
    ABattleUnit* Subject = Manager->GetBattlePlayers().FindChecked(TEXT("sera-camera")).Unit;
    ABattleUnit* Boss = Manager->GetBossUnit();
    if (!TestNotNull(TEXT("Boss exists"), Boss)) return false;
    Subject->SetActorLocation(FVector(1000, 200, 300));
    Boss->SetActorLocation(FVector(2000, 200, 300));
    APlayerController* Controller = Context.World->SpawnActor<APlayerController>();
    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Context.Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
    Controller->SetPlayer(LocalPlayer);
    AActor* PreviousCamera = Context.World->SpawnActor<AActor>();
    Controller->SetViewTarget(PreviousCamera);
    ACutSceneApplier* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Manager;
    Applier->EnsureDefaultCutScenes();
    const FCutSceneDefinition* Def = Applier->CutScenes.Find(TEXT("SeraSkill2"));
    if (!TestTrue(TEXT("SeraSkill2 automatically loads imported sequence"), Def && Def->Sequence)) return false;
    TestEqual(TEXT("Normal playback speed"), Def->WorldTimeScale, 1.f);
    TestTrue(TEXT("Camera-only sequence has no actor override tags"), Def->SubjectBindingTag.IsNone() && Def->BossBindingTag.IsNone());
    TestEqual(TEXT("Four camera/component bindings only"), static_cast<const UMovieScene*>(Def->Sequence->GetMovieScene())->GetBindings().Num(), 4);
    if (!TestTrue(TEXT("Imported shot starts for actual Sera unit"), Applier->ApplyCutSceneForUser(TEXT("sera-camera"), TEXT("SeraSkill2")))) return false;
    ULevelSequencePlayer* Player = nullptr;
    for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It) Player = It->GetSequencePlayer();
    if (!TestNotNull(TEXT("Sequence player created"), Player)) return false;
    auto Sample = [&](int32 Frame) -> UCameraComponent*
    {
        Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(FFrameNumber(Frame)), EUpdatePositionMethod::Jump));
        const auto Runner = Player->GetTickManager()->GetRunner(FMovieSceneSequenceTickInterval());
        if (Runner) Runner->Flush();
        return Player->GetActiveCameraComponent();
    };
    Player->Update(0.05f);
    Player->GetTickManager()->RunLatentActions();
    UCameraComponent* First = Sample(1);
    const UMovieSceneTrack* CutTrack = Def->Sequence->GetMovieScene()->GetCameraCutTrack();
    TestFalse(TEXT("Camera Cuts track is enabled"), CutTrack->IsEvalDisabled());
    for (UMovieSceneSection* Section : CutTrack->GetAllSections())
    {
        const auto* Cut = static_cast<UMovieSceneCameraCutSection*>(Section);
        TestTrue(TEXT("Camera cut section active"), Cut->IsActive());
        TestEqual(TEXT("Saved cut references a spawned runtime camera"), Player->FindBoundObjects(Cut->GetCameraBindingID().GetGuid(), MovieSceneSequenceID::Root).Num(), 1);
    }
    if (!TestNotNull(TEXT("First camera spawns outside Art map and Camera Cuts resolves"), First)) return false;
    const FVector FirstLocation = First->GetComponentLocation();
    FMinimalViewInfo CameraView;
    First->GetCameraView(0.f, CameraView);
    const float InitialFOV = CameraView.FOV;
    const FTransform Origin = ACutSceneApplier::MakeOrigin(*Subject, *Boss);
    const FVector Local = Origin.InverseTransformPosition(FirstLocation);
    TestTrue(TEXT("Camera uses subject-relative position and battle mesh scale"), Local.Equals(FVector(700.94, 60.70, 24.16), 2.0));
    Sample(49);
    First->GetCameraView(0.f, CameraView);
    TestTrue(TEXT("Original lens zoom track evaluates"), !FMath::IsNearlyEqual(CameraView.FOV, InitialFOV, 0.1f));
    Sample(55);
    TestFalse(TEXT("Camera transform animates"), First->GetComponentLocation().Equals(FirstLocation, 1.0));
    UCameraComponent* Second = Sample(100);
    TestNotNull(TEXT("Second camera resolves after frame 87 cut"), Second);
    TestTrue(TEXT("Cut switches to a different camera"), Second && Second != First);
    if (Second)
    {
        const FVector Earlier = Second->GetComponentLocation();
        Sample(140);
        TestFalse(TEXT("Second camera descends"), Second->GetComponentLocation().Equals(Earlier, 1.0));
    }
    Applier->CancelCutScene();
    TestFalse(TEXT("Shot ends cleanly"), Applier->IsPlaying());
    TestEqual(TEXT("Previous view restored"), Controller->GetViewTarget(), PreviousCamera);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGladiusAuthoredCamerasTest, "Gladius.CutSceneApplier.GladiusAuthoredSkills",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGladiusAuthoredCamerasTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    auto* Manager = Context.World->SpawnActor<ABattleManager>();
    FCharacterDef Character; Character.Id = TEXT("gladius"); Character.BaseHP = 5;
    Manager->Classes = {Character};
    FBattleCameraTestAccess::Prepare(Manager);
    if (!Manager->SubmitAction(TEXT("camera-user"), TEXT("Gladius"), EBattleAction::Attack1, TEXT("gladius"))) return false;
    Manager->StopBattle();
    auto* Subject = Manager->GetBattlePlayers().FindChecked(TEXT("camera-user")).Unit.Get();
    auto* Boss = Manager->GetBossUnit();
    auto* Controller = Context.World->SpawnActor<APlayerController>();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Context.Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
    Controller->SetPlayer(LocalPlayer);
    auto* PreviousCamera = Context.World->SpawnActor<AActor>();
    Controller->SetViewTarget(PreviousCamera);
    auto* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Manager;
    Applier->EnsureDefaultCutScenes();
    for (const TCHAR* Id : {TEXT("GladiusSkill1"), TEXT("GladiusSkill2")})
    {
        const auto& Def = Applier->CutScenes.FindChecked(FName(Id));
        if (!TestTrue(TEXT("Authored sequence is default and has no procedural camera"), IsValid(Def.Sequence) && !Def.CameraClass)) return false;
        TestEqual(TEXT("Camera-only bindings"), static_cast<const UMovieScene*>(Def.Sequence->GetMovieScene())->GetBindings().Num(), 4);
        TestEqual(TEXT("Normal world speed"), Def.WorldTimeScale, 1.f);
        FVector LocalCamera;
        for (int32 Pass = 0; Pass < 2; ++Pass)
        {
            Subject->SetActorLocation(Pass ? FVector(-700, 500, 100) : FVector(1000, 200, 300));
            Boss->SetActorLocation(Subject->GetActorLocation() + (Pass ? FVector(0, 1000, 0) : FVector(1000, 0, 0)));
            if (!TestTrue(TEXT("Shot starts without Art map"), Applier->ApplyCutSceneForUser(TEXT("camera-user"), FName(Id)))) return false;
            ULevelSequencePlayer* Player = nullptr;
            for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It) if (IsValid(*It)) Player = It->GetSequencePlayer();
            if (!TestNotNull(TEXT("Sequence player exists"), Player)) return false;
            auto Sample = [&](int32 Frame)
            {
                Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(FFrameNumber(Frame)), EUpdatePositionMethod::Jump));
                auto Runner = Player->GetTickManager()->GetRunner(FMovieSceneSequenceTickInterval());
                if (Runner) Runner->Flush();
                return Player->GetActiveCameraComponent();
            };
            Player->Update(0.01f);
            Player->GetTickManager()->RunLatentActions();
            auto* First = Sample(1);
            if (!TestNotNull(TEXT("First authored camera resolves"), First)) return false;
            const auto Origin = ACutSceneApplier::MakeOrigin(*Subject, *Boss);
            const FVector Position = Origin.InverseTransformPosition(First->GetComponentLocation());
            if (!Pass) LocalCamera = Position;
            else TestTrue(TEXT("Camera follows selected unit position and boss direction"), Position.Equals(LocalCamera, 0.1));
            const auto* Cuts = Def.Sequence->GetMovieScene()->GetCameraCutTrack();
            for (auto* Section : Cuts->GetAllSections())
            {
                const auto* Cut = static_cast<UMovieSceneCameraCutSection*>(Section);
                TestEqual(TEXT("Cut references spawned camera"), Player->FindBoundObjects(Cut->GetCameraBindingID().GetGuid(), MovieSceneSequenceID::Root).Num(), 1);
            }
            const int32 SwitchFrame = FString(Id) == TEXT("GladiusSkill1") ? 20 : 30;
            auto* Second = Sample(SwitchFrame + 1);
            TestNotNull(TEXT("Second authored camera resolves"), Second);
            TestTrue(TEXT("Camera cut switches cameras"), Second && Second != First);
            Applier->CancelCutScene();
            TestFalse(TEXT("Shot stops"), Applier->IsPlaying());
            TestEqual(TEXT("View restored"), Controller->GetViewTarget(), PreviousCamera);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSera1AuthoredCamerasTest, "Gladius.CutSceneApplier.Sera1AuthoredSkill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSera1AuthoredCamerasTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    auto* Manager = Context.World->SpawnActor<ABattleManager>();
    FCharacterDef Character; Character.Id = TEXT("sera"); Character.BaseHP = 5;
    Manager->Classes = {Character};
    FBattleCameraTestAccess::Prepare(Manager);
    if (!Manager->SubmitAction(TEXT("camera-user"), TEXT("Sera"), EBattleAction::Attack1, TEXT("sera"))) return false;
    Manager->StopBattle();
    auto* Subject = Manager->GetBattlePlayers().FindChecked(TEXT("camera-user")).Unit.Get();
    auto* Boss = Manager->GetBossUnit();
    auto* Controller = Context.World->SpawnActor<APlayerController>();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Context.Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
    Controller->SetPlayer(LocalPlayer);
    auto* PreviousCamera = Context.World->SpawnActor<AActor>();
    Controller->SetViewTarget(PreviousCamera);
    auto* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Manager;
    Applier->EnsureDefaultCutScenes();
    for (const TCHAR* Id : {TEXT("SeraSkill1")})
    {
        const auto& Def = Applier->CutScenes.FindChecked(FName(Id));
        if (!TestTrue(TEXT("Authored sequence is default and has no procedural camera"), IsValid(Def.Sequence) && !Def.CameraClass)) return false;
        TestEqual(TEXT("Camera-only bindings"), static_cast<const UMovieScene*>(Def.Sequence->GetMovieScene())->GetBindings().Num(), 6);
        TestEqual(TEXT("Normal world speed"), Def.WorldTimeScale, 1.f);
        FVector LocalCamera;
        for (int32 Pass = 0; Pass < 2; ++Pass)
        {
            Subject->SetActorLocation(Pass ? FVector(-700, 500, 100) : FVector(1000, 200, 300));
            Boss->SetActorLocation(Subject->GetActorLocation() + (Pass ? FVector(0, 1000, 0) : FVector(1000, 0, 0)));
            if (!TestTrue(TEXT("Shot starts without Art map"), Applier->ApplyCutSceneForUser(TEXT("camera-user"), FName(Id)))) return false;
            ULevelSequencePlayer* Player = nullptr;
            for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It) if (IsValid(*It)) Player = It->GetSequencePlayer();
            if (!TestNotNull(TEXT("Sequence player exists"), Player)) return false;
            auto Sample = [&](int32 Frame)
            {
                Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(FFrameNumber(Frame)), EUpdatePositionMethod::Jump));
                auto Runner = Player->GetTickManager()->GetRunner(FMovieSceneSequenceTickInterval());
                if (Runner) Runner->Flush();
                return Player->GetActiveCameraComponent();
            };
            Player->Update(0.01f);
            Player->GetTickManager()->RunLatentActions();
            auto* First = Sample(1);
            if (!TestNotNull(TEXT("First authored camera resolves"), First)) return false;
            const auto Origin = ACutSceneApplier::MakeOrigin(*Subject, *Boss);
            const FVector Position = Origin.InverseTransformPosition(First->GetComponentLocation());
            if (!Pass) LocalCamera = Position;
            else TestTrue(TEXT("Camera follows selected unit position and boss direction"), Position.Equals(LocalCamera, 0.1));
            const auto* Cuts = Def.Sequence->GetMovieScene()->GetCameraCutTrack();
            for (auto* Section : Cuts->GetAllSections())
            {
                const auto* Cut = static_cast<UMovieSceneCameraCutSection*>(Section);
                TestEqual(TEXT("Cut references spawned camera"), Player->FindBoundObjects(Cut->GetCameraBindingID().GetGuid(), MovieSceneSequenceID::Root).Num(), 1);
            }
            const int32 SwitchFrame = 61;
            auto* Second = Sample(SwitchFrame + 1);
            TestNotNull(TEXT("Second authored camera resolves"), Second);
            TestTrue(TEXT("Camera cut switches cameras"), Second && Second != First);
            auto* Third = Sample(90);
            TestNotNull(TEXT("Third authored camera resolves"), Third);
            TestTrue(TEXT("Third cut switches cameras"), Third && Third != Second && Third != First);
            Applier->CancelCutScene();
            TestFalse(TEXT("Shot stops"), Applier->IsPlaying());
            TestEqual(TEXT("View restored"), Controller->GetViewTarget(), PreviousCamera);
        }
    }
    return true;
}


// Compare evaluated runtime cameras against the original assets, including frames
// between keys. Checking only the first frame missed unequal X/Y channel layouts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFourAuthoredCameraPathsTest, "Gladius.CutSceneApplier.AllFourOriginalCameraPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFourAuthoredCameraPathsTest::RunTest(const FString& Parameters)
{
    struct FSpec { const TCHAR* Id; const TCHAR* Source; const TCHAR* Character; FVector ArtOrigin; double ArtScale; };
    const FSpec Specs[] = {
        {TEXT("GladiusSkill1"), TEXT("/Game/Asset/Battle/gladius/gladius/animation/gladius_skill01.gladius_skill01"), TEXT("G"), FVector(240,40,0), 1},
        {TEXT("GladiusSkill2"), TEXT("/Game/Asset/Battle/gladius/gladius/animation/gladius_skill02.gladius_skill02"), TEXT("G"), FVector(240,40,0), 1},
        {TEXT("SeraSkill1"), TEXT("/Game/Asset/Battle/Sera/Animation/Sera_Animation/Sera_Attack/sera_skill01.sera_skill01"), TEXT("S"), FVector(30,0,0), 1},
        {TEXT("SeraSkill2"), TEXT("/Game/Asset/Battle/Sera/CutScene/CutScene_Attack2.CutScene_Attack2"), TEXT("S"), FVector(690,3420,460), 9.75}
    };
    FCutSceneTestWorld Context;
    auto* Manager = Context.World->SpawnActor<ABattleManager>();
    FCharacterDef Character; Character.Id = TEXT("camera-path-test"); Character.BaseHP = 5;
    Manager->Classes = {Character};
    FBattleCameraTestAccess::Prepare(Manager);
    if (!Manager->SubmitAction(TEXT("camera-path-user"), TEXT("Test"), EBattleAction::Attack1, Character.Id)) return false;
    Manager->StopBattle();
    auto* Subject = Manager->GetBattlePlayers().FindChecked(TEXT("camera-path-user")).Unit.Get();
    auto* Boss = Manager->GetBossUnit();
    auto* Controller = Context.World->SpawnActor<APlayerController>();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Context.Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
    Controller->SetPlayer(LocalPlayer);
    auto* PreviousCamera = Context.World->SpawnActor<AActor>();
    Controller->SetViewTarget(PreviousCamera);
    auto* Applier = Context.World->SpawnActor<ACutSceneApplier>();
    Applier->BattleManager = Manager;
    Applier->EnsureDefaultCutScenes();
    for (const auto& Spec : Specs)
    {
        auto* Source = LoadObject<ULevelSequence>(nullptr, Spec.Source);
        const auto& Def = Applier->CutScenes.FindChecked(FName(Spec.Id));
        UClass* UnitClass = LoadClass<ABattleUnit>(nullptr, *FString::Printf(
            TEXT("/Game/Blueprints/ScaledBattleUnits/BP_BattleUnit%s_DevBattle.BP_BattleUnit%s_DevBattle_C"), Spec.Character, Spec.Character));
        if (!TestNotNull(TEXT("Original sequence exists"), Source) || !TestNotNull(TEXT("Battle unit class exists"), UnitClass)) return false;
        const auto MeshTransform = UnitClass->GetDefaultObject<ABattleUnit>()->Mesh->GetRelativeTransform();
        const double Scale = MeshTransform.GetScale3D().X / Spec.ArtScale;
        auto* SourceScene = Source->GetMovieScene();
        auto* RuntimeScene = Def.Sequence->GetMovieScene();
        for (const auto& SourceBinding : SourceScene->GetBindings())
        {
            auto BindingName = [](UMovieScene* Scene, const FGuid& Id) -> FString
            {
                if (const auto* Spawnable = Scene->FindSpawnable(Id)) return Spawnable->GetName();
                if (const auto* Possessable = Scene->FindPossessable(Id)) return Possessable->GetName();
                return FString();
            };
            const auto* RuntimeBinding = static_cast<const UMovieScene*>(RuntimeScene)->GetBindings().FindByPredicate([&](const FMovieSceneBinding& B)
            {
                return BindingName(RuntimeScene, B.GetObjectGuid()) == BindingName(SourceScene, SourceBinding.GetObjectGuid());
            });
            if (!RuntimeBinding) continue; // Animation-only bindings are intentionally not camera bindings.
            auto FindTransform = [](const FMovieSceneBinding& Binding) -> UMovieSceneSection*
            {
                for (auto* Track : Binding.GetTracks())
                    if (Track->GetClass()->GetFName() == TEXT("MovieScene3DTransformTrack") && Track->GetAllSections().Num()) return Track->GetAllSections()[0];
                return nullptr;
            };
            auto* S = FindTransform(SourceBinding); auto* D = FindTransform(*RuntimeBinding);
            if (!S || !D) continue;
            auto SC = S->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
            auto DC = D->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
            const int32 Map[] = {1,0,2,3,4,5,6,7,8};
            if (!TestEqual(TEXT("All transform channels retained"), DC.Num(), 9)) return false;
            for (int32 Index = 0; Index < 9; ++Index)
            {
                const auto A = SC[Map[Index]]->GetData().GetTimes();
                const auto B = DC[Index]->GetData().GetTimes();
                if (!TestEqual(TEXT("Mapped channel keeps every source key"), B.Num(), A.Num())) return false;
                for (int32 Key = 0; Key < A.Num(); ++Key)
                    if (!TestEqual(TEXT("Mapped key keeps exact tick time"), B[Key].Value, A[Key].Value)) return false;
            }
        }
        const int32 EndFrame = FFrameRate::TransformTime(SourceScene->GetPlaybackRange().GetUpperBoundValue(), SourceScene->GetTickResolution(), SourceScene->GetDisplayRate()).FloorToFrame().Value;
        for (const float Heading : {0.f, 90.f, 180.f, 270.f})
        {
            Subject->SetActorLocation(FVector(1700 - Heading, -930 + Heading, 80));
            Boss->SetActorLocation(Subject->GetActorLocation() + FRotator(0, Heading, 0).Vector() * 1200);
            if (!TestTrue(TEXT("Authored camera starts"), Applier->ApplyCutSceneForUser(TEXT("camera-path-user"), FName(Spec.Id)))) return false;
            ULevelSequencePlayer* Player = nullptr;
            for (TActorIterator<ALevelSequenceActor> It(Context.World); It; ++It) if (IsValid(*It)) Player = It->GetSequencePlayer();
            if (!TestNotNull(TEXT("Runtime player exists"), Player)) return false;
            Player->Update(0.001f);
            if (auto Runner = Player->GetTickManager()->GetRunner(FMovieSceneSequenceTickInterval())) Runner->Flush();
            Player->GetTickManager()->RunLatentActions();
            double MaxPositionError = 0, MaxAngleError = 0;
            const FTransform Origin = ACutSceneApplier::MakeOrigin(*Subject, *Boss);
            for (int32 Frame = 0; Frame < EndFrame; ++Frame)
            {
                const FFrameTime Time = FFrameRate::TransformTime(FFrameTime(Frame), SourceScene->GetDisplayRate(), SourceScene->GetTickResolution());
                const UMovieSceneCameraCutSection* SourceCut = nullptr;
                for (auto* Section : SourceScene->GetCameraCutTrack()->GetAllSections())
                    if (Section->GetRange().Contains(Time.FloorToFrame())) SourceCut = static_cast<const UMovieSceneCameraCutSection*>(Section);
                if (!SourceCut) continue; // Original Gladius assets leave their final frame outside camera cuts.
                const auto* Binding = SourceScene->FindBinding(SourceCut->GetCameraBindingID().GetGuid());
                if (!TestNotNull(TEXT("Original cut resolves"), Binding)) return false;
                UMovieSceneSection* TransformSection = nullptr;
                for (auto* Track : Binding->GetTracks())
                    if (Track->GetClass()->GetFName() == TEXT("MovieScene3DTransformTrack")) TransformSection = Track->GetAllSections()[0];
                if (!TestNotNull(TEXT("Original camera transform exists"), TransformSection)) return false;
                auto Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
                double Values[9] = {};
                for (int32 Index = 0; Index < 9; ++Index) Channels[Index]->Evaluate(Time, Values[Index]);
                const FVector ExpectedLocal((Values[1]-Spec.ArtOrigin.Y)*Scale, (Spec.ArtOrigin.X-Values[0])*Scale, (Values[2]-Spec.ArtOrigin.Z)*Scale);
                const FVector Expected = Origin.TransformPosition(ExpectedLocal + MeshTransform.GetLocation());
                const FQuat ExpectedRotation = Origin.GetRotation() * FRotator(Values[4], Values[5]-90, Values[3]).Quaternion();
                Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(Frame), EUpdatePositionMethod::Jump));
                if (auto Runner = Player->GetTickManager()->GetRunner(FMovieSceneSequenceTickInterval())) Runner->Flush();
                auto* Camera = Player->GetActiveCameraComponent();
                if (!TestNotNull(TEXT("Camera resolves at every active frame"), Camera)) return false;
                MaxPositionError = FMath::Max(MaxPositionError, FVector::Distance(Camera->GetComponentLocation(), Expected));
                MaxAngleError = FMath::Max(MaxAngleError, FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(ExpectedRotation)));
            }
            const FString Label = FString::Printf(TEXT("%s toward %.0f degrees"), Spec.Id, Heading);
            TestTrue(*(Label + TEXT(" position matches original at every active frame")), MaxPositionError < 0.01);
            TestTrue(*(Label + TEXT(" rotation matches original at every active frame")), MaxAngleError < 0.001);
            AddInfo(FString::Printf(TEXT("%s: max position %.8f cm, rotation %.8f degrees"), *Label, MaxPositionError, MaxAngleError));
            Applier->CancelCutScene();
            TestEqual(TEXT("Original view restored"), Controller->GetViewTarget(), PreviousCamera);
        }
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGladiusBossOpacityTest, "Gladius.CutSceneApplier.BossOpacityLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGladiusBossOpacityTest::RunTest(const FString& Parameters)
{
    FCutSceneTestWorld Context;
    auto* Manager = Context.World->SpawnActor<ABattleManager>();
    FBattleCameraTestAccess::RecordsOnly(Manager);
    FCharacterDef Character; Character.Id = TEXT("gladius"); Character.BaseHP = 5;
    Manager->Classes = {Character};
    FBattleCameraTestAccess::Prepare(Manager);
    if (!Manager->SubmitAction(TEXT("opacity"), TEXT("Opacity"), EBattleAction::Attack1, TEXT("gladius"))) return false;
    Manager->StopBattle();
    auto* Boss = Manager->GetBossUnit();
    if (!TestNotNull(TEXT("Boss exists"), Boss)) return false;
    auto* Mesh = NewObject<USkeletalMeshComponent>(Boss);
    Boss->AddInstanceComponent(Mesh); Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Asset/Battle/Trudon/SK_Modeling/SK_Trudon.SK_Trudon"))); Mesh->RegisterComponent();
    auto* Original = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Asset/Battle/Trudon/MTL/MTL_Inst/T_Trudon_Body.T_Trudon_Body"));
    if (!TestNotNull(TEXT("Real Trudon material exists"), Original)) return false;
    Mesh->SetMaterial(0, Original);
    auto* Applier = Context.World->SpawnActor<ACutSceneApplier>(); Applier->BattleManager = Manager;
    auto* Sequence = NewObject<ULevelSequence>(Applier); Sequence->Initialize();
    Sequence->GetMovieScene()->SetPlaybackRange(0, 2400);
    for (const TCHAR* Id : {TEXT("GladiusSkill1"), TEXT("GladiusSkill2"), TEXT("SeraSkill1")})
    {
        auto& Def = Applier->CutScenes.FindOrAdd(FName(Id)); Def.Sequence = Sequence;
        Def.CameraClass = nullptr; Def.WorldTimeScale = 1.f; Def.SubjectBindingTag = Def.BossBindingTag = NAME_None;
    }
    for (const TCHAR* Id : {TEXT("GladiusSkill1"), TEXT("GladiusSkill2")})
    {
        TestTrue(TEXT("Gladius attack starts"), Applier->ApplyCutSceneForUser(TEXT("opacity"), FName(Id)));
        auto* Dynamic = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
        if (TestNotNull(TEXT("Boss uses temporary fade material"), Dynamic))
            TestEqual(TEXT("Boss opacity is exactly twenty percent"), Dynamic->K2_GetScalarParameterValue(TEXT("CutsceneOpacity")), .2f);
        Applier->CancelCutScene();
        TestEqual(TEXT("Cancellation restores exact original instance"), Mesh->GetMaterial(0), Original);
        Applier->ApplyCutSceneForUser(TEXT("opacity"), FName(Id));
        AdvanceCurrentShot(Context.World, Applier);
        TestFalse(TEXT("Short shot naturally finished"), Applier->IsPlaying());
        TestEqual(TEXT("Natural finish restores material"), Mesh->GetMaterial(0), Original);
    }
    Applier->ApplyCutSceneForUser(TEXT("opacity"), TEXT("GladiusSkill1"));
    Applier->ApplyCutSceneForUser(TEXT("opacity"), TEXT("GladiusSkill2"));
    TestTrue(TEXT("Replacement retains fade"), Mesh->GetMaterial(0) != Original);
    Applier->ApplyCutSceneForUser(TEXT("opacity"), TEXT("SeraSkill1"));
    TestEqual(TEXT("Other cutscene restores original"), Mesh->GetMaterial(0), Original);
    Applier->CancelCutScene();
    Applier->ApplyCutSceneForUser(TEXT("opacity"), TEXT("GladiusSkill1"));
    Applier->DispatchBeginPlay(); Applier->Destroy();
    TestEqual(TEXT("EndPlay restores original"), Mesh->GetMaterial(0), Original);
    return true;
}


#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossFadeTexturesTest, "Gladius.CutSceneApplier.BossFadeTexturesAndSkeletalUsage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossFadeTexturesTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* Part : {TEXT("Body"), TEXT("Body_ORG"), TEXT("Cloth"), TEXT("Hair"), TEXT("HairClip"), TEXT("Leather"), TEXT("Mace"), TEXT("Metal"), TEXT("Necklace"), TEXT("Rope")})
    {
        const FString Name = FString(TEXT("T_Trudon_")) + Part;
        auto* Original = LoadObject<UMaterial>(nullptr, *(TEXT("/Game/Asset/Battle/Trudon/MTL/") + Name + TEXT(".") + Name));
        auto* Fade = LoadObject<UMaterial>(nullptr, *(TEXT("/Game/Cinematics/Battle/TrudonFade/") + Name + TEXT("_Cutscene.") + Name + TEXT("_Cutscene")));
        if (!TestNotNull(*Name, Original) || !TestNotNull(*(Name + TEXT(" fade")), Fade)) return false;
        // Runtime-assigned materials are never placed on a mesh by the editor. The skeletal
        // permutation must be explicitly authored or a packaged game uses the gray fallback.
        TestTrue(*(Name + TEXT(" has pre-authored skeletal support")), bool(Fade->bUsedWithSkeletalMesh));
        TestTrue(*(Name + TEXT(" uses translucency")), Fade->GetBlendMode() == BLEND_Translucent);
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        TArray<UTexture*> OriginalTextures, FadeTextures;
        Original->GetUsedTextures(OriginalTextures);
        Fade->GetUsedTextures(FadeTextures);
        TestTrue(*(Name + TEXT(" original textures exist")), !OriginalTextures.IsEmpty());
        TArray<UMaterialExpression*> OpacityChain;
        Fade->GetExpressionsInPropertyChain(MP_Opacity, OpacityChain, nullptr);
        TArray<UTexture*> OpacityTextures;
        for (auto* Expression : OpacityChain)
            if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression)) OpacityTextures.Add(Sample->Texture);
        for (UTexture* Texture : OriginalTextures)
        {
            // An original alpha channel may be constant-folded by the translucent compiler.
            // Check its actual opacity connection rather than requiring an unused sampler.
            const bool bOpacityConnected = OpacityTextures.Contains(Texture);
            TestTrue(*(Name + TEXT(" preserves ") + GetNameSafe(Texture)), FadeTextures.Contains(Texture) || bOpacityConnected);
        }
        TArray<UMaterialExpression*> OriginalColorChain, FadeColorChain;
        Original->GetExpressionsInPropertyChain(MP_BaseColor, OriginalColorChain, nullptr);
        Fade->GetExpressionsInPropertyChain(MP_BaseColor, FadeColorChain, nullptr);
        for (auto* Expression : OriginalColorChain)
            if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
            {
                bool bColorPreserved = false;
                for (auto* FadeExpression : FadeColorChain)
                    if (auto* FadeSample = Cast<UMaterialExpressionTextureSample>(FadeExpression))
                        bColorPreserved |= FadeSample->Texture == Sample->Texture;
                TestTrue(*(Name + TEXT(" preserves actual base-color input")), bColorPreserved && FadeTextures.Contains(Sample->Texture));
            }
        auto* Resource = Fade->GetMaterialResource(SP_PCD3D_SM6);
        if (TestNotNull(*(Name + TEXT(" shader resource")), Resource))
        {
            TestTrue(*(Name + TEXT(" has compiled shaders")), Resource->GetGameThreadShaderMap() != nullptr);
            TestEqual(*(Name + TEXT(" has no shader compilation errors")), Resource->GetCompileErrors().Num(), 0);
        }
    }
    return true;
}
#endif // WITH_EDITOR


struct FCharacterGroundingTestAccess
{
 static void Calibrate(ABattleUnit* Unit, double SoleOffset)
 {
  Unit->bHasGroundReference=true; Unit->GroundSoleOffsetZ=SoleOffset;
 }
 static void Ground(ABattleUnit* Unit, double Height) { Unit->ApplyGroundHeight(Height); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundContactContinuityTest,"Gladius.Grounding.NoDriftAcrossMovementAndGroundChanges",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGroundContactContinuityTest::RunTest(const FString& Parameters)
{
 FCutSceneTestWorld T;
 auto* Unit=T.World->SpawnActor<ABattleUnit>();
 Unit->SetActorLocation(FVector(50,100,5));
 const FVector OriginalActor=Unit->GetActorLocation();
 const FVector MeshBefore=Unit->Mesh->GetComponentLocation();
 const FVector PlateBefore=Unit->Nameplate->GetComponentLocation();
 FCharacterGroundingTestAccess::Calibrate(Unit,2.0);
 FCharacterGroundingTestAccess::Ground(Unit,0.0);
 TestTrue(TEXT("Grounding preserves combat position"),Unit->GetActorLocation().Equals(OriginalActor));
 TestTrue(TEXT("Neutral sole reaches floor"),FMath::IsNearlyZero(Unit->GetActorLocation().Z+2.0+Unit->GetGroundMeshOffsetZ()));
 TestTrue(TEXT("Nameplate moves with body, preserving head clearance"),
  FMath::IsNearlyEqual(Unit->Mesh->GetComponentLocation().Z-MeshBefore.Z,Unit->Nameplate->GetComponentLocation().Z-PlateBefore.Z));
 const FVector GroundedMesh=Unit->Mesh->GetComponentLocation();
 for(int32 I=0;I<30;++I) FCharacterGroundingTestAccess::Ground(Unit,0.0);
 TestTrue(TEXT("Repeated grounding never accumulates offsets"),Unit->Mesh->GetComponentLocation().Equals(GroundedMesh));
 Unit->SetActorLocation(FVector(100,100,12));
 FCharacterGroundingTestAccess::Ground(Unit,0.0);
 TestTrue(TEXT("A higher formation marker does not make the character float"),FMath::IsNearlyEqual(Unit->Mesh->GetComponentLocation().Z,GroundedMesh.Z));
 FCharacterGroundingTestAccess::Ground(Unit,4.0);
 TestTrue(TEXT("New floor height raises the body exactly once"),FMath::IsNearlyEqual(Unit->Mesh->GetComponentLocation().Z,GroundedMesh.Z+4.0));
 TestTrue(TEXT("New floor height does not alter character scale"),Unit->GetActorScale3D().Equals(FVector::OneVector));
 Unit->Destroy();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundedCutSceneFrameTest,"Gladius.Grounding.CutsceneFramingFollowsGroundCorrection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGroundedCutSceneFrameTest::RunTest(const FString& Parameters)
{
 FCutSceneTestWorld T;
 auto* Subject=T.World->SpawnActor<ABattleUnit>(); auto* Boss=T.World->SpawnActor<ABattleUnit>();
 Subject->SetActorLocation(FVector(100,-400,12)); Boss->SetActorLocation(FVector(500,-200,80));
 const FTransform Original=ACutSceneApplier::MakeOrigin(*Subject,*Boss);
 const FVector LocalCamera(230,-140,170);
 const FVector OriginalWorldCamera=Original.TransformPosition(LocalCamera);
 FCharacterGroundingTestAccess::Calibrate(Subject,2.0);
 for(double FloorHeight : {0.0,7.5,-10.0})
 {
  FCharacterGroundingTestAccess::Ground(Subject,FloorHeight);
  const FTransform Grounded=ACutSceneApplier::MakeGroundedOrigin(*Subject,*Boss);
  const FVector NewWorldCamera=Grounded.TransformPosition(LocalCamera);
  TestTrue(TEXT("Ground correction preserves camera X/Y"),FMath::IsNearlyEqual(NewWorldCamera.X,OriginalWorldCamera.X)&&FMath::IsNearlyEqual(NewWorldCamera.Y,OriginalWorldCamera.Y));
  TestTrue(TEXT("Camera and body receive the same height correction"),FMath::IsNearlyEqual(NewWorldCamera.Z-OriginalWorldCamera.Z,Subject->GetGroundMeshOffsetZ()));
  TestTrue(TEXT("Boss-facing rotation is unchanged"),Grounded.GetRotation().Equals(Original.GetRotation()));
  TestTrue(TEXT("Original camera curve scale is unchanged"),Grounded.GetScale3D().Equals(FVector::OneVector));
 }
 Subject->Destroy(); Boss->Destroy();
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActualSoleGroundContactTest,"Gladius.Grounding.ActualCharacterSolesOnFloor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActualSoleGroundContactTest::RunTest(const FString& Parameters)
{
 FCutSceneTestWorld T;
 auto* Floor=T.World->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Floor);
 Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
 Box->SetBoxExtent(FVector(5000,5000,25));
 Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Box->SetCollisionObjectType(ECC_WorldStatic);
 Box->SetCollisionResponseToAllChannels(ECR_Block);
 Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-25));
 for(const TCHAR* Path : {TEXT("/Game/Blueprints/ScaledBattleUnits/BP_BattleUnitG_DevBattle.BP_BattleUnitG_DevBattle_C"),TEXT("/Game/Blueprints/ScaledBattleUnits/BP_BattleUnitS_DevBattle.BP_BattleUnitS_DevBattle_C")})
 {
  UClass* Class=LoadClass<ABattleUnit>(nullptr,Path);
  if(!TestNotNull(TEXT("Actual battle class"),Class)) return false;
  auto* Unit=T.World->SpawnActor<ABattleUnit>(Class,FVector(0,0,20),FRotator::ZeroRotator);
  if(!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
  Unit->Mesh->InitAnim(true);
  if(!TestTrue(TEXT("Actual body finds physical floor"),Unit->SnapMeshToGround())) return false;
  auto Sole=[&]()
  {
   TArray<FFinalSkinVertex> Vertices;
   Unit->Mesh->GetCPUSkinnedVertices(Vertices,0);
   TestTrue(TEXT("Actual posed body vertices are available"),!Vertices.IsEmpty());
   double Z=TNumericLimits<double>::Max();
   for(const auto& V:Vertices) Z=FMath::Min(Z,Unit->Mesh->GetComponentTransform().TransformPosition(FVector(V.Position)).Z);
   return Z;
  };
  const double InitialSole=Sole();
  AddInfo(FString::Printf(TEXT("%s: actual sole Z %.6f cm, correction %.6f cm"),Path,InitialSole,Unit->GetGroundMeshOffsetZ()));
  TestTrue(TEXT("Actual visible sole touches the floor"),FMath::Abs(InitialSole)<.05);
  Unit->MoveTo(FVector(100,0,60),0.f,EMoveGait::Walk);
  TestTrue(TEXT("Higher formation target cannot lift the actual sole"),FMath::Abs(Sole())<.05);
  Unit->NotifyDied(); Unit->NotifyRevived();
  TestTrue(TEXT("Ground correction survives revival"),FMath::Abs(Sole())<.05);
  Unit->Destroy();
 }
 Floor->Destroy();
 return true;
}

#endif
