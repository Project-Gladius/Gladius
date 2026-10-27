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
#include "Scripts/Backend/YouTubeChatManager.h"

struct FBattleTutorialTestAccess
{
 static const TArray<FTutorialMessage>& Messages(ABattleManager* M) { return M->Tutorial->Messages; }
 static float MessageSeconds(ABattleManager* M) { return M->GetWorldTimerManager().GetTimerRate(M->Tutorial->MessageTimer); }
 static void ApplyExternalTimings(ABattleManager* M) { M->bUseBalanceFiles=true;M->ApplyBalanceFiles(); }
 static bool Polling(AYouTubeChatManager* Chat) { return Chat->bCombatAutoPollingEnabled; }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialPollingResumeTest,"Gladius.Tutorial.ResumeChatAfterEachPracticeTurn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialPollingResumeTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; T.LoginTutorial();
 auto* Chat = T.World->SpawnActor<AYouTubeChatManager>();
 for (int32 Turn : {2,3,4})
 {
  Chat->SetCombatAutoPollingEnabled(false);
  FBattleTutorialTestAccess::Turn(T.Manager, Turn);
  TestTrue(TEXT("Next practice explanation resumes command reception"), FBattleTutorialTestAccess::Polling(Chat));
  Chat->SetCombatAutoPollingEnabled(false);
  FBattleTutorialTestAccess::ExplainDone(T.Manager);
  TestTrue(TEXT("Input wait resumes command reception"), FBattleTutorialTestAccess::Polling(Chat));
 }
 Chat->Destroy();
 return true;
}

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
  TestEqual(TEXT("Practice selection window is five seconds"),M->GetInputPhaseDuration(),5.f);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialPollingResumeTest,"Gladius.Tutorial.ResumeChatAfterEachPracticeTurn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialPollingResumeTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; T.LoginTutorial();
 auto* Chat = T.World->SpawnActor<AYouTubeChatManager>();
 for (int32 Turn : {2,3,4})
 {
  Chat->SetCombatAutoPollingEnabled(false);
  FBattleTutorialTestAccess::Turn(T.Manager, Turn);
  TestTrue(TEXT("Next practice explanation resumes command reception"), FBattleTutorialTestAccess::Polling(Chat));
  Chat->SetCombatAutoPollingEnabled(false);
  FBattleTutorialTestAccess::ExplainDone(T.Manager);
  TestTrue(TEXT("Input wait resumes command reception"), FBattleTutorialTestAccess::Polling(Chat));
 }
 Chat->Destroy();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleExternalTimingTest,"Gladius.Tutorial.ExternalBattleTimings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleExternalTimingTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T;
 FBattleTutorialTestAccess::ApplyExternalTimings(T.Manager);
 TestEqual(TEXT("External selection timing overrides level value"),T.Manager->PlayerSelectCountdown,15.f);
 TestEqual(TEXT("External collection timing overrides level value"),T.Manager->CollectSeconds,5.f);
 T.LoginTutorial();
 FBattleTutorialTestAccess::Turn(T.Manager,2);
 FBattleTutorialTestAccess::ExplainDone(T.Manager);
 TestEqual(TEXT("Practice keeps its own five-second selection window"),T.Manager->GetInputPhaseDuration(),5.f);
 FBattleTutorialTestAccess::Collect(T.Manager);
 TestEqual(TEXT("Practice uses external collection duration"),T.Manager->GetInputPhaseDuration(),5.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialMessageTimingTest,"Gladius.Tutorial.MessageDurations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialMessageTimingTest::RunTest(const FString& Parameters)
{
 FTutorialWorld T; auto* M=T.Manager;
 const auto Check=[&](int32 FirstSeconds)
 {
  const auto& Messages=FBattleTutorialTestAccess::Messages(M);
  for(int32 I=0;I<Messages.Num();++I)
   TestEqual(TEXT("Message uses requested duration"),Messages[I].Seconds,float(I==0?FirstSeconds:10));
 };
 Check(10); M->RequestTutorial(); Check(10);
 FBattleTutorialTestAccess::ExplainDone(M); M->LoginPlayer(TEXT("viewer"),TEXT("Viewer"),TEXT("gladius")); Check(10);
 for(int32 Turn:{2,3,4})
 {
  FBattleTutorialTestAccess::Turn(M,Turn); Check(Turn==3?10:20);
  if(Turn!=3)
  {
   FBattleTutorialTestAccess::ExplainDone(M);
   TestEqual(TEXT("Turn 2 and 4 action preparation lasts five seconds"),M->GetInputPhaseDuration(),5.f);
  }
 }
 TestEqual(TEXT("Turn 4 retains its real attack command"),FBattleTutorialTestAccess::Messages(M)[0].Text,FString(TEXT("채팅창에 \"!공격 2\"를 입력하세요.")));
 return true;
}
namespace
{
class FTutorialAnnouncementClock : public IAutomationLatentCommand
{
 TSharedPtr<FTutorialWorld> T; FAutomationTestBase* Test; int32 Step=0;
public:
 FTutorialAnnouncementClock(TSharedPtr<FTutorialWorld> In,FAutomationTestBase* InTest):T(In),Test(InTest){}
 bool Update() override
 {
  auto* M=T->Manager;
  if(Step++==0){T->World->GetTimerManager().Tick(0.f);return false;}
  if(Step==2)
  {
   T->World->GetTimerManager().Tick(19.9f);
   Test->TestTrue(TEXT("Battle cannot begin before twenty seconds"),FBattleTutorialTestAccess::Flow(M)==ETutorialFlow::StartingBattle);
   Test->TestEqual(TEXT("Tutorial still ends at turn four during announcement"),M->TurnIndex,4);
   return false;
  }
  T->World->GetTimerManager().Tick(.11f);
  Test->TestTrue(TEXT("Twenty seconds completes tutorial"),FBattleTutorialTestAccess::Flow(M)==ETutorialFlow::Complete);
  Test->TestEqual(TEXT("Main battle starts at turn one"),M->TurnIndex,1);
  return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialAnnouncementTest,"Gladius.Tutorial.TwentySecondBattleAnnouncement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialAnnouncementTest::RunTest(const FString& Parameters)
{
 auto T=MakeShared<FTutorialWorld>(); auto* M=T->Manager; T->LoginTutorial();
 FBattleTutorialTestAccess::Turn(M,4);
 M->SubmitAction(TEXT("warrior"),TEXT("Warrior"),EBattleAction::Attack2);
 FBattleTutorialTestAccess::CloseInput(M); FBattleTutorialTestAccess::FinishTurn(M);
 TestTrue(TEXT("Final practice starts announcement before main"),FBattleTutorialTestAccess::Flow(M)==ETutorialFlow::StartingBattle);
 TestEqual(TEXT("Announcement lasts twenty seconds"),FBattleTutorialTestAccess::MessageSeconds(M),20.f);
 TestEqual(TEXT("Announcement text"),FBattleTutorialTestAccess::Messages(M)[0].Text,FString(TEXT("\"이제 곧 전투가 시작됩니다!\"")));
 TestFalse(TEXT("Announcement rejects combat input"),M->Tutorial->AcceptsAction());
 TestFalse(TEXT("Announcement rejects tutorial health login"),M->Tutorial->AcceptsLogin());
 ADD_LATENT_AUTOMATION_COMMAND(FTutorialAnnouncementClock(T,this));
 return true;
}

#endif
