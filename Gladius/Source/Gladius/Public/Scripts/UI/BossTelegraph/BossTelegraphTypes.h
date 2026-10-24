#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "BossTelegraphTypes.generated.h"

// 보스 행동 암시 텍스트 상자의 외형. 지금은 기본 도형이지만, UI 에셋이 오면 각 브러시에 텍스처만 지정하면 된다
USTRUCT(BlueprintType)
struct GLADIUS_API FBossTelegraphStyle
{
	GENERATED_BODY()

	FBossTelegraphStyle();

	//바깥 테두리와 바탕
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Brush") FSlateBrush OuterFrameBrush;

	//안쪽 테두리 (이중 테두리 느낌)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Brush") FSlateBrush InnerFrameBrush;

	//모서리 4개와 위·아래 가운데 장식
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Brush") FSlateBrush OrnamentBrush;

	//장식 크기(px)와 회전(도). 기본은 작은 사각형을 45° 돌린 마름모
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Brush") FVector2D OrnamentSize = FVector2D(10.0f, 10.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Brush") float OrnamentAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Text") FLinearColor TextColor;

	//지정하면 이 폰트를 사용 (비우면 엔진 기본 폰트)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Text", meta = (AllowedClasses = "/Script/Engine.Font")) TObjectPtr<UObject> FontFamily;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Text") int32 FontSize = 26;

	//상자 크기(px). 기본 폰트 크기로 30자가 한 줄에 들어가는 폭
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Layout") FVector2D BoxSize = FVector2D(860.0f, 110.0f);

	//바깥 테두리와 안쪽 테두리 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Layout") FMargin FramePadding = FMargin(8.0f);

	//안쪽 테두리와 글자 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Telegraph|Layout") FMargin TextPadding = FMargin(36.0f, 10.0f);
};
