#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Scripts/Backend/AdminSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Scripts/Backend/GameManager.h"

struct FBroadcastReloadTestAccess
{
 static void Combat(AYouTubeChatManager* A, AGameManager* Game){A->GameManagerRef=Game;Game->SetGameState(EGameState::Combat);}
 static void Finish(AYouTubeChatManager* A,bool Success,float Delay){A->FinishFetch(Success,Delay);}
 static bool Polling(AYouTubeChatManager* A){return A->GetWorldTimerManager().IsTimerActive(A->ChatFetchTimerHandle);}
 static float PollRate(AYouTubeChatManager* A){return A->GetWorldTimerManager().GetTimerRate(A->ChatFetchTimerHandle);}
 static void LiveIdFailure(AYouTubeChatManager* A){A->bLiveChatIdInFlight=true;A->OnLiveChatIdResponse(nullptr,nullptr,false,TEXT(""));}
 static void BindPatch(AYouTubeChatManager* A){A->BindLocalPatchKey();}
 static void Path(AYouTubeChatManager* A,const FString& Path){A->BroadcastSettingsOverridePath=Path;}
 static void Reload(AYouTubeChatManager* A){A->RefreshBroadcastSettings();}
 static int32 Count(AYouTubeChatManager* A){return A->KeyPool->Num();}
 static bool Waiting(AYouTubeChatManager* A){return A->GetWorldTimerManager().IsTimerActive(A->BroadcastRefreshTimerHandle);}
 static float Rate(AYouTubeChatManager* A){return A->GetWorldTimerManager().GetTimerRate(A->BroadcastRefreshTimerHandle);}
 static bool Stream(AYouTubeChatManager* A){return A->BroadcastStreamKey==TEXT("test-stream");}
 static void Exhaust(AYouTubeChatManager* A){A->KeyPool->MarkExhausted(TEXT("test-key"),FDateTime::UtcNow());}
 static bool UsesSpare(AYouTubeChatManager* A){return A->KeyPool->GetCurrentKey(FDateTime::UtcNow())==TEXT("test-spare");}
};
namespace
{
struct FRefreshWorld
{
 UGameInstance* Instance=NewObject<UGameInstance>(GEngine);
 UWorld* World=nullptr;
 AYouTubeChatManager* Chat=nullptr;
 FString Path=FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".json"));
 FRefreshWorld()
 {
  Instance->AddToRoot(); Instance->InitializeStandalone(); World=Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
  World->EnableActorTickAndUserCallbacks(true);
#endif
  World->InitializeActorsForPlay(FURL());
  Chat=World->SpawnActor<AYouTubeChatManager>();
  FBroadcastReloadTestAccess::Path(Chat,Path);
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
 }
 void Write(const TCHAR* Json){FFileHelper::SaveStringToFile(Json,*Path);}
 ~FRefreshWorld()
 {
  Chat->Destroy(); Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot(); IFileManager::Get().Delete(*Path);
 }
};
class FRefreshTimingAssertions : public IAutomationLatentCommand
{
 TSharedPtr<FRefreshWorld> T; FAutomationTestBase* Test; int32 Step=0;
public:
 FRefreshTimingAssertions(TSharedPtr<FRefreshWorld> In,FAutomationTestBase* InTest):T(In),Test(InTest){}
 bool Update() override
 {
  auto* A=T->Chat;
  switch(Step++)
  {
  case 0:
   T->World->GetTimerManager().Tick(29.f);
   Test->TestEqual(TEXT("No reload before thirty seconds"),FBroadcastReloadTestAccess::Count(A),0);
   return false;
  case 1:
   T->World->GetTimerManager().Tick(1.01f);
   Test->TestEqual(TEXT("API key filled in file is loaded at thirty seconds"),FBroadcastReloadTestAccess::Count(A),1);
   Test->TestTrue(TEXT("Missing stream key keeps refresh running"),FBroadcastReloadTestAccess::Waiting(A));
   FBroadcastReloadTestAccess::Exhaust(A);
   T->Write(TEXT(R"json({"youtube":{"apiKeys":["test-key","test-spare"],"streamKey":"test-stream"}})json"));
   return false;
  case 2:
   T->World->GetTimerManager().Tick(29.f);
   Test->TestEqual(TEXT("Second reload waits for its interval"),FBroadcastReloadTestAccess::Count(A),1);
   return false;
  case 3:
   T->World->GetTimerManager().Tick(1.f);
   Test->TestEqual(TEXT("New keys applied on next interval"),FBroadcastReloadTestAccess::Count(A),2);
   Test->TestTrue(TEXT("Stream key applied"),FBroadcastReloadTestAccess::Stream(A));
   Test->TestTrue(TEXT("Quota exhaustion survives reload"),FBroadcastReloadTestAccess::UsesSpare(A));
   Test->TestFalse(TEXT("Refresh stops once both keys are filled"),FBroadcastReloadTestAccess::Waiting(A));
   T->Write(TEXT(R"json({"youtube":{"apiKeys":["other"],"streamKey":"other"}})json"));
   return false;
  default:
   T->World->GetTimerManager().Tick(35.f);
   Test->TestEqual(TEXT("Complete settings are not polled again"),FBroadcastReloadTestAccess::Count(A),2);
   T->Write(TEXT("{")); FBroadcastReloadTestAccess::Reload(A);
   Test->TestEqual(TEXT("Partial JSON edit preserves working keys"),FBroadcastReloadTestAccess::Count(A),2);
   Test->TestTrue(TEXT("Partial file edit is retried"),FBroadcastReloadTestAccess::Waiting(A));
   T->Write(TEXT(R"json({"youtube":{"apiKeys":[],"streamKey":"test-stream"}})json")); FBroadcastReloadTestAccess::Reload(A);
   Test->TestEqual(TEXT("Empty API list applies"),FBroadcastReloadTestAccess::Count(A),0);
   Test->TestTrue(TEXT("Missing API keys alone trigger retry"),FBroadcastReloadTestAccess::Waiting(A));
   return true;
  }
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBroadcastReloadTimerTest,"Gladius.Broadcast.MissingKeysReloadEveryThirtySeconds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBroadcastReloadTimerTest::RunTest(const FString& Parameters)
{
 auto T=MakeShared<FRefreshWorld>();
 T->Write(TEXT(R"json({"youtube":{"apiKeys":["  "],"streamKey":"  "}})json"));
 T->World->GetTimerManager().Tick(0.f);
 FBroadcastReloadTestAccess::Reload(T->Chat);
 TestTrue(TEXT("Blank keys start periodic file reload"),FBroadcastReloadTestAccess::Waiting(T->Chat));
 TestEqual(TEXT("Reload interval is thirty seconds"),FBroadcastReloadTestAccess::Rate(T->Chat),30.f);
 T->Write(TEXT(R"json({"youtube":{"apiKeys":["test-key"],"streamKey":""}})json"));
 ADD_LATENT_AUTOMATION_COMMAND(FRefreshTimingAssertions(T,this));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalPatchBindingTest,"Gladius.Broadcast.LocalPatchHotkeyBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLocalPatchBindingTest::RunTest(const FString& Parameters)
{
 FRefreshWorld T;
 FBroadcastReloadTestAccess::BindPatch(T.Chat);
 auto* Controller=T.World->SpawnActor<APlayerController>();
 auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);
 T.Instance->AddLocalPlayer(LocalPlayer,FPlatformUserId::CreateFromInternalId(0));
 Controller->SetPlayer(LocalPlayer);
 for(int32 I=0;I<5;++I) FBroadcastReloadTestAccess::BindPatch(T.Chat);
 if(!TestNotNull(TEXT("Input registers after controller appears"),T.Chat->InputComponent.Get())) return false;
 int32 Count=0;
 for(const auto& Key:T.Chat->InputComponent->KeyBindings)
  if(Key.Chord.Key==EKeys::P)
  {
   ++Count;
   TestEqual(TEXT("P triggers once per press"),Key.KeyEvent,IE_Pressed);
   TestFalse(TEXT("Patch binding does not consume other input"),bool(Key.bConsumeInput));
   TestTrue(TEXT("Local operator can patch while paused"),bool(Key.bExecuteWhenPaused));
   TestTrue(TEXT("P has an executable handler"),Key.KeyDelegate.IsBound());
  }
 TestEqual(TEXT("Repeated ticks do not duplicate P"),Count,1);
 T.Instance->GetSubsystem<UAdminSubsystem>()->SetMaintenance(TEXT("전체"),true);
 FBroadcastReloadTestAccess::BindPatch(T.Chat);
 TestEqual(TEXT("Maintenance does not remove local recovery key"),T.Chat->InputComponent->KeyBindings.Num(),1);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatPollingRecoveryTest,"Gladius.Broadcast.CombatPollingRecoversFromFailures",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FChatPollingRecoveryTest::RunTest(const FString& Parameters)
{
 FRefreshWorld T;
 auto* Game=T.World->SpawnActor<AGameManager>();
 FBroadcastReloadTestAccess::Combat(T.Chat,Game);
 FBroadcastReloadTestAccess::Finish(T.Chat,false,5.f);
 TestTrue(TEXT("Transient failure schedules another fetch"),FBroadcastReloadTestAccess::Polling(T.Chat));
 TestEqual(TEXT("First failure backs off"),FBroadcastReloadTestAccess::PollRate(T.Chat),10.f);
 FBroadcastReloadTestAccess::Finish(T.Chat,false,5.f);
 TestEqual(TEXT("Repeated failures increase retry delay"),FBroadcastReloadTestAccess::PollRate(T.Chat),20.f);
 FBroadcastReloadTestAccess::Finish(T.Chat,true,7.f);
 T.Chat->SetCombatAutoPollingEnabled(true);
 TestEqual(TEXT("Resume preserves the API minimum interval"),FBroadcastReloadTestAccess::PollRate(T.Chat),7.f);
 FBroadcastReloadTestAccess::Finish(T.Chat,false,5.f);
 TestEqual(TEXT("Successful response resets failure backoff"),FBroadcastReloadTestAccess::PollRate(T.Chat),10.f);
 AddExpectedError(TEXT("LiveChatID 요청 실패"),EAutomationExpectedErrorFlags::Contains,1);
 FBroadcastReloadTestAccess::LiveIdFailure(T.Chat);
 TestEqual(TEXT("Initial chat discovery retries after network failure"),FBroadcastReloadTestAccess::PollRate(T.Chat),30.f);
 T.Chat->SetCombatAutoPollingEnabled(false);
 FBroadcastReloadTestAccess::Finish(T.Chat,false,5.f);
 TestFalse(TEXT("Cutscenes cannot restart polling from a late response"),FBroadcastReloadTestAccess::Polling(T.Chat));
 Game->SetGameState(EGameState::Temple);
 T.Chat->SetCombatAutoPollingEnabled(true);
 FBroadcastReloadTestAccess::Finish(T.Chat,false,5.f);
 TestFalse(TEXT("Temple remains controlled by its own cycle"),FBroadcastReloadTestAccess::Polling(T.Chat));
 return true;
}
#endif
