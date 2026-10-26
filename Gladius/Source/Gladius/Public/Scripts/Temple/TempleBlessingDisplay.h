#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "TempleBlessingDisplay.generated.h"

class ABattleManager;
class UWidgetComponent;
class UWidgetTree;
class UTextBlock;

UCLASS()
class GLADIUS_API UTempleBlessingCardWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blessing", meta=(ClampMin="1", ClampMax="4"))
 int32 BlessingTier = 1;

 void ShowBlessing(int32 Tier, ABattleManager* Manager = nullptr);
 static FText BlessingTitle(int32 Tier);
 static FText BlessingDescription(int32 Tier, const ABattleManager* Manager);

 UFUNCTION(BlueprintCallable, Category="Blessing|Editor")
 static bool BuildDesignerLayout(UObject* WidgetBlueprintAsset);
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativePreConstruct() override;
private:
 TWeakObjectPtr<ABattleManager> BattleManager;
 static void BuildLayout(UWidgetTree* Tree);
 void RefreshText();
};

// Four placed displays in MainTemple, each below its matching 3D icon.
UCLASS()
class GLADIUS_API ATempleBlessingDisplay : public AActor
{
 GENERATED_BODY()
public:
 ATempleBlessingDisplay();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blessing")
 TObjectPtr<UWidgetComponent> Card;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blessing", meta=(ClampMin="1",ClampMax="4"))
 int32 BlessingTier = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blessing")
 TSubclassOf<UTempleBlessingCardWidget> CardClass;
 UFUNCTION(BlueprintCallable, CallInEditor, Category="Blessing")
 void RefreshCard();
 UFUNCTION(BlueprintCallable, CallInEditor, Category="Blessing")
 void DrawEditorPreview();
protected:
 virtual void PostRegisterAllComponents() override;
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 void ShowAfterIntro();
};


