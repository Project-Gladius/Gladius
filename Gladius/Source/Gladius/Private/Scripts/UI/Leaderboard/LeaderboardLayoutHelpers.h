#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"

class UHorizontalBox;
class UScaleBox;
class USizeBox;
class UWidget;
class UWidgetTree;

// 리더보드 보드(열 머리글)와 줄(칸)이 같은 열 구성을 쓰기 위한 기본 레이아웃 도우미
namespace LeaderboardRowLayout
{
	inline constexpr float MedalSize = 32.0f;
	inline constexpr float CellPadding = 8.0f;

	//아무것도 그리지 않는 브러시 (4위 이하 메달 자리)
	FSlateBrush MakeEmptyBrush();

	//텍스트가 칸보다 길면 잘리지 않고 작게 줄어들도록 ScaleBox로 감쌈
	UScaleBox* WrapShrinkToFit(UWidgetTree* Tree, UWidget* Content, EHorizontalAlignment HAlign);

	//고정 폭 칸
	USizeBox* AddFixedCell(UWidgetTree* Tree, UHorizontalBox* Row, FName Name, float Width, UWidget* Content, EHorizontalAlignment HAlign);

	//남는 폭을 모두 쓰는 칸 (사용자 ID)
	void AddFillCell(UWidgetTree* Tree, UHorizontalBox* Row, UWidget* Content);
}
