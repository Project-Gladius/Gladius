#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/UI/UIWidgetBuild.h"
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

	//Designer에서 만든 레이아웃이 없을 때(C++ 클래스 단독 사용)만 기본 도형 레이아웃 생성
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout(WidgetTree);
	}
	ResolveWidgets();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	Apply(FBattleTopHudState());
}

void UBattleTopHudWidget::ResolveWidgets()
{
	if (!WidgetTree)
	{
		return;
	}
	//WBP에서는 BindWidgetOptional로 이미 연결됨. C++로 만든 기본 레이아웃은 이름으로 찾아 연결
	auto Find = [this](auto& Member, const TCHAR* Name)
	{
		using FWidgetType = std::remove_reference_t<decltype(*Member)>;
		if (!Member)
		{
			Member = Cast<FWidgetType>(WidgetTree->FindWidget(Name));
		}
	};
	Find(TopHudBar, TEXT("TopHudBar"));
	Find(TurnValueText, TEXT("TurnValueText"));
	Find(TurnTotalText, TEXT("TurnTotalText"));
	Find(GuideText, TEXT("GuideText"));
	Find(Gauge, TEXT("Gauge"));
	Find(TimerContent, TEXT("TimerContent"));
	Find(TimerText, TEXT("TimerText"));
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
	//보임/문구/표시 여부가 바뀌었을 때만 디자이너 이벤트 호출
	const bool bChanged = !bHasLastState
		|| LastState.bVisible != State.bVisible
		|| LastState.GuideText != State.GuideText
		|| LastState.bShowTimer != State.bShowTimer
		|| LastState.bShowGauge != State.bShowGauge;
	bHasLastState = true;
	LastState = State;

	UWidget* Container = TopHudBar ? TopHudBar.Get() : this;
	Container->SetVisibility(State.bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (State.bVisible)
	{
		SetTextIfChanged(TurnValueText, State.TurnText);
		SetTextIfChanged(TurnTotalText, State.TurnTotalText);
		SetTextIfChanged(GuideText, State.GuideText);
		SetTextIfChanged(TimerText, State.TimerText);

		//공간은 유지한 채 숨겨 레이아웃이 흔들리지 않게 함
		if (TimerContent)
		{
			TimerContent->SetVisibility(State.bShowTimer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (Gauge)
		{
			Gauge->SetVisibility(State.bShowGauge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			Gauge->SetPercent(State.GaugePercent);
		}
	}

	if (bChanged)
	{
		OnHudStateChanged(State.bVisible, State.GuideText, State.bShowTimer, State.bShowGauge);
	}
}

void UBattleTopHudWidget::SetTextIfChanged(UTextBlock* Text, const FString& Value)
{
	if (Text && !Text->GetText().ToString().Equals(Value, ESearchCase::CaseSensitive))
	{
		Text->SetText(FText::FromString(Value));
	}
}

void UBattleTopHudWidget::BuildDefaultLayout(UWidgetTree* Tree) const
{
	using GladiusUI::ConstructNamed;

	if (!Tree)
	{
		return;
	}

	auto MakeText = [&](FName Name, int32 FontSize, const FLinearColor& Color, bool bBold, const TCHAR* Sample)
	{
		UTextBlock* Text = ConstructNamed<UTextBlock>(Tree, Name);
		GladiusUI::ApplyFont(Text, Style.FontFamily, FontSize, bBold);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetText(FText::FromString(Sample)); //Designer 미리보기용 예시 문구
		return Text;
	};

	auto MakePanel = [&](FName Name, const FSlateBrush& Brush, const FVector2D& Size, UWidget* Content)
	{
		USizeBox* SizeBox = ConstructNamed<USizeBox>(Tree);
		SizeBox->SetWidthOverride(Size.X);
		SizeBox->SetHeightOverride(Size.Y);
		UBorder* Panel = ConstructNamed<UBorder>(Tree, Name);
		Panel->SetBrush(Brush);
		Panel->SetPadding(FMargin(12.0f, 6.0f));
		Panel->SetHorizontalAlignment(HAlign_Center);
		Panel->SetVerticalAlignment(VAlign_Center);
		//패널보다 긴 내용은 잘리지 않고 작게 줄어듦
		UScaleBox* Shrink = ConstructNamed<UScaleBox>(Tree);
		Shrink->SetStretch(EStretch::ScaleToFit);
		Shrink->SetStretchDirection(EStretchDirection::DownOnly);
		Shrink->AddChild(Content);
		Panel->SetContent(Shrink);
		SizeBox->AddChild(Panel);
		return SizeBox;
	};

	UCanvasPanel* Canvas = ConstructNamed<UCanvasPanel>(Tree, TEXT("RootCanvas"));
	Tree->RootWidget = Canvas;

	UHorizontalBox* Bar = ConstructNamed<UHorizontalBox>(Tree, TEXT("TopHudBar"));

	//화면 최상단 중앙 고정
	UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(Bar);
	BarSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	BarSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BarSlot->SetPosition(ScreenOffset);
	BarSlot->SetAutoSize(true);

	// ===== 턴 패널: "Turn" + "5/10" =====
	UVerticalBox* TurnBox = ConstructNamed<UVerticalBox>(Tree);
	TurnBox->AddChildToVerticalBox(MakeText(TEXT("TurnLabelText"), Style.TurnLabelFontSize, Style.TextColor, false, TEXT("Turn")))
		->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBox* TurnValueRow = ConstructNamed<UHorizontalBox>(Tree);
	TurnValueRow->AddChildToHorizontalBox(MakeText(TEXT("TurnValueText"), Style.TurnValueFontSize, Style.AccentColor, true, TEXT("5")))
		->SetVerticalAlignment(VAlign_Bottom);
	if (UHorizontalBoxSlot* TotalSlot = TurnValueRow->AddChildToHorizontalBox(MakeText(TEXT("TurnTotalText"), Style.TurnTotalFontSize, Style.TextColor, false, TEXT("/10"))))
	{
		TotalSlot->SetVerticalAlignment(VAlign_Bottom);
		TotalSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 3.0f));
	}
	TurnBox->AddChildToVerticalBox(TurnValueRow)->SetHorizontalAlignment(HAlign_Center);

	Bar->AddChildToHorizontalBox(MakePanel(TEXT("TurnPanel"), Style.TurnPanelBrush, Style.TurnPanelSize, TurnBox))
		->SetVerticalAlignment(VAlign_Center);

	// ===== 가운데 바: 안내 문구 + 게이지 =====
	UVerticalBox* CenterBox = ConstructNamed<UVerticalBox>(Tree);
	if (UVerticalBoxSlot* GuideSlot = CenterBox->AddChildToVerticalBox(MakeText(TEXT("GuideText"), Style.GuideFontSize, Style.TextColor, false, TEXT("행동을 선택하세요..."))))
	{
		GuideSlot->SetHorizontalAlignment(HAlign_Center);
		GuideSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	UProgressBar* GaugeBar = ConstructNamed<UProgressBar>(Tree, TEXT("Gauge"));
	FProgressBarStyle GaugeStyle = GaugeBar->GetWidgetStyle();
	GaugeStyle.BackgroundImage = Style.GaugeBackgroundBrush;
	GaugeStyle.FillImage = Style.GaugeFillBrush;
	GaugeBar->SetWidgetStyle(GaugeStyle);
	//왼쪽 기준으로 채워져, 비율이 줄면 오른쪽 끝이 왼쪽으로 줄어듦
	GaugeBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	GaugeBar->SetFillColorAndOpacity(FLinearColor::White);
	GaugeBar->SetPercent(0.7f); //Designer 미리보기용
	USizeBox* GaugeSize = ConstructNamed<USizeBox>(Tree);
	GaugeSize->SetWidthOverride(Style.CenterBarSize.X - 48.0f);
	GaugeSize->SetHeightOverride(Style.GaugeHeight);
	GaugeSize->AddChild(GaugeBar);
	CenterBox->AddChildToVerticalBox(GaugeSize)->SetHorizontalAlignment(HAlign_Center);

	if (UHorizontalBoxSlot* CenterSlot = Bar->AddChildToHorizontalBox(MakePanel(TEXT("CenterPanel"), Style.CenterBarBrush, Style.CenterBarSize, CenterBox)))
	{
		CenterSlot->SetVerticalAlignment(VAlign_Center);
		CenterSlot->SetPadding(FMargin(-6.0f, 0.0f)); //양쪽 패널과 살짝 겹쳐 한 덩어리로 보이게
	}

	// ===== 타이머 패널: 아이콘 + 남은 시간 =====
	UHorizontalBox* TimerRow = ConstructNamed<UHorizontalBox>(Tree, TEXT("TimerContent"));
	UImage* TimerIcon = ConstructNamed<UImage>(Tree, TEXT("TimerIcon"));
	TimerIcon->SetBrush(Style.TimerIconBrush);
	TimerIcon->SetDesiredSizeOverride(Style.TimerIconSize);
	if (UHorizontalBoxSlot* IconSlot = TimerRow->AddChildToHorizontalBox(TimerIcon))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}
	TimerRow->AddChildToHorizontalBox(MakeText(TEXT("TimerText"), Style.TimerFontSize, Style.TextColor, true, TEXT("30.00")))
		->SetVerticalAlignment(VAlign_Center);

	Bar->AddChildToHorizontalBox(MakePanel(TEXT("TimerPanel"), Style.TimerPanelBrush, Style.TimerPanelSize, TimerRow))
		->SetVerticalAlignment(VAlign_Center);
}
