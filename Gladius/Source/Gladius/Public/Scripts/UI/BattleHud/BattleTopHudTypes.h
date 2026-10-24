#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "BattleTopHudTypes.generated.h"

enum class EBattlePhase : uint8;

// 상단 UI 외형. 지금은 기본 도형이지만, UI 에셋이 오면 각 브러시에 텍스처만 지정하면 된다
USTRUCT(BlueprintType)
struct GLADIUS_API FBattleTopHudStyle
{
	GENERATED_BODY()

	FBattleTopHudStyle();

	// ===== 브러시 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Brush") FSlateBrush TurnPanelBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Brush") FSlateBrush CenterBarBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Brush") FSlateBrush TimerPanelBrush;

	//게이지 배경(기본 회색)과 채움(기본 흰색)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Brush") FSlateBrush GaugeBackgroundBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Brush") FSlateBrush GaugeFillBrush;

	//타이머 아이콘(기본은 작은 원)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Brush") FSlateBrush TimerIconBrush;

	// ===== 글자 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") FLinearColor TextColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") FLinearColor AccentColor;

	//지정하면 이 폰트를 사용 (비우면 엔진 기본 폰트)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text", meta = (AllowedClasses = "/Script/Engine.Font")) TObjectPtr<UObject> FontFamily;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") int32 TurnLabelFontSize = 14;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") int32 TurnValueFontSize = 28;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") int32 TurnTotalFontSize = 18;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") int32 GuideFontSize = 16;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Text") int32 TimerFontSize = 24;

	// ===== 레이아웃 (px) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Layout") FVector2D TurnPanelSize = FVector2D(120.0f, 76.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Layout") FVector2D CenterBarSize = FVector2D(460.0f, 60.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Layout") FVector2D TimerPanelSize = FVector2D(150.0f, 76.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Layout") float GaugeHeight = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Top HUD|Layout") FVector2D TimerIconSize = FVector2D(22.0f, 22.0f);
};

// 상단 UI에 그릴 내용 (전투 상태에서 계산한 결과)
struct FBattleTopHudState
{
	bool bVisible = false;
	FString TurnText;      //"5"
	FString TurnTotalText; //"/10"
	FString GuideText;
	bool bShowTimer = false;
	FString TimerText;     //"30.00"
	bool bShowGauge = false;
	float GaugePercent = 0.0f; //1 = 가득, 0 = 끝
};

// 전투 상태 -> 상단 UI 표시 내용 변환. 상태가 없는 순수 함수라 단독으로 테스트할 수 있음
//  Prepare: "행동을 선택하세요..." + 남은 시간 + 게이지
//  Collect: "대기 중..." + 남은 시간 + 게이지
//  Move~BossAction 컷신 재생 중: 전체 숨김
//  Move(컷신 종료 후)~Resolve: "전투 중!" (시간·게이지 숨김)
//  BattleStart / BattleEnd: 전체 숨김
class GLADIUS_API FBattleTopHudPresenter
{
public:
	static FBattleTopHudState Build(EBattlePhase Phase, bool bCutScenePlaying, float RemainingSeconds, float DurationSeconds, int32 TurnIndex, int32 Turns);

	//30 -> "30.00"
	static FString FormatSeconds(float Seconds);
};
