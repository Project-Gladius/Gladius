#include "TempleAIChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "TempleAIQuickTesterActor.h"

void UTempleAIChatWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
}

void UTempleAIChatWidget::InitializeTester(ATempleAIQuickTesterActor* InTester)
{
    Tester = InTester;
    if (MessageInput && Tester)
    {
        MessageInput->SetText(FText::FromString(Tester->TestMessage));
        MessageInput->SetKeyboardFocus();
    }
}

void UTempleAIChatWidget::BuildWidgetTree()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    WidgetTree->RootWidget = Canvas;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatPanel"));
    Panel->SetPadding(FMargin(18.0f));
    Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.94f));

    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    PanelSlot->SetAutoSize(true);

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChatLayout"));
    Panel->SetContent(Layout);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
    Title->SetText(FText::FromString(TEXT("여신에게 질문하기")));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.82f, 0.45f)));
    FSlateFontInfo TitleFont = Title->GetFont();
    TitleFont.Size = 20;
    Title->SetFont(TitleFont);
    UVerticalBoxSlot* TitleSlot = Layout->AddChildToVerticalBox(Title);
    TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InputRow"));
    Layout->AddChildToVerticalBox(InputRow);

    USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InputSize"));
    InputSize->SetWidthOverride(520.0f);
    InputSize->SetHeightOverride(42.0f);
    MessageInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("MessageInput"));
    MessageInput->SetHintText(FText::FromString(TEXT("여신에게 전할 말을 입력하세요")));
    MessageInput->SetClearKeyboardFocusOnCommit(false);
    MessageInput->OnTextCommitted.AddDynamic(this, &UTempleAIChatWidget::HandleMessageCommitted);
    InputSize->SetContent(MessageInput);
    UHorizontalBoxSlot* InputSlot = InputRow->AddChildToHorizontalBox(InputSize);
    InputSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

    USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ButtonSize"));
    ButtonSize->SetWidthOverride(96.0f);
    ButtonSize->SetHeightOverride(42.0f);
    SendButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SendButton"));
    SendButton->OnClicked.AddDynamic(this, &UTempleAIChatWidget::HandleSendClicked);
    UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SendButtonLabel"));
    ButtonLabel->SetText(FText::FromString(TEXT("전송")));
    ButtonLabel->SetJustification(ETextJustify::Center);
    SendButton->SetContent(ButtonLabel);
    ButtonSize->SetContent(SendButton);
    InputRow->AddChildToHorizontalBox(ButtonSize);

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
    StatusText->SetText(FText::FromString(TEXT("메시지를 입력하고 전송을 누르세요.")));
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.7f, 0.78f)));
    UVerticalBoxSlot* StatusSlot = Layout->AddChildToVerticalBox(StatusText);
    StatusSlot->SetPadding(FMargin(0.0f, 9.0f, 0.0f, 4.0f));

    ResponseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResponseText"));
    ResponseText->SetAutoWrapText(true);
    ResponseText->SetWrapTextAt(616.0f);
    ResponseText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Layout->AddChildToVerticalBox(ResponseText);
}

void UTempleAIChatWidget::SubmitCurrentMessage()
{
    if (!Tester || !MessageInput)
    {
        ShowError(TEXT("Temple AI 테스트 액터를 찾을 수 없습니다."));
        return;
    }

    const FString Message = MessageInput->GetText().ToString().TrimStartAndEnd();
    if (Message.IsEmpty())
    {
        ShowError(TEXT("메시지를 입력해 주세요."));
        MessageInput->SetKeyboardFocus();
        return;
    }

    Tester->SubmitMessageFromUI(Message);
}

void UTempleAIChatWidget::HandleSendClicked()
{
    SubmitCurrentMessage();
}

void UTempleAIChatWidget::HandleMessageCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        SubmitCurrentMessage();
    }
}

void UTempleAIChatWidget::SetRequestPending(bool bPending)
{
    if (SendButton)
    {
        SendButton->SetIsEnabled(!bPending);
    }
    if (MessageInput)
    {
        MessageInput->SetIsEnabled(!bPending);
    }
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(bPending ? TEXT("여신의 응답을 기다리는 중...") : TEXT("응답을 받았습니다.")));
        StatusText->SetColorAndOpacity(FSlateColor(bPending
            ? FLinearColor(0.92f, 0.72f, 0.25f)
            : FLinearColor(0.45f, 0.85f, 0.6f)));
    }
}

void UTempleAIChatWidget::ShowResponse(const FTempleAIResponse& Response)
{
    SetRequestPending(false);
    if (ResponseText)
    {
        ResponseText->SetText(FText::FromString(FString::Printf(
            TEXT("[%s] %s\n감정: %s · 우선순위: %d"),
            *Response.Username,
            *Response.ResponseText,
            *Response.Emotion,
            Response.Priority)));
    }
    if (MessageInput)
    {
        MessageInput->SetText(FText::GetEmpty());
        MessageInput->SetKeyboardFocus();
    }
}

void UTempleAIChatWidget::ShowError(const FString& ErrorMessage)
{
    if (SendButton)
    {
        SendButton->SetIsEnabled(true);
    }
    if (MessageInput)
    {
        MessageInput->SetIsEnabled(true);
        MessageInput->SetKeyboardFocus();
    }
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("오류: %s"), *ErrorMessage)));
        StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.35f, 0.35f)));
    }
}
