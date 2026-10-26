#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Backend/YouTubeApiKeyPool.h"
#include "Scripts/Backend/ExternalData.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBroadcastOperatorTest,"Gladius.Broadcast.OperatorConfig",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBroadcastOperatorTest::RunTest(const FString& Parameters)
{
 auto* Instance=NewObject<UGameInstance>();
 auto* Admin=NewObject<UAdminSubsystem>(Instance);
 const FString Path=FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".json"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
 FFileHelper::SaveStringToFile(TEXT("{\"users\":[{\"userId\":\"former-admin\",\"permission\":\"admin\",\"eventBuff\":true},{\"userId\":\"viewer\",\"permission\":\"blocked\"}]}"),*Path);
 if(!TestTrue(TEXT("Permission fixture loads"),Admin->LoadFromFile(Path))) return false;
 auto Parse=[](const TCHAR* Json){ TSharedPtr<FJsonObject> Root; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root); return Root; };
 const auto Config=Parse(TEXT("{\"operators\":[{\"userId\":\"UC0000000000000000000001\",\"adminName\":\"운영자\"},{\"userId\":\"@pending\",\"adminName\":\"대기\"}]}"));
 TestTrue(TEXT("Configured operators apply"),Admin->ApplyBroadcastOperators(Config));
 TestTrue(TEXT("Explicit channel ID authenticates"),Admin->IsAdmin(TEXT("UC0000000000000000000001")));
 TestFalse(TEXT("Unverified handle never authenticates"),Admin->IsAdmin(TEXT("@pending")));
 TestFalse(TEXT("Removed operator loses permission"),Admin->IsAdmin(TEXT("former-admin")));
 TestTrue(TEXT("Event history is preserved"),Admin->HasEventBuff(TEXT("former-admin")));
 TestEqual(TEXT("Unrelated block remains"),Admin->FindUser(TEXT("viewer"))->Permission,FString(TEXT("blocked")));
 TestEqual(TEXT("Configured alias applies"),Admin->GetAdminName(TEXT("UC0000000000000000000001")),FString(TEXT("운영자")));
 TestFalse(TEXT("Duplicate operator IDs rejected"),Admin->ApplyBroadcastOperators(Parse(TEXT("{\"operators\":[{\"userId\":\"duplicate\"},{\"userId\":\"duplicate\"}]}"))));
 TestFalse(TEXT("Numeric user IDs are rejected without changing roles"),Admin->ApplyBroadcastOperators(Parse(TEXT(R"json({"operators":[{"userId":42}]})json"))));
 TestTrue(TEXT("Invalid config retains prior permission"),Admin->IsAdmin(TEXT("UC0000000000000000000001")));
 auto* Reload=NewObject<UAdminSubsystem>(Instance);
 TestTrue(TEXT("Updated permissions persist"),Reload->LoadFromFile(Path));
 TestTrue(TEXT("Reload retains configured admin"),Reload->IsAdmin(TEXT("UC0000000000000000000001")));
 TestTrue(TEXT("Empty list explicitly clears operators"),Admin->ApplyBroadcastOperators(Parse(TEXT("{\"operators\":[]}"))));
 TestFalse(TEXT("Former operator is ordinary user"),Admin->IsAdmin(TEXT("UC0000000000000000000001")));
 IFileManager::Get().Delete(*Path);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBroadcastKeysTest,"Gladius.Broadcast.YouTubeKeys",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBroadcastKeysTest::RunTest(const FString& Parameters)
{
 const FString Path=FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".json"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
 FFileHelper::SaveStringToFile(TEXT("{\"youtube\":{\"videoId\":\"video\",\"apiKeys\":[\" key-a \",\"key-b\",\"key-a\"],\"streamKey\":\"not-an-api-key\"}}"),*Path);
 FYouTubeApiKeyPool Pool;
 TestEqual(TEXT("JSON keys trim and deduplicate"),Pool.LoadFromFile(Path),2);
 TestEqual(TEXT("Only API keys are pooled"),Pool.Num(),2);
 TestEqual(TEXT("First API key selected"),Pool.GetCurrentKey(FDateTime::UtcNow()),FString(TEXT("key-a")));
 FFileHelper::SaveStringToFile(TEXT("{\"youtube\":{\"apiKeys\":[\"partial-key\",42]}}"),*Path);
 TestEqual(TEXT("Invalid list is rejected atomically"),Pool.LoadFromFile(Path),0);
 TestEqual(TEXT("Invalid list adds no partial keys"),Pool.Num(),2);
 TestEqual(TEXT("Production path is broadcast JSON"),FYouTubeApiKeyPool::GetDefaultKeyFilePath(),GladiusExternalData::File(TEXT("BroadcastSettings.json")));
 IFileManager::Get().Delete(*Path);
 return true;
}
#endif
