#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Animation/AnimMontage.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/Battle/CutSceneApplier.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "MovieScene.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Scripts/UI/BattleDataResetWidget.h"
#include "Scripts/UI/SessionLogWidget.h"
#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Scripts/UI/ChatLogSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/InputComponent.h"
#include "Scripts/Battle/GladiusRunCamera.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Scripts/Battle/BattlePlayerNameplate.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include <type_traits>
#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"

struct FBattleUnificationTestAccess
{
 static FBattlePlayer& UIPlayer(ABattleManager* B) { return B->Players.FindOrAdd(TEXT("UIProbe")); }
 static void RefreshUI(ABattleManager* B) { B->RefreshPlayerUI(); }
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
 static bool PressKey(ABattleManager* B, FKey Key)
 {
  if (!B->InputComponent) return false;
  for (const FInputKeyBinding& Binding : B->InputComponent->KeyBindings)
   if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed) { Binding.KeyDelegate.Execute(Key); return true; }
  return false;
 }
 static FBattlePlayer& LifePlayer(ABattleManager* B, const FString& Id) { return B->Players.FindChecked(Id); }
 static void SetPath(ABattleManager* B, const FString& Path) { B->TestRecordPath = Path; }
 static void Prepare(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->StartTurn(); }
 static void Select(ABattleManager* B) { B->BeginInput(); }
 static void Collect(ABattleManager* B) { B->BeginCollect(); }
 static void Move(ABattleManager* B) { B->GetWorldTimerManager().ClearTimer(B->PhaseTimerHandle); B->BeginMove(); }
 static float PhaseRemaining(ABattleManager* B) { return B->GetWorldTimerManager().GetTimerRemaining(B->PhaseTimerHandle); }
 static void Resolve(ABattleManager* B) { B->ResolveTurn(); }
 static void Land(ABattleManager* B, EBattleLane Lane) { B->LandBossStrike(Lane); }
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
 //채팅 한 줄을 실제 디스패처로 그대로 보냄 (로그인 확인 포함)
 bool DispatchRaw(const FString& User, const FString& Text)
 {
  FParsedCommand C;
  if (!FChatCommandParser::TryParse(User, Text, C)) return false;
  C.Username = TEXT("Visible name");
  Chat->GetCommandDispatcher()->Execute(C);
  return true;
 }
 //전투 명령 전에 아직 로그인하지 않았으면 실제 시청자처럼 '!로그인 글라디우스'를 먼저 보냄
 bool Dispatch(const FString& User, const FString& Text)
 {
  FParsedCommand C;
  EBattleAction Action;
  if (FChatCommandParser::TryParse(User, Text, C) && FChatCommandParser::TryGetBattleAction(C.CommandType, Action)
   && !Battle->IsPlayerLoggedIn(User))
  {
   DispatchRaw(User, TEXT("!로그인 글라디우스"));
  }
  return DispatchRaw(User, Text);
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleNameplateDamageDataTest, "Gladius.BattleUnified.NameplateDamageData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBattleNameplateDamageDataTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B=T.Battle;
 auto* Unit=T.World->SpawnActor<ABattleUnit>();
 B->Phase=EBattlePhase::Prepare;
 B->FeverAttackDamage=5; B->FeverDamageCoefficient=999.f; B->FeverCap=9;
 B->Classes[0].Attacks[0].BaseDamage=10; B->Classes[0].Attacks[1].BaseDamage=7;
 B->Classes[0].DefenseDamageRatio=0.5f;
 auto& P=FBattleUnificationTestAccess::UIPlayer(B);
 P.Unit=Unit; P.ClassIndex=0; P.Fever=3; P.DamageBonus=6;
 for (EBattleAction Action : {EBattleAction::Attack1, EBattleAction::Attack2})
 {
  P.PendingAction=Action;
  FBattleUnificationTestAccess::RefreshUI(B);
  auto* UI=Cast<UBattlePlayerNameplate>(Unit->Nameplate->GetUserWidgetObject());
  if (!TestNotNull(TEXT("Player gets the actual nameplate widget"),UI)) return false;
  TestEqual(TEXT("Temple enhancement is included once"),UI->Data.BonusDamage,6);
  TestEqual(TEXT("Fever hit damage is the gameplay value"),UI->Data.FeverAttackDamage,5);
  TestEqual(TEXT("UI uses the actual fever hit count"),UI->Data.Fever,3);
  TestEqual(TEXT("Total follows skill + enhancement + fever hits"),UI->Data.BaseDamage+UI->Data.BonusDamage+UI->Data.Fever*UI->Data.FeverAttackDamage,
   Action==EBattleAction::Attack1 ? 31 : 28);
  Unit->Nameplate->SetPivot(FVector2D(0.5f,0.5f));
  Unit->SetPlayerUI(UI->Data);
  TestEqual(TEXT("Same widget class still restores bottom-center pivot"),Unit->Nameplate->GetPivot(),FVector2D(0.5f,1.f));
 }
 P.Fever=30;
 FBattleUnificationTestAccess::RefreshUI(B);
 auto* UI=CastChecked<UBattlePlayerNameplate>(Unit->Nameplate->GetUserWidgetObject());
 TestEqual(TEXT("Display count respects the same cap as gameplay"),UI->Data.Fever,9);
 P.PendingAction=EBattleAction::Defense;
 FBattleUnificationTestAccess::RefreshUI(B);
 TestEqual(TEXT("Defense displays the gameplay reduction"),UI->Data.DefenseReduction,0.5f);
 Unit->Destroy();
 return true;
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
  if (State->World->GetSubsystem<ULevelTransitionCinematics>()->IsPlaying())
  {
   Test->TestTrue(TEXT("Outro plays in BattleEnd"),State->Battle->Phase==EBattlePhase::BattleEnd);
   Test->TestTrue(TEXT("Travel waits for battle outro"),GEngine->GetWorldContextFromWorldChecked(State->World).TravelURL.IsEmpty());
   for (TActorIterator<ALevelSequenceActor> It(State->World);It;++It)
    if (ULevelSequencePlayer* P=It->GetSequencePlayer()) for (int32 I=0;I<3;++I) P->Update(10.f);
   return false;
  }
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
 T.DispatchRaw(TEXT("A"), TEXT("!로그인 글라디우스"));
 TestEqual(TEXT("Login accepted before first turn"), B->GetBattlePlayers().Num(), 1);
 T.DispatchRaw(TEXT("A"), TEXT("!로그아웃"));
 TestEqual(TEXT("Logout accepted in login window"), B->GetBattlePlayers().Num(), 0);
 FBattleUnificationTestAccess::Prepare(B);
 TestTrue(TEXT("Telegraph belongs to Prepare"), B->Phase == EBattlePhase::Prepare);
 T.DispatchRaw(TEXT("A"), TEXT("!공격 1"));
 TestEqual(TEXT("Battle command without login is ignored"), B->GetBattlePlayers().Num(), 0);
 T.DispatchRaw(TEXT("A"), TEXT("!로그인 글라디우스"));
 TestEqual(TEXT("Login joins through BattleManager"), B->GetBattlePlayers().Num(), 1);
 T.DispatchRaw(TEXT("A"), TEXT("!공격 1"));
 const FBattlePlayer* P = B->GetBattlePlayers().Find(TEXT("A"));
 if (!TestNotNull(TEXT("Player exists"), P)) return false;
 TestEqual(TEXT("Display name preserved"), P->DisplayName, FString(TEXT("Visible name")));
 TestTrue(TEXT("Attack one routed"), P->PendingAction == EBattleAction::Attack1);
 T.DispatchRaw(TEXT("B"), TEXT("!로그인 세라"));
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
 T.DispatchRaw(TEXT("A"), TEXT("!로그인 글라디우스"));
 TestEqual(TEXT("Stopped battle rejects participation"), B->GetBattlePlayers().Num(), 0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleLoginLogoutTest, "Gladius.BattleUnified.LoginLogout",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBattleLoginLogoutTest::RunTest(const FString& Parameters)
{
 FParsedCommand Parsed;
 TestTrue(TEXT("Login command parses"), FChatCommandParser::TryParse(TEXT("u"), TEXT("!로그인 세라"), Parsed));
 TestTrue(TEXT("Login command type"), Parsed.CommandType == ECommandType::Login);
 TestEqual(TEXT("Login carries character name"), Parsed.Argument, FString(TEXT("세라")));
 TestFalse(TEXT("Login needs a character"), FChatCommandParser::TryParse(TEXT("u"), TEXT("!로그인"), Parsed));
 TestTrue(TEXT("Logout still parses"), FChatCommandParser::TryParse(TEXT("u"), TEXT("!로그아웃"), Parsed) && Parsed.CommandType == ECommandType::Logout);

 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 TestEqual(TEXT("Korean name resolves to class id"), B->ResolveCharacterId(TEXT("글라디우스")), FString(TEXT("gladius")));

 T.DispatchRaw(TEXT("u"), TEXT("!로그인 기사"));
 TestFalse(TEXT("Unknown character cannot log in"), B->IsPlayerLoggedIn(TEXT("u")));

 T.DispatchRaw(TEXT("u"), TEXT("!로그인 세라"));
 TestTrue(TEXT("Login joins the battle"), B->IsPlayerLoggedIn(TEXT("u")));
 const FPlayerRecord* Record = FBattleUnificationTestAccess::Record(B, TEXT("u"));
 TestTrue(TEXT("Chosen character is recorded"), Record && Record->CharacterId == TEXT("sera"));

 T.DispatchRaw(TEXT("u"), TEXT("!로그인 글라디우스"));
 Record = FBattleUnificationTestAccess::Record(B, TEXT("u"));
 TestTrue(TEXT("Logged-in user keeps character until logout"), Record && Record->CharacterId == TEXT("sera"));

 T.DispatchRaw(TEXT("u"), TEXT("!회피 좌"));
 TestTrue(TEXT("Logged-in command reaches the player"), B->GetBattlePlayers().FindChecked(TEXT("u")).PendingAction == EBattleAction::AvoidL);

 T.DispatchRaw(TEXT("u"), TEXT("!로그아웃"));
 TestFalse(TEXT("Logout leaves the battle"), B->IsPlayerLoggedIn(TEXT("u")));

 T.DispatchRaw(TEXT("u"), TEXT("!로그인 글라디우스"));
 Record = FBattleUnificationTestAccess::Record(B, TEXT("u"));
 TestTrue(TEXT("Re-login switches character"), B->IsPlayerLoggedIn(TEXT("u")) && Record && Record->CharacterId == TEXT("gladius"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadLoginStateTest,"Gladius.LifeState.DeadLoginAndNaturalRevival",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeadLoginStateTest::RunTest(const FString& Parameters)
{
 for (const FString ClassId : {FString(TEXT("gladius")), FString(TEXT("sera"))})
 {
  FUnifiedBattleWorld T; auto* B=T.Battle;
  B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
  TestTrue(TEXT("Initial login"),B->LoginPlayer(TEXT("dead-rejoin"),TEXT("Viewer"),ClassId));
  B->SetPlayerHealth(TEXT("dead-rejoin"),0);
  B->LogoutPlayer(TEXT("dead-rejoin"));
  TestTrue(TEXT("Zero-health user registers awaiting revival"),B->LoginPlayer(TEXT("dead-rejoin"),TEXT("Viewer"),ClassId));
  auto& P=FBattleUnificationTestAccess::LifePlayer(B,TEXT("dead-rejoin"));
  TestTrue(TEXT("Dead rejoin remains registered"),B->IsPlayerLoggedIn(TEXT("dead-rejoin")));
  TestFalse(TEXT("Dead rejoin cannot act"),P.IsAlive());
  TestTrue(TEXT("Dead actor flag initialized"),P.Unit->IsDead());
  TestTrue(TEXT("Dead actor cannot appear as a live summon"),P.Unit->IsHidden());
  auto* Plate=Cast<UBattlePlayerNameplate>(P.Unit->Nameplate->GetUserWidgetObject());
  if(TestNotNull(TEXT("Dead rejoin initializes full character UI"),Plate))
  {
   TestEqual(TEXT("UI knows the selected class"),Plate->Data.CharacterId,ClassId);
   TestEqual(TEXT("UI displays actual health"),Plate->Data.HP,0);
  }
  const int32 Wait=P.ReviveCountdown;
  TestFalse(TEXT("Duplicate login is not a resurrection command"),B->LoginPlayer(TEXT("dead-rejoin"),TEXT("Viewer"),ClassId));
  TestEqual(TEXT("Duplicate login does not restart the wait"),P.ReviveCountdown,Wait);
  TestFalse(TEXT("Dead attack rejected"),B->SubmitAction(TEXT("dead-rejoin"),TEXT("Viewer"),EBattleAction::Attack1));
  for(int32 I=0;I<Wait;++I) FBattleUnificationTestAccess::Resolve(B);
  TestTrue(TEXT("Natural revival restores gameplay eligibility"),P.IsAlive());
  TestEqual(TEXT("Natural revival restores health"),P.HP,P.MaxHP);
  TestFalse(TEXT("Natural revival clears actor death flag"),P.Unit->IsDead());
  TestFalse(TEXT("Natural revival reveals actor"),P.Unit->IsHidden());
  TestFalse(TEXT("Old death presentation cannot fire again"),P.bDiedThisTurn);
  FBattleUnificationTestAccess::Prepare(B);
  TestTrue(TEXT("Attack is accepted after revival"),B->SubmitAction(TEXT("dead-rejoin"),TEXT("Viewer"),EBattleAction::Attack1));
  TMap<FString,FBattlePlayer> Candidate; Candidate.Add(TEXT("dead-rejoin"),P);
  FRandomStream Random(19);
  TestEqual(TEXT("Revived character can be chosen for a cutscene"),ABattleManager::SelectTurnCutSceneUser(Candidate,B->GetCharacterClasses(),FString(),{},Random),FString(TEXT("dead-rejoin")));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevivalStateRepairTest,"Gladius.LifeState.RepairsOrphanedAndStaleDeathState",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRevivalStateRepairTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* B=T.Battle;
 B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
 B->LoginPlayer(TEXT("repair"),TEXT("Viewer"),TEXT("gladius"));
 auto& P=FBattleUnificationTestAccess::LifePlayer(B,TEXT("repair"));
 P.HP=0; P.ReviveCountdown=0;
 TestFalse(TEXT("HP zero is never alive even without a countdown"),P.IsAlive());
 TestFalse(TEXT("Orphaned zero-health input rejected"),B->SubmitAction(TEXT("repair"),TEXT("Viewer"),EBattleAction::Attack1));
 for(int32 I=0;I<FMath::Max(1,B->ReviveTurns);++I) FBattleUnificationTestAccess::Resolve(B);
 TestTrue(TEXT("Orphaned zero-health player eventually revives"),P.IsAlive());
 // Reproduce equal-HP early return with stale death flags and pending lethal presentation.
 P.Unit->NotifyDied(); P.bDiedThisTurn=true; P.HPLossToShow=3; P.DamageTakenThisTurn=3;
 TestTrue(TEXT("Equal-health setter repairs stale actor state"),B->SetPlayerHealth(TEXT("repair"),P.HP));
 TestFalse(TEXT("Equal-health repair clears actor death"),P.Unit->IsDead());
 TestFalse(TEXT("Pending lethal event cleared"),P.bDiedThisTurn);
 TestEqual(TEXT("Pending lethal damage cleared"),P.DamageTakenThisTurn,0);
 TestEqual(TEXT("Pending HP presentation cleared"),P.HPLossToShow,0);
 B->SetPlayerHealth(TEXT("repair"),0);
 P.ReviveCountdown=1;
 B->SetPlayerHealth(TEXT("repair"),0);
 TestEqual(TEXT("Repeated zero update does not postpone revival"),P.ReviveCountdown,1);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevivalMotionResetTest,"Gladius.LifeState.RevivalClearsOldMotionAndDelayedHits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRevivalMotionResetTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; T.Battle->StopBattle();
 auto* Unit=T.World->SpawnActor<ABattleUnit>();
 Unit->NotifyActionSelected(EBattleAction::Attack2); Unit->SetDefending(true);
 Unit->MoveTo(Unit->GetActorLocation()+FVector(100,0,0),3.f,EMoveGait::Run);
 Unit->NotifyDied();
 int32 Hits=0;
 const auto Handle=Unit->OnHitLanded.AddLambda([&Hits](){++Hits;});
 Unit->ReportHit(.1f);
 Unit->HoldMotion(10.f);
 Unit->MoveTo(Unit->GetActorLocation()+FVector(200,0,0),3.f,EMoveGait::Run);
 Unit->Mesh->bPauseAnims=true; Unit->Mesh->GlobalAnimRateScale=0.f;
 Unit->NotifyRevived();
 TestFalse(TEXT("Revival clears death flag"),Unit->IsDead());
 TestFalse(TEXT("Revival clears old movement"),Unit->IsMoving());
 TestFalse(TEXT("Revival clears defense"),Unit->IsDefending());
 TestEqual(TEXT("Revival clears old selection"),Unit->GetSelectedAction(),EBattleAction::Idle);
 TestFalse(TEXT("Revival resumes animation updates"),bool(Unit->Mesh->bPauseAnims));
 TestTrue(TEXT("Revival resumes animation playback"),Unit->Mesh->GlobalAnimRateScale>0.f);
 T.World->GetTimerManager().Tick(.2f);
 TestEqual(TEXT("Previous life's delayed hit cannot fire"),Hits,0);
 Unit->MoveTo(Unit->GetActorLocation()+FVector(50,0,0),1.f,EMoveGait::Walk);
 TestTrue(TEXT("Death motion hold cannot block new movement"),Unit->IsMoving());
 Unit->OnHitLanded.Remove(Handle); Unit->Destroy();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterDeathAnimationResetTest,"Gladius.LifeState.CharacterDeathAnimationResets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterDeathAnimationResetTest::RunTest(const FString& Parameters)
{
 for(const TCHAR* Path : {TEXT("/Game/Blueprints/BP_BattleUnitG.BP_BattleUnitG_C"),TEXT("/Game/Blueprints/BP_BattleUnitS.BP_BattleUnitS_C")})
 {
  FUnifiedBattleWorld T; T.Battle->StopBattle();
  UClass* Class=LoadClass<ABattleUnit>(nullptr,Path);
  if(!TestNotNull(TEXT("Actual character blueprint"),Class)) return false;
  auto* Unit=T.World->SpawnActor<ABattleUnit>(Class);
  if(!Unit->HasActorBegunPlay()) Unit->DispatchBeginPlay();
  Unit->Mesh->InitAnim(true);
  UClass* LivingClass=Unit->Mesh->GetAnimClass();
  Unit->OnHit(1,true); Unit->NotifyDied();
  UAnimMontage* DeathMontage=Unit->Mesh->GetAnimInstance() ? Unit->Mesh->GetAnimInstance()->GetCurrentActiveMontage() : nullptr;
  Unit->NotifyRevived();
  TestFalse(TEXT("Character exits dead state"),Unit->IsDead());
  TestEqual(TEXT("Living animation class restored"),Unit->Mesh->GetAnimClass(),LivingClass);
  auto* Anim=Unit->Mesh->GetAnimInstance();
  if(TestNotNull(TEXT("Living animation instance restored"),Anim) && DeathMontage)
   TestFalse(TEXT("Lethal-hit montage does not remain active"),Anim->Montage_IsPlaying(DeathMontage));
  Unit->Destroy();
 }
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
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("yt-login"),TEXT("yt-viewer"),TEXT("YouTube name"),TEXT("!로그인 세라"));
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("yt-first"),TEXT("yt-viewer"),TEXT("YouTube name"),TEXT("!공격 1"));
 TestEqual(TEXT("Received YouTube login joins exactly one player"),B->GetBattlePlayers().Num(),1);
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
  const double OriginalAnchorZ=Subject->Nameplate->GetComponentLocation().Z;
  if(!Subject->HasActorBegunPlay()) Subject->DispatchBeginPlay();
  auto* Mesh=Subject->FindComponentByClass<USkeletalMeshComponent>();
  const double HeadZ=Mesh->GetSocketLocation(TEXT("def-head")).Z;
  TestTrue(TEXT("Nameplate clearance is one sixth of authored head gap"),FMath::IsNearlyEqual(Subject->Nameplate->GetComponentLocation().Z,HeadZ+(OriginalAnchorZ-HeadZ)/6.,.1));
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
   bMontagePlayingAtSignal=Index==Skill&&Anim->GetCurrentActiveMontage()==(Skill==1?Roll:Attack);
  });
  Anim->Montage_Play(Roll);
  Subject->MoveTo(Subject->GetActorLocation()+FVector(100,0,0),1.f,EMoveGait::Run);
  const double ArrivalTime=T.World->GetTimeSeconds();
  static_cast<AActor*>(Subject)->Tick(1.f);
  TestEqual(TEXT("One skill starts immediately on arrival"),StartCount,1);
  TestTrue(TEXT("Opening motion is playing when the camera start signal is emitted"),bMontagePlayingAtSignal);
  TestEqual(TEXT("No timer or frame wait between attack and camera"),T.World->GetTimeSeconds(),ArrivalTime);
  TestEqual(TEXT("Skill camera starts in the attack call"),FBattleUnificationTestAccess::ShotIndex(B),2);
  TestTrue(TEXT("Skill camera is playing"),Applier->IsPlaying());
  TestTrue(TEXT("Skill one camera opens on Roll, skill two opens on Attack2"),Anim->GetCurrentActiveMontage()==(Skill==1?Roll:Attack));
  if (Skill==1)
  {
   ++GFrameCounter;
   T.World->GetTimerManager().Tick(0.f);
   ++GFrameCounter;
   T.World->GetTimerManager().Tick(Roll->GetPlayLength()+.01f);
   TestTrue(TEXT("Attack1 follows the opening roll"),Anim->GetCurrentActiveMontage()==Attack);
   TestEqual(TEXT("Roll and Attack1 each report their start"),StartCount,2);
  }
  FBattleUnificationTestAccess::Resolve(B);
  FBattleUnificationTestAccess::FlushOneAttack(B);
  TestEqual(TEXT("Attack queue does not replay the skill"),StartCount,Skill==1?2:1);
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
  Anim->Montage_Play(Roll);
  TestTrue(TEXT("Reproduction begins with an active Roll montage"),Anim->Montage_IsPlaying(Roll));
  TestTrue(TEXT("Skill command is accepted"),B->SubmitAction(TEXT("@testuser00"),TEXT("Keyboard user"),Skill==1?EBattleAction::Attack1:EBattleAction::Attack2));
  TestTrue(TEXT("Command selects the requested skill"),Subject->GetSelectedAction()==(Skill==1?EBattleAction::Attack1:EBattleAction::Attack2));
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

//피해는 ResolveTurn에서 정해지지만 화면(보스 누적·순위판·이름표 체력)은 타격이 닿는 순간에 바뀐다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleHitDisplayTimingTest,"Gladius.BattleUnified.HitDisplayFollowsImpact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleHitDisplayTimingTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* B=T.Battle;
 const FString Id=TEXT("@hit");
 FBossAction Strike; Strike.Id=TEXT("Test"); Strike.HitCount=1;
 for(int32& Damage : Strike.DamageByZone) Damage=2;
 auto Shown=[&](){ const FBattleSessionEntry* E=B->GetSessionStats().GetEntries().Find(Id); return E?E->DamageDealt:-1; };

 B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
 TestTrue(TEXT("Attacker joins"),B->SubmitAction(Id,TEXT("Hit"),EBattleAction::Attack1));
 FBattleUnificationTestAccess::Fever(B,Id,5);
 FBattleUnificationTestAccess::SetBossAction(B,Strike);
 const FBattlePlayer& P=B->GetBattlePlayers().FindChecked(Id);
 const int32 HPBefore=P.HP;
 FBattleUnificationTestAccess::Resolve(B);

 int32 Queued=0; for(const FPendingHit& Hit : P.HitsToShow) Queued+=Hit.Damage;
 TestTrue(TEXT("Skill and fever hits are queued separately"),P.HitsToShow.Num()>1);
 TestEqual(TEXT("Queued hits add up to the resolved damage"),Queued,B->BossDamageTaken);
 TestEqual(TEXT("Nothing shows before the hit lands"),B->BossDamageShown,0);
 TestEqual(TEXT("Leaderboard waits too"),Shown(),(int64)0);
 TestTrue(TEXT("Player HP is already cut but held for display"),P.HPLossToShow>0&&P.HP+P.HPLossToShow==HPBefore);

 const int32 First=P.HitsToShow[0].Damage;
 P.Unit->ReportHit();
 TestEqual(TEXT("One report shows one hit"),B->BossDamageShown,First);
 TestEqual(TEXT("Leaderboard follows the shown hit"),Shown(),(int64)First);

 FBattleUnificationTestAccess::Land(B,P.Lane);
 TestEqual(TEXT("Blade landing releases the held HP"),P.HPLossToShow,0);

 FBattleUnificationTestAccess::EndTurn(B);
 TestEqual(TEXT("Unreported hits are settled at turn end"),B->BossDamageShown,B->BossDamageTaken);
 TestEqual(TEXT("Leaderboard is settled too"),Shown(),(int64)B->BossDamageTaken);

 //세라 컷신처럼 피해가 정해지기 전에 맞으면, 정해지는 즉시 띄운다
 const int32 Before=B->BossDamageTaken;
 FBattleUnificationTestAccess::Prepare(B);
 B->SubmitAction(Id,TEXT("Hit"),EBattleAction::Attack1);
 P.Unit->ReportHit();
 TestEqual(TEXT("Early report waits for the damage"),B->BossDamageShown,Before);
 FBattleUnificationTestAccess::Resolve(B);
 TestTrue(TEXT("Early report shows as soon as damage is resolved"),B->BossDamageShown>Before);
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionNotificationTest, "Gladius.SessionLog.CommandsAndIsolation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSessionNotificationTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* B = T.Battle;
 auto* Log = T.Instance->GetSubsystem<USessionLogSubsystem>();
 auto* ChatLog = T.Instance->GetSubsystem<UChatLogSubsystem>();
 const int32 ChatCount = ChatLog->GetEntries().Num();
 B->StartBattle();
 FBattleUnificationTestAccess::Prepare(B);
 T.DispatchRaw(TEXT("u"), TEXT("!로그인 세라"));
 TestEqual(TEXT("Login succeeds after unit creation"), Log->GetEntries().Last().Message, FString(TEXT("Visible name 님이 로그인하셨습니다")));
 TestTrue(TEXT("Logged-in unit exists"), IsValid(B->GetBattlePlayers().FindChecked(TEXT("u")).Unit));
 T.DispatchRaw(TEXT("u"), TEXT("!로그아웃"));
 TestEqual(TEXT("Explicit logout"), Log->GetEntries().Last().Message, FString(TEXT("Visible name 님이 로그아웃하셨습니다")));
 const int32 LoggedOutCount = Log->GetEntries().Num();
 T.DispatchRaw(TEXT("u"), TEXT("!로그아웃"));
 TestEqual(TEXT("No false logout for absent user"), Log->GetEntries().Num(), LoggedOutCount);
 T.DispatchRaw(TEXT("u"), TEXT("!로그인 없는캐릭터"));
 TestEqual(TEXT("Failure reports no successful login"), Log->GetEntries().Last().Message, FString(TEXT("Visible name님의 로그인이 정상 처리되지 못했습니다")));
 B->MaxPlayers = 0;
 T.DispatchRaw(TEXT("u"), TEXT("!로그인 세라"));
 TestEqual(TEXT("Capacity rejection reports failure"), Log->GetEntries().Last().Event, ESessionLogEvent::LoginFailed);
 B->MaxPlayers = 100;
 T.DispatchRaw(TEXT("u"), TEXT("!로그인 세라"));
 B->IdleTurnsBeforeKick = 1;
 FBattleUnificationTestAccess::Prepare(B);
 FBattleUnificationTestAccess::Resolve(B);
 TestFalse(TEXT("Inactive participant removed"), B->IsPlayerLoggedIn(TEXT("u")));
 TestEqual(TEXT("Inactivity has its own message"), Log->GetEntries().Last().Message, FString(TEXT("Visible name 님의 입력이 없어 자동 로그아웃되었습니다")));
 TestEqual(TEXT("Notifications never enter chat history"), ChatLog->GetEntries().Num(), ChatCount);
 Log->MaxEntries = 2;
 Log->AddEvent(TEXT("fallback"), TEXT(""), ESessionLogEvent::Login);
 TestEqual(TEXT("Bounded history"), Log->GetEntries().Num(), 2);
 TestEqual(TEXT("Missing nickname falls back to user id"), Log->GetEntries().Last().Message, FString(TEXT("fallback 님이 로그인하셨습니다")));
 return true;
}

struct FSessionPresentationTestAccess
{
 static void Time(USessionLogSubsystem* Log,double Now) { Log->PresentationTimeOverride=Now; }
 static int32 Pending(USessionLogSubsystem* Log) { return Log->PendingMessages.Num(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionLogWidgetTest, "Gladius.SessionLog.WidgetStyleAndHistory",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSessionLogWidgetTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* Log = T.Instance->GetSubsystem<USessionLogSubsystem>();
 FSessionPresentationTestAccess::Time(Log,0.0);
 Log->AddMessage(TEXT("이전 알림"), ESessionLogEvent::Admin);
 FSessionPresentationTestAccess::Time(Log,20.0);
 Log->AddMessage(TEXT("최근 알림"), ESessionLogEvent::AdminUrgent);
 auto* W = CreateWidget<USessionLogWidget>(T.Instance, USessionLogWidget::StaticClass());
 const auto SlateWidget = W->TakeWidget();
 auto* Rows = Cast<UVerticalBox>(W->WidgetTree->FindWidget(TEXT("LogMessages")));
 if (!TestNotNull(TEXT("Message layer exists"), Rows) || !TestNotNull(TEXT("Single text exists"), W->CurrentText.Get())) return false;
 TestEqual(TEXT("Exactly one line widget"), Rows->GetChildrenCount(), 1);
 TestEqual(TEXT("Map entry resumes the current presentation"), W->CurrentText->GetText().ToString(), FString(TEXT("최근 알림")));
 TestFalse(TEXT("No wrapping"), W->CurrentText->GetAutoWrapText());
 TestEqual(TEXT("Urgent color"), W->CurrentText->GetColorAndOpacity().GetSpecifiedColor(), FLinearColor::Red);
 TestNotNull(TEXT("Original notice texture resolved"), W->BackgroundBrush.GetResourceObject());
 auto* Layout = Cast<UCanvasPanelSlot>(W->LogWindow->Slot);
 TestEqual(TEXT("Upper-right anchor"), Layout->GetAnchors().Minimum, FVector2D(1, 0));
 TestEqual(TEXT("Upper-right alignment"), Layout->GetAlignment(), FVector2D(1, 0));
 TestTrue(TEXT("Inset from upper right"), W->WindowOffset.X < 0 && W->WindowOffset.Y > 0);
 TestTrue(TEXT("MaruBuri font resolved"), W->LogFont.FontObject && W->LogFont.FontObject->GetName().Contains(TEXT("MaruBuri")));
 W->LogFont.Size = 23;
 W->RefreshStyle();
 TestEqual(TEXT("Designer font size applied"), W->CurrentText->GetFont().Size, 23.f);
 Log->AddNotice(TEXT("다음 공지\n두 번째 줄"));
 Log->AddMessage(TEXT("이벤트 알림"), ESessionLogEvent::Event);
 TestEqual(TEXT("Incoming messages wait until current finishes"), W->CurrentText->GetText().ToString(), FString(TEXT("최근 알림")));
 TestEqual(TEXT("Both incoming messages queued"), FSessionPresentationTestAccess::Pending(Log), 2);
 FSessionPresentationTestAccess::Time(Log,39.99);
 W->AdvanceTicker(0.f);
 TestEqual(TEXT("Ordinary message lasts the full twenty seconds"),W->CurrentText->GetText().ToString(),FString(TEXT("최근 알림")));
 W->RebuildEntries();
 FSessionPresentationTestAccess::Time(Log,40.0);
 W->AdvanceTicker(0.f);
 TestEqual(TEXT("FIFO and newline normalization"), W->CurrentText->GetText().ToString(), FString(TEXT("다음 공지 두 번째 줄")));
 TestEqual(TEXT("Admin color preserved"), W->CurrentText->GetColorAndOpacity().GetSpecifiedColor(), FLinearColor::Yellow);
 FSessionPresentationTestAccess::Time(Log,60.0);
 W->AdvanceTicker(0.f);
 TestEqual(TEXT("Next event displayed"), W->CurrentText->GetText().ToString(), FString(TEXT("이벤트 알림")));
 W->EventColor = FLinearColor::Blue;
 W->RefreshStyle();
 TestEqual(TEXT("Restyling keeps current event"), W->CurrentText->GetColorAndOpacity().GetSpecifiedColor(), FLinearColor::Blue);
 TestEqual(TEXT("No stacked history after arrivals"), Rows->GetChildrenCount(), 1);
 FString LongMessage;
 for (int32 I=0; I<40; ++I) LongMessage += TEXT("긴 운영진 공지사항 ");
 FSessionPresentationTestAccess::Time(Log,80.0);
 W->AdvanceTicker(0.f);
 TestEqual(TEXT("Idle ticker returns to the most recently shown notice"),W->CurrentText->GetText().ToString(),FString(TEXT("다음 공지 두 번째 줄")));
 Log->AddNotice(LongMessage);
 W->AdvanceTicker(0.5f);
 TestEqual(TEXT("Readable pause at beginning"), W->LogScrollBox->GetScrollOffset(), 0.f);
 W->AdvanceTicker(1.5f);
 AddInfo(FString::Printf(TEXT("Ticker width=%f offset=%f elapsed=%f"), W->CurrentText->GetDesiredSize().X, W->LogScrollBox->GetScrollOffset(), W->MessageElapsed));
 TestEqual(TEXT("Long line moves left smoothly"), W->LogScrollBox->GetScrollOffset(), 55.f);
 Log->AddMessage(TEXT("긴 공지 다음 알림"), ESessionLogEvent::AdminUrgent);
 W->AdvanceTicker(0.1f);
 TestEqual(TEXT("Long message not interrupted"), W->CurrentText->GetText().ToString(), LongMessage);
 FSessionPresentationTestAccess::Time(Log,100.0);
 W->AdvanceTicker(0.f);
 TestEqual(TEXT("Long messages also have a twenty-second lifetime"), W->CurrentText->GetText().ToString(), FString(TEXT("긴 공지 다음 알림")));
 TestEqual(TEXT("Next line resets position"), W->LogScrollBox->GetScrollOffset(), 0.f);
 TestEqual(TEXT("History retained independently"), Log->GetEntries().Num(), 6);
 if (!FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
 {
  W->WindowSize=FVector2D(384,64);
  W->RefreshStyle();
  W->LogFont.Size = 18;
  Log->OnEntryAdded.RemoveAll(W);
  W->BoundLog.Reset(); // Isolate the manual visual preview from the live presentation clock.
  W->AppendMessage(TEXT("[운영진 공지] 글라디우스에 오신 것을 환영합니다. 오늘 전투는 오후 8시에 시작합니다. 끝까지 즐겁게 참여해 주세요!"), ESessionLogEvent::Admin);
  FWidgetRenderer Renderer(true);
  auto SavePreview = [&](const TCHAR* Name)
  {
   UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateWidget, FVector2D(1280, 720));
   TArray<FColor> Pixels;
   FReadSurfaceDataFlags ReadFlags;
   ReadFlags.SetLinearToGamma(false);
   if (!TestTrue(TEXT("Preview render readback"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) return;
   TArray64<uint8> Png;
   FImageUtils::PNGCompressImageArray(1280, 720, Pixels, Png);
   FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / Name));
  };
  SavePreview(TEXT("NoticeTicker-start.png"));
  AddInfo(FString::Printf(TEXT("Notice text height=%f scroll viewport height=%f"),W->CurrentText->GetDesiredSize().Y,W->LogScrollBox->GetCachedGeometry().GetLocalSize().Y));
  TestTrue(TEXT("Expanded notice viewport fits complete glyph height"),W->LogScrollBox->GetCachedGeometry().GetLocalSize().Y>=W->CurrentText->GetDesiredSize().Y);
  W->AdvanceTicker(5.f);
  SavePreview(TEXT("NoticeTicker-scroll.png"));
 }
 return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNotificationLifetimeTest,"Gladius.SessionLog.TwentySecondsAndLatestNotice",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNotificationLifetimeTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* Log=NewObject<USessionLogSubsystem>(T.Instance);
 FSessionLogEntry Entry;
 FSessionPresentationTestAccess::Time(Log,0.0);
 TestFalse(TEXT("No invented message before any notification"),Log->GetPresentedEntry(Entry));
 Log->MaxEntries=1;
 Log->AddNotice(TEXT("기본 공지"));
 FSessionPresentationTestAccess::Time(Log,1.0);
 Log->AddMessage(TEXT("일반 알림"),ESessionLogEvent::Login);
 FSessionPresentationTestAccess::Time(Log,19.99); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Notice not interrupted by arrival"),Entry.Message,FString(TEXT("기본 공지")));
 FSessionPresentationTestAccess::Time(Log,20.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Next message starts at twenty seconds"),Entry.Message,FString(TEXT("일반 알림")));
 FSessionPresentationTestAccess::Time(Log,39.99); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Queued message gets twenty seconds after it is shown"),Entry.Message,FString(TEXT("일반 알림")));
 FSessionPresentationTestAccess::Time(Log,40.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Latest notice is idle fallback"),Entry.Message,FString(TEXT("기본 공지")));
 TestEqual(TEXT("Fallback survives history eviction"),Log->GetEntries()[0].Message,FString(TEXT("일반 알림")));
 FSessionPresentationTestAccess::Time(Log,5000.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Fallback never expires while queue is empty"),Entry.Message,FString(TEXT("기본 공지")));
 Log->AddNotice(TEXT("새 공지")); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("New notice immediately replaces idle fallback"),Entry.Message,FString(TEXT("새 공지")));
 FSessionPresentationTestAccess::Time(Log,5020.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Newly displayed notice becomes the fallback"),Entry.Message,FString(TEXT("새 공지")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistentMaintenanceNoticeTest,"Gladius.SessionLog.MaintenanceUntilAllScopesEnd",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPersistentMaintenanceNoticeTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* Log=NewObject<USessionLogSubsystem>(T.Instance);
 FSessionLogEntry Entry;
 FSessionPresentationTestAccess::Time(Log,0.0); Log->AddNotice(TEXT("기본 공지"));
 FSessionPresentationTestAccess::Time(Log,5.0);
 Log->AddMaintenanceMessage(TEXT("전투"),true,TEXT("전투 점검 중"),ESessionLogEvent::AdminUrgent);
 FSessionPresentationTestAccess::Time(Log,200.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Maintenance does not time out"),Entry.Message,FString(TEXT("전투 점검 중")));
 Log->AddMessage(TEXT("대기 알림"),ESessionLogEvent::Login);
 Log->AddMaintenanceMessage(TEXT("AI"),true,TEXT("AI 점검 중"),ESessionLogEvent::Admin);
 Log->GetPresentedEntry(Entry);
 TestTrue(TEXT("Simultaneous maintenance scopes are visible"),Entry.Message.Contains(TEXT("전투"))&&Entry.Message.Contains(TEXT("AI")));
 FSessionPresentationTestAccess::Time(Log,400.0);
 Log->AddMaintenanceMessage(TEXT("AI"),false,TEXT("AI 점검 완료"),ESessionLogEvent::Admin);
 Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Ending one scope preserves the remaining maintenance"),Entry.Message,FString(TEXT("전투 점검 중")));
 FSessionPresentationTestAccess::Time(Log,500.0);
 Log->AddMaintenanceMessage(TEXT("전투"),false,TEXT("전투 점검 완료"),ESessionLogEvent::Admin);
 Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Interrupted notice resumes after maintenance"),Entry.Message,FString(TEXT("기본 공지")));
 FSessionPresentationTestAccess::Time(Log,514.99); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Paused remaining fifteen seconds are preserved"),Entry.Message,FString(TEXT("기본 공지")));
 FSessionPresentationTestAccess::Time(Log,515.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Ordinary messages were queued, not expired during maintenance"),Entry.Message,FString(TEXT("대기 알림")));
 FSessionPresentationTestAccess::Time(Log,535.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Completion messages have normal lifetimes"),Entry.Message,FString(TEXT("AI 점검 완료")));
 FSessionPresentationTestAccess::Time(Log,555.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("No stale maintenance-start message is replayed"),Entry.Message,FString(TEXT("전투 점검 완료")));
 FSessionPresentationTestAccess::Time(Log,575.0); Log->GetPresentedEntry(Entry);
 TestEqual(TEXT("Maintenance completion cannot replace latest notice fallback"),Entry.Message,FString(TEXT("기본 공지")));
 return true;
}

struct FAdminTestAccess
{
 static bool Bind(UAdminSubsystem* A, const FString& Handle, const FString& Id) { return A->BindVerifiedHandle(Handle, Id); }
 static FString Path(UAdminSubsystem* A) { return A->StoragePath; }
 static void SetAdminFields(UAdminSubsystem* A, const FString& Id, const FString& Name, const FString& Until)
 { auto& U=A->Data.Users[A->FindIndex(Id)]; U.AdminName=Name; U.BlockTime=Until; }
};
struct FAdminScheduleTestAccess
{
 static void Set(UBattleScheduleSubsystem* S, const TArray<int32>& Times) { S->StartMinutes=Times; S->ManualStarts.Reset(); S->ConsumedSlots.Reset(); }
 static bool Consume(UBattleScheduleSubsystem* S, FDateTime Before, FDateTime Now) { return S->ConsumeDue(Before,Now,S->StartMinutes,S->ConsumedSlots); }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdminCommandsTest,"Gladius.Admin.CommandsPermissionsPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdminCommandsTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* A=T.Instance->GetSubsystem<UAdminSubsystem>();
 auto* L=T.Instance->GetSubsystem<USessionLogSubsystem>();
 const FString GM=TEXT("UC1111111111111111111111");
 A->RegisterUser(TEXT("viewer"),TEXT("Viewer"));
 TestTrue(TEXT("Seed operator handle exists"),A->FindUser(TEXT("@자작곡싸개"))!=nullptr);
 TestFalse(TEXT("Unverified handle is not identity"),A->IsAdmin(TEXT("@자작곡싸개")));
 TestTrue(TEXT("Bind verified API channel"),FAdminTestAccess::Bind(A,TEXT("@자작곡싸개"),GM));
 TestTrue(TEXT("Verified operator authenticated"),A->IsAdmin(GM));
 TestEqual(TEXT("Alias"),A->GetAdminName(GM),FString(TEXT("GM복어")));
 TestEqual(TEXT("Second operator alias"),A->GetAdminName(TEXT("@WorkingHives")),FString(TEXT("GM도리")));
 FAdminTestAccess::SetAdminFields(A,GM,TEXT(""),(FDateTime::UtcNow()+FTimespan::FromDays(1)).ToIso8601());
 TestEqual(TEXT("Empty admin alias falls back"),A->GetAdminName(GM),FString(TEXT("GM")));
 TestFalse(TEXT("Admin ignores blockTime"),A->IsBlocked(GM,FDateTime::UtcNow()));
 FAdminTestAccess::SetAdminFields(A,GM,TEXT("GM복어"),TEXT(""));
 auto Send=[&](const FString& Id,const TCHAR* Cmd) { FParsedCommand C; if (FChatCommandParser::TryParse(Id,Cmd,C)) { C.Username=TEXT("@자작곡싸개"); T.Chat->GetCommandDispatcher()->Execute(C); } };
 const int32 Initial=L->GetEntries().Num();
 Send(TEXT("imposter"),TEXT("!공지 위조"));
 TestEqual(TEXT("Display name cannot impersonate admin"),L->GetEntries().Num(),Initial);
 Send(GM,TEXT("!공지 공백 있는 공지"));
 TestEqual(TEXT("Notice text"),L->GetEntries().Last().Message,FString(TEXT("[GM복어] 공백 있는 공지")));
 TestEqual(TEXT("Notice style"),L->GetEntries().Last().Event,ESessionLogEvent::Admin);
 Send(GM,TEXT("!차단 viewer 0.5"));
 TestTrue(TEXT("Blocked until expiry"),A->IsBlocked(TEXT("viewer"),FDateTime::UtcNow()));
 FDateTime Expiry; TestTrue(TEXT("ISO timestamp saved"),FDateTime::ParseIso8601(*A->FindUser(TEXT("viewer"))->BlockTime,Expiry));
 TestTrue(TEXT("Fractional hours"),FMath::Abs((Expiry-FDateTime::UtcNow()).GetTotalSeconds()-1800)<3);
 TestFalse(TEXT("Expiry allows input"),A->IsBlocked(TEXT("viewer"),Expiry));
 Send(GM,TEXT("!차단해제 viewer"));
 TestTrue(TEXT("Unblock removes timestamp"),A->FindUser(TEXT("viewer"))->BlockTime.IsEmpty());
 TestEqual(TEXT("Unblock restores role"),A->FindUser(TEXT("viewer"))->Permission,FString(TEXT("user")));
 Send(GM,TEXT("!차단 viewer"));
 FDateTime::ParseIso8601(*A->FindUser(TEXT("viewer"))->BlockTime,Expiry);
 TestEqual(TEXT("Default calendar year"),Expiry.GetYear(),FDateTime::UtcNow().GetYear()+1);
 const int32 BeforeProtected=L->GetEntries().Num();
 Send(GM,*FString::Printf(TEXT("!차단 %s"),*GM));
 TestEqual(TEXT("Admin channel-ID block silently ignored"),L->GetEntries().Num(),BeforeProtected);
 Send(GM,TEXT("!차단해제 viewer"));
 Send(GM,TEXT("!점검 전투"));
 TestFalse(TEXT("Battle disabled for user"),A->CanInput(TEXT("viewer"),EGameState::Combat));
 TestTrue(TEXT("Temple remains enabled"),A->CanInput(TEXT("viewer"),EGameState::Temple));
 TestTrue(TEXT("Operator bypasses maintenance"),A->CanInput(GM,EGameState::Combat));
 Send(GM,TEXT("!점검해제 전투"));
 Send(GM,TEXT("!점검 신전"));
 TestFalse(TEXT("Temple disabled"),A->CanInput(TEXT("viewer"),EGameState::Temple));
 Send(GM,TEXT("!점검해제 신전"));
 Send(GM,TEXT("!점검 전체"));
 TestFalse(TEXT("All input disabled"),A->CanInput(TEXT("viewer"),EGameState::Combat));
 TestFalse(TEXT("All temple input disabled"),A->CanInput(TEXT("viewer"),EGameState::Temple));
 Send(GM,TEXT("!점검해제 전체"));
 Send(GM,TEXT("!점검 AI")); TestTrue(TEXT("AI disabled"),A->IsAIMaintenance());
 Send(GM,TEXT("!점검해제 AI")); TestFalse(TEXT("AI enabled"),A->IsAIMaintenance());
 auto* B=T.Battle; B->StartBattle(); FBattleUnificationTestAccess::Prepare(B);
 Send(TEXT("viewer"),TEXT("!로그인 세라"));
 const auto Before=B->GetBattlePlayers().FindChecked(TEXT("viewer"));
 Send(GM,TEXT("!이벤트 viewer"));
 const auto After=B->GetBattlePlayers().FindChecked(TEXT("viewer"));
 TestEqual(TEXT("Event max health +1"),After.MaxHP,Before.MaxHP+1);
 TestEqual(TEXT("Event attack +5"),After.DamageBonus,Before.DamageBonus+5);
 TestEqual(TEXT("Event green category"),L->GetEntries().Last().Event,ESessionLogEvent::Event);
 const int32 EventCount=L->GetEntries().Num(); Send(GM,TEXT("!이벤트 viewer"));
 TestEqual(TEXT("No repeat notice"),L->GetEntries().Num(),EventCount);
 TestEqual(TEXT("No stacking"),B->GetBattlePlayers().FindChecked(TEXT("viewer")).MaxHP,After.MaxHP);
 auto* Reloaded=NewObject<UAdminSubsystem>(T.Instance);
 TestTrue(TEXT("Reload saved permissions"),Reloaded->LoadFromFile(FAdminTestAccess::Path(A)));
 TestTrue(TEXT("Event survives reload"),Reloaded->HasEventBuff(TEXT("viewer")));
 TestTrue(TEXT("Admin survives reload"),Reloaded->IsAdmin(GM));
 Send(GM,TEXT("!차단 viewer 1"));
 const int32 BlockedCount=L->GetEntries().Num(); Send(TEXT("viewer"),TEXT("!로그아웃"));
 TestTrue(TEXT("Blocked user's command ignored"),B->IsPlayerLoggedIn(TEXT("viewer")));
 TestEqual(TEXT("Ignored command makes no log"),L->GetEntries().Num(),BlockedCount);
 B->StopBattle();
 Send(GM,TEXT("!전투"));
 FDateTime Next; auto* S=T.Instance->GetSubsystem<UBattleScheduleSubsystem>();
 TestTrue(TEXT("Immediate battle queued"),S->GetNextStart(FDateTime::Now(),Next));
 TestTrue(TEXT("Five-second delay"),(Next-FDateTime::Now()).GetTotalSeconds()<=5.1);
 Send(GM,TEXT("!전투 취소"));
 Send(GM,TEXT("!전투 23:59"));
 TestEqual(TEXT("Schedule style"),L->GetEntries().Last().Event,ESessionLogEvent::Admin);
 Send(GM,TEXT("!전투 25:99"));
 TestEqual(TEXT("Invalid time rejected"),L->GetEntries().Last().Event,ESessionLogEvent::AdminUrgent);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPatchExhibitTest,"Gladius.PatchExhibit.RolesRoutingAndRanking",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPatchExhibitTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* A=T.Instance->GetSubsystem<UAdminSubsystem>();
 const FString UserPath = FAdminTestAccess::Path(A);
 const FString Seed = TEXT(R"({"users":[{"userId":"operator","permission":"admin"},{"userId":"demo","permission":"exhibit"},{"userId":"viewer","permission":"user"}]})");
 FFileHelper::SaveStringToFile(Seed,*UserPath);
 TestTrue(TEXT("Exhibit permission loads"),A->LoadFromFile(UserPath));
 TestTrue(TEXT("Exhibit recognized by channel ID"),A->IsExhibit(TEXT("demo")));
 TestFalse(TEXT("Exhibit has no admin rights"),A->IsAdmin(TEXT("demo")));
 TestTrue(TEXT("Admin excluded"),A->IsLeaderboardExcluded(TEXT("operator")));
 TestTrue(TEXT("Exhibit excluded"),A->IsLeaderboardExcluded(TEXT("demo")));
 TestFalse(TEXT("Regular user included"),A->IsLeaderboardExcluded(TEXT("viewer")));
 FParsedCommand C;
 TestTrue(TEXT("Patch parses"),FChatCommandParser::TryParse(TEXT("operator"),TEXT("!패치"),C));
 TestEqual(TEXT("Patch type"),C.CommandType,ECommandType::Patch);
 TestFalse(TEXT("Patch rejects arguments"),FChatCommandParser::TryParse(TEXT("operator"),TEXT("!패치 잘못된인자"),C));
 C.UserId=TEXT("demo"); C.CommandType=ECommandType::Patch;
 TestTrue(TEXT("Exhibit patch consumed without execution"),A->HandleAdminCommand(C,T.Battle));
 T.Battle->StopBattle();
 auto* Schedule=T.Instance->GetSubsystem<UBattleScheduleSubsystem>();
 FAdminScheduleTestAccess::Set(Schedule,{});
 T.DispatchRaw(TEXT("viewer"),TEXT("!전투"));
 FDateTime Next;
 TestFalse(TEXT("Regular user cannot start combat"),Schedule->GetNextStart(FDateTime::Now(),Next));
 T.DispatchRaw(TEXT("demo"),TEXT("!전투"));
 TestTrue(TEXT("Exhibit can start combat"),Schedule->GetNextStart(FDateTime::Now(),Next));
 TestTrue(TEXT("Five seconds to combat"),(Next-FDateTime::Now()).GetTotalSeconds()<=5.1);
 T.Battle->StartBattle(); FBattleUnificationTestAccess::Prepare(T.Battle);
 for (const FString Id : {FString(TEXT("operator")),FString(TEXT("demo")),FString(TEXT("viewer"))})
  T.DispatchRaw(Id,TEXT("!로그인 글라디우스"));
 auto* Widget=CreateWidget<ULeaderboardRotatorWidget>(T.Instance);
 Widget->BattleManager=T.Battle;
 const auto Total=Widget->RankTotal();
 TestEqual(TEXT("Total includes only regular viewers"),Total.Num(),1);
 if(Total.Num()==1) TestEqual(TEXT("Filtered ranks are contiguous"),Total[0].Rank,1);
 TestEqual(TEXT("Battle includes all roles"),Widget->RankBattle().Num(),3);
 T.Battle->SaveRecords();
 TestTrue(TEXT("Player file reloads"),T.Battle->ReloadExternalFiles());
 TestEqual(TEXT("Reload retains active participants"),T.Battle->GetBattlePlayers().Num(),3);
 FFileHelper::SaveStringToFile(TEXT("{"),*UserPath);
 TestFalse(TEXT("Partial file rejected"),A->LoadFromFile(UserPath));
 TestTrue(TEXT("Last valid exhibit survives malformed reload"),A->IsExhibit(TEXT("demo")));
 TestTrue(TEXT("Last valid admin survives malformed reload"),A->IsAdmin(TEXT("operator")));
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNicknameTargetResolverTest,"Gladius.Admin.NicknameTargetResolution",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNicknameTargetResolverTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* A=T.Instance->GetSubsystem<UAdminSubsystem>();
 const FString Path=FAdminTestAccess::Path(A);
 FFileHelper::SaveStringToFile(TEXT(R"({"users":[{"userId":"target1","displayName":"@홍 길동","permission":"user"},{"userId":"target2","displayName":"중복","permission":"user"},{"userId":"target3","displayName":"@중복","permission":"user"},{"userId":"@verified","channelId":"verified-channel","displayName":"인증된 별칭","permission":"user"}]})"),*Path);
 TestTrue(TEXT("Load isolated users"),A->LoadFromFile(Path));
 FString Id,Name; bool Ambiguous=false;
 TestTrue(TEXT("Display nickname with spaces resolves"),A->ResolveUserTarget(TEXT("홍 길동"),T.Battle,Id,Name,Ambiguous));
 TestEqual(TEXT("Nickname maps to stable account"),Id,FString(TEXT("target1")));
 TestTrue(TEXT("At-prefix and whitespace accepted"),A->ResolveUserTarget(TEXT("  @홍 길동  "),T.Battle,Id,Name,Ambiguous));
 TestTrue(TEXT("Exact ID compatibility"),A->ResolveUserTarget(TEXT("target1"),T.Battle,Id,Name,Ambiguous));
 TestFalse(TEXT("Duplicate nickname rejected"),A->ResolveUserTarget(TEXT("중복"),T.Battle,Id,Name,Ambiguous));
 TestTrue(TEXT("Duplicate explicitly reported"),Ambiguous);
 TestTrue(TEXT("Registered verified alias resolves canonically"),A->ResolveUserTarget(TEXT("인증된 별칭"),T.Battle,Id,Name,Ambiguous));
 TestEqual(TEXT("Alias uses channel identity"),Id,FString(TEXT("verified-channel")));
 TestFalse(TEXT("Unknown nickname rejected"),A->ResolveUserTarget(TEXT("없는 닉네임"),T.Battle,Id,Name,Ambiguous));
 TestFalse(TEXT("Unknown is not duplicate"),Ambiguous);
 TestTrue(TEXT("Resolution does not invent a user"),A->FindUser(TEXT("없는 닉네임"))==nullptr);
 T.Battle->StartBattle(); FBattleUnificationTestAccess::Prepare(T.Battle);
 T.Battle->LoginPlayer(TEXT("target1"),TEXT("@홍 길동"),TEXT("gladius"));
 A->RegisterUser(TEXT("target1"),TEXT("@새 닉네임"));
 TestTrue(TEXT("Latest chat nickname resolves"),A->ResolveUserTarget(TEXT("새 닉네임"),T.Battle,Id,Name,Ambiguous));
 TestFalse(TEXT("Stale record nickname is not retained as an alias"),A->ResolveUserTarget(TEXT("홍 길동"),T.Battle,Id,Name,Ambiguous));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExhibitNicknameEventTest,"Gladius.Admin.ExhibitEventsByNickname",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExhibitNicknameEventTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* A=T.Instance->GetSubsystem<UAdminSubsystem>();
 auto* Log=T.Instance->GetSubsystem<USessionLogSubsystem>();
 const FString Path=FAdminTestAccess::Path(A);
 FFileHelper::SaveStringToFile(TEXT(R"({"users":[{"userId":"operator","permission":"admin"},{"userId":"exhibitor","displayName":"전시 운영","permission":"exhibit"},{"userId":"viewer","permission":"user"},{"userId":"recipient","displayName":"@홍 길동","permission":"user"},{"userId":"offline","displayName":"오프라인 닉","permission":"user"},{"userId":"duplicate1","displayName":"중복","permission":"user"},{"userId":"duplicate2","displayName":"@중복","permission":"user"}]})"),*Path);
 TestTrue(TEXT("Load isolated roles"),A->LoadFromFile(Path));
 auto Send=[&](const FString& Sender,const TCHAR* Text)
 {
  FParsedCommand C; TestTrue(TEXT("Parse command"),FChatCommandParser::TryParse(Sender,Text,C));
  C.Username=Sender==TEXT("exhibitor")?TEXT("전시 운영"):Sender;
  A->HandleAdminCommand(C,T.Battle);
 };
 T.Battle->StartBattle(); FBattleUnificationTestAccess::Prepare(T.Battle);
 T.Battle->LoginPlayer(TEXT("recipient"),TEXT("@홍 길동"),TEXT("gladius"));
 const auto Before=T.Battle->GetBattlePlayers().FindChecked(TEXT("recipient"));
 Send(TEXT("viewer"),TEXT("!이벤트 @홍 길동"));
 TestFalse(TEXT("Ordinary viewers cannot grant events"),A->HasEventBuff(TEXT("recipient")));
 Send(TEXT("exhibitor"),TEXT("!이벤트 홍 길동"));
 TestTrue(TEXT("Exhibit can grant by nickname"),A->HasEventBuff(TEXT("recipient")));
 const auto After=T.Battle->GetBattlePlayers().FindChecked(TEXT("recipient"));
 TestEqual(TEXT("Active recipient health bonus applied"),After.MaxHP,Before.MaxHP+1);
 TestEqual(TEXT("Active recipient attack bonus applied"),After.DamageBonus,Before.DamageBonus+5);
 TestTrue(TEXT("Notification uses exhibitor name"),Log->GetEntries().Last().Message.StartsWith(TEXT("[전시 운영]")));
 TestTrue(TEXT("No nickname-keyed phantom account"),A->FindUser(TEXT("홍 길동"))==nullptr);
 Send(TEXT("exhibitor"),TEXT("!이벤트 @홍 길동"));
 TestEqual(TEXT("Event remains nonstacking across nickname forms"),T.Battle->GetBattlePlayers().FindChecked(TEXT("recipient")).MaxHP,After.MaxHP);
 Send(TEXT("operator"),TEXT("!이벤트 오프라인 닉"));
 TestTrue(TEXT("Offline known user can receive event by name"),A->HasEventBuff(TEXT("offline")));
 Send(TEXT("exhibitor"),TEXT("!이벤트 중복"));
 TestFalse(TEXT("Duplicate first user unchanged"),A->HasEventBuff(TEXT("duplicate1")));
 TestFalse(TEXT("Duplicate second user unchanged"),A->HasEventBuff(TEXT("duplicate2")));
 TestEqual(TEXT("Ambiguity produces visible error"),Log->GetEntries().Last().Event,ESessionLogEvent::AdminUrgent);
 Send(TEXT("exhibitor"),TEXT("!이벤트 등록되지 않은 이름"));
 TestTrue(TEXT("Unknown names cannot create a target row"),A->FindUser(TEXT("등록되지 않은 이름"))==nullptr);
 const int32 LogCount=Log->GetEntries().Num();
 Send(TEXT("exhibitor"),TEXT("!공지 금지"));
 Send(TEXT("exhibitor"),TEXT("!패치"));
 Send(TEXT("exhibitor"),TEXT("!차단 @홍 길동 1"));
 Send(TEXT("exhibitor"),TEXT("!점검 전체"));
 TestEqual(TEXT("Other privileged commands remain denied"),Log->GetEntries().Num(),LogCount);
 TestFalse(TEXT("Exhibit cannot block"),A->IsBlocked(TEXT("recipient"),FDateTime::UtcNow()));
 TestTrue(TEXT("Exhibit cannot enable maintenance"),A->CanInput(TEXT("viewer"),EGameState::Temple));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNicknameModerationTest,"Gladius.Admin.ModerationByNickname",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNicknameModerationTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* A=T.Instance->GetSubsystem<UAdminSubsystem>();
 const FString Path=FAdminTestAccess::Path(A);
 FFileHelper::SaveStringToFile(TEXT(R"({"users":[{"userId":"operator","permission":"admin"},{"userId":"target","displayName":"@공백 닉","permission":"user"},{"userId":"numeric","displayName":"닉네임 2","permission":"user"}]})"),*Path);
 TestTrue(TEXT("Load isolated moderation users"),A->LoadFromFile(Path));
 auto Send=[&](const TCHAR* Text)
 {
  FParsedCommand C; FChatCommandParser::TryParse(TEXT("operator"),Text,C); A->HandleAdminCommand(C,T.Battle);
 };
 Send(TEXT("!차단 @공백 닉 0.5"));
 TestTrue(TEXT("Spaced nickname blocked"),A->IsBlocked(TEXT("target"),FDateTime::UtcNow()));
 FDateTime Until; FDateTime::ParseIso8601(*A->FindUser(TEXT("target"))->BlockTime,Until);
 TestTrue(TEXT("Duration still parsed after a spaced nickname"),FMath::Abs((Until-FDateTime::UtcNow()).GetTotalSeconds()-1800)<3);
 Send(TEXT("!차단해제 공백 닉"));
 TestFalse(TEXT("Spaced nickname unblocked"),A->IsBlocked(TEXT("target"),FDateTime::UtcNow()));
 Send(TEXT("!차단 닉네임 2"));
 TestTrue(TEXT("Trailing number can be part of the exact nickname"),A->IsBlocked(TEXT("numeric"),FDateTime::UtcNow()));
 FDateTime::ParseIso8601(*A->FindUser(TEXT("numeric"))->BlockTime,Until);
 TestEqual(TEXT("Exact numeric name uses default one-year duration"),Until.GetYear(),FDateTime::UtcNow().GetYear()+1);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdminScheduleTest,"Gladius.Admin.ScheduleCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdminScheduleTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 auto* S=T.Instance->GetSubsystem<UBattleScheduleSubsystem>();
 auto* Settings=GetMutableDefault<URealTimeSettings>();
 TGuardValue<bool> Enabled(Settings->bEnableBattleSchedule,true);
 const FDateTime Now(2030,1,1,12,0,0);
 FAdminScheduleTestAccess::Set(S,{13*60,14*60});
 S->AddManualStart(Now+FTimespan::FromMinutes(30));
 FDateTime Cancelled, Next;
 TestTrue(TEXT("Cancel manual first"),S->CancelNext(Now,Cancelled,Next));
 TestEqual(TEXT("Manual cancelled"),Cancelled,Now+FTimespan::FromMinutes(30));
 TestEqual(TEXT("Daily follows manual"),Next,Now+FTimespan::FromHours(1));
 TestTrue(TEXT("Cancel daily next"),S->CancelNext(Now,Cancelled,Next));
 TestEqual(TEXT("Following daily preserved"),Next,Now+FTimespan::FromHours(2));
 TestFalse(TEXT("Cancelled daily never fires"),FAdminScheduleTestAccess::Consume(S,Now+FTimespan::FromMinutes(59),Now+FTimespan::FromHours(1)));
 TestTrue(TEXT("Following daily fires"),FAdminScheduleTestAccess::Consume(S,Now+FTimespan::FromMinutes(119),Now+FTimespan::FromHours(2)));
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathRevivalTickerTest,"Gladius.SessionLog.GroupedDeathsAndRevivals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeathRevivalTickerTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T; auto* B=T.Battle;
 B->StartBattle(); FBattleUnificationTestAccess::Prepare(B); B->IdleTurnsBeforeKick=100;
 auto* Log=T.Instance->GetSubsystem<USessionLogSubsystem>();
 for(const TCHAR* Id : {TEXT("Alice"),TEXT("Bob")})
 {
  B->SubmitAction(Id,Id,EBattleAction::Idle,TEXT("gladius"));
  B->SetPlayerHealth(Id,1);
 }
 const int32 Before=Log->GetEntries().Num();
 for(int32 Lane=0;Lane<3;++Lane) B->TelegraphedAction.DamageByZone[Lane]=1000;
 B->TelegraphedAction.HitCount=1;
 FBattleUnificationTestAccess::Resolve(B);
 if(!TestEqual(TEXT("One grouped death announcement"),Log->GetEntries().Num(),Before+1)) return false;
 TestEqual(TEXT("Death text joins names"),Log->GetEntries().Last().Message,FString(TEXT("Alice, Bob이 사망하였습니다. 3턴 후에 부활합니다.")));
 TestTrue(TEXT("Death uses AdminColor category"),Log->GetEntries().Last().Event==ESessionLogEvent::Admin);
 for(int32 Lane=0;Lane<3;++Lane) B->TelegraphedAction.DamageByZone[Lane]=0;
 for(int32 Turn=1;Turn<=3;++Turn)
 {
  FBattleUnificationTestAccess::Resolve(B);
  TestEqual(TEXT("Only revive on the third turn"),Log->GetEntries().Num(),Before+(Turn==3?2:1));
 }
 TestEqual(TEXT("Revival text joins names"),Log->GetEntries().Last().Message,FString(TEXT("사망한 Alice, Bob이 부활하였습니다. 강화한 능력치가 소멸됩니다.")));
 TestTrue(TEXT("Revival uses AdminColor category"),Log->GetEntries().Last().Event==ESessionLogEvent::Admin);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAuthoredMessageTest,"Gladius.BattleUI.AuthoredBossTelegraphUsesJson",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBossAuthoredMessageTest::RunTest(const FString& Parameters)
{
 FUnifiedBattleWorld T;
 if(!TestTrue(TEXT("Real boss JSON loads"),FBattleUnificationTestAccess::LoadBossData(T.Battle))) return false;
 auto* Class=LoadClass<UBossTelegraphWidget>(nullptr,TEXT("/Game/Blueprints/UI/WBP_BossTelegraph.WBP_BossTelegraph_C"));
 if(!TestNotNull(TEXT("Actual designer widget loads"),Class)) return false;
 auto* UI=CreateWidget<UBossTelegraphWidget>(T.Instance,Class); UI->TakeWidget();
 auto* Text=Cast<UTextBlock>(UI->WidgetTree->FindWidget(TEXT("BT_MessageText")));
 if(!TestNotNull(TEXT("Authored text block exists"),Text)) return false;
 for(const TCHAR* Id : {TEXT("SingleStrikeLeft"),TEXT("SingleStrikeCenter"),TEXT("SingleStrikeRight")})
 {
  const auto* Action=FBattleUnificationTestAccess::BossData(T.Battle).FindAction(Id);
  if(!TestNotNull(TEXT("JSON attack exists"),Action)) return false;
  UI->ShowTelegraph(Action->TelegraphText);
  TestEqual(TEXT("Real widget displays the selected JSON announcement"),Text->GetText().ToString(),Action->TelegraphText);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossActualImpactTest,"Gladius.BattleUnified.ActualBossMontageImpacts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBossActualImpactTest::RunTest(const FString& Parameters)
{
 auto* Class=LoadClass<ABattleUnit>(nullptr,TEXT("/Game/Blueprints/ScaledBattleUnits/BP_BossUnit_DevBattle.BP_BossUnit_DevBattle_C"));
 if(!TestNotNull(TEXT("Actual main battle boss class"),Class))return false;
 auto* Boss=Class->GetDefaultObject<ABattleUnit>();
 for(const auto& State:{TEXT("SingleStrikeLeft"),TEXT("SingleStrikeCenter"),TEXT("SingleStrikeRight"),TEXT("LeftRight")})
 {
  auto* Montage=Boss->GetStrikeMontage(State);
  if(!TestNotNull(State,Montage))continue;
  const int32 Section=Montage->GetSectionIndex(TEXT("Strike"));
  if(!TestTrue(TEXT("Actual attack has Strike section"),Section!=INDEX_NONE))continue;
  float Start=0.f,End=0.f; Montage->GetSectionStartAndEndTime(Section,Start,End);
  TArray<float> Contacts;
  for(const auto& Event:Montage->Notifies)
   if(Event.NotifyName==TEXT("PlayNiagaraEffect") && Event.GetTriggerTime()>=Start && Event.GetTriggerTime()<End)
    Contacts.Add(Event.GetTriggerTime()-Start);
  Contacts.Sort();
  if(!TestTrue(TEXT("Actual collision effect remains inside attack"),!Contacts.IsEmpty()))continue;
  const float Left=ABattleManager::StrikeImpactSeconds(Montage,EBattleLane::Left);
  const float Right=ABattleManager::StrikeImpactSeconds(Montage,EBattleLane::Right);
  TestTrue(TEXT("Victim and shake contact happens after attack begins"),Left>0.f);
  TestEqual(TEXT("First impact matches visible collision effect"),Left,Contacts[0]);
  if(FString(State)==TEXT("LeftRight"))
  {
   TestTrue(TEXT("Both stomps retain separate contacts"),Contacts.Num()>=2 && Right>Left);
   if(Contacts.Num()>=2)TestEqual(TEXT("Right impact matches second stomp"),Right,Contacts[1]);
  }
  else TestEqual(TEXT("Single attack has one shared impact"),Right,Left);
  AddInfo(FString::Printf(TEXT("%s (%s): left %.3fs, right %.3fs after Strike begins"),State,*Montage->GetName(),Left,Right));
 }
 return true;
}

#endif


