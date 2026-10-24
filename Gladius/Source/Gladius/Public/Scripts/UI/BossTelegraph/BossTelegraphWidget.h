#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphTypes.h"
#include "BossTelegraphWidget.generated.h"

class ABattleManager;
class UTextBlock;

// 보스 행동 암시 텍스트 상자. BattleManager::OnBossTelegraph로 문구를 받아 보스 바로 아래(화면 좌표)에 보여준다
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

	//보스 액터 위치에서 상자를 붙일 기준점까지의 월드 오프셋(cm). 기본은 보스 발밑
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Position")
	FVector BossWorldOffset = FVector::ZeroVector;

	//기준점의 화면 위치에서 상자 위쪽 가운데까지의 거리(px). +Y가 아래
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Position")
	FVector2D BelowBossScreenOffset = FVector2D(0.0f, 24.0f);

	//보스가 없거나 화면에 투영할 수 없을 때 쓰는 위치. 화면 상단 중앙 기준(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Position")
	FVector2D FallbackScreenOffset = FVector2D(0.0f, 140.0f);

	//화면 가장자리와 최소 간격(px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Position")
	float ScreenEdgeMargin = 16.0f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleBossTelegraph(const FString& MotionId, const FString& TelegraphText);

private:
	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient) TObjectPtr<UWidget> Box;
	UPROPERTY(Transient) TObjectPtr<class UCanvasPanelSlot> BoxSlot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;

	bool bShowing = false;
	float ElapsedSeconds = 0.0f;

	void BuildWidgetTree();
	void AddOrnament(class UOverlay* Overlay, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, const FVector2D& Translation);
	bool IsPreparePhase() const;

	//보스 바로 아래로 상자를 옮김 (화면 밖으로 나가지 않게 보정)
	void UpdateBoxPosition();
};
