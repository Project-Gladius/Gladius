#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BattleCameraTestAccess.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
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
#include "Scripts/Temple/TempleCommandQueue.h"
#include "Scripts/UI/ChatLogSubsystem.h"

struct FTempleFlowTestAccess
{
 static void Cycle(ATempleCycleController* C) { C->HandleCycleTimer(); }
 static void AllowAllBlessings(ATempleCycleController* C) { C->CommandQueue->AvailableBless = 4; }
 static bool Leaving(ATempleCycleController* C) { return C->bLeavingTemple; }
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
 ABattleManager* Records;
 FTempleTestWorld()
 {
  Instance->AddToRoot(); Instance->InitializeStandalone(); World=Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
  World->EnableActorTickAndUserCallbacks(true);
#endif
  World->InitializeActorsForPlay(FURL());
  World->SpawnActor<AGameManager>();
  Records=World->SpawnActor<ABattleManager>();
  FBattleCameraTestAccess::RecordsOnly(Records);
  Records->DispatchBeginPlay();
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
  FBattleCameraTestAccess::Cleanup(World);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleManualFlowTest,"Gladius.Temple.Flow.UnlimitedCycles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleManualFlowTest::RunTest(const FString& Parameters)
{
 FTempleTestWorld T;
 for(int32 I=0;I<8;++I)FTempleFlowTestAccess::Cycle(T.Controller);
 TestEqual(TEXT("More than five cycles can run"),T.Controller->GetCompletedCycles(),8);
 TestFalse(TEXT("Cycles never request combat automatically"),FTempleFlowTestAccess::Leaving(T.Controller));
 T.Controller->RequestCombatTravel();
 TestTrue(TEXT("Manager takeover request pauses temple"),FTempleFlowTestAccess::Leaving(T.Controller));
 TestEqual(TEXT("Cycles stop after manual request"),T.Controller->GetCompletedCycles(),8);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatLogIntegrationTest,"Gladius.Temple.Flow.ChatLogIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FChatLogIntegrationTest::RunTest(const FString& Parameters)
{
 FTempleTestWorld T;
 T.Chat->MaxAllowedComments=100;
 UChatLogSubsystem* Log=T.Instance->GetSubsystem<UChatLogSubsystem>();
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("normal-1"),TEXT("viewer-1"),TEXT("시청자"),TEXT("안녕하세요"));
 T.Chat->OnReceiveYouTubeCommentWithUsername(TEXT("command-1"),TEXT("viewer-1"),TEXT("시청자"),TEXT("!대화 오늘 어때요?"));
 TestEqual(TEXT("Only ordinary chat appears immediately"),Log->GetEntries().Num(),1);
 if(Log->GetEntries().Num()!=1)return false;
 TestEqual(TEXT("Display name comes from YouTube"),Log->GetEntries()[0].DisplayName,FString(TEXT("시청자")));

 FGoddessStateContext Reading;
 Reading.Username=TEXT("시청자");Reading.Text=TEXT("오늘 어때요?");
 T.Goddess->OnStateChanged.Broadcast(EGoddessState::Reading,Reading);
 FGoddessStateContext Speaking;
 Speaking.Username=TEXT("시청자");Speaking.Text=TEXT("좋은 하루예요.");
 T.Goddess->OnStateChanged.Broadcast(EGoddessState::Speaking,Speaking);
 TestEqual(TEXT("Question and AI answer follow the ordinary chat"),Log->GetEntries().Num(),3);
 if(Log->GetEntries().Num()==3)
 {
  TestEqual(TEXT("The command prefix is hidden"),Log->GetEntries()[1].Message,FString(TEXT("오늘 어때요?")));
  TestEqual(TEXT("AI response uses the goddess name"),Log->GetEntries()[2].DisplayName,FString(TEXT("아우렐리아")));
  TestEqual(TEXT("AI response text is preserved"),Log->GetEntries()[2].Message,FString(TEXT("좋은 하루예요.")));
 }
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
namespace
{
class FTempleEnhancementAssertions : public IAutomationLatentCommand
{
 TSharedPtr<FTempleTestWorld> T;
 FAutomationTestBase* Test;
 int32 Frames=0;
public:
 FTempleEnhancementAssertions(TSharedPtr<FTempleTestWorld> In, FAutomationTestBase* InTest):T(In),Test(InTest){}
 bool Update() override
 {
  T->World->GetTimerManager().Tick(0.1f);
  if (T->Goddess->GetState()!=EGoddessState::Idle)
  {
   if (++Frames>30) { Test->AddError(TEXT("Blessing execution did not complete")); return true; }
   return false;
  }
  const FPlayerRecord& R=FBattleCameraTestAccess::Record(T->Records,TEXT("temple-viewer"));
  Test->TestEqual(TEXT("Healing blessing restores current health"),R.CurrentHealth,T->Records->MaxHPFor(R));
  Test->TestEqual(TEXT("Recovery does not silently change restored max HP bonus"),R.MaxHPBonus,2);
  Test->TestEqual(TEXT("Strength increases damage level once"),R.DamageBonus,1);
  Test->TestEqual(TEXT("Agility increases crit chance level once"),R.CritChanceBonus,1);
  Test->TestEqual(TEXT("Sharpness increases crit damage level once"),R.CritDamageBonus,1);
  const TArray<FChatLogEntry>& Entries=T->Instance->GetSubsystem<UChatLogSubsystem>()->GetEntries();
  Test->TestEqual(TEXT("Only applied blessings enter the chat log"),Entries.Num(),4);
  if(Entries.Num()==4)
  {
   for(int32 Index=0;Index<4;++Index)
   {
    Test->TestEqual(TEXT("Blessing appears in tier order"),Entries[Index].EnhancementTier,Index+1);
    Test->TestEqual(TEXT("Blessing is a temple result, not a raw command"),Entries[Index].Speaker,EChatLogSpeaker::Temple);
    Test->TestFalse(TEXT("Raw command prefix is hidden"),Entries[Index].Message.Contains(TEXT("!강화")));
   }
  }
  FString Json;FFileHelper::LoadFileToString(Json,*FBattleCameraTestAccess::Path(T->Records));
  FPlayerRecordFile Saved;FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Saved);
  Test->TestEqual(TEXT("Completed blessings persisted one user"),Saved.Records.Num(),1);
  if(Saved.Records.Num())
  {
   Test->TestEqual(TEXT("JSON stores restored maximum HP level"),Saved.Records[0].MaxHPBonus,2);
   Test->TestEqual(TEXT("JSON stores healed current HP"),Saved.Records[0].CurrentHealth,R.CurrentHealth);
   Test->TestEqual(TEXT("JSON stores enhancement result"),Saved.Records[0].CritDamageBonus,1);
  }
  auto* Next=T->World->SpawnActor<ABattleManager>();
  FBattleCameraTestAccess::RecordsOnly(Next);
  FBattleCameraTestAccess::UsePath(Next,FBattleCameraTestAccess::Path(T->Records));
  Next->InitializePlayerRecords();
  FBattleCameraTestAccess::BeginPrepared(Next);
  Test->TestTrue(TEXT("Next battle can join with temple result"),Next->SubmitAction(TEXT("temple-viewer"),TEXT("Temple user"),EBattleAction::Attack1));
  if(const FBattlePlayer* P=Next->GetBattlePlayers().Find(TEXT("temple-viewer")))
  {
   Test->TestEqual(TEXT("Next battle maximum includes MaxHPBonus"),P->MaxHP,FMath::Clamp(Next->Classes[0].BaseHP+2*Next->HPPerBonus, 5, 6));
   Test->TestEqual(TEXT("Next battle starts with stored healed HP"),P->HP,R.CurrentHealth);
   Test->TestEqual(TEXT("Next battle applies strength"),P->DamageBonus,Next->DamagePerBonus);
   Test->TestEqual(TEXT("Next battle applies agility"),P->CritChanceBonus,Next->CritChancePerBonus);
   Test->TestEqual(TEXT("Next battle applies sharpness"),P->CritDamageBonus,Next->CritDamagePerBonus);
  }
  return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleEnhancementIntegrationTest,"Gladius.Temple.Flow.YouTubeBlessingsPersistToBattle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleEnhancementIntegrationTest::RunTest(const FString& Parameters)
{
 auto T=MakeShared<FTempleTestWorld>();
 T->Records->BlessingHealAmount=2;
 TestFalse(TEXT("Temple record manager never starts battle"),T->Records->IsBattleRunning());
 T->Records->ApplyTempleEnhancement(TEXT("temple-viewer"),TEXT("Temple user"),2);
 auto& R=FBattleCameraTestAccess::Record(T->Records,TEXT("temple-viewer"));
 R.MaxHPBonus=2;R.DamageBonus=0;R.CurrentHealth=T->Records->MaxHPFor(R)-2;
 T->Records->SaveRecords();
 T->Chat->MaxAllowedComments=100;
 FTempleFlowTestAccess::AllowAllBlessings(T->Controller);
 for(int32 Tier=1;Tier<=4;++Tier)
  T->Chat->OnReceiveYouTubeCommentWithUsername(FString::Printf(TEXT("blessing-%d"),Tier),TEXT("temple-viewer"),TEXT("Temple user"),FString::Printf(TEXT("!강화 %d"),Tier));
 TestEqual(TEXT("Queueing alone does not apply strength"),R.DamageBonus,0);
 TestEqual(TEXT("Pending commands are not shown as applied"),T->Instance->GetSubsystem<UChatLogSubsystem>()->GetEntries().Num(),0);
 T->Goddess->BlessingDurations.Init(0.01f,4);
 FTempleFlowTestAccess::Cycle(T->Controller);
 ADD_LATENT_AUTOMATION_COMMAND(FTempleEnhancementAssertions(T,this));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleBlessLimitTest,"Gladius.Temple.Flow.BlessingSelectionLimit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleBlessLimitTest::RunTest(const FString& Parameters)
{
 auto* D=NewObject<UCommandDispatcher>(); D->Initialize();
 auto* Q=NewObject<UTempleCommandQueue>(); Q->Bind(D);
 TestEqual(TEXT("Default blessing allowance"),Q->AvailableBless,3);
 auto Select=[D](const TCHAR* User,int32 Tier) { D->OnEnhanceCommand.Broadcast(User,User,FString::FromInt(Tier)); };
 Select(TEXT("A"),1); Select(TEXT("A"),1); Select(TEXT("A"),3); Select(TEXT("A"),4);
 auto First=Q->TakeSnapshot();
 TestEqual(TEXT("Repeated choices count"),First.EnhanceTiers[0].Commands.Num(),2);
 TestEqual(TEXT("Third choice accepted"),First.EnhanceTiers[2].Commands.Num(),1);
 TestTrue(TEXT("Fourth choice ignored"),First.EnhanceTiers[3].Commands.IsEmpty());
 Select(TEXT("A"),2); Select(TEXT("B"),2);
 auto Next=Q->TakeSnapshot();
 TestEqual(TEXT("Allowance survives cycles and is independent per user"),Next.EnhanceTiers[1].Commands.Num(),1);
 TestEqual(TEXT("Only second user accepted"),Next.EnhanceTiers[1].Commands[0].UserId,FString(TEXT("B")));
 Q->Unbind();
 return true;
}

#endif
