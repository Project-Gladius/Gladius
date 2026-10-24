#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphTypes.h"
#include "BossTelegraphWidget.generated.h"

class ABattleManager;
class UTextBlock;

// 보스 행동 암시 텍스트 상자. BattleManager::OnBossTelegraph로 문구를 받아 화면 상단 중앙에 보여준다
// 턴이 Prepare 단계일 때만 보이며, DisplaySeconds가 지나거나 Prepare가 끝나면 사라진다
// 외형은 Style로 바꾸며, WBP로 상속해 Style에 텍스처를 지정하면 기본 도형 대신 에셋이 쓰인다
UCLASS()
class GLADIUS_API UBossTelegraphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToBattleManager(ABattleManager* InBattleManager);

	//문구를 표시 (빈 문구면 무시)
	UFUNCTION(BlueprintCallable, Category = "Boss Telegraph")
	void ShowTelegraph(const FString& TelegraphText);

	UFUNCTION(BlueprintCallable, Category = "Boss Telegraph")
	void HideTelegraph();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Telegraph")
	FBossTelegraphStyle Style;

	//표시 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph", meta = (ClampMin = "0.1"))
	float DisplaySeconds = 5.0f;

	//나타나고 사라질 때의 페이드 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph", meta = (ClampMin = "0.0"))
	float FadeSeconds = 0.25f;

	//화면 상단 중앙 기준 위치(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph")
	FVector2D ScreenOffset = FVector2D(0.0f, 40.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleBossTelegraph(const FString& MotionId, const FString& TelegraphText);

private:
	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient) TObjectPtr<UWidget> Box;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;

	bool bShowing = false;
	float ElapsedSeconds = 0.0f;

	void BuildWidgetTree();
	void AddOrnament(class UOverlay* Overlay, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, const FVector2D& Translation);
	bool IsPreparePhase() const;
};
