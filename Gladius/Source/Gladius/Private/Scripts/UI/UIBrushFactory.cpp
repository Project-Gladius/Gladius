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

#include "Scripts/UI/UIWidgetBuild.h"
#include "Components/TextBlock.h"

void GladiusUI::ApplyFont(UTextBlock* Text, UObject* FontFamily, int32 Size, bool bBold)
{
	if (!Text)
	{
		return;
	}
	FSlateFontInfo Font = FontFamily ? FSlateFontInfo(FontFamily, Size) : Text->GetFont();
	Font.Size = Size;
	if (bBold)
	{
		Font.TypefaceFontName = TEXT("Bold");
	}
	Text->SetFont(Font);
}
