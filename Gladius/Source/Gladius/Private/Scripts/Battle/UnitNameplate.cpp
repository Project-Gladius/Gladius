#include "Scripts/Battle/UnitNameplate.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UUnitNameplate::RebuildWidget()
{
	if (!Label && WidgetTree)
	{
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 16;
		//글자가 배경과 같은 밝기여도 읽히도록 그림자를 준다
		Label->SetShadowOffset(FVector2D(1.f, 1.f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Label->SetFont(Font);
		Label->SetText(FText::FromString(PendingText));

		WidgetTree->RootWidget = Label;
	}

	return Super::RebuildWidget();
}

void UUnitNameplate::SetText(const FString& InText)
{
	PendingText = InText;
	if (Label)
	{
		Label->SetText(FText::FromString(InText));
	}
}
