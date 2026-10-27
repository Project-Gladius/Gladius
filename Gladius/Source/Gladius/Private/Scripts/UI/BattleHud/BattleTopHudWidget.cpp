#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Scripts/Time/RealTimeClockSubsystem.h"
#include "Engine/GameInstance.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif
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
        if (auto* Found = Cast<FWidgetType>(WidgetTree->FindWidget(Name)))
        {
            Member = Found;
        }
	};
	Find(TopHudBar, TEXT("TopHudBar"));
	Find(TurnValueText, TEXT("TurnValueText"));
	Find(TurnValueText, TEXT("TH_TurnValueText"));
	Find(TurnTotalText, TEXT("TurnTotalText"));
	Find(TurnTotalText, TEXT("TH_TurnTotalText"));
	Find(GuideText, TEXT("GuideText"));
	Find(GuideText, TEXT("TH_GuideText"));
	Find(Gauge, TEXT("Gauge"));
	Find(Gauge, TEXT("TH_Gauge"));
	Find(TimerContent, TEXT("TimerContent"));
	Find(TimerContent, TEXT("TH_TimerContent"));
	Find(TimerText, TEXT("TimerText"));
	Find(TimerText, TEXT("TH_TimerText"));
}

void UBattleTopHudWidget::BindToBattleManager(ABattleManager* InBattleManager)
{
	BattleManager = InBattleManager;
	TempleController.Reset();
}

void UBattleTopHudWidget::BindToTempleController(ATempleCycleController* InController)
{
 TempleController = InController;
 BattleManager.Reset();
 if (InController)
 {
  ResolveWidgets();
  PrepareTempleLayout();
  RefreshTempleState();
 }
}

void UBattleTopHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

 if (TempleController.IsValid())
 {
  RefreshTempleState();
  return;
 }

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
	// The background shares the content's visibility, including cutscenes.
 if (Container->GetParent() && Container->GetParent()->GetFName()==TEXT("TopHudFrame")) Container=Container->GetParent();
 Container->SetVisibility(State.bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (State.bVisible)
	{
  SetTextIfChanged(WidgetTree ? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TurnLabelText"))) : nullptr, State.TurnLabel);
		SetTextIfChanged(TurnValueText, State.TurnText);
		SetTextIfChanged(TurnTotalText, State.TurnTotalText);
		SetTextIfChanged(GuideText, State.GuideText);
		SetTextIfChanged(TimerText, State.TimerText);
  SetTextIfChanged(WidgetTree ? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TempleTimerLabelText"))) : nullptr, State.TimerLabel);

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
		FSlateBrush EmptyBrush; EmptyBrush.DrawAs=ESlateBrushDrawType::NoDrawType;
		Panel->SetBrush(EmptyBrush);
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
 UOverlay* Frame = ConstructNamed<UOverlay>(Tree, TEXT("TopHudFrame"));
 UImage* Background = ConstructNamed<UImage>(Tree, TEXT("TopHudBackgroundImage"));
 Background->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/UIDesign/TimePanel.TimePanel")));
 // The content determines HUD size; the image stretches to precisely that size.
 FSlateBrush BackgroundBrush=Background->GetBrush(); BackgroundBrush.ImageSize=FVector2D::ZeroVector;
 Background->SetBrush(BackgroundBrush); Background->SetVisibility(ESlateVisibility::HitTestInvisible);
 Frame->AddChildToOverlay(Background);
 Frame->AddChildToOverlay(Bar);

	//화면 최상단 중앙 고정
	UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(Frame);
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
	GaugeStyle.BackgroundImage.DrawAs = ESlateBrushDrawType::NoDrawType;
	GaugeStyle.FillImage = Style.GaugeFillBrush;
 GaugeStyle.FillImage.DrawAs=ESlateBrushDrawType::Image;
 GaugeStyle.MarqueeImage.DrawAs=ESlateBrushDrawType::NoDrawType;
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
	FSlateBrush EmptyIcon; EmptyIcon.DrawAs=ESlateBrushDrawType::NoDrawType;
	TimerIcon->SetBrush(EmptyIcon);
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

bool UBattleTopHudWidget::UseFullSizeBackground(UObject* Asset)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset);
 if (!BP || !BP->WidgetTree) return false;
 UWidgetTree* Tree=BP->WidgetTree;
 auto* Bar=Tree->FindWidget(TEXT("TopHudBar"));
 if (!Bar) return false;
 if (Tree->FindWidget(TEXT("TopHudFrame"))) return true;
 auto* Parent=Bar->GetParent(); if (!Parent) return false;
 BP->Modify(); Tree->Modify();
 auto* Frame=Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("TopHudFrame"));
 Frame->SetRenderTransform(Bar->GetRenderTransform());
 Frame->SetRenderTransformPivot(Bar->GetRenderTransformPivot());
 Frame->SetRenderOpacity(Bar->GetRenderOpacity());
 // ReplaceChild retains the original Canvas anchors, position, autosize and Z order.
 if (!Parent->ReplaceChild(Bar,Frame)) return false;
 Bar->SetRenderTransform(FWidgetTransform()); Bar->SetRenderOpacity(1.0f);
 auto* Background=Cast<UImage>(Tree->FindWidget(TEXT("Image_35")));
 if (Background) Background->RemoveFromParent();
 else Background=Tree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("TopHudBackgroundImage"));
 if (!Background->GetBrush().GetResourceObject())
  Background->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/UIDesign/TimePanel.TimePanel")));
 auto Brush=Background->GetBrush(); Brush.DrawAs=ESlateBrushDrawType::Image; Brush.ImageSize=FVector2D::ZeroVector;
 Background->SetBrush(Brush); Background->SetVisibility(ESlateVisibility::HitTestInvisible);
 Background->SetRenderTransform(FWidgetTransform());
 auto* ImageSlot=Frame->AddChildToOverlay(Background);
 ImageSlot->SetHorizontalAlignment(HAlign_Fill); ImageSlot->SetVerticalAlignment(VAlign_Fill);
 auto* ContentSlot=Frame->AddChildToOverlay(Bar);
 ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
 // Keep layout containers, padding and authored text positions while removing their shapes.
 Tree->ForEachWidget([BP](UWidget* W) {
  if (auto* Border=Cast<UBorder>(W))
  {
   auto Brush=Border->Background; Brush.DrawAs=ESlateBrushDrawType::NoDrawType; Border->SetBrush(Brush);
  }
  if (auto* Progress=Cast<UProgressBar>(W))
  {
   auto Style=Progress->GetWidgetStyle();
   Style.BackgroundImage.DrawAs=ESlateBrushDrawType::NoDrawType;
   Style.MarqueeImage.DrawAs=ESlateBrushDrawType::NoDrawType;
   Style.FillImage.DrawAs=ESlateBrushDrawType::Image;
   Progress->SetWidgetStyle(Style);
  }
  if (auto* Icon=Cast<UImage>(W); Icon && W->GetFName()==TEXT("TimerIcon"))
  {
   auto Brush=Icon->GetBrush(); Brush.DrawAs=ESlateBrushDrawType::NoDrawType; Icon->SetBrush(Brush);
  }
  if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName())) BP->OnVariableAdded(W->GetFName());
 });
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status!=BS_Error;
#else
 return false;
#endif
}

