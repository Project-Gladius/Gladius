#include "Scripts/Battle/UnitNameplate.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UUnitNameplate::RebuildWidget()
{
	//위젯 블루프린트로 만든 경우 RootWidget에 이미 디자인한 내용이 들어 있다.
	//그때 여기서 새로 만들면 디자인을 통째로 덮어쓰게 되므로, 비어 있을 때만 만든다
	if (WidgetTree && WidgetTree->RootWidget && !Label)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Battle] 이름표 위젯에 'Label'이라는 이름의 Text Block이 없습니다 — 글자가 표시되지 않습니다"));
	}

	if (!Label && WidgetTree && !WidgetTree->RootWidget)
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
