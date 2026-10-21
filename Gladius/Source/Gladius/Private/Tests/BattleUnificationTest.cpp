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
 static void Move(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->BeginMove(); }
 static float PhaseRemaining(ABattleManager* B) { return B->GetWorldTimerManager().GetTimerRemaining(B->PhaseTimerHandle); }
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
 for(const TCHAR* Id:{TEXT("GladiusSkill2"),TEXT("SeraSkill1"),TEXT("SeraSkill2")})
  TestTrue(TEXT("Missing shots have saved blank sequence assets"),IsValid(Applier->CutScenes.FindChecked(FName(Id)).Sequence));
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
 TestEqual(TEXT("Sera is explicit subject"),Applier->GetCurrentSubject(),B->GetBattlePlayers().FindChecked(TEXT("S")).Unit.Get());
 Applier->CancelCutScene();
 FBattleUnificationTestAccess::Prepare(B);
 Submit(TEXT("G1"),TEXT("gladius"),EBattleAction::Attack2,40);
 Submit(TEXT("G2"),TEXT("gladius"),EBattleAction::Attack2,40);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack1,40);
 FBattleUnificationTestAccess::Move(B);
 TestEqual(TEXT("Class returns to Gladius and unseen user wins"),B->CutSceneUserId,FString(TEXT("G2")));
 TestTrue(TEXT("Gladius skill-two list"),B->CutSceneIds==TArray<FName>({TEXT("GladiusRun"),TEXT("GladiusSkill2")}));
 Applier->CancelCutScene();Applier->CancelCutScene();
 FBattleUnificationTestAccess::Prepare(B);
 Submit(TEXT("S"),TEXT("sera"),EBattleAction::Attack1,50);
 FBattleUnificationTestAccess::Move(B);
 TestTrue(TEXT("Sera skill-one list"),B->CutSceneIds==TArray<FName>({TEXT("SeraSkill1")}));
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
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectStartsMoveCutSceneTest,"Gladius.BattleUnified.CollectEntersMoveWithCutScene",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectStartsMoveCutSceneTest::RunTest(const FString& Parameters)
{
 auto Context=MakeShared<FUnifiedBattleWorld>();auto& T=*Context;auto* B=T.Battle;
 B->bEnableTurnCutScenes=true;B->StartBattle();FBattleUnificationTestAccess::Prepare(B);
 T.Dispatch(TEXT("A"),TEXT("!공격 1"));
 auto* Applier=T.World->SpawnActor<ACutSceneApplier>();B->CutSceneApplier=Applier;
 auto* Seq=NewObject<ULevelSequence>(Applier);Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,24000);
 FCutSceneDefinition& Def=Applier->CutScenes.FindOrAdd(TEXT("GladiusRun"));
 Def.Sequence=Seq;Def.CameraClass=nullptr;Def.WorldTimeScale=1.f;
 Def.SubjectBindingTag=Def.BossBindingTag=NAME_None;
 B->CollectSeconds=0.01f;B->MoveSeconds=10.f;
 FBattleUnificationTestAccess::Collect(B);
 ADD_LATENT_AUTOMATION_COMMAND(FCollectMoveTimerAssertions(Context,Applier,this));
 return true;
}
#endif
