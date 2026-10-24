#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/BattleHud/BattleTopHudTypes.h"
#include "BattleTopHudWidget.generated.h"

class ABattleManager;
class UProgressBar;
class UTextBlock;
class UWidget;
class UWidgetTree;

// 전투 화면 최상단 중앙 상단 UI의 로직
// [Turn 5/10] [안내 문구 + 남은 시간 게이지] [⏱ 30.00]
// 표시 내용은 FBattleTopHudPresenter가 계산하고, 이 클래스는 아래 이름의 위젯에 값을 넣기만 한다
// - 디자이너용 WBP(/Game/Blueprints/UI/WBP_TopHud): Designer에서 아래 이름의 위젯을 자유롭게 배치·꾸밈
// - WBP 없이 C++ 클래스로 쓰면 BuildDefaultLayout으로 기본 도형 레이아웃을 만든다
UCLASS(Blueprintable)
class GLADIUS_API UBattleTopHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToBattleManager(ABattleManager* InBattleManager);

	//아래 BindWidgetOptional 위젯들을 같은 이름으로 Tree에 만든다 (C++ 단독 사용, WBP 자동 생성 공용)
	void BuildDefaultLayout(UWidgetTree* Tree) const;

	//기본 레이아웃(BuildDefaultLayout)에만 쓰이는 외형. WBP에서는 Designer에서 직접 꾸민다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Top HUD|Default Layout")
	FBattleTopHudStyle Style;

	//기본 레이아웃의 화면 상단 중앙 기준 위치(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Default Layout")
	FVector2D ScreenOffset = FVector2D(0.0f, 16.0f);

	//표시 상태(보임/문구/시간·게이지 표시 여부)가 바뀔 때 호출. 디자이너가 UMG 애니메이션을 붙이는 용도
	UFUNCTION(BlueprintImplementableEvent, Category = "Top HUD")
	void OnHudStateChanged(bool bVisible, const FString& NewGuideText, bool bShowTimer, bool bShowGauge);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== 디자이너가 같은 이름으로 배치해야 하는 위젯 (없으면 해당 기능만 빠짐) =====

	//상단 UI 전체. 숨길 때 이 위젯을 접는다 (없으면 위젯 전체를 숨김)
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TopHudBar;

	//현재 턴 숫자 "5"
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnValueText;

	//전체 턴 "/10"
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnTotalText;

	//안내 문구
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GuideText;

	//남은 시간 게이지 (Percent만 바꿈. 배경/채움 이미지는 Designer에서 지정)
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Gauge;

	//남은 시간 영역(아이콘+숫자). 시간을 숨길 때 이 위젯을 숨김
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TimerContent;

	//남은 시간 "30.00"
	UPROPERTY(BlueprintReadOnly, Category = "Top HUD|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

private:
	TWeakObjectPtr<ABattleManager> BattleManager;

	bool bHasLastState = false;
	FBattleTopHudState LastState;

	void ResolveWidgets();
	void Apply(const FBattleTopHudState& State);

	static void SetTextIfChanged(UTextBlock* Text, const FString& Value);
};
