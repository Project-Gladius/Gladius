#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"
#include "LeaderboardRowWidget.generated.h"

class UBorder;
class UTextBlock;
class UWidget;
class UWidgetTree;

// 리더보드 한 줄의 로직 (순위/메달, 사용자 ID, 캐릭터, 체력, 데미지)
// 보드가 이 위젯을 10개 만들어 RowContainer에 넣는다
// - 디자이너용 WBP(/Game/Blueprints/UI/WBP_LeaderboardRow): 한 줄의 디자인을 Designer에서 꾸미면 10줄 모두에 적용
// - 실행 중 바뀌는 외형(1~3위 메달, 1위 줄 강조, 체력 0 흐림, 1위 데미지 색)은 Class Defaults의 Style에서 지정
UCLASS(Blueprintable)
class GLADIUS_API ULeaderboardRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaderboard|Preview")
	bool bPreviewShowHealth = true;

	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void SetRow(const FLeaderboardRow& Row, bool bShowHealth);

	//줄 내용이 바뀔 때 호출. 디자이너가 애니메이션/추가 연출을 붙이는 용도
	UFUNCTION(BlueprintImplementableEvent, Category = "Leaderboard")
	void OnRowChanged(const FLeaderboardRow& Row, bool bShowHealth);

	//아래 BindWidgetOptional 위젯들을 같은 이름으로 Tree에 만든다 (C++ 단독 사용, WBP 자동 생성 공용)
	void BuildDefaultLayout(UWidgetTree* Tree) const;

	//메달·1위 강조·흐림 등 실행 중 바뀌는 외형 + 기본 레이아웃용 크기·폰트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaderboard")
	FLeaderboardStyle Style;

	//1위 줄 배경을 Style.FirstRowBrush로 바꿀지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	bool bHighlightFirstRow = true;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	// ===== 디자이너가 같은 이름으로 배치해야 하는 위젯 =====

	//줄 배경 (1위일 때 FirstRowBrush로 바뀜)
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RowBackground;

	//순위 배경 (1~3위는 MedalBrushes, 그 외는 비움)
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Medal;

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankText;

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CharacterText;

	//체력 칸 전체 (LEADERBOARD에서는 접힘)
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HealthCell;

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageText;

private:
	//Designer에서 지정한 원래 줄 배경/데미지 색 (1위가 아닐 때 되돌릴 값)
	FSlateBrush DesignerRowBrush;
	FSlateColor DesignerDamageColor;
	bool bCapturedDesignerLook = false;

	void ResolveWidgets();
	void CaptureDesignerLook();
};
