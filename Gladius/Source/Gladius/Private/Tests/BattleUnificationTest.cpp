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
#include "Scripts/Battle/CutSceneApplier.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include <type_traits>

struct FBattleUnificationTestAccess
{
 static void SetPath(ABattleManager* B, const FString& Path) { B->TestRecordPath = Path; }
 static void Prepare(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->StartTurn(); }
 static void Select(ABattleManager* B) { B->BeginInput(); }
 static void Collect(ABattleManager* B) { B->BeginCollect(); }
 static void CutScenes(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->BeginCutScenes(); }
 static void Resolve(ABattleManager* B) { B->ResolveTurn(); }
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
  Battle->CutSceneIds.Reset();
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
 FBattleUnificationTestAccess::CutScenes(B);
 TestTrue(TEXT("No cutscenes advances to Move"), B->Phase == EBattlePhase::Move);
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
 FBattleUnificationTestAccess::CutScenes(B);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnifiedBattleCutSceneTest, "Gladius.BattleUnified.CutScenesGateMovementAndStop",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUnifiedBattleCutSceneTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 ACutSceneApplier* Applier = T.World->SpawnActor<ACutSceneApplier>();
 B->CutSceneApplier = Applier;
 B->CutSceneIds = {TEXT("First"), TEXT("Second")};
 for (FName Id : B->CutSceneIds)
 {
  ULevelSequence* Sequence = NewObject<ULevelSequence>(Applier);
  Sequence->Initialize();
  Sequence->GetMovieScene()->SetPlaybackRange(0,24000);
  FCutSceneDefinition& Def = Applier->CutScenes.Add(Id);
  Def.Sequence = Sequence;
  Def.WorldTimeScale = 1.f;
  Def.SubjectBindingTag = Def.BossBindingTag = NAME_None;
 }
 FBattleUnificationTestAccess::Collect(B);
 FBattleUnificationTestAccess::CutScenes(B);
 TestTrue(TEXT("Cutscene stage holds movement"), B->Phase == EBattlePhase::CutScenes);
 TestTrue(TEXT("First cutscene plays"), Applier->IsPlaying());
 TestFalse(TEXT("Cutscenes reject inputs"), B->IsAcceptingInput());
 Applier->CancelCutScene();
 TestTrue(TEXT("Second cutscene holds same phase"), B->Phase == EBattlePhase::CutScenes && Applier->IsPlaying());
 Applier->CancelCutScene();
 TestTrue(TEXT("Final completion releases movement"), B->Phase == EBattlePhase::Move);
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"), TEXT("!공격 1"));
 FBattleUnificationTestAccess::CutScenes(B);
 B->StopBattle();
 TestFalse(TEXT("Stop cancels active shot"), Applier->IsPlaying());
 TestTrue(TEXT("Cancelled callback cannot restart movement"), B->Phase == EBattlePhase::BattleEnd);
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
 TestEqual(TEXT("Legacy migration initializes current HP"),ReadHealth(),B->Classes[0].BaseHP + 3 * B->HPPerBonus);
 const FPlayerRecord* R=FBattleUnificationTestAccess::Record(B,TEXT("Legacy"));
 TestTrue(TEXT("Migration keeps other progression"),R && R->DamageBonus==2 && R->TotalDamageDealt==123);
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

#endif
