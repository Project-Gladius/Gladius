#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/Battle/BattleBalance.h"
#include "BattlePlayerNameplate.generated.h"

class UBorder;
class UTextBlock;
class UImage;
class USizeBox;
class UTexture2D;

USTRUCT(BlueprintType)
struct FBattlePlayerUIData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") FString CharacterId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") int32 HP = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") int32 MaxHP = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") int32 Fever = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") EBattleAction Action = EBattleAction::Idle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") int32 BaseDamage = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") int32 BonusDamage = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") float CritChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") float CritDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") float DefenseReduction = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Data") bool bFeverWillDrop = false;
};

// Position is relative to the top-left of the panel, in UMG layout units.
USTRUCT(BlueprintType)
struct FBattleUIPlacement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector2D Size = FVector2D(100.f, 20.f);
	FBattleUIPlacement() = default;
	FBattleUIPlacement(FVector2D InPosition, FVector2D InSize) : Position(InPosition), Size(InSize) {}
};

USTRUCT(BlueprintType)
struct FBattlePlayerUILayout
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement NameText = {FVector2D(8, 5), FVector2D(220, 24)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement HealthIcon = {FVector2D(8, 34), FVector2D(20, 20)};
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category="Layout") TArray<FBattleUIPlacement> HealthSegments;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement HealthText = {FVector2D(170, 34), FVector2D(100, 22)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement FeverIcon = {FVector2D(8, 62), FVector2D(20, 20)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement FeverText = {FVector2D(36, 62), FVector2D(130, 22)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement ActionText = {FVector2D(36, 90), FVector2D(230, 24)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement CritChanceIcon = {FVector2D(8, 122), FVector2D(20, 20)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement CritChanceText = {FVector2D(36, 122), FVector2D(80, 22)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement CritDamageIcon = {FVector2D(140, 122), FVector2D(20, 20)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FBattleUIPlacement CritDamageText = {FVector2D(168, 122), FVector2D(100, 22)};
	FBattlePlayerUILayout()
	{
		for (int32 Index=0; Index<6; ++Index)
			HealthSegments.Add(FBattleUIPlacement(FVector2D(36 + Index*21, 37), FVector2D(18, 14)));
	}
};

UCLASS(Blueprintable)
class GLADIUS_API UBattlePlayerNameplate : public UUserWidget
{
	GENERATED_BODY()
public:
	// One-time editor migration; never called by runtime widget initialization.
	UFUNCTION(BlueprintCallable, Category="Battle UI|Editor") static bool ConvertToDesignerBlueprint(UObject* Asset);
	// Legacy settings retained for one-time migration. Geometry now belongs to the Designer.
	UPROPERTY() FVector2D PanelSize = FVector2D(300, 200);
	// Multiplies the reference size and both distance limits without changing their ratios.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Distance Scale", meta=(ClampMin="0.01", UIMin="0.01", ToolTip="Overall size multiplier. Scales the reference size and the effective minimum/maximum together.")) float BaseDistanceScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Distance Scale", meta=(ClampMin="0.01")) float MinimumDistanceScale = 0.70f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Distance Scale", meta=(ClampMin="0.01")) float MaximumDistanceScale = 1.20f;
	UPROPERTY() FLinearColor PanelColor = FLinearColor(0.025f, 0.035f, 0.065f, 0.9f);
	UPROPERTY() FBattlePlayerUILayout DefaultLayout;
	// Missing actions use DefaultLayout. Add an entry for each action you want to customize.
	UPROPERTY() TMap<EBattleAction, FBattlePlayerUILayout> ActionLayouts;
	// Legacy seed placement; each Designer state now owns its action icon geometry.
	UPROPERTY() FBattleUIPlacement ActionIconPlacement = {FVector2D(8, 90), FVector2D(20, 20)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images") TObjectPtr<UTexture2D> HealthIconTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images") TObjectPtr<UTexture2D> HealthFilledTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images") TObjectPtr<UTexture2D> HealthEmptyTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images") TObjectPtr<UTexture2D> FeverIconTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images", meta=(DisplayName="Gladius Action Icons")) TMap<EBattleAction, TObjectPtr<UTexture2D>> GladiusActionIconTextures;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images", meta=(DisplayName="Sera Action Icons")) TMap<EBattleAction, TObjectPtr<UTexture2D>> SeraActionIconTextures;
	// Preserve previously saved common icons as a fallback, without a third editor field.
	UPROPERTY() TMap<EBattleAction, TObjectPtr<UTexture2D>> ActionIconTextures;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images") TObjectPtr<UTexture2D> CritChanceIconTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Images") TObjectPtr<UTexture2D> CritDamageIconTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle UI|Preview") FBattlePlayerUIData PreviewData;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Battle UI|Data") FBattlePlayerUIData Data;

	UFUNCTION(BlueprintCallable, Category="Battle|UI") void SetDisplayData(const FBattlePlayerUIData& InData);
	// Call after changing image assignments from a Blueprint graph at runtime.
	UFUNCTION(BlueprintCallable, Category="Battle|UI") void RefreshAppearance();
	UFUNCTION(BlueprintCallable, Category="Battle|UI") void SetPanelDimensions(FVector2D InSize);
	UFUNCTION(BlueprintPure, Category="Battle|UI") float GetDistanceScale(float Distance) const;
	UFUNCTION(BlueprintImplementableEvent, Category="Battle|UI") void OnDisplayDataChanged(const FBattlePlayerUIData& NewData);

protected:
	
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

private:
	friend struct FPlayerNameplateTestAccess;
	UPROPERTY(Transient) TObjectPtr<USizeBox> PanelSizeBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UImage> HealthIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> HealthSegments;
	UPROPERTY(Transient) TObjectPtr<UImage> FeverIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FeverText;
	UPROPERTY(Transient) TObjectPtr<UImage> ActionIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionText;
	UPROPERTY(Transient) TObjectPtr<UImage> CritChanceIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CritChanceText;
	UPROPERTY(Transient) TObjectPtr<UImage> CritDamageIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CritDamageText;
	void ResolveDesignerWidgets();
};
