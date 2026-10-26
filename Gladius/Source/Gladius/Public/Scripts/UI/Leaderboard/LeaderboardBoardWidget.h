#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"
#include "LeaderboardBoardWidget.generated.h"

class ULeaderboardRowWidget;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UWidgetTree;

// 리더보드 보드 한 장(제목, 열 머리글, 10줄)의 로직. 데이터는 SetPage로 받기만 한다
// 줄은 RowWidgetClass(기본 WBP_LeaderboardRow)로 10개를 만들어 RowContainer에 넣는다
// - 디자이너용 WBP(/Game/Blueprints/UI/WBP_LeaderboardBoard): 패널·제목·머리글을 Designer에서 꾸밈
// - WBP 없이 C++ 클래스로 쓰면 BuildDefaultLayout으로 기본 도형 레이아웃을 만든다
UCLASS(Blueprintable)
class GLADIUS_API ULeaderboardBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
 UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
 static bool CopyAlignedDesigner(UObject* SourceAsset, UObject* TargetAsset, UObject* TargetRowAsset);

 UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
 static bool NormalizeEditorTree(UObject* Asset);

 UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
 static bool ExportImageAlignedPreview(UObject* Asset, const FString& FilePath, bool bBattle, bool bLower);

 // Convert copied rows to shared row assets and freely positioned Canvas slots.
 UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
 static bool EnableFreeLayout(UObject* Asset, UObject* SharedRowAsset);

 // One-time conversion: copies authored row trees into this Board's Designer.
 UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
 static bool EmbedEditableRows(UObject* Asset);

 // Serialized binding names, independent of hierarchy position. Empty keeps legacy rows.
 UPROPERTY()
 TArray<FName> InlineRowRoots;

	UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
	static bool EnsureStateBindings(UObject* Asset);
	void RefreshDesignerPreview();
	// Preview data only; never changes authored geometry, fonts or images.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaderboard|Preview")
	ELeaderboardPreviewPage PreviewPage = ELeaderboardPreviewPage::BattleTop;

	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void SetPage(const FLeaderboardPage& Page);

	//보드 내용이 바뀔 때(보드 교대 포함) 호출. 디자이너가 전환 애니메이션을 붙이는 용도
	UFUNCTION(BlueprintImplementableEvent, Category = "Leaderboard")
	void OnPageChanged(const FLeaderboardPage& Page, bool bTitleChanged);

	//아래 BindWidgetOptional 위젯들을 같은 이름으로 Tree에 만든다 (C++ 단독 사용, WBP 자동 생성 공용)
	void BuildDefaultLayout(UWidgetTree* Tree) const;

	//줄 위젯 클래스. 비워 두면 WBP_LeaderboardRow, 없으면 C++ 기본 줄
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaderboard")
	TSubclassOf<ULeaderboardRowWidget> RowWidgetClass;

	//기본 레이아웃(BuildDefaultLayout)에만 쓰이는 외형. WBP에서는 Designer에서 직접 꾸민다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaderboard|Default Layout")
	FLeaderboardStyle Style;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	// ===== 디자이너가 같은 이름으로 배치해야 하는 위젯 =====

	//"LEADERBOARD" / "BATTLE ANALYSIS"
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	//체력 열 머리글 (LEADERBOARD에서는 접힘)
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HealthHeaderCell;

	//"누적 데미지" / "데미지"
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageHeaderText;

	//줄 10개가 들어갈 자리 (Vertical Box 권장). 디자이너가 배치한 자식은 보존한다
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RowContainer;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULeaderboardRowWidget>> Rows;

	FString LastTitle;

	void ResolveWidgets();
	void CreateRows();
	FLeaderboardPage MakePreviewPage() const;
};
