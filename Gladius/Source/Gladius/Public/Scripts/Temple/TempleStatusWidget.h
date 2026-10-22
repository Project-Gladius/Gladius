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

// 신전 텍스트 UI. 기본 레이아웃에는 상태와 여신 대사만 표시한다.
// 채팅은 별도의 UChatLogWidget이 담당하며, 기존 WBP의 LogText는 선택 사항이다.
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

	UFUNCTION()
	void HandleCombatClicked();
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
