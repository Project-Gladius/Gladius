#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/Battle/BattleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBattleTopHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Apply(FBattleTopHudState());
}

void UBattleTopHudWidget::BindToBattleManager(ABattleManager* InBattleManager)
{
	BattleManager = InBattleManager;
}

void UBattleTopHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ABattleManager* Manager = BattleManager.Get();
	if (!Manager)
	{
		Apply(FBattleTopHudState());
		return;
	}

	Apply(FBattleTopHudPresenter::Build(
		Manager->Phase,
		Manager->IsTurnCutScenePlaying(),
		Manager->GetInputPhaseRemainingSeconds(),
		Manager->GetInputPhaseDuration(),
		Manager->TurnIndex,
		Manager->Turns));
}

void UBattleTopHudWidget::Apply(const FBattleTopHudState& State)
{
	if (!Root)
	{
		return;
	}

	Root->SetVisibility(State.bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!State.bVisible)
	{
		return;
	}

	SetTextIfChanged(TurnValueText, State.TurnText);
	SetTextIfChanged(TurnTotalText, State.TurnTotalText);
	SetTextIfChanged(GuideText, State.GuideText);
	SetTextIfChanged(TimerText, State.TimerText);

	//공간은 유지한 채 숨겨 레이아웃이 흔들리지 않게 함
	TimerContent->SetVisibility(State.bShowTimer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	Gauge->SetVisibility(State.bShowGauge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	Gauge->SetPercent(State.GaugePercent);
}

void UBattleTopHudWidget::SetTextIfChanged(UTextBlock* Text, const FString& Value)
{
	if (Text && !Text->GetText().ToString().Equals(Value, ESearchCase::CaseSensitive))
	{
		Text->SetText(FText::FromString(Value));
	}
}

UTextBlock* UBattleTopHudWidget::MakeText(int32 FontSize, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Style.FontFamily ? FSlateFontInfo(Style.FontFamily.Get(), FontSize) : Text->GetFont();
	Font.Size = FontSize;
	if (bBold)
	{
		Font.TypefaceFontName = TEXT("Bold");
	}
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	return Text;
}

void UBattleTopHudWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Canvas;

	UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopHudBar"));
	Root = Bar;

	//화면 최상단 중앙 고정
	UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(Bar);
	BarSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	BarSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BarSlot->SetPosition(ScreenOffset);
	BarSlot->SetAutoSize(true);

	auto MakePanel = [this](const FSlateBrush& Brush, const FVector2D& Size, UWidget* Content)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(Size.X);
		SizeBox->SetHeightOverride(Size.Y);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrush(Brush);
		Panel->SetPadding(FMargin(12.0f, 6.0f));
		Panel->SetHorizontalAlignment(HAlign_Center);
		Panel->SetVerticalAlignment(VAlign_Center);
		//패널보다 긴 내용은 잘리지 않고 작게 줄어듦
		UScaleBox* Shrink = WidgetTree->ConstructWidget<UScaleBox>();
		Shrink->SetStretch(EStretch::ScaleToFit);
		Shrink->SetStretchDirection(EStretchDirection::DownOnly);
		Shrink->AddChild(Content);
		Panel->SetContent(Shrink);
		SizeBox->AddChild(Panel);
		return SizeBox;
	};

	// ===== 턴 패널: "Turn" + "5/10" =====
	UVerticalBox* TurnBox = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* TurnLabel = MakeText(Style.TurnLabelFontSize, Style.TextColor);
	TurnLabel->SetText(FText::FromString(TEXT("Turn")));
	TurnBox->AddChildToVerticalBox(TurnLabel)->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBox* TurnValueRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	TurnValueText = MakeText(Style.TurnValueFontSize, Style.AccentColor, true);
	TurnTotalText = MakeText(Style.TurnTotalFontSize, Style.TextColor);
	TurnValueRow->AddChildToHorizontalBox(TurnValueText)->SetVerticalAlignment(VAlign_Bottom);
	if (UHorizontalBoxSlot* TotalSlot = TurnValueRow->AddChildToHorizontalBox(TurnTotalText))
	{
		TotalSlot->SetVerticalAlignment(VAlign_Bottom);
		TotalSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 3.0f));
	}
	TurnBox->AddChildToVerticalBox(TurnValueRow)->SetHorizontalAlignment(HAlign_Center);

	if (UHorizontalBoxSlot* TurnSlot = Bar->AddChildToHorizontalBox(MakePanel(Style.TurnPanelBrush, Style.TurnPanelSize, TurnBox)))
	{
		TurnSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ===== 가운데 바: 안내 문구 + 게이지 =====
	UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>();
	GuideText = MakeText(Style.GuideFontSize, Style.TextColor);
	if (UVerticalBoxSlot* GuideSlot = CenterBox->AddChildToVerticalBox(GuideText))
	{
		GuideSlot->SetHorizontalAlignment(HAlign_Center);
		GuideSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	Gauge = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle GaugeStyle = Gauge->GetWidgetStyle();
	GaugeStyle.BackgroundImage = Style.GaugeBackgroundBrush;
	GaugeStyle.FillImage = Style.GaugeFillBrush;
	Gauge->SetWidgetStyle(GaugeStyle);
	//왼쪽 기준으로 채워져, 비율이 줄면 오른쪽 끝이 왼쪽으로 줄어듦
	Gauge->SetBarFillType(EProgressBarFillType::LeftToRight);
	Gauge->SetFillColorAndOpacity(FLinearColor::White);
	USizeBox* GaugeSize = WidgetTree->ConstructWidget<USizeBox>();
	GaugeSize->SetWidthOverride(Style.CenterBarSize.X - 48.0f);
	GaugeSize->SetHeightOverride(Style.GaugeHeight);
	GaugeSize->AddChild(Gauge);
	CenterBox->AddChildToVerticalBox(GaugeSize)->SetHorizontalAlignment(HAlign_Center);

	if (UHorizontalBoxSlot* CenterSlot = Bar->AddChildToHorizontalBox(MakePanel(Style.CenterBarBrush, Style.CenterBarSize, CenterBox)))
	{
		CenterSlot->SetVerticalAlignment(VAlign_Center);
		CenterSlot->SetPadding(FMargin(-6.0f, 0.0f)); //양쪽 패널과 살짝 겹쳐 한 덩어리로 보이게
	}

	// ===== 타이머 패널: 아이콘 + 남은 시간 =====
	UHorizontalBox* TimerRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UImage* TimerIcon = WidgetTree->ConstructWidget<UImage>();
	TimerIcon->SetBrush(Style.TimerIconBrush);
	TimerIcon->SetDesiredSizeOverride(Style.TimerIconSize);
	if (UHorizontalBoxSlot* IconSlot = TimerRow->AddChildToHorizontalBox(TimerIcon))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}
	TimerText = MakeText(Style.TimerFontSize, Style.TextColor, true);
	TimerRow->AddChildToHorizontalBox(TimerText)->SetVerticalAlignment(VAlign_Center);
	TimerContent = TimerRow;

	if (UHorizontalBoxSlot* TimerSlot = Bar->AddChildToHorizontalBox(MakePanel(Style.TimerPanelBrush, Style.TimerPanelSize, TimerRow)))
	{
		TimerSlot->SetVerticalAlignment(VAlign_Center);
	}
}
