#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"
#include "LeaderboardBoardWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;

// 리더보드 보드 한 장(제목, 열 머리글, 10줄)을 그리는 위젯. 데이터는 SetPage로 받기만 한다
// 외형은 Style로 바꾸며, WBP로 상속해 Style에 텍스처를 지정하면 기본 도형 대신 에셋이 쓰인다
UCLASS()
class GLADIUS_API ULeaderboardBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void SetPage(const FLeaderboardPage& Page);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaderboard")
	FLeaderboardStyle Style;

protected:
	virtual void NativeOnInitialized() override;

private:
	//한 줄을 구성하는 위젯들
	struct FRowWidgets
	{
		TObjectPtr<UBorder> Background;
		TObjectPtr<UBorder> Medal;
		TObjectPtr<UTextBlock> Rank;
		TObjectPtr<UTextBlock> Name;
		TObjectPtr<UTextBlock> Character;
		TObjectPtr<USizeBox> HealthCell;
		TObjectPtr<UTextBlock> Health;
		TObjectPtr<UTextBlock> Damage;
	};

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HealthHeaderCell;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DamageHeaderText;

	//GC가 위젯을 지우지 않도록 WidgetTree가 소유하므로 여기서는 참조만 보관
	TArray<FRowWidgets> RowWidgets;

	void BuildWidgetTree();
	void ApplyRow(FRowWidgets& Widgets, const FLeaderboardRow& Row, bool bShowHealth);
	UTextBlock* MakeText(const FName& Name, int32 FontSize, const FLinearColor& Color);
	FSlateFontInfo MakeFont(int32 Size) const;
};
