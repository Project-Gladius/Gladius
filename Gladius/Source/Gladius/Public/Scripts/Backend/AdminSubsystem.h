#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "GladiusTypes.h"
#include "AdminSubsystem.generated.h"
class ABattleManager;
class FJsonObject;
USTRUCT()
struct FUserPermissionRecord
{
 GENERATED_BODY()
 UPROPERTY() FString UserId;
 UPROPERTY() FString ChannelId;
 UPROPERTY() FString DisplayName;
 UPROPERTY() FString Permission = TEXT("user");
 UPROPERTY() FString AdminName;
 UPROPERTY() FString BlockTime;
 UPROPERTY() bool EventBuff = false;
};
USTRUCT()
struct FUserPermissionFile
{
 GENERATED_BODY()
 UPROPERTY() TArray<FUserPermissionRecord> Users;
};
UCLASS()
class GLADIUS_API UAdminSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 // Explicit IDs and API-verified handle mappings authenticate; display names and unresolved handles never do.
 bool IsAdmin(const FString& Id) const;
 bool IsExhibit(const FString& Id) const;
 bool IsLeaderboardExcluded(const FString& Id) const;
 bool ReloadExternalData(ABattleManager* Battle, TArray<FString>& Failures);
 // Shared operation and result messages for authenticated chat and the local operator hotkey.
 bool ExecutePatch(ABattleManager* Battle);
 bool IsBlocked(const FString& Id, const FDateTime& Now) const;
 bool CanInput(const FString& Id, EGameState State) const;
 bool HasEventBuff(const FString& Id) const;
 bool IsAIMaintenance() const { return bAI; }
 FString GetAdminName(const FString& Id) const;
 FString CanonicalId(const FString& Id) const;
 // Display-name targeting never changes authentication: return one canonical account or reject ambiguity.
 bool ResolveUserTarget(const FString& Target, const ABattleManager* Battle,
  FString& OutId, FString& OutName, bool& bAmbiguous) const;
 void RegisterUser(const FString& Id, const FString& Name);
 bool Block(const FString& Id, const FDateTime& Until);
 bool Unblock(const FString& Id);
 bool GrantEvent(const FString& Id);
 void ResolveAdminHandles(const FString& ApiKey);
 bool HandleAdminCommand(const FParsedCommand& Command, ABattleManager* Battle);
 bool SetMaintenance(const FString& Scope, bool bEnabled);
 static bool IsAdminCommand(ECommandType Type);
 bool LoadFromFile(const FString& Path);
 const FUserPermissionRecord* FindUser(const FString& Id) const;
private:
 friend struct FAdminTestAccess;
 friend class FBroadcastOperatorTest;
 UPROPERTY() FUserPermissionFile Data;
 FString StoragePath;
 bool bValid = false, bTemporary = false;
 bool bBattle = false, bTemple = false, bAll = false, bAI = false;
 TMap<FString, FHttpRequestPtr> HandleRequests;
 int32 FindIndex(const FString& Id) const;
 bool IsReservedAdmin(const FString& Id) const;
 bool Commit(FUserPermissionFile Next);
 bool ApplyBroadcastOperators(const TSharedPtr<FJsonObject>& Settings);
 bool BindVerifiedHandle(const FString& Handle, const FString& ChannelId);
};
