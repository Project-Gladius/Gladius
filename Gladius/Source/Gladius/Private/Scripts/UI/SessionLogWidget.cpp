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
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"

USessionLogWidget::USessionLogWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/UI/Fonts/MaruBuri/MaruBuri_Regular_Font.MaruBuri_Regular_Font"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> Frame(TEXT("/Game/UI/UIDesign/T_NoticePanel.T_NoticePanel"));
 LogFont = FSlateFontInfo(Font.Object, 18);
 BackgroundBrush.SetResourceObject(Frame.Object);
 BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
 BackgroundBrush.ImageSize = FVector2D(600, 100);
}

TSharedRef<SWidget> USessionLogWidget::RebuildWidget()
{
 // Build before Slate takes the root, including worlds without a local player.
 BuildDefaultLayout();
 return Super::RebuildWidget();
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
 RebuildEntries();
 RefreshStyle();
}

void USessionLogWidget::NativeDestruct()
{
 if (BoundLog.IsValid()) BoundLog->OnEntryAdded.RemoveAll(this);
 BoundLog.Reset();
 PendingEntries.Reset();
 bHasCurrentEntry = false;
 Super::NativeDestruct();
}

void USessionLogWidget::BuildDefaultLayout()
{
 if (!WidgetTree || WidgetTree->RootWidget) return;
 UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
 Root->SetVisibility(ESlateVisibility::HitTestInvisible);
 WidgetTree->RootWidget = Root;
 LogWindow = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LogWindow"));
 UCanvasPanelSlot* LayoutSlot = Root->AddChildToCanvas(LogWindow);
 LayoutSlot->SetAutoSize(true);
 UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
 LogWindow->SetContent(Layers);
 LogBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogBackground"));
 auto* ImageSlot = Layers->AddChildToOverlay(LogBackground);
 ImageSlot->SetHorizontalAlignment(HAlign_Fill);
 ImageSlot->SetVerticalAlignment(VAlign_Fill);
 LogScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LogScrollBox"));
 LogScrollBox->SetOrientation(Orient_Horizontal);
 LogScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
 LogScrollBox->SetAllowOverscroll(false);
 LogScrollBox->SetClipping(EWidgetClipping::ClipToBoundsAlways);
 LogScrollBox->SetVisibility(ESlateVisibility::HitTestInvisible);
 auto* TextSlot = Layers->AddChildToOverlay(LogScrollBox);
 TextSlot->SetHorizontalAlignment(HAlign_Fill);
 TextSlot->SetVerticalAlignment(VAlign_Center);
 LogMessages = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LogMessages"));
 LogScrollBox->AddChild(LogMessages);
 CurrentText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LogCurrentMessage"));
 CurrentText->SetAutoWrapText(false);
 CurrentText->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
 LogMessages->AddChildToVerticalBox(CurrentText);
}

void USessionLogWidget::RefreshStyle()
{
 if (LogBackground) { LogBackground->SetBrush(BackgroundBrush); LogBackground->SetColorAndOpacity(BackgroundTint); }
 if (LogWindow)
 {
  LogWindow->SetWidthOverride(FMath::Max(1.f, WindowSize.X));
  LogWindow->SetHeightOverride(FMath::Max(1.f, WindowSize.Y));
  if (auto* WindowSlot = Cast<UCanvasPanelSlot>(LogWindow->Slot))
  {
   WindowSlot->SetAnchors(FAnchors(1, 0));
   WindowSlot->SetAlignment(FVector2D(1, 0));
   WindowSlot->SetPosition(WindowOffset);
  }
 }
 if (LogScrollBox)
  if (auto* TextLayoutSlot = Cast<UOverlaySlot>(LogScrollBox->Slot)) TextLayoutSlot->SetPadding(TextPadding);
 ApplyCurrentStyle();
 if (IsDesignTime()) AppendMessage(TEXT("[운영진 공지] 글라디우스에 오신 것을 환영합니다."), ESessionLogEvent::Admin);
}

