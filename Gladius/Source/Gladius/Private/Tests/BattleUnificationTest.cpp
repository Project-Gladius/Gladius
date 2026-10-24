#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/Battle/CutSceneApplier.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Scripts/UI/BattleDataResetWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/InputComponent.h"
#include "Scripts/Battle/GladiusRunCamera.h"
#include "Scripts/Battle/BattleUnit.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include <type_traits>

struct FBattleUnificationTestAccess
{
 static bool LoadBossData(ABattleManager* B) { return B->Balance.LoadFromFolder(B->BalanceFolderPath()); }
 static const FBattleBalance& BossData(ABattleManager* B) { return B->Balance; }
 static void SetBossAction(ABattleManager* B, const FBossAction& Action) { B->TelegraphedAction = Action; }
 static const FBossAction& BossAction(ABattleManager* B) { return B->TelegraphedAction; }
 static void LockTarget(ABattleManager* B) { B->ResolveBattleStartTarget(); }
 static void ClearPlayers(ABattleManager* B) { B->Players.Reset(); }
 static void PutPlayer(ABattleManager* B, const TCHAR* Id, EBattleLane Lane, int32 Fever, int32 HP = 7, int32 Revive = 0)
 {
  FBattlePlayer P; P.Lane=Lane; P.Fever=Fever; P.HP=HP; P.MaxHP=7; P.ReviveCountdown=Revive;
  P.PendingAction=EBattleAction::AvoidR;
  B->Players.Add(Id,P);
 }
 static void BindKeys(ABattleManager* B) { B->BindActionKeys(); }
 static bool PressKey(ABattleManager* B, FKey Key)
 {
  if (!B->InputComponent) return false;
  for (const FInputKeyBinding& Binding : B->InputComponent->KeyBindings)
   if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed) { Binding.KeyDelegate.Execute(Key); return true; }
  return false;
 }
 static void SetPath(ABattleManager* B, const FString& Path) { B->TestRecordPath = Path; }
 static void Prepare(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->StartTurn(); }
 static void Select(ABattleManager* B) { B->BeginInput(); }
 static void Collect(ABattleManager* B) { B->BeginCollect(); }
 static void Move(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->BeginMove(); }
 static float PhaseRemaining(ABattleManager* B) { return B->GetWorldTimerManager().GetTimerRemaining(B->PhaseTimerHandle); }
 static void Resolve(ABattleManager* B) { B->ResolveTurn(); }
 static EMoveGait MovementGait(ABattleManager* B, const ABattleUnit* Unit, float Distance, float Seconds) { return B->ChooseMovementGait(Unit,Distance,Seconds); }
 static int32 ShotIndex(ABattleManager* B) { return B->CutSceneIndex; }
 static FString StartedCutSceneAttack(ABattleManager* B) { return B->CutSceneAttackUserId; }
 static void FlushOneAttack(ABattleManager* B) { B->AttackCount = 1; B->FlushAttackCues(); }
 static void SelectedAttackLast(ABattleManager* B)
 {
  B->AttackCues.Sort([B](const FAttackCue& A, const FAttackCue& C)
  { return A.UserId != B->CutSceneUserId && C.UserId == B->CutSceneUserId; });
 }
 static void EndTurn(ABattleManager* B) { B->EndTurn(); }
 static void Fever(ABattleManager* B, const FString& Id, int32 Value) { B->Players.FindChecked(Id).Fever = Value; }
 static const FPlayerRecord* Record(ABattleManager* B, const FString& Id) { return B->Records.Find(Id); }
};

namespace
{
struct FUnifiedBattleWorld
{
 UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
 UWorld* World;
 ABattleManager* Battle;
 AYouTubeChatManager* Chat;
 FString Path;
 FUnifiedBattleWorld()
 {
  Instance->AddToRoot();
  Instance->InitializeStandalone();
  World = Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
  World->EnableActorTickAndUserCallbacks(true);

#endif
  World->InitializeActorsForPlay(FURL());
  Battle = World->SpawnActor<ABattleManager>();
  Battle->bUseBalanceFiles = false;
  Battle->bRandomizeTestPlayers = false;
  Battle->bEnableTurnCutScenes = false;
  Path = FPaths::ProjectSavedDir() / TEXT("Automation") / (FGuid::NewGuid().ToString() + TEXT(".json"));
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
  FBattleUnificationTestAccess::SetPath(Battle, Path);
  Chat = World->SpawnActor<AYouTubeChatManager>();
 }
 ~FUnifiedBattleWorld()
 {
  Battle->StopBattle();
  Battle->Destroy();
  Chat->Destroy();
  Instance->Shutdown();
  GEngine->DestroyWorldContext(World);
  World->DestroyWorld(false);
  Instance->RemoveFromRoot();
  IFileManager::Get().Delete(*Path);
 }
 bool Dispatch(const FString& User, const FString& Text)
 {
  FParsedCommand C;
  if (!FChatCommandParser::TryParse(User, Text, C)) return false;
  C.Username = TEXT("Visible name");
  Chat->GetCommandDispatcher()->Execute(C);
  return true;
 }
};
}

