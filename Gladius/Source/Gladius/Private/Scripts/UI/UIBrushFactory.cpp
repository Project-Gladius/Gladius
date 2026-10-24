#include "Scripts/UI/UIBrushFactory.h"

FSlateBrush GladiusUI::MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth, float Radius)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
	return Brush;
}

FSlateBrush GladiusUI::MakeCircleBrush(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FSlateColor(Outline), OutlineWidth);
	return Brush;
}
