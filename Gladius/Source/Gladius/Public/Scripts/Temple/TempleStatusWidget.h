#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/Temple/TempleTypes.h"
#include "TempleStatusWidget.generated.h"

class ATempleCycleController;
class UButton;
class AGoddess;
class UScrollBox;
class UTextBlock;

// 신전 텍스트 UI. 세 영역으로 구성됨
//  - StateText    : 여신 상태 (좌상단)
//  - LogText      : 상태 로그 (우측, 넓고 작은 글씨로 최대한 많은 로그 표시. 여신 대사도 모두 여기에 기록)
//  - DialogueText : 여신 대사 (하단 중앙, 큰 글씨)
// C++에서 기본 레이아웃을 만들어 에셋 없이 동작하며, WBP로 상속해 같은 이름의 위젯을 배치하면 그 레이아웃을 사용함
UCLASS()
class GLADIUS_API UTempleStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToGoddess(AGoddess* InGoddess);
	void BindToController(ATempleCycleController* InController);

	//상태 로그에 남겨둘 최대 줄 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple Status", meta = (ClampMin = "1"))
	int32 MaxLogLines = 300;

	//Blessing 대사에 이름을 나열할 최대 인원 (넘으면 "외 N명")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple Status", meta = (ClampMin = "1"))
	int32 MaxBlessingNamesInDialogue = 6;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Temple Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(BlueprintReadOnly, Category = "Temple Status", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> LogScrollBox;

	UPROPERTY(BlueprintReadOnly, Category = "Temple Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LogText;

	UPROPERTY(BlueprintReadOnly, Category = "Temple Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY(BlueprintReadOnly, Category = "Temple Status", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CombatButton;

	UPROPERTY(BlueprintReadOnly, Category = "Temple Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnText;

	UFUNCTION()
	void HandleCombatClicked();
	UFUNCTION()
	void HandleTurnStarted(int32 TurnNumber);
	UFUNCTION()
	void HandleCombatRequested();

	UFUNCTION()
	void HandleGoddessStateChanged(EGoddessState NewState, const FGoddessStateContext& Context);

private:
	TWeakObjectPtr<AGoddess> Goddess;
	TWeakObjectPtr<ATempleCycleController> Controller;
	TArray<FString> LogLines;

	void BuildDefaultWidgetTree();
	void SetDialogue(const FString& Dialogue);
	void AppendLog(const FString& Line);

	static int32 GetBlessingTier(EGoddessState State);
	static FString GetStateLabel(EGoddessState State, const FGoddessStateContext& Context);
	FString BuildBlessingDialogue(const FGoddessStateContext& Context) const;
};
