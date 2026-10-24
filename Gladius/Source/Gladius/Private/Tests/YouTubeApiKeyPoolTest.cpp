#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Scripts/Backend/YouTubeApiKeyPool.h"
#include "Scripts/Backend/YouTubeChatManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYouTubeApiKeyPoolTest,"Gladius.YouTube.ApiKeyPool.RotatesOnQuota",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FYouTubeApiKeyPoolTest::RunTest(const FString& Parameters)
{
 const FString Path=FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".txt"));
 FFileHelper::SaveStringToFile(TEXT("# comment\nkey-A\n\n  key-B  \nkey-A\nkey-C\n"),*Path);
 FYouTubeApiKeyPool Pool;
 TestEqual(TEXT("Comments, blanks and duplicates are skipped"),Pool.LoadFromFile(Path),3);
 IFileManager::Get().Delete(*Path);
 Pool.AddKey(TEXT("key-C"));
 TestEqual(TEXT("Level fallback key already in file is not added twice"),Pool.Num(),3);

 const FDateTime Now(2026,10,25,3,0,0); //03:00 UTC
 TestEqual(TEXT("First key is used first"),Pool.GetCurrentKey(Now),FString(TEXT("key-A")));
 TestTrue(TEXT("Next key remains"),Pool.MarkExhausted(TEXT("key-A"),Now));
 TestEqual(TEXT("Switches to second key"),Pool.GetCurrentKey(Now),FString(TEXT("key-B")));
 TestTrue(TEXT("Stale report of an old key does not skip the current one"),Pool.MarkExhausted(TEXT("key-A"),Now));
 TestEqual(TEXT("Still on second key"),Pool.GetCurrentKey(Now),FString(TEXT("key-B")));
 Pool.MarkExhausted(TEXT("key-B"),Now);
 TestEqual(TEXT("Third key"),Pool.GetCurrentKey(Now),FString(TEXT("key-C")));
 TestFalse(TEXT("No key left after all are exhausted"),Pool.MarkExhausted(TEXT("key-C"),Now));
 TestTrue(TEXT("Nothing available"),Pool.GetCurrentKey(Now).IsEmpty());
 TestEqual(TEXT("Still exhausted before reset"),Pool.NumAvailable(FDateTime(2026,10,25,7,59,0)),0);
 TestEqual(TEXT("All keys come back after quota reset"),Pool.NumAvailable(FDateTime(2026,10,25,8,0,0)),3);
 TestFalse(TEXT("Recovered key is usable"),Pool.GetCurrentKey(FDateTime(2026,10,25,8,0,0)).IsEmpty());

 TestEqual(TEXT("Reset later the same day"),FYouTubeApiKeyPool::NextQuotaReset(FDateTime(2026,10,25,3,0,0)),FDateTime(2026,10,25,8,0,0));
 TestEqual(TEXT("Reset next day after 08:00 UTC"),FYouTubeApiKeyPool::NextQuotaReset(FDateTime(2026,10,25,9,0,0)),FDateTime(2026,10,26,8,0,0));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYouTubeKeyErrorTest,"Gladius.YouTube.ApiKeyPool.DetectsKeyErrors",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FYouTubeKeyErrorTest::RunTest(const FString& Parameters)
{
 const FString Quota=TEXT("{\"error\":{\"code\":403,\"errors\":[{\"domain\":\"youtube.quota\",\"reason\":\"quotaExceeded\"}]}}");
 const FString BadKey=TEXT("{\"error\":{\"code\":400,\"message\":\"API key not valid.\",\"details\":[{\"reason\":\"API_KEY_INVALID\"}]}}");
 const FString Disabled=TEXT("{\"error\":{\"code\":403,\"errors\":[{\"reason\":\"accessNotConfigured\"}]}}");
 const FString ChatOff=TEXT("{\"error\":{\"code\":403,\"errors\":[{\"reason\":\"liveChatDisabled\"}]}}");
 TestTrue(TEXT("Quota exceeded switches key"),AYouTubeChatManager::IsKeyUnusableResponse(403,Quota));
 TestTrue(TEXT("Invalid key switches key"),AYouTubeChatManager::IsKeyUnusableResponse(400,BadKey));
 TestTrue(TEXT("API not enabled switches key"),AYouTubeChatManager::IsKeyUnusableResponse(403,Disabled));
 TestFalse(TEXT("Disabled live chat is not a key problem"),AYouTubeChatManager::IsKeyUnusableResponse(403,ChatOff));
 TestFalse(TEXT("Success is not a key error"),AYouTubeChatManager::IsKeyUnusableResponse(200,Quota));
 TestFalse(TEXT("Server error is not a key error"),AYouTubeChatManager::IsKeyUnusableResponse(500,Quota));
 return true;
}
#endif
