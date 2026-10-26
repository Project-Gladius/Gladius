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

struct FBroadcastReloadTestAccess
{
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
#endif
