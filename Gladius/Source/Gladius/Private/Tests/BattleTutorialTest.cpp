#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BattleCameraTestAccess.h"
#include "Scripts/Battle/BattleTutorialController.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

struct FBattleTutorialTestAccess
{
 static void Attach(ABattleManager* M)
 {
  M->GetWorldTimerManager().ClearTimer(M->PhaseTimerHandle);
  M->Tutorial=NewObject<UBattleTutorialController>(M);
  M->Tutorial->Manager=M; M->Tutorial->EntryTime=M->GetWorld()->GetTimeSeconds(); M->Tutorial->BeginOffer();
 }
 static void ExplainDone(ABattleManager* M) { M->GetWorldTimerManager().ClearTimer(M->Tutorial->MessageTimer); M->Tutorial->MessagesFinished(); }
 static void Turn(ABattleManager* M,int32 Turn) { M->Tutorial->EnterTurn(Turn); }
 static void CloseInput(ABattleManager* M) { M->GetWorldTimerManager().ClearTimer(M->PhaseTimerHandle); M->Tutorial->OnInputClosed(); }
 static void FinishTurn(ABattleManager* M) { M->Tutorial->OnTurnFinished(); }
 static int32 Empty(ABattleManager* M) { return M->Tutorial->EmptyTurns; }
 static int32 Health(ABattleManager* M,const FString& Id) { return M->Records.FindChecked(Id).CurrentHealth; }
 static void Seed(ABattleManager* M,const FString& Id,int32 HP) { M->InitializePlayerRecords(); M->GetOrCreateRecord(Id,Id,TEXT("gladius")).CurrentHealth=HP; }
 static void Timeout(ABattleManager* M) { M->Tutorial->ReturnToOffer(); }
 static float LoginTimeout(ABattleManager* M) { return M->GetWorldTimerManager().GetTimerRemaining(M->Tutorial->DeadlineTimer); }
 static void Collect(ABattleManager* M) { M->BeginCollect(); }
 static ETutorialFlow Flow(ABattleManager* M) { return M->Tutorial->Flow; }
 static void ResolvePracticeAttack(ABattleManager* M)
 {
  M->SubmitAction(TEXT("veteran"),TEXT("Veteran"),EBattleAction::Attack1);
  M->BeginMove(); M->GetWorldTimerManager().ClearTimer(M->PhaseTimerHandle); M->ResolveTurn();
 }
 static int32 TotalDamage(ABattleManager* M,const FString& Id) { return M->Records.FindChecked(Id).TotalDamageDealt; }
 static void Complete(ABattleManager* M) { M->Tutorial->CompleteToMain(); }
 static void Expire(ABattleManager* M) { M->Tutorial->ExpireOffer(); }
 static void SetEntryAge(ABattleManager* M,float Seconds) { M->Tutorial->EntryTime=M->GetWorld()->GetTimeSeconds()-Seconds; M->Tutorial->BeginOffer(); }
};
namespace
{
struct FTutorialWorld
{
 UGameInstance* Instance=NewObject<UGameInstance>(GEngine);
 UWorld* World=nullptr;
 ABattleManager* Manager=nullptr;
 FTutorialWorld()
 {
  Instance->AddToRoot(); Instance->InitializeStandalone(); World=Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
  World->EnableActorTickAndUserCallbacks(true);
#endif
  World->InitializeActorsForPlay(FURL());
  Manager=World->SpawnActor<ABattleManager>(); FBattleCameraTestAccess::RecordsOnly(Manager);
  FCharacterDef G; G.Id=TEXT("gladius"); G.BaseHP=5;
  FCharacterDef S; S.Id=TEXT("sera"); S.BaseHP=5;
  Manager->Classes={G,S}; Manager->bEnableTurnCutScenes=false;
  Manager->StartBattle(); FBattleTutorialTestAccess::Attach(Manager);
 }
 ~FTutorialWorld()
 {
  FBattleCameraTestAccess::Cleanup(World); Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
 }
 void LoginTutorial(const FString& Id=TEXT("warrior"))
 {
  Manager->RequestTutorial(); FBattleTutorialTestAccess::ExplainDone(Manager);
  Manager->LoginPlayer(Id,Id,TEXT("gladius"));
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialOfferTest,"Gladius.Tutorial.OfferAndChat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialOfferTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; auto* M=T.Manager;
 auto Hud=M->Tutorial->BuildHud();
 TestEqual(TEXT("Offer turn is zero"),Hud.TurnText,FString(TEXT("0")));
 TestFalse(TEXT("Zero timer hidden"),Hud.bShowTimer);
 TestEqual(TEXT("Offer prompt"),Hud.GuideText,FString(TEXT("캐릭터를 소환하세요!")));
 TestTrue(TEXT("Boss hidden"),M->GetBossUnit()->IsHidden());
 FParsedCommand Cmd;
 TestTrue(TEXT("Tutorial command parses"),FChatCommandParser::TryParse(TEXT("viewer"),TEXT("!튜토리얼"),Cmd));
 TestFalse(TEXT("Unexpected tutorial arguments rejected"),FChatCommandParser::TryParse(TEXT("viewer"),TEXT("!튜토리얼 extra"),Cmd));
 FChatCommandParser::TryParse(TEXT("viewer"),TEXT("!튜토리얼"),Cmd);
 auto* Dispatcher=NewObject<UCommandDispatcher>(T.Instance); Dispatcher->BindBattleManager(M); Dispatcher->Execute(Cmd);
 TestTrue(TEXT("Tutorial request starts practice"),M->IsTutorialPracticing());
 TestFalse(TEXT("Repeated request cannot restart tutorial"),M->RequestTutorial());
 Hud=M->Tutorial->BuildHud();
 TestEqual(TEXT("Every tutorial turn starts with tutorial heading"),Hud.GuideText,FString(TEXT("튜토리얼")));
 TestEqual(TEXT("Four tutorial turns"),Hud.TurnTotalText,FString(TEXT("/4")));
 Dispatcher->UnbindBattleManager(M);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialLoginTest,"Gladius.Tutorial.LoginTimeoutAndHealth",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialLoginTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; auto* M=T.Manager;
 M->RequestTutorial(); FBattleTutorialTestAccess::ExplainDone(M);
 TestFalse(TEXT("Login waiting has no visible timer"),M->Tutorial->BuildHud().bShowTimer);
 TestEqual(TEXT("Internal login deadline is sixty seconds"),FBattleTutorialTestAccess::LoginTimeout(M),60.f,.01f);
 FBattleTutorialTestAccess::Timeout(M);
 TestTrue(TEXT("Timeout returns to offer"),FBattleTutorialTestAccess::Flow(M)==ETutorialFlow::Offer);
 FBattleTutorialTestAccess::Seed(M,TEXT("veteran"),2);
 T.LoginTutorial(TEXT("veteran"));
 const auto& P=M->GetBattlePlayers().FindChecked(TEXT("veteran"));
 TestEqual(TEXT("Tutorial restores maximum temporary health"),P.HP,5);
 TestTrue(TEXT("Tutorial spawns left"),P.Lane==EBattleLane::Left);
 TestEqual(TEXT("Saved health unchanged at login"),FBattleTutorialTestAccess::Health(M,TEXT("veteran")),2);
 M->SetPlayerHealth(TEXT("veteran"),1);
 TestEqual(TEXT("Practice damage never changes saved health"),FBattleTutorialTestAccess::Health(M,TEXT("veteran")),2);
 FBattleTutorialTestAccess::ResolvePracticeAttack(M);
 TestTrue(TEXT("Tutorial uses real damage calculation"),M->GetBattlePlayers().FindChecked(TEXT("veteran")).DamageDealt>0);
 TestEqual(TEXT("Tutorial attack never changes permanent damage"),FBattleTutorialTestAccess::TotalDamage(M,TEXT("veteran")),0);
 TestEqual(TEXT("Attack does not overwrite saved health"),FBattleTutorialTestAccess::Health(M,TEXT("veteran")),2);
 FBattleTutorialTestAccess::Complete(M);
 TestEqual(TEXT("Main restores pre-tutorial health"),M->GetBattlePlayers().FindChecked(TEXT("veteran")).HP,2);
 TestTrue(TEXT("Participant retained"),M->IsPlayerLoggedIn(TEXT("veteran")));
 TestEqual(TEXT("Main starts at turn one"),M->TurnIndex,1);
 TestFalse(TEXT("Main is no longer practice"),M->IsTutorialPracticing());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialDeadHealthRestoreTest,"Gladius.LifeState.TutorialRestoresDeadParticipant",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialDeadHealthRestoreTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; auto* M=T.Manager;
 FBattleTutorialTestAccess::Seed(M,TEXT("veteran"),0);
 T.LoginTutorial(TEXT("veteran"));
 const auto& P=M->GetBattlePlayers().FindChecked(TEXT("veteran"));
 TestTrue(TEXT("Dead participant can practice with temporary health"),P.IsAlive());
 TestFalse(TEXT("Practice actor is not dead"),P.Unit->IsDead());
 FBattleTutorialTestAccess::Complete(M);
 TestEqual(TEXT("Permanent zero health is restored"),P.HP,0);
 TestFalse(TEXT("Restored participant waits for natural revival"),P.IsAlive());
 TestTrue(TEXT("Restored actor agrees with dead gameplay state"),P.Unit->IsDead());
 TestTrue(TEXT("Restored dead actor is hidden"),P.Unit->IsHidden());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialInputRulesTest,"Gladius.Tutorial.NormalInputRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialInputRulesTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; auto* M=T.Manager; T.LoginTutorial();
 M->PlayerSelectCountdown=23.f; M->CollectSeconds=7.f;
 for (int32 Turn:{2,4})
 {
  FBattleTutorialTestAccess::Turn(M,Turn);
  TestFalse(TEXT("Explanatory zero timer is hidden"),M->Tutorial->BuildHud().bShowTimer);
  TestEqual(TEXT("Turn begins with tutorial heading"),M->Tutorial->BuildHud().GuideText,FString(TEXT("튜토리얼")));
  TestTrue(TEXT("Commands during explanation accepted"),M->SubmitAction(TEXT("warrior"),TEXT("Warrior"),EBattleAction::Attack2));
  FBattleTutorialTestAccess::ExplainDone(M);
  TestEqual(TEXT("Uses live normal selection setting"),M->GetInputPhaseDuration(),23.f);
  TestTrue(TEXT("Timer is visible for input"),M->Tutorial->BuildHud().bShowTimer);
  FBattleTutorialTestAccess::Collect(M);
  TestTrue(TEXT("Uses normal collection phase"),M->Phase==EBattlePhase::Collect);
  TestEqual(TEXT("Uses live normal collection setting"),M->GetInputPhaseDuration(),7.f);
  TestTrue(TEXT("Late defense commands accepted"),M->SubmitAction(TEXT("warrior"),TEXT("Warrior"),EBattleAction::Defense));
  FBattleTutorialTestAccess::CloseInput(M);
  TestEqual(TEXT("Accepted commands reset empty streak"),FBattleTutorialTestAccess::Empty(M),0);
 }
 FBattleTutorialTestAccess::Turn(M,3);
 TestFalse(TEXT("Only turn three attacks"),M->Tutorial->SuppressesBossAttack());
 TestTrue(TEXT("Left is hit"),M->TelegraphedAction.DamageByZone[0]>0);
 TestEqual(TEXT("Middle is safe"),M->TelegraphedAction.DamageByZone[1],0);
 TestEqual(TEXT("Right is safe"),M->TelegraphedAction.DamageByZone[2],0);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialIdlePriorityTest,"Gladius.Tutorial.IdleReturnOverridesFinalCompletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialIdlePriorityTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; auto* M=T.Manager; T.LoginTutorial();
 FBattleTutorialTestAccess::Turn(M,3); FBattleTutorialTestAccess::CloseInput(M);
 TestEqual(TEXT("First idle turn"),FBattleTutorialTestAccess::Empty(M),1);
 FBattleTutorialTestAccess::FinishTurn(M);
 TestEqual(TEXT("First idle still advances"),M->TurnIndex,4);
 FBattleTutorialTestAccess::CloseInput(M);
 TestEqual(TEXT("Second idle on final turn detected"),FBattleTutorialTestAccess::Empty(M),2);
 FBattleTutorialTestAccess::FinishTurn(M);
 TestTrue(TEXT("Returns to offer rather than main"),FBattleTutorialTestAccess::Flow(M)==ETutorialFlow::Offer);
 TestEqual(TEXT("HUD is turn zero"),M->Tutorial->BuildHud().TurnText,FString(TEXT("0")));
 TestTrue(TEXT("Tutorial actors removed so offer does not auto-skip"),M->GetBattlePlayers().IsEmpty());
 TestTrue(TEXT("Boss hidden again"),M->GetBossUnit()->IsHidden());
 TestTrue(TEXT("Tutorial can be requested again"),M->RequestTutorial());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialSkipTest,"Gladius.Tutorial.LoginSkipsAndDeadline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialSkipTest::RunTest(const FString& Parameters)
{
 {
  FTutorialWorld T; auto* M=T.Manager;
  TestTrue(TEXT("Normal login succeeds in offer"),M->LoginPlayer(TEXT("skip"),TEXT("Skip"),TEXT("sera")));
  TestTrue(TEXT("Normal login skips tutorial"),FBattleTutorialTestAccess::Flow(M)==ETutorialFlow::Complete);
  TestEqual(TEXT("Starts main turn one"),M->TurnIndex,1);
  TestFalse(TEXT("Main boss shown"),M->GetBossUnit()->IsHidden());
 }
 {
  FTutorialWorld T; auto* M=T.Manager;
  FBattleTutorialTestAccess::SetEntryAge(M,601.f);
  TestFalse(TEXT("Ten-minute timeout does not start a battle"),M->IsBattleRunning());
  TestEqual(TEXT("No turn played on timeout"),M->TurnIndex,0);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialTextTest,"Gladius.Tutorial.RichText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialTextTest::RunTest(const FString& Parameters)
{
 TestEqual(TEXT("Quotes become emphasis markup"),UBattleTutorialController::FormatRichText(TEXT("'채팅' \"!공격 1\" <&>")),FString(TEXT("채팅 <TutorialEmphasis>!공격 1</> &lt;&amp;&gt;")));
 FTutorialWorld T;
 auto* PC=T.World->SpawnActor<APlayerController>();
 auto* LP=NewObject<ULocalPlayer>(GEngine); T.Instance->AddLocalPlayer(LP,FPlatformUserId::CreateFromInternalId(0)); PC->SetPlayer(LP);
 auto* W=CreateWidget<UBossTelegraphWidget>(PC,UBossTelegraphWidget::StaticClass());
 if (!TestNotNull(TEXT("Existing telegraph gains rich text"),W->RichMessageText.Get())) return false;
 auto* Table=W->RichMessageText->GetTextStyleSet();
 const auto* Row=Table->FindRow<FRichTextStyleRow>(TEXT("TutorialEmphasis"),TEXT("Test"));
 if (TestNotNull(TEXT("Emphasis style exists"),Row))
 {
  TestTrue(TEXT("Emphasis is yellow"),Row->TextStyle.ColorAndOpacity.GetSpecifiedColor().Equals(FLinearColor(1,1,0)));
  TestTrue(TEXT("Emphasis uses real Korean bold font"),Row->TextStyle.Font.FontObject->GetPathName().Contains(TEXT("MaruBuri_Bold")));
 }
 W->BindToBattleManager(T.Manager); W->ShowTutorialMessage(TEXT("<TutorialEmphasis>피버</>"),10.f);
 TestEqual(TEXT("Ten-second tutorial message"),W->DisplaySeconds,10.f);
 TestTrue(TEXT("Tutorial ignores normal prepare visibility restriction"),W->bTutorialMessage);
 W->EndTutorial(); TestFalse(TEXT("Normal telegraph mode restored"),W->bTutorialMessage);
 return true;
}
#endif
