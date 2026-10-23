#include "Scripts/Battle/CutSceneApplier.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BattleCameraTestAccess.h"
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
#include "MovieSceneSequenceTickManager.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Misc/AutomationTest.h"

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

#endif
