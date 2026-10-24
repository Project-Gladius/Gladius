#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleDataResetWidget.generated.h"
class ABattleManager;
class UTextBlock;

// Temporary MainBattle development controls. Remove when the operator UI is ready.
UCLASS()
class GLADIUS_API UBattleDataResetWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetBattleManager(ABattleManager* Manager) { BattleManager = Manager; }
protected:
    virtual void NativeOnInitialized() override;
private:
    TWeakObjectPtr<ABattleManager> BattleManager;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
    UFUNCTION() void ResetAllClicked();
    UFUNCTION() void ResetTestsClicked();
    void Reset(bool bTestsOnly);
};
