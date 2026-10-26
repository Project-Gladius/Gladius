#include "Scripts/UI/ChatLogWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WidgetComponent.h"
#include "Scripts/Temple/TempleBlessingDisplay.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/SlateBrush.h"

namespace
{
	void SetTextStyle(UTextBlock* Text, int32 Size, const FLinearColor& Color)
	{
		FSlateFontInfo Font(LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/MaruBuri/MaruBuri_Regular_Font.MaruBuri_Regular_Font")), Size);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
	}

	void SetRoundedBackground(UBorder* Border, float Radius, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius);
		Border->SetBrush(Brush);
		Border->SetBrushColor(Color);
	}
}

TSharedRef<SWidget> UChatLogWidget::RebuildWidget()
{
	// Native widgets can be built before a player context is available.
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UChatLogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultWidgetTree();
}

void UChatLogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ConfigureMessageViewport();
	if (ExpandButton) ExpandButton->OnClicked.AddUniqueDynamic(this, &UChatLogWidget::HandleExpandClicked);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		BoundLog = GameInstance->GetSubsystem<UChatLogSubsystem>();
		if (BoundLog.IsValid()) BoundLog->OnEntryAdded.AddUObject(this, &UChatLogWidget::HandleEntryAdded);
	}
	RefreshLayout();
	RebuildEntries();
}

void UChatLogWidget::NativeDestruct()
{
	if (BoundLog.IsValid()) BoundLog->OnEntryAdded.RemoveAll(this);
	BoundLog.Reset();
	if (ExpandButton) ExpandButton->OnClicked.RemoveDynamic(this, &UChatLogWidget::HandleExpandClicked);
	Super::NativeDestruct();
}

void UChatLogWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatPanel"));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Panel->SetPadding(FMargin(12.0f, 9.0f));
	SetRoundedBackground(Panel, 11.0f, FLinearColor(0.015f, 0.028f, 0.055f, 0.86f));
	UCanvasPanelSlot* PanelCanvasSlot = Canvas->AddChildToCanvas(Panel);
	PanelCanvasSlot->SetAnchors(FAnchors(1.0f, 1.0f));
	PanelCanvasSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	PanelSlot = PanelCanvasSlot;

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Content->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("CHAT LOG")));
	SetTextStyle(Title, 17, FLinearColor(0.91f, 0.76f, 0.45f));
	Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ExpandButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExpandButton"));
	ExpandButtonText = WidgetTree->ConstructWidget<UTextBlock>();
	SetTextStyle(ExpandButtonText, 13, FLinearColor::White);
	ExpandButton->SetContent(ExpandButtonText);
	Header->AddChildToHorizontalBox(ExpandButton);

	MessageViewport = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MessageViewport"));
	Content->AddChildToVerticalBox(MessageViewport)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MessageList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MessageList"));
	ConfigureMessageViewport();
}

void UChatLogWidget::ConfigureMessageViewport()
{
	if (!WidgetTree || !MessageList) return;
	if (!MessageViewport && MessageScrollBox)
	{
		// Preserve the authored parent slot while removing the old scrolling control.
		UPanelWidget* Parent = MessageScrollBox->GetParent();
		if (!Parent) return;
		MessageViewport = WidgetTree->ConstructWidget<UCanvasPanel>();
		const int32 Index = Parent->GetChildIndex(MessageScrollBox);
		UPanelSlot* SlotTemplate = DuplicateObject<UPanelSlot>(MessageScrollBox->Slot, WidgetTree);
		Parent->RemoveChild(MessageScrollBox);
		Parent->InsertChildAt(Index, MessageViewport, SlotTemplate);
	}
	if (!MessageViewport) return;
	MessageViewport->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	MessageViewport->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (MessageList->GetParent() != MessageViewport)
	{
		MessageList->RemoveFromParent();
		UCanvasPanelSlot* ListSlot = MessageViewport->AddChildToCanvas(MessageList);
		ListSlot->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
		ListSlot->SetOffsets(FMargin(0.0f));
		ListSlot->SetAutoSize(true);
		// Keep the list's full desired height, growing upward from its bottom edge.
		// Wrapped rows therefore never depend on a stale scroll offset.
		ListSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	}
}

void UChatLogWidget::SetExpansionAllowed(bool bAllowed)
{
	bExpansionAllowed = bAllowed;
	if (!bAllowed) bExpanded = false;
	RefreshLayout();
}

void UChatLogWidget::SetExpanded(bool bInExpanded)
{
	bExpanded = bExpansionAllowed && bInExpanded;
	RefreshLayout();
	RebuildEntries();
}

