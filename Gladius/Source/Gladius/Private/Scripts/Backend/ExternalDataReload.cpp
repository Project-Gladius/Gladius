#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Backend/ExternalData.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Scripts/UI/SessionLogSubsystem.h"

bool UAdminSubsystem::ReloadExternalData(ABattleManager* Battle, TArray<FString>& Failures)
{
 const FString Path = StoragePath;
 const bool bUsersLoaded = LoadFromFile(Path);
 if (!bUsersLoaded) Failures.Add(TEXT("UserData.json"));
 const auto Settings = GladiusExternalData::Broadcast();
 if (bUsersLoaded && !ApplyBroadcastOperators(Settings)) Failures.Add(TEXT("BroadcastSettings.json (operators)"));
 for (TActorIterator<AYouTubeChatManager> It(GetWorld()); It; ++It)
  if (!It->ReloadBroadcastSettings()) Failures.Add(TEXT("BroadcastSettings.json (youtube)"));
 if (!GetGameInstance()->GetSubsystem<UBattleScheduleSubsystem>()->ReloadSchedule()) Failures.Add(TEXT("BattleSchedule.csv"));
 if (Battle && !Battle->ReloadExternalFiles()) Failures.Add(TEXT("abilities/enhancements/bosses/PlayerData"));
 for (TActorIterator<ATempleCycleController> It(GetWorld()); It; ++It)
  if (!It->ReloadExternalConnections()) Failures.Add(TEXT("ConnectionSettings.json"));
 if (!FFileHelper::SaveStringToFile(FGuid::NewGuid().ToString(), *GladiusExternalData::File(TEXT(".reload-token")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
  Failures.Add(TEXT("TempleAI.env / goddess_profile.md"));
 return Failures.IsEmpty();
}

bool UAdminSubsystem::ExecutePatch(ABattleManager* Battle)
{
 TArray<FString> Failures;
 const bool bSuccess = ReloadExternalData(Battle, Failures);
 if (auto* Log = GetGameInstance()->GetSubsystem<USessionLogSubsystem>())
 {
  Log->AddMessage(bSuccess
   ? FString(TEXT("[패치] 방송 설정과 외부 데이터를 새로고침했습니다. 진행 중인 전투의 밸런스는 다음 전투부터, 여신 AI 설정은 다음 요청부터 적용됩니다."))
   : FString(TEXT("[패치] 새로고침 실패: ")) + FString::Join(Failures, TEXT(", ")),
   bSuccess ? ESessionLogEvent::Admin : ESessionLogEvent::AdminUrgent);
 }
 return bSuccess;
}
