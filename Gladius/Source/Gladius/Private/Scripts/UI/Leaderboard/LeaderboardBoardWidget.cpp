#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "LeaderboardLayoutHelpers.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Scripts/UI/Leaderboard/LeaderboardRowWidget.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

void ULeaderboardBoardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//Designer에서 만든 레이아웃이 없을 때(C++ 클래스 단독 사용)만 기본 도형 레이아웃 생성
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout(WidgetTree);
	}
	ResolveWidgets();
	CreateRows();
	SetPage(FLeaderboardBuilder::MakeTotalPage({}));
}

void ULeaderboardBoardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshDesignerPreview();
}

void ULeaderboardBoardWidget::RefreshDesignerPreview()
{
 if (!IsDesignTime()) return;
 ResolveWidgets();
 // Reuse preview rows on repeated refreshes; never rebuild authored board/row layouts.
 if (Rows.IsEmpty()) CreateRows();
 SetPage(MakePreviewPage());
}

void ULeaderboardBoardWidget::ResolveWidgets()
{
	if (!WidgetTree)
	{
		return;
	}
	auto Find = [this](auto& Member, const TCHAR* Name)
	{
		using FWidgetType = std::remove_reference_t<decltype(*Member)>;
		if (!Member)
		{
			Member = Cast<FWidgetType>(WidgetTree->FindWidget(Name));
		}
	};
	Find(TitleText, TEXT("TitleText"));
	Find(TitleText, TEXT("LB_TitleText"));
	Find(HealthHeaderCell, TEXT("HealthHeaderCell"));
	Find(HealthHeaderCell, TEXT("LB_HealthHeaderCell"));
	Find(DamageHeaderText, TEXT("DamageHeaderText"));
	Find(DamageHeaderText, TEXT("LB_DamageHeaderText"));
	Find(RowContainer, TEXT("RowContainer"));
	Find(RowContainer, TEXT("LB_RowContainer"));
}

void ULeaderboardBoardWidget::CreateRows()
{
	if (!RowContainer)
	{
		return;
	}

	// Remove only rows created by this instance. Never delete designer-authored children.
	for (ULeaderboardRowWidget* Row : Rows)
	{
		if (Row) RowContainer->RemoveChild(Row);
	}
	Rows.Reset();

	const TSubclassOf<ULeaderboardRowWidget> RowClass = GladiusUI::ResolveWidgetClass<ULeaderboardRowWidget>(RowWidgetClass, GladiusUI::AssetPaths::LeaderboardRow);
	for (int32 Index = 0; Index < FLeaderboardBuilder::RowsPerPage; ++Index)
	{
		ULeaderboardRowWidget* Row = CreateWidget<ULeaderboardRowWidget>(this, RowClass);
		if (!Row)
		{
			continue;
		}
		UPanelSlot* RowSlot = RowContainer->AddChild(Row);
		if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(RowSlot))
		{
			VerticalSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
		Rows.Add(Row);
	}
}

void ULeaderboardBoardWidget::SetPage(const FLeaderboardPage& Page)
{
 // A nested board can receive data before NativeOnInitialized has a player context.
 // Resolve existing designer widgets lazily; only generated data rows are created here.
 ResolveWidgets();
 if (Rows.IsEmpty()) CreateRows();
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Page.Title));
	}
	if (DamageHeaderText)
	{
		DamageHeaderText->SetText(FText::FromString(Page.DamageHeader));
	}
	if (HealthHeaderCell)
	{
		HealthHeaderCell->SetVisibility(Page.bShowHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FLeaderboardRow EmptyRow;
		Rows[Index]->SetRow(Page.Rows.IsValidIndex(Index) ? Page.Rows[Index] : EmptyRow, Page.bShowHealth);
	}

	const bool bTitleChanged = LastTitle != Page.Title;
	LastTitle = Page.Title;
	OnPageChanged(Page, bTitleChanged);
}

FLeaderboardPage ULeaderboardBoardWidget::MakePreviewPage() const
{
	TArray<FLeaderboardRow> Ranked;
	for (int32 Index = 0; Index < 15; ++Index)
	{
		FLeaderboardRow Row;
		Row.Rank = Index + 1;
		Row.Name = FString::Printf(TEXT("사용자 %d"), Index + 1);
		Row.CharacterName = Index % 2 == 0 ? TEXT("글라디우스") : TEXT("세라");
		Row.Damage = 98540 - Index * 6000;
		Row.Health = Index == 3 ? 0 : 5 - Index % 5;
		Row.bDimmed = Row.Health == 0;
		Row.bEmpty = false;
		Ranked.Add(Row);
	}
	switch (PreviewPage)
	{
	case ELeaderboardPreviewPage::Total: return FLeaderboardBuilder::MakeTotalPage(Ranked);
	case ELeaderboardPreviewPage::BattleLower: return FLeaderboardBuilder::MakeBattleLowerPage(Ranked);
	default: return FLeaderboardBuilder::MakeBattleTopPage(Ranked);
	}
}