#if WITH_EDITOR
namespace
{
struct FFreeHudNode
{
 UWidget* Widget=nullptr;
 FVector2D Position=FVector2D::ZeroVector;
 FVector2D Size=FVector2D::ZeroVector;
 float LayoutScale=1.0f;
 TArray<FFreeHudNode> Children;
};
bool CaptureHudLayout(FFreeHudNode& Node)
{
 auto* Panel=Cast<UPanelWidget>(Node.Widget);
 if (!Panel) return true;
 FArrangedChildren Arranged(EVisibility::All);
 Panel->TakeWidget()->ArrangeChildren(FGeometry::MakeRoot(Node.Size,FSlateLayoutTransform()),Arranged);
 for (UWidget* Child:Panel->GetAllChildren())
 {
  FFreeHudNode Next; Next.Widget=Child;
  bool bFound=false;
  for (int32 I=0; I<Arranged.Num(); ++I)
   if (Arranged[I].Widget==Child->GetCachedWidget())
   {
    Next.Position=FVector2D(Arranged[I].Geometry.GetAccumulatedLayoutTransform().GetTranslation());
    Next.LayoutScale=Arranged[I].Geometry.GetAccumulatedLayoutTransform().GetScale();
    Next.Size=FVector2D(Arranged[I].Geometry.GetLocalSize());
    bFound=true; break;
   }
  if (!bFound) return false;
  if (!CaptureHudLayout(Next)) return false;
  Node.Children.Add(MoveTemp(Next));
 }
 return true;
}
UWidget* BuildFreeHud(UWidgetTree* Tree,const FFreeHudNode& Node)
{
 auto* Old=Cast<UPanelWidget>(Node.Widget);
 if (!Old) return Node.Widget;
 const FName Name=Old->GetFName();
 const FWidgetTransform Transform=Old->GetRenderTransform();
 const FVector2D Pivot=Old->GetRenderTransformPivot();
 const float Opacity=Old->GetRenderOpacity();
 const auto Visibility=Old->GetVisibility(); const auto Clipping=Old->GetClipping();
 Old->ClearChildren();
 Old->Rename(*MakeUniqueObjectName(GetTransientPackage(),Old->GetClass(),Name).ToString(),GetTransientPackage(),REN_DontCreateRedirectors);
 auto* Canvas=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),Name);
 Canvas->SetRenderTransform(Transform); Canvas->SetRenderTransformPivot(Pivot);
 Canvas->SetRenderOpacity(Opacity); Canvas->SetVisibility(Visibility); Canvas->SetClipping(Clipping);
 for (const auto& Child:Node.Children)
 {
  UWidget* Widget=BuildFreeHud(Tree,Child);
  auto* Slot=Canvas->AddChildToCanvas(Widget);
  Slot->SetPosition(Child.Position+(Child.LayoutScale-1.0f)*Widget->GetRenderTransformPivot()*Child.Size); Slot->SetSize(Child.Size.ComponentMax(FVector2D(1,1)));
  // Preserve scaling formerly supplied by ScaleBox's layout transform.
  auto Render=Widget->GetRenderTransform();
  Render.Scale*=Child.LayoutScale; Render.Translation*=Child.LayoutScale;
  Widget->SetRenderTransform(Render);
 }
 return Canvas;
}
}
#endif

bool UBattleTopHudWidget::EnableFreeLayout(UObject* Asset)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset);
 if (!BP || !BP->WidgetTree || !FSlateApplication::IsInitialized()) return false;
 UWidgetTree* Tree=BP->WidgetTree;
 auto* Frame=Tree->FindWidget(TEXT("TopHudFrame"));
 if (!Frame) return false;
 if (Cast<UCanvasPanel>(Frame)) return true;
 auto* Parent=Frame->GetParent(); if (!Parent) return false;
 const auto Slate=Frame->TakeWidget(); Slate->SlatePrepass(1.0f);
 FFreeHudNode Node; Node.Widget=Frame; Node.Size=FVector2D(Slate->GetDesiredSize());
 if (!CaptureHudLayout(Node)) return false;
 BP->Modify(); Tree->Modify();
 auto* Canvas=Cast<UCanvasPanel>(BuildFreeHud(Tree,Node));
 Parent->ReplaceChild(Frame,Canvas); Frame->Slot=nullptr;
 // The background keeps matching the HUD frame while foreground elements move independently.
 for (const TCHAR* Name:{TEXT("Image_35"),TEXT("TopHudBackgroundImage")})
  if (auto* Image=Tree->FindWidget(Name))
   if (auto* Slot=Cast<UCanvasPanelSlot>(Image->Slot))
   {
    Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0)); Slot->SetZOrder(-1);
   }
 TSet<FName> Names;
 Tree->ForEachWidget([BP,&Names](UWidget* W) {
  Names.Add(W->GetFName()); W->bIsVariable=true;
  if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName())) BP->OnVariableAdded(W->GetFName());
 });
 for (const auto& Animation:BP->Animations) if (Animation) Names.Add(Animation->GetFName());
 for (auto It=BP->WidgetVariableNameToGuidMap.CreateIterator();It;++It)
  if (!Names.Contains(It.Key())) It.RemoveCurrent();
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status!=BS_Error;
#else
 return false;
