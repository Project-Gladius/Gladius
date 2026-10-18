#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Scripts/Temple/Goddess.h"
#include "Scripts/Temple/TempleCycleController.h"

struct FTempleFlowTestAccess
{
 static void Cycle(ATempleCycleController* C) { C->HandleCycleTimer(); }
 static void Turn(ATempleCycleController* C) { C->HandleTurnTimer(); }
 static bool Leaving(ATempleCycleController* C) { return C->bLeavingTemple; }
 static double TurnRate(ATempleCycleController* C) { return C->GetWorldTimerManager().GetTimerRate(C->TurnTimerHandle); }
};
namespace
{
struct FTempleTestWorld
{
 UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
 UWorld* World;
 AGoddess* Goddess;
 ATempleCycleController* Controller;
 AYouTubeChatManager* Chat;
 FTempleTestWorld()
 {
  Instance->AddToRoot(); Instance->InitializeStandalone(); World=Instance->GetWorld();
  World->SpawnActor<AGameManager>();
  Chat=World->SpawnActor<AYouTubeChatManager>();
  Goddess=World->SpawnActor<AGoddess>();
  Controller=World->SpawnActor<ATempleCycleController>();
  Controller->StatusWidgetClass=nullptr;
  Controller->bAutoTravelOnCombatRequest=false;
  Controller->DispatchBeginPlay();
 }
 ~FTempleTestWorld()
 {
  Controller->Destroy(); Goddess->Destroy(); Chat->Destroy();
  Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
 }
};
class FTempleBlessingSequence : public IAutomationLatentCommand
{
 TSharedPtr<FTempleTestWorld> Context;
 FAutomationTestBase* Test;
 int32 Phase=0;
 int32 Frames=0;
public:
 FTempleBlessingSequence(TSharedPtr<FTempleTestWorld> InContext, FAutomationTestBase* InTest):Context(InContext),Test(InTest){}
 bool Update() override
 {
  Context->World->GetTimerManager().Tick(0.1f);
  const EGoddessState State=Context->Goddess->GetState();
  if(State==EGoddessState::Blessing2||State==EGoddessState::Blessing4){Test->AddError(TEXT("Empty blessing tier was played"));return true;}
  if(Phase==0&&State==EGoddessState::Blessing3)Phase=1;
  if(Phase==1&&State==EGoddessState::Idle)
  {
   TArray<FTempleTalkItem> Items; FTempleTalkItem Item;
   Item.QueryMessage=TEXT("hello"); Item.ResponseText=TEXT("reply"); Items.Add(Item);
   Context->Goddess->ReceiveTalkItems(Items);
   Test->TestEqual(TEXT("Idle waiting accepts late AI response"),Context->Goddess->GetState(),EGoddessState::Reading);
   return true;
  }
  if(++Frames>30){Test->AddError(TEXT("Blessing1 -> Blessing3 -> Idle did not finish"));return true;}
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleManualFlowTest,"Gladius.Temple.Flow.UnlimitedCyclesAndTurns",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleManualFlowTest::RunTest(const FString& Parameters)
{
 FTempleTestWorld T;
 TestEqual(TEXT("Default turn duration is ten minutes"),T.Controller->TurnTime,600.0f);
 TestEqual(TEXT("Independent turn timer is armed"),FTempleFlowTestAccess::TurnRate(T.Controller),600.0);
 TestEqual(TEXT("Starts on turn one"),T.Controller->GetCurrentTurn(),1);
 for(int32 I=0;I<8;++I)FTempleFlowTestAccess::Cycle(T.Controller);
 TestEqual(TEXT("More than five cycles can run"),T.Controller->GetCompletedTurns(),8);
 TestFalse(TEXT("Cycles never request combat automatically"),FTempleFlowTestAccess::Leaving(T.Controller));
 FTempleFlowTestAccess::Turn(T.Controller);
 TestEqual(TEXT("Turn deadline advances the turn"),T.Controller->GetCurrentTurn(),2);
 TestEqual(TEXT("Turn transition does not reset cycles"),T.Controller->GetCompletedTurns(),8);
 T.Controller->RequestCombatTravel();
 TestTrue(TEXT("Manager takeover request pauses temple"),FTempleFlowTestAccess::Leaving(T.Controller));
 FTempleFlowTestAccess::Cycle(T.Controller);FTempleFlowTestAccess::Turn(T.Controller);
 TestEqual(TEXT("Cycles stop after manual request"),T.Controller->GetCompletedTurns(),8);
 TestEqual(TEXT("Turn timer stops after manual request"),T.Controller->GetCurrentTurn(),2);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleCommandTest,"Gladius.Temple.Flow.CombatCommand",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleCommandTest::RunTest(const FString& Parameters)
{
 FParsedCommand C;
 TestTrue(TEXT("Combat travel command parses"),FChatCommandParser::TryParse(TEXT("viewer"),TEXT("!전투"),C));
 TestEqual(TEXT("New command has its own type"),C.CommandType,ECommandType::TravelToCombat);
 FParsedCommand Bad;TestFalse(TEXT("Unexpected arguments rejected"),FChatCommandParser::TryParse(TEXT("viewer"),TEXT("!전투 1"),Bad));
 UCommandDispatcher* D=NewObject<UCommandDispatcher>();D->Initialize();
 int32 Calls=0;D->OnTempleCombatCommand.AddLambda([&Calls](){++Calls;});
 D->GetGameStateDelegate.BindLambda([](){return EGameState::Temple;});D->Execute(C);
 D->GetGameStateDelegate.BindLambda([](){return EGameState::Combat;});D->Execute(C);
 TestEqual(TEXT("Command is only active in temple"),Calls,1);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleBlessingTest,"Gladius.Temple.Flow.RequestedBlessingsOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleBlessingTest::RunTest(const FString& Parameters)
{
 TSharedPtr<FTempleTestWorld> T=MakeShared<FTempleTestWorld>();
 FTempleCycleCommands C;C.EnhanceTiers.SetNum(4);C.Talks.Add(FTempleTalkBatchMessage());
 T->Goddess->StartCycle(C);
 TestEqual(TEXT("Talk-only immediately waits in Idle"),T->Goddess->GetState(),EGoddessState::Idle);
 TArray<FTempleTalkItem> Reply;FTempleTalkItem R;R.QueryMessage=TEXT("hi");R.ResponseText=TEXT("hello");Reply.Add(R);
 T->Goddess->ReceiveTalkItems(Reply);
 TestEqual(TEXT("Talk-only response starts reading without blessing delay"),T->Goddess->GetState(),EGoddessState::Reading);
 C.EnhanceTiers[0].Commands.Add(FParsedCommand());C.EnhanceTiers[2].Commands.Add(FParsedCommand());
 T->Goddess->BlessingDurations.Init(0.05f,4);T->Goddess->StartCycle(C);
 TestEqual(TEXT("First requested tier starts"),T->Goddess->GetState(),EGoddessState::Blessing1);
 ADD_LATENT_AUTOMATION_COMMAND(FTempleBlessingSequence(T,this));
 return true;
}
#endif
