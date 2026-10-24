#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"
#include "LeaderboardRotatorWidget.generated.h"

class ABattleManager;
class ULeaderboardBoardWidget;

// 전투씬 좌측 상단에서 LEADERBOARD -> BATTLE ANALYSIS(1~10위) -> BATTLE ANALYSIS(1위+11~15위)를 일정 시간마다 교대로 보여줌
// 데이터는 ABattleManager에서 읽어 FLeaderboardBuilder로 페이지를 만들고, 그리는 일은 ULeaderboardBoardWidget에 맡김
UCLASS()
class GLADIUS_API ULeaderboardRotatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToBattleManager(ABattleManager* InBattleManager);

	//보드가 바뀌는 간격(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard", meta = (ClampMin = "1.0"))
	float SwitchSeconds = 10.0f;

	//보이는 보드의 데이터를 다시 읽는 간격(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard", meta = (ClampMin = "0.1"))
	float RefreshSeconds = 1.0f;

	//화면 좌측 상단에서의 여백(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FVector2D ScreenOffset = FVector2D(24.0f, 24.0f);

	//보드 전체 크기 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard", meta = (ClampMin = "0.1"))
	float BoardScale = 0.75f;

	//보드 위젯 클래스. 에셋이 오면 Style을 지정한 WBP로 교체
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	TSubclassOf<ULeaderboardBoardWidget> BoardWidgetClass;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	enum class EPage : uint8
	{
		Total,
		BattleTop,
		BattleLower,
	};

	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient)
	TObjectPtr<ULeaderboardBoardWidget> Board;

	EPage CurrentPage = EPage::Total;
	FTimerHandle SwitchTimerHandle;
	FTimerHandle RefreshTimerHandle;

	void BuildWidgetTree();
	void AdvancePage();
	void RefreshCurrentPage();

	TMap<FString, FString> CollectCharacterNames() const;
	TArray<FLeaderboardRow> RankTotal() const;
	TArray<FLeaderboardRow> RankBattle() const;
};