#endif
}

void UBattleTopHudWidget::RefreshTempleState()
{
 UGameInstance* Instance = GetGameInstance();
 auto* Clock = Instance ? Instance->GetSubsystem<URealTimeClockSubsystem>() : nullptr;
 auto* Schedule = Instance ? Instance->GetSubsystem<UBattleScheduleSubsystem>() : nullptr;
 const FDateTime Now = Clock ? Clock->GetLocalTime() : FDateTime::Now();
 FDateTime Next;
 const bool bHasNext = Schedule && Schedule->GetNextStart(Now, Next);
 const FDateTime Start = Schedule ? Schedule->GetTempleIntervalStart(Now) : Now;
 Apply(FBattleTopHudPresenter::BuildTemple(Now, Start, Next, bHasNext));
}

void UBattleTopHudWidget::PrepareTempleLayout()
{
 if (bTempleLayoutPrepared || !WidgetTree) return;
 auto* Frame = WidgetTree->FindWidget(TEXT("TopHudFrame"));
 auto* Canvas = Cast<UCanvasPanel>(Frame);
 if (!Canvas)
 {
  if (auto* Overlay = Cast<UOverlay>(Frame))
  {
   Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
   auto* TempleOverlaySlot = Overlay->AddChildToOverlay(Canvas);
   TempleOverlaySlot->SetHorizontalAlignment(HAlign_Fill); TempleOverlaySlot->SetVerticalAlignment(VAlign_Fill);
  }
 }
 if (!Canvas) return;
 bTempleLayoutPrepared = true;
 // Keep the battle artwork, size and screen position. Fit temple fields into its three visible regions.
 // Hidden retains the original desired size in the native fallback layout.
 if (TopHudBar) TopHudBar->SetVisibility(ESlateVisibility::Hidden);
 auto Place = [Canvas](UWidget* Widget, float Left, float Right, float Top, float Height)
 {
  if (!Widget) return;
  Widget->RemoveFromParent(); Widget->SetRenderTransform(FWidgetTransform());
  auto* Slot = Canvas->AddChildToCanvas(Widget);
  Slot->SetAnchors(FAnchors(Left/718.f,0.f,Right/718.f,0.f));
  Slot->SetOffsets(FMargin(0.f,Top,0.f,Height));
  Slot->SetZOrder(1);
 };
 auto TextStyle = [](UTextBlock* Text, int32 Size)
 {
  if (!Text) return;
  auto Font = Text->GetFont(); Font.Size = Size; Text->SetFont(Font);
  Text->SetJustification(ETextJustify::Center);
 };
 auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TurnLabelText")));
 Place(Label,50.f,170.f,5.f,28.f); TextStyle(Label,14);
 Place(TurnValueText,50.f,170.f,28.f,40.f); TextStyle(TurnValueText,20);
 if (TurnTotalText) TurnTotalText->SetVisibility(ESlateVisibility::Collapsed);
 Place(GuideText,176.f,544.f,9.f,30.f); TextStyle(GuideText,16);
 Place(Gauge,180.f,540.f,48.f,8.f);
 Place(TimerText,596.f,672.f,28.f,40.f); TextStyle(TimerText,18);
 TimerContent = TimerText;
 auto* TimerLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TempleTimerLabelText"));
 if (Label) { TimerLabel->SetFont(Label->GetFont()); TimerLabel->SetColorAndOpacity(Label->GetColorAndOpacity()); }
 Place(TimerLabel,552.f,672.f,5.f,28.f); TextStyle(TimerLabel,14);
}
