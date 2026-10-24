#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/BattleHud/BattleTopHudTypes.h"
#include "BattleTopHudWidget.generated.h"

class ABattleManager;
class UProgressBar;
class UTextBlock;
class UWidget;

// 전투 화면 최상단 중앙에 고정되는 상단 UI
// [Turn 5/10] [안내 문구 + 남은 시간 게이지] [⏱ 30.00]
// 표시 내용은 FBattleTopHudPresenter가 전투 상태로부터 계산하고, 이 위젯은 그리기만 한다
// 외형은 Style로 바꾸며, WBP로 상속해 Style에 텍스처를 지정하면 기본 도형 대신 에셋이 쓰인다
UCLASS()
class GLADIUS_API UBattleTopHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToBattleManager(ABattleManager* InBattleManager);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Top HUD")
	FBattleTopHudStyle Style;

	//화면 상단 중앙 기준 위치(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD")
	FVector2D ScreenOffset = FVector2D(0.0f, 16.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient) TObjectPtr<UWidget> Root;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TurnValueText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TurnTotalText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GuideText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> Gauge;
	UPROPERTY(Transient) TObjectPtr<UWidget> TimerContent;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimerText;

	void BuildWidgetTree();
	void Apply(const FBattleTopHudState& State);
	UTextBlock* MakeText(int32 FontSize, const FLinearColor& Color, bool bBold = false);

	static void SetTextIfChanged(UTextBlock* Text, const FString& Value);
};
