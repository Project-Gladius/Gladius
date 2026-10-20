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
private:
 TWeakObjectPtr<ABattleManager> BoundBattleManager;
 FSubmitBattleAction SubmitBattleAction;
 ABattleManager* ResolveBattleManager();
};