void UChatLogWidget::RefreshLayout()
{
	if (UCanvasPanelSlot* PanelCanvasSlot = PanelSlot.Get())
	{
		PanelCanvasSlot->SetPosition(FVector2D(-24.0f, -24.0f));
		PanelCanvasSlot->SetSize(bExpanded ? FVector2D(530.0f, 560.0f) : FVector2D(450.0f, 240.0f));
	}
	if (ExpandButton) ExpandButton->SetVisibility(bExpansionAllowed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (ExpandButtonText) ExpandButtonText->SetText(FText::FromString(bExpanded ? TEXT("축소 ▲") : TEXT("확장 ▼")));
}

void UChatLogWidget::RebuildEntries()
{
	if (!MessageList) return;
	MessageList->ClearChildren();
	if (BoundLog.IsValid())
	{
		const TArray<FChatLogEntry>& Entries = BoundLog->GetEntries();
		for (int32 Index = FMath::Max(0, Entries.Num() - 100); Index < Entries.Num(); ++Index)
		{
			AppendEntry(Entries[Index]);
		}
	}
	if (MessageList->GetChildrenCount() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
		Empty->SetText(FText::FromString(TEXT("채팅을 기다리는 중...")));
		SetTextStyle(Empty, 14, FLinearColor(0.65f, 0.70f, 0.78f));
		MessageList->AddChildToVerticalBox(Empty);
	}
}

void UChatLogWidget::AppendEntry(const FChatLogEntry& Entry)
{
	if (!MessageList || !WidgetTree) return;
	if (MessageList->GetChildrenCount() == 1 && BoundLog.IsValid() && BoundLog->GetEntries().Num() == 1)
	{
		MessageList->ClearChildren();
	}

	const FLinearColor NameColor = Entry.Speaker == EChatLogSpeaker::Goddess
		? FLinearColor(1.0f, 0.62f, 0.78f)
		: Entry.Speaker == EChatLogSpeaker::Admin || Entry.Speaker == EChatLogSpeaker::Temple
			? FLinearColor(1.0f, 0.82f, 0.32f)
			: FLinearColor(0.28f, 0.82f, 1.0f);
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>();
	Row->SetPadding(FMargin(8.0f, 5.0f));
	SetRoundedBackground(Row, 6.0f, FLinearColor(0.08f, 0.12f, 0.19f, 0.86f));
	MessageList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->SetContent(Line);
	USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>();
	NameBox->SetWidthOverride(104.0f);
	Line->AddChildToHorizontalBox(NameBox);
	UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>();
	Name->SetText(FText::FromString(Entry.DisplayName));
	Name->SetAutoWrapText(true);
	SetTextStyle(Name, 13, NameColor);
	NameBox->SetContent(Name);
	UTextBlock* Message = WidgetTree->ConstructWidget<UTextBlock>();
	Message->SetText(FText::FromString(Entry.Message));
	Message->SetAutoWrapText(true);
	SetTextStyle(Message, 14, Entry.Speaker == EChatLogSpeaker::Viewer
		? FLinearColor(0.96f, 0.97f, 1.0f) : NameColor);
	Line->AddChildToHorizontalBox(Message)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

void UChatLogWidget::HandleEntryAdded(const FChatLogEntry& Entry)
{
	if (!MessageList) return;
	if (MessageList->GetChildrenCount() >= 100) MessageList->RemoveChildAt(0);
	AppendEntry(Entry);
}

void UChatLogWidget::HandleExpandClicked()
{
	SetExpanded(!bExpanded);
}

void UChatLogWidget::ApplyBattleSideLayout(float Width, float Margin, float Top, float Height)
{
 if (UCanvasPanelSlot* Layout = PanelSlot.Get())
 {
  Layout->SetAnchors(FAnchors(1.f, 0.f));
  Layout->SetAlignment(FVector2D(1.f, 0.f));
  Layout->SetPosition(FVector2D(-Margin, Top));
  Layout->SetSize(FVector2D(Width, Height));
 }
}

void UChatLogWidget::SetFixedExpanded()
{
 bFitBelowBlessings = true;
 bExpansionAllowed = false;
 bExpanded = true;
 RefreshLayout();
}

void UChatLogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
 Super::NativeTick(MyGeometry, InDeltaTime);
 if (!bFitBelowBlessings) return;
 auto* Layout = PanelSlot.Get();
 auto* PC = UGameplayStatics::GetPlayerController(this, 0);
 if (!Layout || !PC || MyGeometry.GetLocalSize().Y <= 0) return;
 double RightmostX = -TNumericLimits<double>::Max();
 double BlessingBottom = 0;
 for (TActorIterator<ATempleBlessingDisplay> It(GetWorld()); It; ++It)
 {
  if (!It->Card || It->IsHidden()) continue;
  FVector2D Position;
  if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, It->Card->GetComponentLocation(), Position, false)
   && Position.X > RightmostX)
  {
   RightmostX = Position.X;
   BlessingBottom = Position.Y + It->Card->GetDrawSize().Y * (1.f - It->Card->GetPivot().Y);
  }
 }
 // Keep the lower edge fixed, leaving a small gap below the rightmost blessing card.
 const double AvailableHeight = MyGeometry.GetLocalSize().Y - 24.0 - BlessingBottom - 12.0;
 const float Height = FMath::Clamp(float(AvailableHeight), 1.f, 480.f);
 const FVector2D Size(530.f, Height);
 if (!Layout->GetSize().Equals(Size, .1f)) Layout->SetSize(Size);
}
