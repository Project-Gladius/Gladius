#include "Scripts/UI/SessionLogWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"

USessionLogWidget::USessionLogWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/UI/Fonts/MaruBuri/MaruBuri_Regular_Font.MaruBuri_Regular_Font"));
 LogFont = FSlateFontInfo(Font.Object, 14);
 BackgroundBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
 BackgroundBrush.OutlineSettings = FSlateBrushOutlineSettings(8.f);
 BackgroundBrush.TintColor = FSlateColor(FLinearColor(0.015f, 0.028f, 0.055f, 0.86f));
}

void USessionLogWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 BuildDefaultLayout();
}

void USessionLogWidget::NativePreConstruct()
{
 Super::NativePreConstruct();
 BuildDefaultLayout();
 RefreshStyle();
}

void USessionLogWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (UGameInstance* Instance = GetGameInstance())
 {
  BoundLog = Instance->GetSubsystem<USessionLogSubsystem>();
  if (BoundLog.IsValid())
  {
   BoundLog->OnEntryAdded.RemoveAll(this);
   BoundLog->OnEntryAdded.AddUObject(this, &USessionLogWidget::HandleEntryAdded);
  }
 }
 RefreshStyle();
}

void USessionLogWidget::NativeDestruct()
{
 if (BoundLog.IsValid()) BoundLog->OnEntryAdded.RemoveAll(this);
 BoundLog.Reset();
 Super::NativeDestruct();
}

void USessionLogWidget::BuildDefaultLayout()
{
 if (!WidgetTree || WidgetTree->RootWidget) return;
 UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
 Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
 WidgetTree->RootWidget = Root;
 LogWindow = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LogWindow"));
 UCanvasPanelSlot* LayoutSlot = Root->AddChildToCanvas(LogWindow);
 LayoutSlot->SetAnchors(FAnchors(0, 1));
 LayoutSlot->SetAlignment(FVector2D(0, 1));
 LayoutSlot->SetAutoSize(true);
 UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
 LogWindow->SetContent(Layers);
 LogBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogBackground"));
 LogBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
 auto* ImageSlot = Layers->AddChildToOverlay(LogBackground);
 ImageSlot->SetHorizontalAlignment(HAlign_Fill);
 ImageSlot->SetVerticalAlignment(VAlign_Fill);
 LogScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LogScrollBox"));
 auto* TextSlot = Layers->AddChildToOverlay(LogScrollBox);
 TextSlot->SetHorizontalAlignment(HAlign_Fill);
 TextSlot->SetVerticalAlignment(VAlign_Fill);
 LogMessages = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LogMessages"));
 LogScrollBox->AddChild(LogMessages);
}

void USessionLogWidget::RefreshStyle()
{
 if (LogBackground) { LogBackground->SetBrush(BackgroundBrush); LogBackground->SetColorAndOpacity(BackgroundTint); }
 if (LogWindow)
 {
  LogWindow->SetWidthOverride(FMath::Max(1.f, WindowSize.X));
  LogWindow->SetHeightOverride(FMath::Max(1.f, WindowSize.Y));
  if (auto* LayoutSlot = Cast<UCanvasPanelSlot>(LogWindow->Slot)) LayoutSlot->SetPosition(WindowOffset);
 }
 if (LogScrollBox)
  if (auto* LayoutSlot = Cast<UOverlaySlot>(LogScrollBox->Slot)) LayoutSlot->SetPadding(TextPadding);
 RebuildEntries();
}

void USessionLogWidget::AppendMessage(const FString& Message, ESessionLogEvent Event)
{
 if (!WidgetTree || !LogMessages) return;
 UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
 Text->SetText(FText::FromString(Message));
 Text->SetFont(LogFont);
 const FLinearColor Color = Event == ESessionLogEvent::Admin ? AdminColor
  : Event == ESessionLogEvent::AdminUrgent ? AdminUrgentColor : Event == ESessionLogEvent::Event ? EventColor : LogColor;
 Text->SetColorAndOpacity(FSlateColor(Color));
 Text->SetAutoWrapText(true);
 LogMessages->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 0, 0, FMath::Max(0.f, LineSpacing)));
 while (LogMessages->GetChildrenCount() > FMath::Max(1, MaxVisibleEntries)) LogMessages->RemoveChildAt(0);
}

void USessionLogWidget::RebuildEntries()
{
 if (!LogMessages) return;
 LogMessages->ClearChildren();
 if (IsDesignTime()) AppendMessage(TEXT("닉네임 님이 로그인하셨습니다"));
 else if (BoundLog.IsValid())
 {
  const auto& Entries = BoundLog->GetEntries();
  for (int32 I = FMath::Max(0, Entries.Num() - FMath::Max(1, MaxVisibleEntries)); I < Entries.Num(); ++I)
   AppendMessage(Entries[I].Message, Entries[I].Event);
 }
 if (LogScrollBox) LogScrollBox->ScrollToEnd();
}

void USessionLogWidget::HandleEntryAdded(const FSessionLogEntry& Entry)
{
 AppendMessage(Entry.Message, Entry.Event);
 if (LogScrollBox) LogScrollBox->ScrollToEnd();
}
