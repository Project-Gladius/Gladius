#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphTypes.h"
#include "BossTelegraphWidget.generated.h"

class URichTextBlock;
class ABattleManager;
class UCanvasPanelSlot;
class UTextBlock;
class UWidgetTree;

// 보스 행동 암시 텍스트 상자의 로직. BattleManager::OnBossTelegraph로 문구를 받아 보스 바로 아래(화면 좌표)에 보여준다
// 턴이 Prepare 단계일 때만 보이며, DisplaySeconds가 지나거나 Prepare가 끝나면 사라진다
// - 디자이너용 WBP(/Game/Blueprints/UI/WBP_BossTelegraph): Designer에서 아래 이름의 위젯을 꾸밈
//   (TelegraphBox는 루트 Canvas Panel의 바로 아래 자식이어야 보스를 따라 움직임)
// - WBP 없이 C++ 클래스로 쓰면 BuildDefaultLayout으로 기본 도형 레이아웃을 만든다
UCLASS(Blueprintable)
class GLADIUS_API UBossTelegraphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToBattleManager(ABattleManager* InBattleManager);

	//아래 BindWidgetOptional 위젯들을 같은 이름으로 Tree에 만든다 (C++ 단독 사용, WBP 자동 생성 공용)
	void BuildDefaultLayout(UWidgetTree* Tree) const;

	//문구를 표시 (빈 문구면 무시)
	UFUNCTION(BlueprintCallable, Category = "Boss Telegraph")
	void ShowTelegraph(const FString& TelegraphText);

	UFUNCTION(BlueprintCallable, Category = "Boss Telegraph")
	void HideTelegraph();
    void ShowTutorialMessage(const FString& RichText,float Seconds);
    void EndTutorial();
    UPROPERTY(Transient) TObjectPtr<URichTextBlock> RichMessageText;
    bool bTutorialMessage=false;
    float NormalDisplaySeconds=5.f;

	//문구가 나타날 때 / 사라질 때 호출. 디자이너가 UMG 애니메이션을 붙이는 용도
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Telegraph")
	void OnTelegraphShown(const FString& TelegraphText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Telegraph")
	void OnTelegraphHidden();

	//기본 레이아웃(BuildDefaultLayout)에만 쓰이는 외형. WBP에서는 Designer에서 직접 꾸민다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Telegraph|Default Layout")
	FBossTelegraphStyle Style;

	//표시 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph", meta = (ClampMin = "0.1"))
	float DisplaySeconds = 5.0f;

	//기본 페이드 사용 여부. UMG 애니메이션(OnTelegraphShown/Hidden)으로 연출할 때는 끔
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph")
	bool bUseBuiltInFade = true;

	//나타나고 사라질 때의 페이드 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph", meta = (ClampMin = "0.0", EditCondition = "bUseBuiltInFade"))
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

	// ===== 디자이너가 같은 이름으로 배치해야 하는 위젯 =====

	//문구 상자 전체. 보이기/숨기기, 보스 따라가기 위치 지정의 대상 (루트 Canvas Panel의 자식)
	UPROPERTY(BlueprintReadOnly, Category = "Boss Telegraph|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TelegraphBox;

	//암시 문구
	UPROPERTY(BlueprintReadOnly, Category = "Boss Telegraph|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UFUNCTION()
	void HandleBossTelegraph(const FString& MotionId, const FString& TelegraphText);

private:
	TWeakObjectPtr<ABattleManager> BattleManager;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> BoxSlot;

	bool bShowing = false;
	float ElapsedSeconds = 0.0f;

	void ResolveWidgets();
	bool IsPreparePhase() const;

	//보스 바로 아래로 상자를 옮김 (화면 밖으로 나가지 않게 보정)
	void UpdateBoxPosition();
	FVector2D GetBoxSize() const;
};
