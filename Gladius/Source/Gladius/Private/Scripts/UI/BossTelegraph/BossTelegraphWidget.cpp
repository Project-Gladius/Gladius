#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/UI/TutorialRichText.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Blueprint/WidgetLayoutLibrary.h"
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
#include "GameFramework/PlayerController.h"

void UBossTelegraphWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//Designer에서 만든 레이아웃이 없을 때(C++ 클래스 단독 사용)만 기본 도형 레이아웃 생성
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout(WidgetTree);
	}
	ResolveWidgets();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	bShowing = false;
	if (TelegraphBox)
	{
		TelegraphBox->SetVisibility(ESlateVisibility::Collapsed);
	}
    RichMessageText=TutorialText::Replace(WidgetTree,MessageText);
    if (RichMessageText && MessageText) TutorialText::Set(RichMessageText,MessageText->GetText().ToString());
}

void UBossTelegraphWidget::ResolveWidgets()
{
	if (!WidgetTree)
	{
		return;
	}
	//WBP에서는 BindWidgetOptional로 이미 연결됨. C++로 만든 기본 레이아웃은 이름으로 찾아 연결
	if (!TelegraphBox)
	{
		TelegraphBox = WidgetTree->FindWidget(TEXT("TelegraphBox"));
	}
	if (!MessageText)
	{
		MessageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MessageText")));
		//The authored WBP names this text block BT_MessageText.
		if (!MessageText) MessageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("BT_MessageText")));
	}
	//루트 Canvas Panel의 자식일 때만 위치를 옮길 수 있음
	BoxSlot = TelegraphBox ? Cast<UCanvasPanelSlot>(TelegraphBox->Slot) : nullptr;
	if (BoxSlot)
	{
		BoxSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	}
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
	ResolveWidgets();
	const FString CleanText = TelegraphText.TrimStartAndEnd();
	if (CleanText.IsEmpty() || !TelegraphBox)
	{
		return;
	}

	if (MessageText)
	{
		MessageText->SetText(FText::FromString(CleanText));
        if (RichMessageText) RichMessageText->SetText(FText::FromString(CleanText));
	}
	bShowing = true;
	ElapsedSeconds = 0.0f;
	UpdateBoxPosition();
	TelegraphBox->SetRenderOpacity(bUseBuiltInFade && FadeSeconds > 0.0f ? 0.0f : 1.0f);
	TelegraphBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	OnTelegraphShown(CleanText);
}

void UBossTelegraphWidget::HideTelegraph()
{
	const bool bWasShowing = bShowing;
	bShowing = false;
	if (TelegraphBox)
	{
		TelegraphBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (bWasShowing)
	{
		OnTelegraphHidden();
	}
}

FVector2D UBossTelegraphWidget::GetBoxSize() const
{
	//Designer에서 바꾼 크기를 우선 사용, 아직 측정 전이면 기본 레이아웃 크기
	const FVector2D Desired = TelegraphBox ? TelegraphBox->GetDesiredSize() : FVector2D::ZeroVector;
	return Desired.X > 1.0f && Desired.Y > 1.0f ? Desired : Style.BoxSize;
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
	if (!bTutorialMessage && Boss && PC
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Boss->GetActorLocation() + BossWorldOffset, BossScreen, false))
	{
		Position = BossScreen + BelowBossScreenOffset;
	}

	//상자가 화면 밖으로 나가지 않게 보정 (기준점은 상자 위쪽 가운데)
	const FVector2D BoxSize = GetBoxSize();
	const float HalfWidth = BoxSize.X * 0.5f;
	const float MinX = HalfWidth + ScreenEdgeMargin;
	const float MaxX = FMath::Max(MinX, ViewportSize.X - HalfWidth - ScreenEdgeMargin);
	const float MaxY = FMath::Max(ScreenEdgeMargin, ViewportSize.Y - BoxSize.Y - ScreenEdgeMargin);
	Position.X = FMath::Clamp(Position.X, MinX, MaxX);
	Position.Y = FMath::Clamp(Position.Y, ScreenEdgeMargin, MaxY);

	BoxSlot->SetPosition(Position);
}

bool UBossTelegraphWidget::IsPreparePhase() const
{
	//BattleManager 없이 ShowTelegraph만 호출해 쓰는 경우에는 시간만 보고 사라짐
	const ABattleManager* Manager = BattleManager.Get();
	return bTutorialMessage || !Manager || Manager->Phase == EBattlePhase::Prepare;
}

void UBossTelegraphWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bShowing || !TelegraphBox)
	{
		return;
	}

	const float Fade = bUseBuiltInFade ? FadeSeconds : 0.0f;

	//Prepare가 끝나면(입력 수집 시작, 전투 중단 등) 남은 시간과 관계없이 바로 사라지기 시작
	if (!IsPreparePhase())
	{
		ElapsedSeconds = FMath::Max(ElapsedSeconds, DisplaySeconds - Fade);
	}

	ElapsedSeconds += InDeltaTime;
	if (ElapsedSeconds >= DisplaySeconds)
	{
		HideTelegraph();
		return;
	}

	//카메라가 움직여도 보스 아래에 붙어 있도록 매 프레임 갱신
	UpdateBoxPosition();

	if (Fade > 0.0f)
	{
		const float FadeIn = ElapsedSeconds / Fade;
		const float FadeOut = (DisplaySeconds - ElapsedSeconds) / Fade;
		TelegraphBox->SetRenderOpacity(FMath::Clamp(FMath::Min(FadeIn, FadeOut), 0.0f, 1.0f));
	}
}

