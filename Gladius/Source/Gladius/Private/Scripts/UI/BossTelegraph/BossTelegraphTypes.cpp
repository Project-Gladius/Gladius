#include "Scripts/UI/BossTelegraph/BossTelegraphTypes.h"

namespace
{
	//텍스처 없이 색과 테두리만 있는 둥근 사각형
	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		return Brush;
	}
}

FBossTelegraphStyle::FBossTelegraphStyle()
{
	const FLinearColor Gold(0.83f, 0.66f, 0.32f);

	OuterFrameBrush = MakeRoundedBrush(FLinearColor(0.01f, 0.02f, 0.045f, 0.92f), Gold, 2.0f, 12.0f);
	InnerFrameBrush = MakeRoundedBrush(FLinearColor(0.02f, 0.04f, 0.08f, 0.85f), Gold.CopyWithNewOpacity(0.6f), 1.0f, 6.0f);
	OrnamentBrush = MakeRoundedBrush(FLinearColor(1.0f, 0.82f, 0.45f), FLinearColor(1.0f, 0.92f, 0.7f), 1.0f, 1.0f);

	TextColor = FLinearColor(0.95f, 0.93f, 0.88f);
}