class FReturnToTempleAssertions : public IAutomationLatentCommand
{
 TSharedPtr<FUnifiedBattleWorld> State;
 FAutomationTestBase* Test;
 int32 Frames = 0;
public:
 FReturnToTempleAssertions(TSharedPtr<FUnifiedBattleWorld> InState, FAutomationTestBase* InTest)
  : State(InState), Test(InTest) {}
 bool Update() override
 {
  State->World->GetTimerManager().Tick(0.1f);
  if (State->Battle->Phase == EBattlePhase::Resolve && ++Frames < 10) return false;
  FWorldContext& Context = GEngine->GetWorldContextFromWorldChecked(State->World);
  Test->TestTrue(TEXT("Completed battle enters BattleEnd"), State->Battle->Phase == EBattlePhase::BattleEnd);
  Test->TestTrue(TEXT("Completion saves player records before travel"), IFileManager::Get().FileExists(*State->Path));
  Test->TestTrue(TEXT("Final turn requests the configured temple"),
   Context.TravelURL.Contains(GetDefault<URealTimeSettings>()->TempleMap.ToSoftObjectPath().GetLongPackageName()));
  Context.TravelURL.Reset();
  return true;
 }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleReturnToTempleTest, "Gladius.BattleUnified.ReturnToTempleAfterFinalTurn",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBattleReturnToTempleTest::RunTest(const FString& Parameters)
{
 TSharedPtr<FUnifiedBattleWorld> T = MakeShared<FUnifiedBattleWorld>();
 ABattleManager* B = T->Battle;
 FWorldContext& Context = GEngine->GetWorldContextFromWorldChecked(T->World);
 B->Turns = 1;
 B->ReturnSeconds = 0.01f;
 B->ResolveSeconds = 0.01f;
 B->StartBattle();
 TestTrue(TEXT("Starting a battle does not request travel"), Context.TravelURL.IsEmpty());
 B->StopBattle();
 TestTrue(TEXT("Stopping for cleanup does not request travel"), Context.TravelURL.IsEmpty());
 B->StartBattle();
 B->TurnIndex = B->Turns;
 FBattleUnificationTestAccess::EndTurn(B);
 TestTrue(TEXT("Final results finish before travel"), Context.TravelURL.IsEmpty());
 ADD_LATENT_AUTOMATION_COMMAND(FReturnToTempleAssertions(T, this));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnifiedBattleRoutingTest, "Gladius.BattleUnified.ChatRoutingAndParticipation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUnifiedBattleRoutingTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 B->MaxPlayers = 1;
 B->StartBattle();
 TestTrue(TEXT("Starts in BattleStart"), B->Phase == EBattlePhase::BattleStart);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 TestEqual(TEXT("No join before preparation"), B->GetBattlePlayers().Num(), 0);
 FBattleUnificationTestAccess::Prepare(B);
 TestTrue(TEXT("Telegraph belongs to Prepare"), B->Phase == EBattlePhase::Prepare);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 TestEqual(TEXT("Dispatcher joins through BattleManager"), B->GetBattlePlayers().Num(), 1);
 const FBattlePlayer* P = B->GetBattlePlayers().Find(TEXT("A"));
 if (!TestNotNull(TEXT("Player exists"), P)) return false;
 TestEqual(TEXT("Display name preserved"), P->DisplayName, FString(TEXT("Visible name")));
 TestTrue(TEXT("Attack one routed"), P->PendingAction == EBattleAction::Attack1);
 T.Dispatch(TEXT("B"), TEXT("!공격 2"));
 TestEqual(TEXT("Single participation limit"), B->GetBattlePlayers().Num(), 1);
 FBattleUnificationTestAccess::Select(B);
 TestTrue(TEXT("Selection remains Prepare"), B->Phase == EBattlePhase::Prepare);
 const TCHAR* Commands[] = {TEXT("!공격 2"), TEXT("!방어"), TEXT("!회피 좌"), TEXT("!회피 중"), TEXT("!회피 우")};
 const EBattleAction Actions[] = {EBattleAction::Attack2, EBattleAction::Defense, EBattleAction::AvoidL, EBattleAction::AvoidM, EBattleAction::AvoidR};
 for (int32 i=0; i<5; ++i)
 {
  T.Dispatch(TEXT("A"), Commands[i]);
  TestTrue(TEXT("Canonical command mapped"), B->GetBattlePlayers().FindChecked(TEXT("A")).PendingAction == Actions[i]);
 }
 FBattleUnificationTestAccess::Collect(B);
 TestTrue(TEXT("Delayed input accepted"), B->IsAcceptingInput());
 T.Dispatch(TEXT("A"), TEXT("!공격 2"));
 FBattleUnificationTestAccess::Move(B);
 TestTrue(TEXT("Move starts without cutscenes"), B->Phase == EBattlePhase::Move);
 T.Dispatch(TEXT("A"), TEXT("!방어"));
 TestTrue(TEXT("Input frozen after collection"), B->GetBattlePlayers().FindChecked(TEXT("A")).PendingAction == EBattleAction::Attack2);
 T.Dispatch(TEXT("A"), TEXT("!로그아웃"));
 TestEqual(TEXT("Logout removes actual participant"), B->GetBattlePlayers().Num(), 0);
 TestNotNull(TEXT("Logout preserves persistent record"), FBattleUnificationTestAccess::Record(B,TEXT("A")));
 B->StopBattle();
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 TestEqual(TEXT("Stopped battle rejects participation"), B->GetBattlePlayers().Num(), 0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnifiedBattleStateTest, "Gladius.BattleUnified.IntegerFeverPersistenceAndTurns",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUnifiedBattleStateTest::RunTest(const FString& Parameters)
{
 static_assert(std::is_same_v<decltype(FBattlePlayer::Fever), int32>);
 static_assert(std::is_same_v<decltype(FAttackCue::Fever), int32>);
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 FBattleUnificationTestAccess::Fever(B, TEXT("A"), 5);
 TestEqual(TEXT("Integer reduction floors legacy none mode"), ABattleManager::DecayFever(5,2,2,0.5f,EEvadeRepeatMode::EveryAfter,EFeverRounding::None),2);
 TestEqual(TEXT("Round mode still returns integer"), ABattleManager::DecayFever(5,2,2,0.5f,EEvadeRepeatMode::EveryAfter,EFeverRounding::Round),3);
 FBattleUnificationTestAccess::Collect(B);
 FBattleUnificationTestAccess::Move(B);
 FBattleUnificationTestAccess::Resolve(B);
 TestTrue(TEXT("Move advances to attack/hit"), B->Phase == EBattlePhase::BossAction);
 const auto* Record = FBattleUnificationTestAccess::Record(B, TEXT("A"));
 const int64 Damage = Record ? Record->TotalDamageDealt : 0;
 TestTrue(TEXT("Damage updates unified record"), Damage > 0);
 B->LoadRecords();
 Record = FBattleUnificationTestAccess::Record(B,TEXT("A"));
 TestTrue(TEXT("JSON reload retains damage"), Record && Record->TotalDamageDealt == Damage);
 FBattleUnificationTestAccess::EndTurn(B);
 TestTrue(TEXT("Attack/hit advances to result/return"), B->Phase == EBattlePhase::Resolve);
 FBattleUnificationTestAccess::Prepare(B);
 TestEqual(TEXT("Next turn advances once"), B->TurnIndex,2);
 B->StartBattle();
 TestEqual(TEXT("Restart clears participants"),B->GetBattlePlayers().Num(),0);
 TestTrue(TEXT("Restart preserves record"),FBattleUnificationTestAccess::Record(B,TEXT("A")) != nullptr);
 B->RunSelfTest();
 return true;
}

namespace
{
class FConcurrentMoveTimerAssertions : public IAutomationLatentCommand
{
 TSharedPtr<FUnifiedBattleWorld> T;
 TWeakObjectPtr<ACutSceneApplier> Applier;
 FAutomationTestBase* Test;
 int32 Frames=0;
public:
 FConcurrentMoveTimerAssertions(TSharedPtr<FUnifiedBattleWorld> In,ACutSceneApplier* A,FAutomationTestBase* InTest):T(In),Applier(A),Test(InTest){}
 bool Update() override
 {
  auto* B=T->Battle;
  T->World->GetTimerManager().Tick(0.2f);
  if (B->Phase==EBattlePhase::Move && ++Frames<10) return false;
  Test->TestTrue(TEXT("Move timer advances to attack while cutscene still plays"),B->Phase==EBattlePhase::BossAction && Applier.IsValid() && Applier->IsPlaying());
  if(!Applier.IsValid()) return true;
  const float BeforeEnd=FBattleUnificationTestAccess::PhaseRemaining(B);
  Applier->CancelCutScene();
  Test->TestTrue(TEXT("Next shot continues during attack without switching phase"),B->Phase==EBattlePhase::BossAction && Applier->IsPlaying());
  Test->TestEqual(TEXT("Shot completion does not reset combat timer"),FBattleUnificationTestAccess::PhaseRemaining(B),BeforeEnd);
  Applier->CancelCutScene();
  Test->TestTrue(TEXT("Last shot does not restart Move"),B->Phase==EBattlePhase::BossAction);
  B->StartBattle();FBattleUnificationTestAccess::Prepare(B);
  T->Dispatch(TEXT("A"),TEXT("!공격 1"));
  FBattleUnificationTestAccess::Move(B);
  FBattleUnificationTestAccess::EndTurn(B);
  Test->TestTrue(TEXT("Result entry cancels remaining shots"),B->Phase==EBattlePhase::Resolve && !Applier->IsPlaying());
  Applier->CancelCutScene();
  Test->TestTrue(TEXT("Late cutscene stop cannot restart movement"),B->Phase==EBattlePhase::Resolve);
  B->StopBattle();
  Test->TestTrue(TEXT("Stopped phase remains BattleEnd"),B->Phase==EBattlePhase::BattleEnd);
  return true;
 }
};
class FCollectMoveTimerAssertions : public IAutomationLatentCommand
{
 TSharedPtr<FUnifiedBattleWorld> T;
 TWeakObjectPtr<ACutSceneApplier> Applier;
 FAutomationTestBase* Test;
 int32 Frames=0;
public:
 FCollectMoveTimerAssertions(TSharedPtr<FUnifiedBattleWorld> In,ACutSceneApplier* A,FAutomationTestBase* InTest):T(In),Applier(A),Test(InTest){}
 bool Update() override
 {
  T->World->GetTimerManager().Tick(0.02f);
  if(T->Battle->Phase==EBattlePhase::Collect && ++Frames<10) return false;
  Test->TestTrue(TEXT("Collection timer enters Move directly and starts cutscene"),T->Battle->Phase==EBattlePhase::Move && Applier.IsValid() && Applier->IsPlaying());
  Test->TestEqual(TEXT("Selection is computed on Move entry"),T->Battle->CutSceneUserId,FString(TEXT("A")));
  return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnifiedBattleCutSceneTest, "Gladius.BattleUnified.MoveStartsCutScenesAndStopsSafely",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUnifiedBattleCutSceneTest::RunTest(const FString& Parameters)
{
 auto Context=MakeShared<FUnifiedBattleWorld>();
 auto& T=*Context;
 auto* B = T.Battle;
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 ACutSceneApplier* Applier = T.World->SpawnActor<ACutSceneApplier>();
 B->CutSceneApplier = Applier;
 B->bEnableTurnCutScenes = true;
 const TArray<FName> Shots = {TEXT("GladiusRun"), TEXT("GladiusSkill1")};
 for (FName Id : Shots)
 {
  ULevelSequence* Sequence = NewObject<ULevelSequence>(Applier);
  Sequence->Initialize();
  Sequence->GetMovieScene()->SetPlaybackRange(0,24000);
  FCutSceneDefinition& Def = Applier->CutScenes.Add(Id);
  Def.Sequence = Sequence;
  Def.WorldTimeScale = 1.f;
  Def.SubjectBindingTag = Def.BossBindingTag = NAME_None;
 }
 T.Dispatch(TEXT("Other"), TEXT("!방어"));
 auto* Other=B->GetBattlePlayers().FindChecked(TEXT("Other")).Unit.Get();
 Other->SetActorLocation(FVector(9999,9999,0));
 const FVector Before=Other->GetActorLocation();
 B->MoveSeconds=0.1f;
 FBattleUnificationTestAccess::Collect(B);
 FBattleUnificationTestAccess::Move(B);
 TestTrue(TEXT("Move and first cutscene start together"), B->Phase==EBattlePhase::Move && Applier->IsPlaying());
 static_cast<AActor*>(Other)->Tick(0.02f);
 TestFalse(TEXT("Another unit moves while the shot is playing"),Other->GetActorLocation().Equals(Before));
 TestFalse(TEXT("Move rejects inputs"),B->IsAcceptingInput());
 ADD_LATENT_AUTOMATION_COMMAND(FConcurrentMoveTimerAssertions(Context,Applier,this));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnifiedHealthPersistenceTest, "Gladius.BattleUnified.CurrentHealthPersistence",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUnifiedHealthPersistenceTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 auto ReadHealth = [&]()
 {
  FString Json;
  FFileHelper::LoadFileToString(Json, *T.Path);
  TestTrue(TEXT("JSON has currentHealth"), Json.Contains(TEXT("currentHealth")));
  TestTrue(TEXT("JSON retains maxHPBonus alongside currentHealth"), Json.Contains(TEXT("maxHPBonus")));
  FPlayerRecordFile File;
  FJsonObjectConverter::JsonObjectStringToUStruct(Json, &File);
  return File.Records.Num() == 1 ? File.Records[0].CurrentHealth : -999;
 };
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 const int32 Max = B->GetBattlePlayers().FindChecked(TEXT("A")).MaxHP;
 TestEqual(TEXT("Joining saves initial HP"),ReadHealth(),Max);
 TestTrue(TEXT("Damage setter accepted"),B->SetPlayerHealth(TEXT("A"),Max-1));
 TestEqual(TEXT("Damage updates disk immediately"),ReadHealth(),Max-1);
 B->LogoutPlayer(TEXT("A"));
 T.Dispatch(TEXT("A"),TEXT("!방어"));
 TestEqual(TEXT("Rejoin preserves health"),B->GetBattlePlayers().FindChecked(TEXT("A")).HP,Max-1);
 TestTrue(TEXT("Healing setter accepted"),B->SetPlayerHealth(TEXT("A"),Max));
 TestEqual(TEXT("Healing updates disk immediately"),ReadHealth(),Max);
 T.Dispatch(TEXT("A"),TEXT("!공격 1"));
 for (int32 Zone=0;Zone<3;++Zone) B->TelegraphedAction.DamageByZone[Zone]=1;
 B->TelegraphedAction.HitCount=1;
 FBattleUnificationTestAccess::Resolve(B);
 TestEqual(TEXT("Combat damage updates disk immediately"),ReadHealth(),Max-1);
 B->SetPlayerHealth(TEXT("A"),0);
 TestEqual(TEXT("Death is persisted as zero"),ReadHealth(),0);
 B->StopBattle();
 B->StartBattle();
 B->LoadRecords();
 FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"),TEXT("!방어"));
 TestEqual(TEXT("Zero HP survives reload"),B->GetBattlePlayers().FindChecked(TEXT("A")).HP,0);
 TestTrue(TEXT("Zero HP rejoins awaiting revival"),B->GetBattlePlayers().FindChecked(TEXT("A")).ReviveCountdown>0);
 for (int32 i=0;i<FMath::Max(1,B->ReviveTurns);++i) FBattleUnificationTestAccess::Resolve(B);
 TestEqual(TEXT("Revival updates disk"),ReadHealth(),Max);
 B->StopBattle();
 B->StartBattle();
 const FString Legacy=TEXT("{\"records\":[{\"userId\":\"Legacy\",\"characterId\":\"gladius\",\"maxHPBonus\":3,\"damageBonus\":2,\"totalDamageDealt\":123}]}");
 FFileHelper::SaveStringToFile(Legacy,*T.Path);
 B->LoadRecords();
 TestEqual(TEXT("Legacy migration initializes current HP"),ReadHealth(),FMath::Clamp(B->Classes[0].BaseHP + 3 * B->HPPerBonus, 5, 6));
 const FPlayerRecord* R=FBattleUnificationTestAccess::Record(B,TEXT("Legacy"));
 TestTrue(TEXT("Migration keeps other progression"),R && R->DamageBonus==2 && R->TotalDamageDealt==123);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerHealthCapTest, "Gladius.BattleUnified.PlayerHealthCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerHealthCapTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 B->Classes[0].BaseHP = 5;
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 B->SubmitAction(TEXT("Five"), TEXT("Five"), EBattleAction::Attack1, B->Classes[0].Id);
 TestEqual(TEXT("Base five HP remains five"), B->GetBattlePlayers().FindChecked(TEXT("Five")).MaxHP, 5);
 B->Classes[0].BaseHP = 7;
 B->SubmitAction(TEXT("Capped"), TEXT("Capped"), EBattleAction::Attack1, B->Classes[0].Id);
 TestEqual(TEXT("Legacy base seven HP caps at six"), B->GetBattlePlayers().FindChecked(TEXT("Capped")).MaxHP, 6);
 B->SetPlayerHealth(TEXT("Capped"), 100);
 TestEqual(TEXT("Healing cannot exceed cap"), B->GetBattlePlayers().FindChecked(TEXT("Capped")).HP, 6);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYouTubeBattleSubscriptionTest, "Gladius.BattleUnified.YouTubeReceiveSubscription",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FYouTubeBattleSubscriptionTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.Chat->MaxAllowedComments=100;
 auto* Dispatcher=T.Chat->GetCommandDispatcher();
 Dispatcher->BindBattleManager(B);
 Dispatcher->BindBattleManager(B);
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("yt-first"),TEXT("yt-viewer"),TEXT("YouTube name"),TEXT("!공격 1"));
 TestEqual(TEXT("Received YouTube chat joins exactly one player"),B->GetBattlePlayers().Num(),1);
 auto* Player=B->GetBattlePlayers().Find(TEXT("yt-viewer"));
 if (!TestNotNull(TEXT("Subscribed receiver reached BattleManager"),Player)) return false;
 TestEqual(TEXT("YouTube display name carried through parser"),Player->DisplayName,FString(TEXT("YouTube name")));
 TestTrue(TEXT("Attack reached actual pending action"),Player->PendingAction==EBattleAction::Attack1);
 Dispatcher->UnbindBattleManager(B);
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("yt-second"),TEXT("yt-viewer"),TEXT("YouTube name"),TEXT("!회피 좌"));
 TestTrue(TEXT("Missing initial binding recovers at receive time"),B->GetBattlePlayers().FindChecked(TEXT("yt-viewer")).PendingAction==EBattleAction::AvoidL);
 B->StopBattle();
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("yt-third"),TEXT("yt-viewer"),TEXT("YouTube name"),TEXT("!공격 2"));
 TestTrue(TEXT("Stopped receiver rejects battle command"),B->GetBattlePlayers().FindChecked(TEXT("yt-viewer")).PendingAction==EBattleAction::AvoidL);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnCutSceneSelectionRulesTest,"Gladius.BattleUnified.CutSceneSelectionRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurnCutSceneSelectionRulesTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 TMap<FString,FBattlePlayer> P;
 auto Add=[&](const FString& Id,int32 Class,int32 Fever,EBattleAction Action)
 {
  FBattlePlayer V;V.HP=V.MaxHP=5;V.ClassIndex=Class;V.Fever=Fever;V.PendingAction=Action;
  V.Unit=T.World->SpawnActor<ABattleUnit>();P.Add(Id,V);
 };
 Add(TEXT("G1"),0,20,EBattleAction::Attack1);
 Add(TEXT("G2"),0,20,EBattleAction::Attack2);
 Add(TEXT("S"),1,20,EBattleAction::Attack1);
 Add(TEXT("Defender"),0,100,EBattleAction::Defense);
 Add(TEXT("Dead"),0,100,EBattleAction::Attack1);P[TEXT("Dead")].HP=0;
 Add(TEXT("Reviving"),0,100,EBattleAction::Attack2);P[TEXT("Reviving")].ReviveCountdown=2;
 Add(TEXT("MissingUnit"),0,100,EBattleAction::Attack1);P[TEXT("MissingUnit")].Unit=nullptr;
 Add(TEXT("UnknownClass"),99,100,EBattleAction::Attack1);
 FRandomStream Random(1234);TSet<FString> Seen;
 auto Pick=[&](const FString& Last){return ABattleManager::SelectTurnCutSceneUser(P,T.Battle->Classes,Last,Seen,Random);};
 TestTrue(TEXT("First tie prefers Gladius"),Pick(TEXT("")).StartsWith(TEXT("G")));
 TestEqual(TEXT("Next class differs from prior Gladius"),Pick(TEXT("gladius")),FString(TEXT("S")));
 Seen.Add(TEXT("G1"));
 TestEqual(TEXT("Unseen user wins tied class and fever"),Pick(TEXT("sera")),FString(TEXT("G2")));
 P[TEXT("G1")].Fever=30;
 TestEqual(TEXT("Highest fever precedes unseen preference"),Pick(TEXT("sera")),FString(TEXT("G1")));
 P[TEXT("S")].Fever=40;
 TestEqual(TEXT("Highest fever precedes first-class preference"),Pick(TEXT("")),FString(TEXT("S")));
 P[TEXT("S")].Fever=20;P[TEXT("G1")].Fever=20;Seen.Reset();
 TSet<FString> Outcomes;
 for(int32 i=0;i<100;++i) Outcomes.Add(Pick(TEXT("sera")));
 TestEqual(TEXT("Random tie reaches both Gladius users"),Outcomes.Num(),2);
 Seen={TEXT("G1"),TEXT("G2"),TEXT("S")};
 TestFalse(TEXT("Previously selected pool remains eligible"),Pick(TEXT("sera")).IsEmpty());
 for(auto& Pair:P) Pair.Value.PendingAction=EBattleAction::Defense;
 TestTrue(TEXT("No attackers means no subject"),Pick(TEXT("")).IsEmpty());
 TestTrue(TEXT("Gladius skill one mapping"),ABattleManager::CutSceneIdsFor(TEXT("gladius"),EBattleAction::Attack1)==TArray<FName>({TEXT("GladiusRun"),TEXT("GladiusSkill1")}));
 TestTrue(TEXT("Gladius skill two mapping"),ABattleManager::CutSceneIdsFor(TEXT("gladius"),EBattleAction::Attack2)==TArray<FName>({TEXT("GladiusRun"),TEXT("GladiusSkill2")}));
 TestTrue(TEXT("Sera skill one mapping"),ABattleManager::CutSceneIdsFor(TEXT("sera"),EBattleAction::Attack1)==TArray<FName>({TEXT("SeraSkill1")}));
 TestTrue(TEXT("Sera skill two mapping"),ABattleManager::CutSceneIdsFor(TEXT("sera"),EBattleAction::Attack2)==TArray<FName>({TEXT("SeraSkill2")}));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnCutSceneSubjectChainTest,"Gladius.BattleUnified.SelectedSubjectAndShotChain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurnCutSceneSubjectChainTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;auto* B=T.Battle;
 B->bEnableTurnCutScenes=true;B->StartBattle();FBattleUnificationTestAccess::Prepare(B);
 auto* Applier=T.World->SpawnActor<ACutSceneApplier>();B->CutSceneApplier=Applier;
 Applier->EnsureDefaultCutScenes();
 for(const TCHAR* Id:{TEXT("GladiusSkill2"),TEXT("SeraSkill1"),TEXT("SeraSkill2")})
  TestTrue(TEXT("Default shot assets load on demand"),IsValid(Applier->CutScenes.FindChecked(FName(Id)).Sequence));
 for(const TCHAR* Id:{TEXT("GladiusRun"),TEXT("GladiusSkill1"),TEXT("GladiusSkill2"),TEXT("SeraSkill1"),TEXT("SeraSkill2")})
 {
  ULevelSequence* Seq=NewObject<ULevelSequence>(Applier);Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,24000);
  FCutSceneDefinition& Def=Applier->CutScenes.FindOrAdd(FName(Id));Def.CameraClass=nullptr;Def.Sequence=Seq;
  Def.WorldTimeScale=1.f;Def.SubjectBindingTag=Def.BossBindingTag=NAME_None;
 }
 auto Submit=[&](const TCHAR* Id,const TCHAR* Class,EBattleAction Action,int32 Fever)
 {
  B->SubmitAction(Id,Id,Action,Class);FBattleUnificationTestAccess::Fever(B,Id,Fever);
 };
 Submit(TEXT("G1"),TEXT("gladius"),EBattleAction::Attack1,40);
 Submit(TEXT("G2"),TEXT("gladius"),EBattleAction::Attack2,10);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack1,40);
 FBattleUnificationTestAccess::Move(B);
 TestEqual(TEXT("First turn selects Gladius"),B->CutSceneUserId,FString(TEXT("G1")));
 TestTrue(TEXT("Skill-one shot list computed"),B->CutSceneIds==TArray<FName>({TEXT("GladiusRun"),TEXT("GladiusSkill1")}));
 auto* G1=B->GetBattlePlayers().FindChecked(TEXT("G1")).Unit.Get();
 TestEqual(TEXT("Run uses selected user"),Applier->GetCurrentSubject(),G1);
 FBattleUnificationTestAccess::Fever(B,TEXT("G2"),100);
 Applier->CancelCutScene();
 TestEqual(TEXT("Skill shot keeps same user even if another fever changes"),Applier->GetCurrentSubject(),G1);
 Applier->CancelCutScene();TestTrue(TEXT("Chain completion does not restart Move"),B->Phase==EBattlePhase::Move);
 FBattleUnificationTestAccess::Prepare(B);
 Submit(TEXT("G1"),TEXT("gladius"),EBattleAction::Attack2,40);
 Submit(TEXT("G2"),TEXT("gladius"),EBattleAction::Attack2,40);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack2,40);
 FBattleUnificationTestAccess::Move(B);
 TestEqual(TEXT("Next turn prefers other class"),B->CutSceneUserId,FString(TEXT("S")));
 TestTrue(TEXT("Sera skill-two list"),B->CutSceneIds==TArray<FName>({TEXT("SeraSkill2")}));
 TestTrue(TEXT("Sera camera starts immediately during movement"),Applier->IsPlaying());
 TestEqual(TEXT("Camera starts selected Sera skill before attack dispatch"),FBattleUnificationTestAccess::StartedCutSceneAttack(B),FString(TEXT("S")));
 FBattleUnificationTestAccess::Resolve(B);
 TestTrue(TEXT("Camera is already playing before attack dispatch"),Applier->IsPlaying());
 FBattleUnificationTestAccess::SelectedAttackLast(B);
 FBattleUnificationTestAccess::FlushOneAttack(B);
 TestTrue(TEXT("First unrelated attack does not delay Sera camera"),Applier->IsPlaying());
 FBattleUnificationTestAccess::FlushOneAttack(B);
 TestTrue(TEXT("Second unrelated attack does not delay Sera camera"),Applier->IsPlaying());
 FBattleUnificationTestAccess::FlushOneAttack(B);
 TestTrue(TEXT("Selected Sera attack keeps existing camera playback"),Applier->IsPlaying());
 TestEqual(TEXT("Early skill remains marked to prevent duplicate playback"),FBattleUnificationTestAccess::StartedCutSceneAttack(B),FString(TEXT("S")));
 TestEqual(TEXT("Sera is explicit subject"),Applier->GetCurrentSubject(),B->GetBattlePlayers().FindChecked(TEXT("S")).Unit.Get());
 Applier->CancelCutScene();
 FBattleUnificationTestAccess::Prepare(B);
 Submit(TEXT("G1"),TEXT("gladius"),EBattleAction::Attack2,40);
 Submit(TEXT("G2"),TEXT("gladius"),EBattleAction::Attack2,40);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack1,40);
 FBattleUnificationTestAccess::Move(B);
 TestEqual(TEXT("Class returns to Gladius and unseen user wins"),B->CutSceneUserId,FString(TEXT("G2")));
 TestTrue(TEXT("Gladius skill-two list"),B->CutSceneIds==TArray<FName>({TEXT("GladiusRun"),TEXT("GladiusSkill2")}));
 FBattleUnificationTestAccess::Resolve(B);
 FBattleUnificationTestAccess::SelectedAttackLast(B);
 FBattleUnificationTestAccess::FlushOneAttack(B);
 TestEqual(TEXT("Unrelated attack keeps Gladius run shot"),FBattleUnificationTestAccess::ShotIndex(B),1);
 FBattleUnificationTestAccess::FlushOneAttack(B);
 FBattleUnificationTestAccess::FlushOneAttack(B);
 TestEqual(TEXT("Selected Gladius attack advances run to authored skill"),FBattleUnificationTestAccess::ShotIndex(B),2);
 TestTrue(TEXT("Authored Gladius camera plays at attack dispatch"),Applier->IsPlaying());
 Applier->CancelCutScene();
 FBattleUnificationTestAccess::Prepare(B);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack1,50);
 FBattleUnificationTestAccess::Move(B);
 TestTrue(TEXT("Sera skill-one list"),B->CutSceneIds==TArray<FName>({TEXT("SeraSkill1")}));
 TestEqual(TEXT("Sera skill one starts with its camera"),FBattleUnificationTestAccess::StartedCutSceneAttack(B),FString(TEXT("S")));
 Applier->CancelCutScene();
 FBattleUnificationTestAccess::Prepare(B);
 FBattleUnificationTestAccess::Move(B);
 TestTrue(TEXT("No attack skips shots and clears prior selection"),B->CutSceneUserId.IsEmpty() && B->CutSceneIds.IsEmpty() && B->Phase==EBattlePhase::Move);
 B->StartBattle();FBattleUnificationTestAccess::Prepare(B);
 Submit(TEXT("G1"),TEXT("gladius"),EBattleAction::Attack1,40);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack1,40);
 FBattleUnificationTestAccess::Move(B);
 TestEqual(TEXT("New battle resets class/user history"),B->CutSceneUserId,FString(TEXT("G1")));
 B->StopBattle();
 TestFalse(TEXT("Stop cancels selected-user cutscene"),Applier->IsPlaying());
 TestTrue(TEXT("Stop clears early skill state"),FBattleUnificationTestAccess::StartedCutSceneAttack(B).IsEmpty());
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectStartsMoveCutSceneTest,"Gladius.BattleUnified.CollectEntersMoveWithCutScene",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectStartsMoveCutSceneTest::RunTest(const FString& Parameters)
{
 auto Context=MakeShared<FUnifiedBattleWorld>();auto& T=*Context;auto* B=T.Battle;
 B->bEnableTurnCutScenes=true;B->StartBattle();FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"),TEXT("!공격 1"));
 auto* Applier=T.World->SpawnActor<ACutSceneApplier>();B->CutSceneApplier=Applier;
 Applier->EnsureDefaultCutScenes();
 auto* Seq=NewObject<ULevelSequence>(Applier);Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,24000);
 FCutSceneDefinition& Def=Applier->CutScenes.FindOrAdd(TEXT("GladiusRun"));
 Def.Sequence=Seq;Def.CameraClass=nullptr;Def.WorldTimeScale=1.f;
 Def.SubjectBindingTag=Def.BossBindingTag=NAME_None;
 B->CollectSeconds=0.01f;B->MoveSeconds=10.f;
 FBattleUnificationTestAccess::Collect(B);
 ADD_LATENT_AUTOMATION_COMMAND(FCollectMoveTimerAssertions(Context,Applier,this));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTemporaryDataResetTest,"Gladius.BattleUnified.TemporaryDataResetButtons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTemporaryDataResetTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* B=T.Battle;
 B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
 auto Join=[&](const TCHAR* Id) { return B->SubmitAction(Id,Id,EBattleAction::Attack1,TEXT("gladius")); };
 TestTrue(TEXT("Real user joins"),Join(TEXT("real-user")));
 TestTrue(TEXT("Similar real handle joins"),Join(TEXT("@testuserFan")));
 TestTrue(TEXT("Decimal-looking real handle joins"),Join(TEXT("@testuser1.2")));
 TestTrue(TEXT("Active test user joins"),Join(TEXT("@testuser00")));
 TestTrue(TEXT("Offline test user joins"),Join(TEXT("@testuser42")));
 B->LogoutPlayer(TEXT("@testuser42"));
 const int32 RealHP=B->GetBattlePlayers().FindChecked(TEXT("real-user")).HP;
 auto* Controller=T.World->SpawnActor<APlayerController>();
 auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);
 T.Instance->AddLocalPlayer(LocalPlayer,FPlatformUserId::CreateFromInternalId(0));
 Controller->SetPlayer(LocalPlayer);
 auto* Widget=CreateWidget<UBattleDataResetWidget>(Controller,UBattleDataResetWidget::StaticClass());
 if(!TestNotNull(TEXT("Temporary controls can be created"),Widget)) return false;
 Widget->SetBattleManager(B);
 auto* TestButton=Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("ResetTestUsersButton")));
 auto* AllButton=Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("ResetAllUsersButton")));
 if(!TestNotNull(TEXT("Test-only button exists"),TestButton)||!TestNotNull(TEXT("All-user button exists"),AllButton)) return false;
 TestButton->OnClicked.Broadcast();
 TestTrue(TEXT("Real users remain in battle"),B->GetBattlePlayers().Contains(TEXT("real-user"))&&B->GetBattlePlayers().Contains(TEXT("@testuserFan")));
 TestFalse(TEXT("Active test user leaves"),B->GetBattlePlayers().Contains(TEXT("@testuser00")));
 TestTrue(TEXT("Both active and offline test records are removed"),!FBattleUnificationTestAccess::Record(B,TEXT("@testuser00"))&&!FBattleUnificationTestAccess::Record(B,TEXT("@testuser42")));
 TestEqual(TEXT("Real health is preserved"),B->GetBattlePlayers().FindChecked(TEXT("real-user")).HP,RealHP);
 auto ReadDisk=[&]() { FString Json; FPlayerRecordFile File; FFileHelper::LoadFileToString(Json,*T.Path); FJsonObjectConverter::JsonObjectStringToUStruct(Json,&File); return File; };
 TestEqual(TEXT("JSON keeps only real users"),ReadDisk().Records.Num(),3);
 B->SaveRecords(); B->LoadRecords();
 TestFalse(TEXT("Removed test users stay removed after save/reload"),FBattleUnificationTestAccess::Record(B,TEXT("@testuser42"))!=nullptr);
 TestTrue(TEXT("Reset test user can join afresh"),Join(TEXT("@testuser00")));
 B->bEnableTurnCutScenes=true;
 FBattleUnificationTestAccess::Fever(B,TEXT("@testuser00"),100);
 FBattleUnificationTestAccess::Move(B);
 TestTrue(TEXT("Reset is exercised with a running cutscene"),B->CutSceneApplier&&B->CutSceneApplier->IsPlaying());
 AllButton->OnClicked.Broadcast();
 TestEqual(TEXT("All participants removed"),B->GetBattlePlayers().Num(),0);
 TestEqual(TEXT("JSON is cleared"),ReadDisk().Records.Num(),0);
 TestTrue(TEXT("Battle restarts in an empty state"),B->IsBattleRunning()&&B->Phase==EBattlePhase::BattleStart);
 B->SaveRecords(); B->LoadRecords();
 TestEqual(TEXT("Later saves cannot restore removed users"),ReadDisk().Records.Num(),0);
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGladiusRunArrivalTest,"Gladius.BattleUnified.GladiusRunEndsOnArrival",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGladiusRunArrivalTest::RunTest(const FString& Parameters)
{
 for(EBattleAction Action : {EBattleAction::Attack1,EBattleAction::Attack2})
 {
  FUnifiedBattleWorld T; auto* B=T.Battle;
  B->bEnableTurnCutScenes=true;
  B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
  B->SubmitAction(TEXT("runner"),TEXT("Runner"),Action,TEXT("gladius"));
  B->SubmitAction(TEXT("other"),TEXT("Other"),Action,TEXT("gladius"));
  FBattleUnificationTestAccess::Fever(B,TEXT("runner"),100);
  auto* Applier=T.World->SpawnActor<ACutSceneApplier>(); B->CutSceneApplier=Applier;
  FBattleUnificationTestAccess::Move(B);
  auto* Subject=B->GetBattlePlayers().FindChecked(TEXT("runner")).Unit.Get();
  auto* Other=B->GetBattlePlayers().FindChecked(TEXT("other")).Unit.Get();
  AGladiusRunCamera* RunCamera=nullptr;
  for(TActorIterator<AGladiusRunCamera> It(T.World);It;++It) if(IsValid(*It)) RunCamera=*It;
  if(!TestNotNull(TEXT("Run shot is active"),RunCamera)) return false;
  TestTrue(TEXT("Run camera listens to actual attack start"),Subject->OnAttackStarted.IsBoundToObject(RunCamera));
  TestEqual(TEXT("Skill has not started while running"),FBattleUnificationTestAccess::ShotIndex(B),1);
  Other->MoveTo(Other->GetActorLocation()+FVector(10,0,0),0.1f);
  static_cast<AActor*>(Other)->Tick(0.2f);
  TestTrue(TEXT("Another participant arrival does not end this shot"),IsValid(RunCamera));
  Subject->MoveTo(Subject->GetActorLocation()+FVector(100,0,0),1.f);
  static_cast<AActor*>(Subject)->Tick(0.5f);
  TestTrue(TEXT("Run shot remains during movement"),IsValid(RunCamera)&&Subject->IsMoving());
  static_cast<AActor*>(Subject)->Tick(0.5f);
  TestFalse(TEXT("Movement ends"),Subject->IsMoving());
  TestFalse(TEXT("Run camera is removed in the arrival frame without camera tick or attack dispatch"),IsValid(RunCamera));
  TestEqual(TEXT("Arrival immediately advances to the skill shot"),FBattleUnificationTestAccess::ShotIndex(B),2);
  TestTrue(TEXT("Authored skill camera is already playing"),Applier->IsPlaying());
  TestEqual(TEXT("Skill shot keeps selected participant"),Applier->GetCurrentSubject(),Subject);
  static_cast<AActor*>(Subject)->Tick(0.1f);
  TestEqual(TEXT("Arrival is only delivered once"),FBattleUnificationTestAccess::ShotIndex(B),2);
  B->StopBattle();
  TestFalse(TEXT("Stopped run camera leaves no arrival listener"),Subject->OnMovementFinished.IsBound());
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGladiusDirectAttackMontageTest,"Gladius.BattleUnified.GladiusDirectAttackAndCamera",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGladiusDirectAttackMontageTest::RunTest(const FString& Parameters)
{
 UClass* UnitClass=LoadClass<ABattleUnit>(nullptr,TEXT("/Game/Blueprints/ScaledBattleUnits/BP_BattleUnitG_DevBattle.BP_BattleUnitG_DevBattle_C"));
 if(!TestNotNull(TEXT("Actual Gladius blueprint loads"),UnitClass)) return false;
 for(int32 Skill : {1,2})
 {
  FUnifiedBattleWorld T; auto* B=T.Battle;
  B->UnitClassByCharacter.Add(TEXT("gladius"),UnitClass);
  B->bEnableTurnCutScenes=true; B->RunSpeed=1.f; B->RollSpeed=2.f;
  B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
  B->SubmitAction(TEXT("runner"),TEXT("Runner"),Skill==1?EBattleAction::Attack1:EBattleAction::Attack2,TEXT("gladius"));
  auto* Subject=B->GetBattlePlayers().FindChecked(TEXT("runner")).Unit.Get();
  if(!Subject->HasActorBegunPlay()) Subject->DispatchBeginPlay();
  auto* Mesh=Subject->FindComponentByClass<USkeletalMeshComponent>();
  Mesh->InitAnim(true);
  auto* Anim=Mesh->GetAnimInstance();
  if(!TestNotNull(TEXT("Actual animation instance exists"),Anim)) return false;
  const FString Asset=FString::Printf(TEXT("/Game/Asset/Battle/gladius/AnimationMontage/AM_Gladius_Attack%d.AM_Gladius_Attack%d"),Skill,Skill);
  auto* Attack=LoadObject<UAnimMontage>(nullptr,*Asset);
  auto* Roll=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Asset/Battle/gladius/AnimationMontage/AM_Gladius_Roll.AM_Gladius_Roll"));
  FBattleUnificationTestAccess::Move(B);
  TestTrue(TEXT("Attack approach uses run even at roll speed"),Subject->GetMoveGait()!=EMoveGait::Roll);
  auto* Applier=B->CutSceneApplier.Get();
  int32 StartCount=0;
  bool bMontagePlayingAtSignal=false;
  const FDelegateHandle StartedHandle=Subject->OnAttackStarted.AddLambda([&](int32 Index)
  {
   ++StartCount;
   bMontagePlayingAtSignal=Index==Skill&&Anim->Montage_IsPlaying(Attack)&&Anim->GetCurrentActiveMontage()==Attack;
  });
  Anim->Montage_Play(Roll);
  Subject->MoveTo(Subject->GetActorLocation()+FVector(100,0,0),1.f,EMoveGait::Run);
  const double ArrivalTime=T.World->GetTimeSeconds();
  static_cast<AActor*>(Subject)->Tick(1.f);
  TestEqual(TEXT("One skill starts immediately on arrival"),StartCount,1);
  TestTrue(TEXT("Attack montage is already playing when start signal is emitted"),bMontagePlayingAtSignal);
  TestEqual(TEXT("No timer or frame wait between attack and camera"),T.World->GetTimeSeconds(),ArrivalTime);
  TestEqual(TEXT("Skill camera starts in the attack call"),FBattleUnificationTestAccess::ShotIndex(B),2);
  TestTrue(TEXT("Skill camera is playing"),Applier->IsPlaying());
  TestTrue(TEXT("Attack replaces any roll montage"),Anim->GetCurrentActiveMontage()==Attack);
  FBattleUnificationTestAccess::Resolve(B);
  FBattleUnificationTestAccess::FlushOneAttack(B);
  TestEqual(TEXT("Attack queue does not replay the skill"),StartCount,1);
  TestEqual(TEXT("Attack queue does not restart the camera"),FBattleUnificationTestAccess::ShotIndex(B),2);
  Subject->OnAttackStarted.Remove(StartedHandle);
  B->StopBattle();
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutSceneWalkRunOnlyTest,"Gladius.BattleUnified.CutSceneGladiusWalkRunOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCutSceneWalkRunOnlyTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* B=T.Battle;
 B->bEnableTurnCutScenes=true; B->RunSpeed=350.f; B->RollSpeed=700.f;
 B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
 B->SubmitAction(TEXT("selected"),TEXT("Selected"),EBattleAction::Attack1,TEXT("gladius"));
 B->SubmitAction(TEXT("other"),TEXT("Other"),EBattleAction::Attack2,TEXT("gladius"));
 FBattleUnificationTestAccess::Fever(B,TEXT("selected"),100);
 FBattleUnificationTestAccess::Move(B);
 auto* Selected=B->GetBattlePlayers().FindChecked(TEXT("selected")).Unit.Get();
 auto* Other=B->GetBattlePlayers().FindChecked(TEXT("other")).Unit.Get();
 TestEqual(TEXT("Cutscene applies to selected Gladius"),B->CutSceneApplier->GetCurrentSubject(),Selected);
 TestTrue(TEXT("Low speed remains walk"),FBattleUnificationTestAccess::MovementGait(B,Selected,100.f,1.f)==EMoveGait::Walk);
 TestTrue(TEXT("Normal speed remains run"),FBattleUnificationTestAccess::MovementGait(B,Selected,500.f,1.f)==EMoveGait::Run);
 TestTrue(TEXT("Roll threshold becomes run"),FBattleUnificationTestAccess::MovementGait(B,Selected,700.f,1.f)==EMoveGait::Run);
 TestTrue(TEXT("Very high speed remains run"),FBattleUnificationTestAccess::MovementGait(B,Selected,10000.f,0.1f)==EMoveGait::Run);
 TestTrue(TEXT("Zero duration cannot select roll"),FBattleUnificationTestAccess::MovementGait(B,Selected,100.f,0.f)==EMoveGait::Run);
 TestTrue(TEXT("Non-selected unit keeps default speed choices"),FBattleUnificationTestAccess::MovementGait(B,Other,1000.f,1.f)==EMoveGait::Roll);
 Selected->SetActorLocation(Selected->GetActorLocation()+FVector(10000,0,0));
 Other->SetActorLocation(Other->GetActorLocation()+FVector(10000,0,0));
 FBattleUnificationTestAccess::EndTurn(B);
 TestTrue(TEXT("Actual high-speed return uses run after the cutscene ends"),Selected->GetMoveGait()==EMoveGait::Run);
 TestTrue(TEXT("Other unit return keeps normal speed choice"),Other->GetMoveGait()==EMoveGait::Roll);
 FBattleUnificationTestAccess::Prepare(B);
 B->SubmitAction(TEXT("selected"),TEXT("Selected"),EBattleAction::Attack1,TEXT("gladius"));
 B->SubmitAction(TEXT("other"),TEXT("Other"),EBattleAction::Attack2,TEXT("gladius"));
 FBattleUnificationTestAccess::Fever(B,TEXT("selected"),1);
 FBattleUnificationTestAccess::Fever(B,TEXT("other"),100);
 FBattleUnificationTestAccess::Move(B);
 TestEqual(TEXT("Next turn changes the selected participant"),B->CutSceneApplier->GetCurrentSubject(),Other);
 TestTrue(TEXT("New selected Gladius cannot speed-roll"),FBattleUnificationTestAccess::MovementGait(B,Other,1000.f,1.f)==EMoveGait::Run);
 TestTrue(TEXT("Previous selected participant is released"),FBattleUnificationTestAccess::MovementGait(B,Selected,1000.f,1.f)==EMoveGait::Roll);
 B->StopBattle();
 TestTrue(TEXT("Stopping battle clears the temporary movement policy"),FBattleUnificationTestAccess::MovementGait(B,Other,1000.f,1.f)==EMoveGait::Roll);
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKeyboardSkillRejectsRollTest,"Gladius.BattleUnified.KeyboardSkillRejectsRoll",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKeyboardSkillRejectsRollTest::RunTest(const FString& Parameters)
{
 UClass* UnitClass=LoadClass<ABattleUnit>(nullptr,TEXT("/Game/Blueprints/ScaledBattleUnits/BP_BattleUnitG_DevBattle.BP_BattleUnitG_DevBattle_C"));
 if(!TestNotNull(TEXT("Actual Gladius class loads"),UnitClass)) return false;
 for(int32 Skill : {1,2})
 {
  FUnifiedBattleWorld T; auto* B=T.Battle;
  B->UnitClassByCharacter.Add(TEXT("gladius"),UnitClass);
  B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
  B->SubmitAction(TEXT("@testuser00"),TEXT("Keyboard user"),EBattleAction::Idle,TEXT("gladius"));
  auto* Subject=B->GetBattlePlayers().FindChecked(TEXT("@testuser00")).Unit.Get();
  if(!Subject->HasActorBegunPlay()) Subject->DispatchBeginPlay();
  auto* Mesh=Subject->FindComponentByClass<USkeletalMeshComponent>(); Mesh->InitAnim(true);
  auto* Anim=Mesh->GetAnimInstance();
  if(!TestNotNull(TEXT("Animation instance exists"),Anim)) return false;
  auto* Roll=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Asset/Battle/gladius/AnimationMontage/AM_Gladius_Roll.AM_Gladius_Roll"));
  T.World->SpawnActor<APlayerController>();
  FBattleUnificationTestAccess::BindKeys(B);
  Anim->Montage_Play(Roll);
  TestTrue(TEXT("Reproduction begins with an active Roll montage"),Anim->Montage_IsPlaying(Roll));
  TestTrue(TEXT("Actual numeric skill key is bound"),FBattleUnificationTestAccess::PressKey(B,Skill==1?EKeys::Two:EKeys::Three));
  TestTrue(TEXT("Numeric key selects the requested skill"),Subject->GetSelectedAction()==(Skill==1?EBattleAction::Attack1:EBattleAction::Attack2));
  TestFalse(TEXT("Selecting a skill immediately stops an existing Roll montage"),Anim->Montage_IsPlaying(Roll));
  Subject->OnEvadeRoll(0.8f,200.f);
  TestFalse(TEXT("Roll animation request cannot play during a selected skill"),Anim->Montage_IsPlaying(Roll));
  Subject->NotifyAttack(Skill,false,true);
  auto* Attack=Anim->GetCurrentActiveMontage();
  TestTrue(TEXT("Selected skill montage plays"),Attack&&Attack->GetName()==FString::Printf(TEXT("AM_Gladius_Attack%d"),Skill));
  Subject->OnEvadeRoll(0.8f,200.f);
  TestTrue(TEXT("Roll request cannot interrupt the actual skill montage"),Anim->GetCurrentActiveMontage()==Attack);
  Subject->NotifyActionSelected(EBattleAction::AvoidL);
  Subject->OnEvadeRoll(0.8f,200.f);
  TestTrue(TEXT("Explicit evade action still plays its Roll montage"),Anim->Montage_IsPlaying(Roll));
  B->StopBattle();
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopulationBossTargetTest,"Gladius.BattleUnified.PopulationBossTargets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPopulationBossTargetTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* B=T.Battle;
 B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
 if (!TestTrue(TEXT("Actual balance JSON loads"),FBattleUnificationTestAccess::LoadBossData(B))) return false;
 const auto& Data=FBattleUnificationTestAccess::BossData(B);
 TestEqual(TEXT("Three attacks added"),Data.Actions.Num(),16);
 TestEqual(TEXT("Three single-turn patterns added"),Data.Patterns.Num(),25);
 for (const auto& Action:Data.Actions)
 {
  const bool Strong=Action.Id.StartsWith(TEXT("StrongStrike"))||Action.Id==TEXT("AllStrike");
  for (int32 Lane=0;Lane<3;++Lane)
   if(Action.DamageByZone[Lane]>0) TestEqual(TEXT("All positive boss damage doubled"),Action.DamageByZone[Lane],Strong?4:2);
 }
 const TCHAR* Ids[]={TEXT("MostPlayer"),TEXT("LeastPlayer"),TEXT("MostDamage")};
 const TCHAR* Texts[]={TEXT("트루돈이 사람이 가장 많은 곳을 찾고 있습니다"),TEXT("트루돈이 사람이 가장 적은 곳을 찾고 있습니다"),TEXT("트루돈이 가장 강한 공격이 오는 곳을 찾고 있습니다")};
 const int32 Expected[]={0,2,1};
 for(int32 Index=0;Index<3;++Index)
 {
  const auto* Definition=Data.FindAction(Ids[Index]);
  if(!TestNotNull(TEXT("New attack exists"),Definition)) return false;
  TestEqual(TEXT("Fury telegraph animation reused"),Definition->TelegraphMotion(),FString(TEXT("Fury")));
  TestEqual(TEXT("Fury attack animation reused"),Definition->AttackMotion(),FString(TEXT("Fury")));
  TestEqual(TEXT("Requested telegraph text"),Definition->TelegraphText,FString(Texts[Index]));
  const FString PatternId=FString::Printf(TEXT("pattern_%02d"),23+Index);
  const auto* Pattern=Data.FindPattern(PatternId);
  if(!TestNotNull(TEXT("Single-turn pattern exists"),Pattern)) return false;
  TestEqual(TEXT("One action per pattern"),Pattern->Steps.Num(),1);
  TestEqual(TEXT("Pattern points to new action"),Pattern->Steps[0],FString(Ids[Index]));
  TestTrue(TEXT("New pattern participates in weighted selection"),Data.Pool.ContainsByPredicate([&](const FPatternWeight& W){return W.PatternId==PatternId&&W.Weight>0.f;}));
  FBattleUnificationTestAccess::ClearPlayers(B);
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("L1"),EBattleLane::Left,2);
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("L2"),EBattleLane::Left,3);
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("L3"),EBattleLane::Left,4);
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("M1"),EBattleLane::Mid,20);
  FBattleUnificationTestAccess::SetBossAction(B,*Definition);
  TestTrue(TEXT("Target still pending during input"),FBattleUnificationTestAccess::BossAction(B).NeedsBattleStartTarget());
  //A late input/join must count; dead and reviving participants must not count.
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("Rdead"),EBattleLane::Right,100,0);
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("Rreviving"),EBattleLane::Right,100,7,1);
  FBattleUnificationTestAccess::Move(B);
  const auto& Locked=FBattleUnificationTestAccess::BossAction(B);
  TestFalse(TEXT("Target resolved when collection ends"),Locked.NeedsBattleStartTarget());
  for(int32 Lane=0;Lane<3;++Lane)
   TestEqual(TEXT("Exactly the required snapshot lane takes damage"),Locked.DamageByZone[Lane],Lane==Expected[Index]?2:0);
  TestTrue(TEXT("Snapshot precedes avoidance movement"),B->GetBattlePlayers().FindChecked(TEXT("L1")).Lane==EBattleLane::Left);
  FBattleUnificationTestAccess::ClearPlayers(B);
  FBattleUnificationTestAccess::PutPlayer(B,TEXT("new"),EBattleLane::Right,1000);
  FBattleUnificationTestAccess::LockTarget(B);
  for(int32 Lane=0;Lane<3;++Lane)
   TestEqual(TEXT("Later movement/fever changes cannot retarget"),Locked.DamageByZone[Lane],Lane==Expected[Index]?2:0);
 }
 for(const TCHAR* Id:Ids)
 {
  const auto* Definition=Data.FindAction(Id);
  //No participants: all three zero scores tie and exactly one lane is selected.
  FBattleUnificationTestAccess::ClearPlayers(B);
  for(int32 Trial=0;Trial<30;++Trial)
  {
   FBattleUnificationTestAccess::SetBossAction(B,*Definition);
   FBattleUnificationTestAccess::LockTarget(B);
   int32 Hits=0;
   for(int32 Lane=0;Lane<3;++Lane) if(FBattleUnificationTestAccess::BossAction(B).DamageByZone[Lane]>0) ++Hits;
   TestEqual(TEXT("Empty/tied lanes select exactly one"),Hits,1);
  }
 }
 //A partial tie may never select the strictly worse lane.
 FBattleUnificationTestAccess::PutPlayer(B,TEXT("left"),EBattleLane::Left,5);
 FBattleUnificationTestAccess::PutPlayer(B,TEXT("mid"),EBattleLane::Mid,5);
 for(const TCHAR* Id:{TEXT("MostPlayer"),TEXT("MostDamage")})
  for(int32 Trial=0;Trial<30;++Trial)
  {
   FBattleUnificationTestAccess::SetBossAction(B,*Data.FindAction(Id));
   FBattleUnificationTestAccess::LockTarget(B);
   TestEqual(TEXT("Ties stay within the best lanes"),FBattleUnificationTestAccess::BossAction(B).DamageByZone[2],0);
  }
 B->RunSelfTest();
 return true;
}

#endif
