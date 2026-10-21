#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/Battle/BattleBalance.h"
#include "BattlePlayerNameplate.generated.h"

class UBorder;
class UTextBlock;

// Temporary, asset-free player UI. A Widget Blueprint can subclass this later.
USTRUCT(BlueprintType)
struct FBattlePlayerUIData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) int32 HP = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxHP = 1;
	UPROPERTY(BlueprintReadOnly) int32 Fever = 0;
	UPROPERTY(BlueprintReadOnly) EBattleAction Action = EBattleAction::Idle;
	UPROPERTY(BlueprintReadOnly) int32 BaseDamage = 0;
	UPROPERTY(BlueprintReadOnly) int32 BonusDamage = 0;
	UPROPERTY(BlueprintReadOnly) float CritChance = 0.f;
	UPROPERTY(BlueprintReadOnly) float CritDamage = 0.f;
	UPROPERTY(BlueprintReadOnly) float DefenseReduction = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bFeverWillDrop = false;
};

UCLASS(Blueprintable)
class GLADIUS_API UBattlePlayerNameplate : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle|UI")
	void SetDisplayData(const FBattlePlayerUIData& InData);

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|UI")
	void OnDisplayDataChanged(const FBattlePlayerUIData& NewData);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FeverText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> HealthSegments;
	FBattlePlayerUIData Data;

	void BuildDefaultWidgetTree();
	void Refresh();
};
