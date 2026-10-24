#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

// UI 에셋이 오기 전까지 쓰는 기본 도형 브러시 (텍스처 없이 색과 테두리만)
namespace GladiusUI
{
	//둥근 사각형
	GLADIUS_API FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth, float Radius);

	//원 (높이의 절반을 반지름으로 사용)
	GLADIUS_API FSlateBrush MakeCircleBrush(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth = 2.0f);
}