void ULeaderboardBoardWidget::BuildDefaultLayout(UWidgetTree* Tree) const
{
	using GladiusUI::ConstructNamed;

	if (!Tree)
	{
		return;
	}

	auto MakeText = [&](FName Name, int32 FontSize, const FLinearColor& Color, const TCHAR* Label)
	{
		UTextBlock* Text = ConstructNamed<UTextBlock>(Tree, Name);
		GladiusUI::ApplyFont(Text, Style.FontFamily, FontSize);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetText(FText::FromString(Label));
		return Text;
	};

	USizeBox* Root = ConstructNamed<USizeBox>(Tree, TEXT("BoardRoot"));
	Root->SetWidthOverride(Style.BoardWidth);
	Tree->RootWidget = Root;

	UBorder* Panel = ConstructNamed<UBorder>(Tree, TEXT("BoardPanel"));
	Panel->SetBrush(Style.PanelBrush);
	Panel->SetPadding(FMargin(16.0f, 12.0f, 16.0f, 16.0f));
	Root->AddChild(Panel);

	UVerticalBox* Layout = ConstructNamed<UVerticalBox>(Tree, TEXT("BoardLayout"));
	Panel->SetContent(Layout);

	// ===== 제목 =====
	UTextBlock* Title = MakeText(TEXT("TitleText"), Style.TitleFontSize, Style.TitleColor, TEXT("LEADERBOARD"));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.TypefaceFontName = TEXT("Bold");
	TitleFont.LetterSpacing = 200;
	Title->SetFont(TitleFont);
	if (UVerticalBoxSlot* TitleSlot = Layout->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	// ===== 열 머리글 (줄과 같은 열 구성) =====
	UBorder* Header = ConstructNamed<UBorder>(Tree, TEXT("HeaderPanel"));
	Header->SetBrush(Style.HeaderBrush);
	Header->SetPadding(FMargin(0.0f, 7.0f));
	UHorizontalBox* HeaderRow = ConstructNamed<UHorizontalBox>(Tree, TEXT("HeaderCells"));
	Header->SetContent(HeaderRow);

	LeaderboardRowLayout::AddFixedCell(Tree, HeaderRow, TEXT("RankHeaderCell"), Style.RankColumnWidth,
		MakeText(TEXT("RankHeaderText"), Style.HeaderFontSize, Style.HeaderTextColor, TEXT("순위")), HAlign_Center);
	LeaderboardRowLayout::AddFillCell(Tree, HeaderRow,
		MakeText(TEXT("NameHeaderText"), Style.HeaderFontSize, Style.HeaderTextColor, TEXT("사용자 ID")));
	LeaderboardRowLayout::AddFixedCell(Tree, HeaderRow, TEXT("CharacterHeaderCell"), Style.CharacterColumnWidth,
		MakeText(TEXT("CharacterHeaderText"), Style.HeaderFontSize, Style.HeaderTextColor, TEXT("캐릭터")), HAlign_Left);
	LeaderboardRowLayout::AddFixedCell(Tree, HeaderRow, TEXT("HealthHeaderCell"), Style.HealthColumnWidth,
		MakeText(TEXT("HealthHeaderText"), Style.HeaderFontSize, Style.HeaderTextColor, TEXT("체력")), HAlign_Center);
	LeaderboardRowLayout::AddFixedCell(Tree, HeaderRow, TEXT("DamageHeaderCell"), Style.DamageColumnWidth,
		MakeText(TEXT("DamageHeaderText"), Style.HeaderFontSize, Style.HeaderTextColor, TEXT("데미지")), HAlign_Right);

	if (UVerticalBoxSlot* HeaderSlot = Layout->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// ===== 줄 10개가 들어갈 자리 =====
	UVerticalBox* Container = ConstructNamed<UVerticalBox>(Tree, TEXT("RowContainer"));
	Layout->AddChildToVerticalBox(Container);
}

bool ULeaderboardBoardWidget::EnsureStateBindings(UObject* Asset)
{
#if WITH_EDITOR
 UWidgetBlueprint* BP = Cast<UWidgetBlueprint>(Asset);
 if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(StaticClass()) || !BP->WidgetTree) return false;
 UWidgetTree* Tree = BP->WidgetTree;
 // Repair missing data destinations only; never regenerate or rename authored widgets.
 if (!Tree->FindWidget(TEXT("RowContainer")) && !Tree->FindWidget(TEXT("LB_RowContainer")))
 {
  UPanelWidget* Layout = Cast<UPanelWidget>(Tree->FindWidget(TEXT("BoardLayout")));
  if (!Layout) return false;
  auto* Container = GladiusUI::ConstructNamed<UVerticalBox>(Tree,TEXT("RowContainer"));
  Layout->AddChild(Container);
  BP->OnVariableAdded(Container->GetFName());
 }
 if (!Tree->FindWidget(TEXT("DamageHeaderText")) && !Tree->FindWidget(TEXT("LB_DamageHeaderText")))
 {
  UPanelWidget* Cell = Cast<UPanelWidget>(Tree->FindWidget(TEXT("DamageHeaderCell")));
  if (Cell && Cell->GetChildrenCount() == 0)
  {
   UTextBlock* Sibling = Cast<UTextBlock>(Tree->FindWidget(TEXT("CharacterHeaderText")));
   UTextBlock* Text = Sibling ? DuplicateObject<UTextBlock>(Sibling,Tree,TEXT("DamageHeaderText"))
    : GladiusUI::ConstructNamed<UTextBlock>(Tree,TEXT("DamageHeaderText"));
   Text->Slot = nullptr; Text->bIsVariable = true;
   Text->SetText(FText::FromString(TEXT("데미지")));
   Cell->AddChild(Text); BP->OnVariableAdded(Text->GetFName());
  }
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status != BS_Error;
#else
 return false;
#endif
}
