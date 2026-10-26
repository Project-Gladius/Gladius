#include "Scripts/UI/Leaderboard/LeaderboardRowWidget.h"
#include "LeaderboardLayoutHelpers.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"

namespace LeaderboardRowLayout
{
	FSlateBrush MakeEmptyBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Brush;
	}

	UScaleBox* WrapShrinkToFit(UWidgetTree* Tree, UWidget* Content, EHorizontalAlignment HAlign)
	{
		UScaleBox* Scale = GladiusUI::ConstructNamed<UScaleBox>(Tree);
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Scale->AddChild(Content)))
		{
			ScaleSlot->SetHorizontalAlignment(HAlign);
			ScaleSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Scale;
	}

	USizeBox* AddFixedCell(UWidgetTree* Tree, UHorizontalBox* Row, FName Name, float Width, UWidget* Content, EHorizontalAlignment HAlign)
	{
		USizeBox* Cell = GladiusUI::ConstructNamed<USizeBox>(Tree, Name);
		Cell->SetWidthOverride(Width);
		if (USizeBoxSlot* CellSlot = Cast<USizeBoxSlot>(Cell->AddChild(WrapShrinkToFit(Tree, Content, HAlign))))
		{
			CellSlot->SetHorizontalAlignment(HAlign);
			CellSlot->SetVerticalAlignment(VAlign_Center);
			CellSlot->SetPadding(FMargin(CellPadding, 0.0f));
		}
		Row->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Fill);
		return Cell;
	}

	void AddFillCell(UWidgetTree* Tree, UHorizontalBox* Row, UWidget* Content)
	{
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(WrapShrinkToFit(Tree, Content, HAlign_Left)))
		{
			RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Center);
			RowSlot->SetPadding(FMargin(CellPadding, 0.0f));
		}
	}
}


void ULeaderboardRowWidget::BindInlineWidgets(UWidgetTree* Tree, const FString& Prefix, UWidget* Root)
{
 InlineRoot = Root;
 InlineDesignerOpacity = Root ? Root->GetRenderOpacity() : 1.0f;
 auto Find = [&](auto& Member, const TCHAR* Name)
 {
  using T = std::remove_reference_t<decltype(*Member)>;
  Member = Cast<T>(Tree->FindWidget(FName(*(Prefix + Name))));
 };
 Find(RowBackground, TEXT("RowBackground")); Find(Medal, TEXT("Medal"));
 Find(RankText, TEXT("RankText")); Find(NameText, TEXT("NameText"));
 Find(CharacterText, TEXT("CharacterText")); Find(HealthCell, TEXT("HealthCell"));
 Find(HealthText, TEXT("HealthText")); Find(DamageText, TEXT("DamageText"));
 CaptureDesignerLook();
}

void ULeaderboardRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//Designer에서 만든 레이아웃이 없을 때(C++ 클래스 단독 사용)만 기본 도형 레이아웃 생성
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout(WidgetTree);
	}
	ResolveWidgets();
	CaptureDesignerLook();
}

void ULeaderboardRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	//Designer 미리보기: 1위 줄 예시
	if (IsDesignTime())
	{
		ResolveWidgets();
		CaptureDesignerLook();
		FLeaderboardRow Preview;
		Preview.Rank = 1;
		Preview.Name = TEXT("사용자 1");
		Preview.CharacterName = TEXT("글라디우스");
		Preview.Damage = 98540;
		Preview.Health = 5;
		Preview.bEmpty = false;
		SetRow(Preview, bPreviewShowHealth);
	}
}

void ULeaderboardRowWidget::ResolveWidgets()
{
	if (!WidgetTree || InlineRoot)
	{
		return;
	}
 RowBackground=nullptr; Medal=nullptr; RankText=nullptr; NameText=nullptr;
 CharacterText=nullptr; HealthCell=nullptr; HealthText=nullptr; DamageText=nullptr;
	auto Find = [this](auto& Member, const TCHAR* Name)
	{
		using FWidgetType = std::remove_reference_t<decltype(*Member)>;
		if (!Member)
		{
			Member = Cast<FWidgetType>(WidgetTree->FindWidget(Name));
		}
	};
	Find(RowBackground, TEXT("RowBackground"));
	Find(Medal, TEXT("Medal"));
	Find(RankText, TEXT("RankText"));
	Find(NameText, TEXT("NameText"));
	Find(CharacterText, TEXT("CharacterText"));
	Find(HealthCell, TEXT("HealthCell"));
	Find(HealthText, TEXT("HealthText"));
	Find(DamageText, TEXT("DamageText"));
}

void ULeaderboardRowWidget::CaptureDesignerLook()
{
	if (bCapturedDesignerLook)
	{
		return;
	}
	DesignerRowBrush = RowBackground ? RowBackground->Background : Style.RowBrush;
	DesignerDamageColor = DamageText ? DamageText->GetColorAndOpacity() : FSlateColor(Style.TextColor);
	bCapturedDesignerLook = true;
}

