#pragma once
#include "CoreMinimal.h"
#include "GladiusTypes.h"
#include "Scripts/Battle/BattleUnit.h"
#include "CommandDispatcher.generated.h"

class ABattleManager;
DECLARE_DELEGATE_RetVal_FourParams(bool, FSubmitBattleAction, const FString&, const FString&, EBattleAction, const FString&);
DECLARE_DELEGATE_RetVal(EGameState, FGetGameStateDelegate);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEnhanceCommand, const FString&, const FString&, const FString&);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnTalkCommand, const FString&, const FString&, const FString&, const FString&);
DECLARE_MULTICAST_DELEGATE(FOnTempleCombatCommand);
// !로그인 <캐릭터>: UserId, Username, CharacterName
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnLoginCommand, const FString&, const FString&, const FString&);
// !로그아웃: UserId
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLogoutCommand, const FString&);

UCLASS()
class GLADIUS_API UCommandDispatcher : public UObject
{
 GENERATED_BODY()
public:
 void Initialize() {}
 void Execute(const FParsedCommand& Command);
 void BindBattleManager(ABattleManager* Manager);
 void UnbindBattleManager(ABattleManager* Manager);
 FGetGameStateDelegate GetGameStateDelegate;
 FOnEnhanceCommand OnEnhanceCommand;
 FOnTalkCommand OnTalkCommand;
 FOnTempleCombatCommand OnTempleCombatCommand;
 //전투 참가/퇴장 명령. BindBattleManager로 연결된 BattleManager가 구독
 FOnLoginCommand OnLoginCommand;
 FOnLogoutCommand OnLogoutCommand;
private:
 TWeakObjectPtr<ABattleManager> BoundBattleManager;
 FDelegateHandle LoginHandle;
 FDelegateHandle LogoutHandle;
 FSubmitBattleAction SubmitBattleAction;
 ABattleManager* ResolveBattleManager();
};