void USessionLogWidget::ApplyCurrentStyle()
{
 if (!CurrentText) return;
 CurrentText->SetFont(LogFont);
 const auto Event = CurrentEntry.Event;
 const FLinearColor Color = Event == ESessionLogEvent::Admin ? AdminColor
  : Event == ESessionLogEvent::AdminUrgent ? AdminUrgentColor : Event == ESessionLogEvent::Event ? EventColor : LogColor;
 CurrentText->SetColorAndOpacity(FSlateColor(Color));
}

void USessionLogWidget::ShowEntry(const FSessionLogEntry& Entry)
{
 CurrentEntry = Entry;
 bHasCurrentEntry = true;
 MessageElapsed = 0;
 // Chat input may contain newlines; the ticker always renders exactly one line.
 FString SingleLine = Entry.Message;
 SingleLine.ReplaceInline(TEXT("\r"), TEXT(" "));
 SingleLine.ReplaceInline(TEXT("\n"), TEXT(" "));
 SingleLine.ReplaceInline(TEXT("\t"), TEXT(" "));
 SingleLine.ReplaceInline(TEXT("\u2028"), TEXT(" "));
 SingleLine.ReplaceInline(TEXT("\u2029"), TEXT(" "));
 if (CurrentText) CurrentText->SetText(FText::FromString(SingleLine));
 if (LogScrollBox) LogScrollBox->SetScrollOffset(0);
 ApplyCurrentStyle();
}

void USessionLogWidget::AppendMessage(const FString& Message, ESessionLogEvent Event)
{
 FSessionLogEntry Entry; Entry.Message = Message; Entry.Event = Event;
 ShowEntry(Entry);
}

void USessionLogWidget::RebuildEntries()
{
 PendingEntries.Reset();
 bHasCurrentEntry = false;
 // History stays in the subsystem; a newly opened map resumes with the latest entry.
 if (BoundLog.IsValid() && !BoundLog->GetEntries().IsEmpty()) ShowEntry(BoundLog->GetEntries().Last());
}

void USessionLogWidget::HandleEntryAdded(const FSessionLogEntry& Entry)
{
 if (!bHasCurrentEntry) ShowEntry(Entry);
 else PendingEntries.Add(Entry);
}

void USessionLogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
 Super::NativeTick(MyGeometry, InDeltaTime);
 AdvanceTicker(InDeltaTime);
}

void USessionLogWidget::AdvanceTicker(float DeltaSeconds)
{
 if (!CurrentText || !LogScrollBox || !bHasCurrentEntry) return;
 ForceLayoutPrepass();
 const float ViewWidth = LogScrollBox->GetCachedGeometry().GetLocalSize().X;
 const float AvailableWidth = ViewWidth > 0 ? ViewWidth : FMath::Max(1.f, WindowSize.X - TextPadding.Left - TextPadding.Right);
 const float Overflow = FMath::Max(0.f, CurrentText->GetDesiredSize().X - AvailableWidth);
 const float Speed = FMath::Max(1.f, ScrollSpeed);
 const float StartHold = FMath::Max(0.f, StartHoldSeconds);
 const float Duration = Overflow > 0 ? StartHold + Overflow / Speed + FMath::Max(0.f, EndHoldSeconds) : FMath::Max(1.f, ShortMessageSeconds);
 MessageElapsed += FMath::Max(0.f, DeltaSeconds);
 LogScrollBox->SetScrollOffset(FMath::Clamp((MessageElapsed - StartHold) * Speed, 0.f, Overflow));
 if (MessageElapsed < Duration) return;
 if (!PendingEntries.IsEmpty())
 {
  const FSessionLogEntry Next = PendingEntries[0];
  PendingEntries.RemoveAt(0);
  ShowEntry(Next);
 }
 else if (Overflow > 0)
 {
  MessageElapsed = 0;
  LogScrollBox->SetScrollOffset(0);
 }
}