void ULeaderboardRowWidget::SetRow(const FLeaderboardRow& Row, bool bShowHealth)
{
	ResolveWidgets();
	CaptureDesignerLook();

	if (HealthCell)
	{
		HealthCell->SetVisibility(bShowHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	const bool bFirst = !Row.bEmpty && Row.Rank == 1;
	const int32 MedalIndex = Row.Rank - 1;

	if (RowBackground)
	{
		RowBackground->SetBrush(bFirst && bHighlightFirstRow ? Style.FirstRowBrush : DesignerRowBrush);
	}
	if (Medal)
	{
		Medal->SetBrush(!Row.bEmpty && Style.MedalBrushes.IsValidIndex(MedalIndex)
			? Style.MedalBrushes[MedalIndex]
			: LeaderboardRowLayout::MakeEmptyBrush());
	}
	if (InlineRoot) InlineRoot->SetRenderOpacity(InlineDesignerOpacity * (Row.bDimmed ? Style.DimmedOpacity : 1.0f));
	else SetRenderOpacity(Row.bDimmed ? Style.DimmedOpacity : 1.0f);

	auto SetText = [](UTextBlock* Text, const FText& Value)
	{
		if (Text)
		{
			Text->SetText(Value);
		}
	};

	if (Row.bEmpty)
	{
		for (UTextBlock* Text : { RankText.Get(), NameText.Get(), CharacterText.Get(), HealthText.Get(), DamageText.Get() })
		{
			SetText(Text, FText::GetEmpty());
		}
	}
	else
	{
		SetText(RankText, FText::AsNumber(Row.Rank));
		SetText(NameText, FText::FromString(Row.Name));
		SetText(CharacterText, FText::FromString(Row.CharacterName));
		SetText(HealthText, FText::AsNumber(Row.Health));
		SetText(DamageText, FText::FromString(FLeaderboardBuilder::FormatDamage(Row.Damage)));
	}

	if (DamageText)
	{
		DamageText->SetColorAndOpacity(bFirst ? FSlateColor(Style.FirstDamageColor) : DesignerDamageColor);
	}

	OnRowChanged(Row, bShowHealth);
}

void ULeaderboardRowWidget::BuildDefaultLayout(UWidgetTree* Tree) const
{
	using GladiusUI::ConstructNamed;

	if (!Tree)
	{
		return;
	}

	auto MakeText = [&](FName Name, int32 FontSize)
	{
		UTextBlock* Text = ConstructNamed<UTextBlock>(Tree, Name);
		GladiusUI::ApplyFont(Text, Style.FontFamily, FontSize);
		Text->SetColorAndOpacity(FSlateColor(Style.TextColor));
		return Text;
	};

	UBorder* Background = ConstructNamed<UBorder>(Tree, TEXT("RowBackground"));
	Background->SetBrush(Style.RowBrush);
	Background->SetPadding(FMargin(0.0f));
	Tree->RootWidget = Background;

	USizeBox* Height = ConstructNamed<USizeBox>(Tree, TEXT("RowSize"));
	Height->SetHeightOverride(Style.RowHeight);
	Height->SetWidthOverride(Style.BoardWidth - 32.0f);
	Background->SetContent(Height);

	UHorizontalBox* Row = ConstructNamed<UHorizontalBox>(Tree, TEXT("RowCells"));
	Height->AddChild(Row);

	//순위: 1~3위는 메달 브러시 위에 숫자
	UBorder* MedalBorder = ConstructNamed<UBorder>(Tree, TEXT("Medal"));
	MedalBorder->SetHorizontalAlignment(HAlign_Center);
	MedalBorder->SetVerticalAlignment(VAlign_Center);
	MedalBorder->SetPadding(FMargin(0.0f));
	MedalBorder->SetContent(MakeText(TEXT("RankText"), Style.RowFontSize));
	USizeBox* MedalBox = ConstructNamed<USizeBox>(Tree);
	MedalBox->SetWidthOverride(LeaderboardRowLayout::MedalSize);
	MedalBox->SetHeightOverride(LeaderboardRowLayout::MedalSize);
	MedalBox->AddChild(MedalBorder);
	LeaderboardRowLayout::AddFixedCell(Tree, Row, TEXT("RankCell"), Style.RankColumnWidth, MedalBox, HAlign_Center);

	LeaderboardRowLayout::AddFillCell(Tree, Row, MakeText(TEXT("NameText"), Style.RowFontSize));
	LeaderboardRowLayout::AddFixedCell(Tree, Row, TEXT("CharacterCell"), Style.CharacterColumnWidth, MakeText(TEXT("CharacterText"), Style.RowFontSize), HAlign_Left);
	LeaderboardRowLayout::AddFixedCell(Tree, Row, TEXT("HealthCell"), Style.HealthColumnWidth, MakeText(TEXT("HealthText"), Style.RowFontSize), HAlign_Center);
	LeaderboardRowLayout::AddFixedCell(Tree, Row, TEXT("DamageCell"), Style.DamageColumnWidth, MakeText(TEXT("DamageText"), Style.RowFontSize + 1), HAlign_Right);
}
