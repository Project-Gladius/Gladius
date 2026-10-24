#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/Battle/BattleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"

void UBossTelegraphWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	HideTelegraph();
}

void UBossTelegraphWidget::NativeDestruct()
{
	if (ABattleManager* Manager = BattleManager.Get())
	{
		Manager->OnBossTelegraph.RemoveDynamic(this, &UBossTelegraphWidget::HandleBossTelegraph);
	}
	Super::NativeDestruct();
}

void UBossTelegraphWidget::BindToBattleManager(ABattleManager* InBattleManager)
{
	if (ABattleManager* OldManager = BattleManager.Get())
	{
		OldManager->OnBossTelegraph.RemoveDynamic(this, &UBossTelegraphWidget::HandleBossTelegraph);
	}

	BattleManager = InBattleManager;
	if (InBattleManager)
	{
		InBattleManager->OnBossTelegraph.AddUniqueDynamic(this, &UBossTelegraphWidget::HandleBossTelegraph);
	}
}

void UBossTelegraphWidget::HandleBossTelegraph(const FString& MotionId, const FString& TelegraphText)
{
	ShowTelegraph(TelegraphText);
}

void UBossTelegraphWidget::ShowTelegraph(const FString& TelegraphText)
{
	const FString CleanText = TelegraphText.TrimStartAndEnd();
	if (CleanText.IsEmpty() || !Box || !MessageText)
	{
		return;
	}

	MessageText->SetText(FText::FromString(CleanText));
	bShowing = true;
	ElapsedSeconds = 0.0f;
	UpdateBoxPosition();
	Box->SetRenderOpacity(FadeSeconds > 0.0f ? 0.0f : 1.0f);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBossTelegraphWidget::HideTelegraph()
{
	bShowing = false;
	if (Box)
	{
		Box->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBossTelegraphWidget::UpdateBoxPosition()
{
	if (!BoxSlot)
	{
		return;
	}

	//화면 크기(위젯 좌표계, DPI 배율 적용 후)
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;

	FVector2D Position(ViewportSize.X * 0.5f + FallbackScreenOffset.X, FallbackScreenOffset.Y);

	const ABattleManager* Manager = BattleManager.Get();
	const AActor* Boss = Manager ? Manager->GetBossUnit() : nullptr;
	APlayerController* PC = GetOwningPlayer();
	FVector2D BossScreen;
	if (Boss && PC
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Boss->GetActorLocation() + BossWorldOffset, BossScreen, false))
	{
		Position = BossScreen + BelowBossScreenOffset;
	}

	//상자가 화면 밖으로 나가지 않게 보정 (기준점은 상자 위쪽 가운데)
	const float HalfWidth = Style.BoxSize.X * 0.5f;
	const float MinX = HalfWidth + ScreenEdgeMargin;
	const float MaxX = FMath::Max(MinX, ViewportSize.X - HalfWidth - ScreenEdgeMargin);
	const float MaxY = FMath::Max(ScreenEdgeMargin, ViewportSize.Y - Style.BoxSize.Y - ScreenEdgeMargin);
	Position.X = FMath::Clamp(Position.X, MinX, MaxX);
	Position.Y = FMath::Clamp(Position.Y, ScreenEdgeMargin, MaxY);

	BoxSlot->SetPosition(Position);
}

bool UBossTelegraphWidget::IsPreparePhase() const
{
	//BattleManager 없이 ShowTelegraph만 호출해 쓰는 경우에는 시간만 보고 사라짐
	const ABattleManager* Manager = BattleManager.Get();
	return !Manager || Manager->Phase == EBattlePhase::Prepare;
}

void UBossTelegraphWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bShowing || !Box)
	{
		return;
	}

	//Prepare가 끝나면(입력 수집 시작, 전투 중단 등) 남은 시간과 관계없이 바로 사라지기 시작
	if (!IsPreparePhase())
	{
		ElapsedSeconds = FMath::Max(ElapsedSeconds, DisplaySeconds - FadeSeconds);
	}

	ElapsedSeconds += InDeltaTime;
	if (ElapsedSeconds >= DisplaySeconds)
	{
		HideTelegraph();
		return;
	}

	//카메라가 움직여도 보스 아래에 붙어 있도록 매 프레임 갱신
	UpdateBoxPosition();

	float Opacity = 1.0f;
	if (FadeSeconds > 0.0f)
	{
		const float FadeIn = ElapsedSeconds / FadeSeconds;
		const float FadeOut = (DisplaySeconds - ElapsedSeconds) / FadeSeconds;
		Opacity = FMath::Clamp(FMath::Min(FadeIn, FadeOut), 0.0f, 1.0f);
	}
	Box->SetRenderOpacity(Opacity);
}

void UBossTelegraphWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Canvas;

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TelegraphBox"));
	SizeBox->SetWidthOverride(Style.BoxSize.X);
	SizeBox->SetHeightOverride(Style.BoxSize.Y);
	Box = SizeBox;

	//좌상단 기준 화면 좌표로 배치하고 상자의 위쪽 가운데를 기준점으로 맞춤. 위치는 매 프레임 보스 아래로 옮긴다
	BoxSlot = Canvas->AddChildToCanvas(SizeBox);
	BoxSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BoxSlot->SetAutoSize(true);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
	SizeBox->AddChild(Overlay);

	//바깥 테두리 -> 안쪽 테두리 -> 글자
	UBorder* OuterFrame = WidgetTree->ConstructWidget<UBorder>();
	OuterFrame->SetBrush(Style.OuterFrameBrush);
	OuterFrame->SetPadding(Style.FramePadding);
	if (UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(OuterFrame))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* InnerFrame = WidgetTree->ConstructWidget<UBorder>();
	InnerFrame->SetBrush(Style.InnerFrameBrush);
	InnerFrame->SetPadding(Style.TextPadding);
	InnerFrame->SetHorizontalAlignment(HAlign_Center);
	InnerFrame->SetVerticalAlignment(VAlign_Center);
	OuterFrame->SetContent(InnerFrame);

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	FSlateFontInfo Font = Style.FontFamily ? FSlateFontInfo(Style.FontFamily.Get(), Style.FontSize) : MessageText->GetFont();
	Font.Size = Style.FontSize;
	MessageText->SetFont(Font);
	MessageText->SetColorAndOpacity(FSlateColor(Style.TextColor));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(false);

	//30자를 넘는 긴 문구는 잘리지 않고 한 줄 그대로 작게 줄어듦
	UScaleBox* Shrink = WidgetTree->ConstructWidget<UScaleBox>();
	Shrink->SetStretch(EStretch::ScaleToFit);
	Shrink->SetStretchDirection(EStretchDirection::DownOnly);
	Shrink->AddChild(MessageText);
	InnerFrame->SetContent(Shrink);

	//장식: 모서리 4개 + 위·아래 가운데 (테두리 선에 걸치도록 반만큼 바깥으로)
	const float Half = Style.OrnamentSize.Y * 0.5f;
	AddOrnament(Overlay, HAlign_Left, VAlign_Top, FVector2D(6.0f, 6.0f));
	AddOrnament(Overlay, HAlign_Right, VAlign_Top, FVector2D(-6.0f, 6.0f));
	AddOrnament(Overlay, HAlign_Left, VAlign_Bottom, FVector2D(6.0f, -6.0f));
	AddOrnament(Overlay, HAlign_Right, VAlign_Bottom, FVector2D(-6.0f, -6.0f));
	AddOrnament(Overlay, HAlign_Center, VAlign_Top, FVector2D(0.0f, -Half));
	AddOrnament(Overlay, HAlign_Center, VAlign_Bottom, FVector2D(0.0f, Half));
}

void UBossTelegraphWidget::AddOrnament(UOverlay* Overlay, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, const FVector2D& Translation)
{
	UImage* Ornament = WidgetTree->ConstructWidget<UImage>();
	Ornament->SetBrush(Style.OrnamentBrush);
	Ornament->SetDesiredSizeOverride(Style.OrnamentSize);
	Ornament->SetRenderTransformAngle(Style.OrnamentAngle);
	Ornament->SetRenderTranslation(Translation);

	if (UOverlaySlot* OrnamentSlot = Overlay->AddChildToOverlay(Ornament))
	{
		OrnamentSlot->SetHorizontalAlignment(HAlign);
		OrnamentSlot->SetVerticalAlignment(VAlign);
	}
}
