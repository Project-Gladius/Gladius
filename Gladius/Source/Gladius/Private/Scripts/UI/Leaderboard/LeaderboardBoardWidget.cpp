#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	constexpr float MedalSize = 32.0f;
	constexpr float CellPadding = 8.0f;

	FSlateBrush MakeEmptyBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Brush;
	}

	//텍스트가 칸보다 길면 잘리지 않고 작게 줄어들도록 ScaleBox로 감쌈
	UScaleBox* WrapShrinkToFit(UWidgetTree* Tree, UWidget* Content, EHorizontalAlignment HAlign)
	{
		UScaleBox* Scale = Tree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Scale->AddChild(Content)))
		{
			ScaleSlot->SetHorizontalAlignment(HAlign);
			ScaleSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Scale;
	}

	//고정 폭 칸
	USizeBox* AddFixedCell(UWidgetTree* Tree, UHorizontalBox* Row, float Width, UWidget* Content, EHorizontalAlignment HAlign)
	{
		USizeBox* Cell = Tree->ConstructWidget<USizeBox>();
		Cell->SetWidthOverride(Width);
		if (USizeBoxSlot* CellSlot = Cast<USizeBoxSlot>(Cell->AddChild(WrapShrinkToFit(Tree, Content, HAlign))))
		{
			CellSlot->SetHorizontalAlignment(HAlign);
			CellSlot->SetVerticalAlignment(VAlign_Center);
			CellSlot->SetPadding(FMargin(CellPadding, 0.0f));
		}
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(Cell))
		{
			RowSlot->SetVerticalAlignment(VAlign_Fill);
		}
		return Cell;
	}

	//남는 폭을 모두 쓰는 칸 (사용자 ID)
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

void ULeaderboardBoardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

FSlateFontInfo ULeaderboardBoardWidget::MakeFont(int32 Size) const
{
	if (UObject* Family = Style.FontFamily.Get())
	{
		return FSlateFontInfo(Family, Size);
	}
	return FSlateFontInfo();
}

UTextBlock* ULeaderboardBoardWidget::MakeText(const FName& Name, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	FSlateFontInfo Font = Style.FontFamily ? MakeFont(FontSize) : Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	return Text;
}

void ULeaderboardBoardWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BoardRoot"));
	Root->SetWidthOverride(Style.BoardWidth);
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BoardPanel"));
	Panel->SetBrush(Style.PanelBrush);
	Panel->SetPadding(FMargin(16.0f, 12.0f, 16.0f, 16.0f));
	Root->AddChild(Panel);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Layout);

	// ===== 제목 =====
	TitleText = MakeText(TEXT("TitleText"), Style.TitleFontSize, Style.TitleColor);
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.TypefaceFontName = TEXT("Bold");
	TitleFont.LetterSpacing = 200;
	TitleText->SetFont(TitleFont);
	if (UVerticalBoxSlot* TitleSlot = Layout->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	// ===== 열 머리글 =====
	UBorder* Header = WidgetTree->ConstructWidget<UBorder>();
	Header->SetBrush(Style.HeaderBrush);
	Header->SetPadding(FMargin(0.0f, 7.0f));
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Header->SetContent(HeaderRow);

	auto MakeHeaderText = [this](const TCHAR* Label)
	{
		UTextBlock* Text = MakeText(NAME_None, Style.HeaderFontSize, Style.HeaderTextColor);
		Text->SetText(FText::FromString(Label));
		return Text;
	};

	AddFixedCell(WidgetTree, HeaderRow, Style.RankColumnWidth, MakeHeaderText(TEXT("순위")), HAlign_Center);
	AddFillCell(WidgetTree, HeaderRow, MakeHeaderText(TEXT("사용자 ID")));
	AddFixedCell(WidgetTree, HeaderRow, Style.CharacterColumnWidth, MakeHeaderText(TEXT("캐릭터")), HAlign_Left);
	HealthHeaderCell = AddFixedCell(WidgetTree, HeaderRow, Style.HealthColumnWidth, MakeHeaderText(TEXT("체력")), HAlign_Center);
	DamageHeaderText = MakeHeaderText(TEXT("데미지"));
	AddFixedCell(WidgetTree, HeaderRow, Style.DamageColumnWidth, DamageHeaderText, HAlign_Right);

	if (UVerticalBoxSlot* HeaderSlot = Layout->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// ===== 순위 줄 =====
	RowWidgets.SetNum(FLeaderboardBuilder::RowsPerPage);
	for (FRowWidgets& Widgets : RowWidgets)
	{
		Widgets.Background = WidgetTree->ConstructWidget<UBorder>();
		Widgets.Background->SetBrush(Style.RowBrush);
		Widgets.Background->SetPadding(FMargin(0.0f));

		USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(Style.RowHeight);
		Widgets.Background->SetContent(Height);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Height->AddChild(Row);

		//순위: 1~3위는 메달 브러시 위에 숫자
		Widgets.Rank = MakeText(NAME_None, Style.RowFontSize, Style.TextColor);
		Widgets.Medal = WidgetTree->ConstructWidget<UBorder>();
		Widgets.Medal->SetHorizontalAlignment(HAlign_Center);
		Widgets.Medal->SetVerticalAlignment(VAlign_Center);
		Widgets.Medal->SetPadding(FMargin(0.0f));
		Widgets.Medal->SetContent(Widgets.Rank);
		USizeBox* MedalBox = WidgetTree->ConstructWidget<USizeBox>();
		MedalBox->SetWidthOverride(MedalSize);
		MedalBox->SetHeightOverride(MedalSize);
		MedalBox->AddChild(Widgets.Medal);
		AddFixedCell(WidgetTree, Row, Style.RankColumnWidth, MedalBox, HAlign_Center);

		Widgets.Name = MakeText(NAME_None, Style.RowFontSize, Style.TextColor);
		AddFillCell(WidgetTree, Row, Widgets.Name);

		Widgets.Character = MakeText(NAME_None, Style.RowFontSize, Style.TextColor);
		AddFixedCell(WidgetTree, Row, Style.CharacterColumnWidth, Widgets.Character, HAlign_Left);

		Widgets.Health = MakeText(NAME_None, Style.RowFontSize, Style.TextColor);
		Widgets.HealthCell = AddFixedCell(WidgetTree, Row, Style.HealthColumnWidth, Widgets.Health, HAlign_Center);

		Widgets.Damage = MakeText(NAME_None, Style.RowFontSize + 1, Style.TextColor);
		AddFixedCell(WidgetTree, Row, Style.DamageColumnWidth, Widgets.Damage, HAlign_Right);

		if (UVerticalBoxSlot* RowSlot = Layout->AddChildToVerticalBox(Widgets.Background))
		{
			RowSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
	}

	SetPage(FLeaderboardBuilder::MakeTotalPage({}));
}

void ULeaderboardBoardWidget::SetPage(const FLeaderboardPage& Page)
{
	if (RowWidgets.IsEmpty())
	{
		return;
	}

	TitleText->SetText(FText::FromString(Page.Title));
	DamageHeaderText->SetText(FText::FromString(Page.DamageHeader));
	HealthHeaderCell->SetVisibility(Page.bShowHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FLeaderboardRow EmptyRow;
		ApplyRow(RowWidgets[Index], Page.Rows.IsValidIndex(Index) ? Page.Rows[Index] : EmptyRow, Page.bShowHealth);
	}
}

void ULeaderboardBoardWidget::ApplyRow(FRowWidgets& Widgets, const FLeaderboardRow& Row, bool bShowHealth)
{
	Widgets.HealthCell->SetVisibility(bShowHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (Row.bEmpty)
	{
		Widgets.Background->SetBrush(Style.RowBrush);
		Widgets.Background->SetRenderOpacity(1.0f);
		Widgets.Medal->SetBrush(MakeEmptyBrush());
		for (UTextBlock* Text : { Widgets.Rank.Get(), Widgets.Name.Get(), Widgets.Character.Get(), Widgets.Health.Get(), Widgets.Damage.Get() })
		{
			Text->SetText(FText::GetEmpty());
		}
		return;
	}

	const bool bFirst = Row.Rank == 1;
	const int32 MedalIndex = Row.Rank - 1;

	Widgets.Background->SetBrush(bFirst ? Style.FirstRowBrush : Style.RowBrush);
	Widgets.Background->SetRenderOpacity(Row.bDimmed ? Style.DimmedOpacity : 1.0f);
	Widgets.Medal->SetBrush(Style.MedalBrushes.IsValidIndex(MedalIndex) ? Style.MedalBrushes[MedalIndex] : MakeEmptyBrush());

	Widgets.Rank->SetText(FText::AsNumber(Row.Rank));
	Widgets.Name->SetText(FText::FromString(Row.Name));
	Widgets.Character->SetText(FText::FromString(Row.CharacterName));
	Widgets.Health->SetText(FText::AsNumber(Row.Health));
	Widgets.Damage->SetText(FText::FromString(FLeaderboardBuilder::FormatDamage(Row.Damage)));
	Widgets.Damage->SetColorAndOpacity(FSlateColor(bFirst ? Style.FirstDamageColor : Style.TextColor));
}