void UBossTelegraphWidget::BuildDefaultLayout(UWidgetTree* Tree) const
{
	using GladiusUI::ConstructNamed;

	if (!Tree)
	{
		return;
	}

	UCanvasPanel* Canvas = ConstructNamed<UCanvasPanel>(Tree, TEXT("RootCanvas"));
	Tree->RootWidget = Canvas;

	USizeBox* SizeBox = ConstructNamed<USizeBox>(Tree, TEXT("TelegraphBox"));
	SizeBox->SetWidthOverride(Style.BoxSize.X);
	SizeBox->SetHeightOverride(Style.BoxSize.Y);

	//좌상단 기준 화면 좌표로 배치하고 상자의 위쪽 가운데를 기준점으로 맞춤. 실행 중에는 매 프레임 보스 아래로 옮긴다
	UCanvasPanelSlot* BoxCanvasSlot = Canvas->AddChildToCanvas(SizeBox);
	BoxCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	BoxCanvasSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BoxCanvasSlot->SetPosition(FVector2D(Style.BoxSize.X * 0.5f + 20.0f, 20.0f)); //Designer 미리보기 위치
	BoxCanvasSlot->SetAutoSize(true);

	UOverlay* Overlay = ConstructNamed<UOverlay>(Tree, TEXT("TelegraphOverlay"));
	SizeBox->AddChild(Overlay);

	//바깥 테두리 -> 안쪽 테두리 -> 글자
	UBorder* OuterFrame = ConstructNamed<UBorder>(Tree, TEXT("OuterFrame"));
	OuterFrame->SetBrush(Style.OuterFrameBrush);
	OuterFrame->SetPadding(Style.FramePadding);
	if (UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(OuterFrame))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* InnerFrame = ConstructNamed<UBorder>(Tree, TEXT("InnerFrame"));
	InnerFrame->SetBrush(Style.InnerFrameBrush);
	InnerFrame->SetPadding(Style.TextPadding);
	InnerFrame->SetHorizontalAlignment(HAlign_Center);
	InnerFrame->SetVerticalAlignment(VAlign_Center);
	OuterFrame->SetContent(InnerFrame);

	UTextBlock* Message = ConstructNamed<UTextBlock>(Tree, TEXT("MessageText"));
	GladiusUI::ApplyFont(Message, Style.FontFamily, Style.FontSize);
	Message->SetColorAndOpacity(FSlateColor(Style.TextColor));
	Message->SetJustification(ETextJustify::Center);
	Message->SetAutoWrapText(false);
	Message->SetText(FText::FromString(TEXT("트루돈이 왼팔을 서서히 들어올려 공격을 준비합니다."))); //Designer 미리보기용

	//30자를 넘는 긴 문구는 잘리지 않고 한 줄 그대로 작게 줄어듦
	UScaleBox* Shrink = ConstructNamed<UScaleBox>(Tree);
	Shrink->SetStretch(EStretch::ScaleToFit);
	Shrink->SetStretchDirection(EStretchDirection::DownOnly);
	Shrink->AddChild(Message);
	InnerFrame->SetContent(Shrink);

	//장식: 모서리 4개 + 위·아래 가운데 (테두리 선에 걸치도록 반만큼 바깥으로)
	const float Half = Style.OrnamentSize.Y * 0.5f;
	struct FOrnamentPlace { const TCHAR* Name; EHorizontalAlignment H; EVerticalAlignment V; FVector2D Translation; };
	const FOrnamentPlace Places[] = {
		{ TEXT("OrnamentTopLeft"), HAlign_Left, VAlign_Top, FVector2D(6.0f, 6.0f) },
		{ TEXT("OrnamentTopRight"), HAlign_Right, VAlign_Top, FVector2D(-6.0f, 6.0f) },
		{ TEXT("OrnamentBottomLeft"), HAlign_Left, VAlign_Bottom, FVector2D(6.0f, -6.0f) },
		{ TEXT("OrnamentBottomRight"), HAlign_Right, VAlign_Bottom, FVector2D(-6.0f, -6.0f) },
		{ TEXT("OrnamentTop"), HAlign_Center, VAlign_Top, FVector2D(0.0f, -Half) },
		{ TEXT("OrnamentBottom"), HAlign_Center, VAlign_Bottom, FVector2D(0.0f, Half) },
	};
	for (const FOrnamentPlace& Place : Places)
	{
		UImage* Ornament = ConstructNamed<UImage>(Tree, Place.Name);
		Ornament->SetBrush(Style.OrnamentBrush);
		Ornament->SetDesiredSizeOverride(Style.OrnamentSize);
		Ornament->SetRenderTransformAngle(Style.OrnamentAngle);
		Ornament->SetRenderTranslation(Place.Translation);
		if (UOverlaySlot* OrnamentSlot = Overlay->AddChildToOverlay(Ornament))
		{
			OrnamentSlot->SetHorizontalAlignment(Place.H);
			OrnamentSlot->SetVerticalAlignment(Place.V);
		}
	}
}

void UBossTelegraphWidget::ShowTutorialMessage(const FString& RichText,float Seconds)
{
 if (!bTutorialMessage) NormalDisplaySeconds=DisplaySeconds;
 bTutorialMessage=true; DisplaySeconds=Seconds; ShowTelegraph(RichText);
}
void UBossTelegraphWidget::EndTutorial()
{
 if (bTutorialMessage) DisplaySeconds=NormalDisplaySeconds;
 bTutorialMessage=false; HideTelegraph();
}
