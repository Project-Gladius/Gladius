#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"
#include "LeaderboardRotatorWidget.generated.h"

class ABattleManager;
class ULeaderboardBoardWidget;
class UWidgetTree;
class UWidgetSwitcher;

// 전투씬 좌측 상단에서 LEADERBOARD -> BATTLE ANALYSIS(1~10위) -> BATTLE ANALYSIS(1위+11~15위)를 일정 시간마다 교대로 보여줌
// 데이터는 ABattleManager에서 읽어 FLeaderboardBuilder로 페이지를 만들고, 그리는 일은 ULeaderboardBoardWidget에 맡김
// - 디자이너용 WBP(/Game/Blueprints/UI/WBP_LeaderboardRotator): Designer에서 Board(WBP_LeaderboardBoard)의 위치·크기를 조정
// - WBP 없이 C++ 클래스로 쓰면 BuildDefaultLayout으로 좌측 상단에 보드를 배치한다
UCLASS(Blueprintable)
class GLADIUS_API ULeaderboardRotatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// One-time migration. Keeps the existing Board and its canvas slot intact.
	UFUNCTION(BlueprintCallable, Category="Leaderboard|Editor")
	static bool AddSeparateBattleState(UObject* Asset, TSubclassOf<ULeaderboardBoardWidget> BattleClass);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaderboard|Preview")
	ELeaderboardPreviewPage PreviewPage = ELeaderboardPreviewPage::Total;

	void BindToBattleManager(ABattleManager* InBattleManager);

	//보드가 바뀌는 간격(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard", meta = (ClampMin = "1.0"))
	float SwitchSeconds = 10.0f;

	//보이는 보드의 데이터를 다시 읽는 간격(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard", meta = (ClampMin = "0.1"))
	float RefreshSeconds = 1.0f;

	//기본 레이아웃: 화면 좌측 상단에서의 여백(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Default Layout")
	FVector2D ScreenOffset = FVector2D(24.0f, 24.0f);

	//기본 레이아웃: 보드 전체 크기 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Default Layout", meta = (ClampMin = "0.1"))
	float BoardScale = 0.75f;

	//기본 레이아웃에서 만들 보드 클래스. 비워 두면 WBP_LeaderboardBoard, 없으면 C++ 기본 보드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Default Layout")
	TSubclassOf<ULeaderboardBoardWidget> BoardWidgetClass;

	//Board 위젯을 Tree에 만든다 (C++ 단독 사용, WBP 자동 생성 공용)
	void BuildDefaultLayout(UWidgetTree* Tree) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	//디자이너가 같은 이름으로 배치해야 하는 보드 (WBP_LeaderboardBoard)
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<ULeaderboardBoardWidget> Board;

	// Optional for backward compatibility with existing single-board widgets.
	UPROPERTY(BlueprintReadOnly, Category="Leaderboard|Widgets", meta=(BindWidgetOptional))
	TObjectPtr<ULeaderboardBoardWidget> BattleBoard;

	UPROPERTY(BlueprintReadOnly, Category="Leaderboard|Widgets", meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

private:
	friend struct FLeaderboardStateTestAccess;
	void ResolveStateWidgets();
	ULeaderboardBoardWidget* SelectBoard(bool bBattle);
	enum class EPage : uint8
	{
		Total,
		BattleTop,
		BattleLower,
	};

	TWeakObjectPtr<ABattleManager> BattleManager;

	EPage CurrentPage = EPage::Total;
	FTimerHandle SwitchTimerHandle;
	FTimerHandle RefreshTimerHandle;

	void AdvancePage();
	void RefreshCurrentPage();

	TMap<FString, FString> CollectCharacterNames() const;
	TArray<FLeaderboardRow> RankTotal() const;
	TArray<FLeaderboardRow> RankBattle() const;
};
