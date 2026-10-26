#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "LeaderboardLayoutHelpers.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Scripts/UI/Leaderboard/LeaderboardRowWidget.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Slate/WidgetRenderer.h"
#include "AssetCompilingManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontCache.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Animation/WidgetAnimation.h"
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
	TitleText=nullptr; HealthHeaderCell=nullptr; DamageHeaderText=nullptr; RowContainer=nullptr;
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
 if (!InlineRowRoots.IsEmpty())
 {
  if (!Rows.IsEmpty()) return;
  const TSubclassOf<ULeaderboardRowWidget> RowClass = GladiusUI::ResolveWidgetClass<ULeaderboardRowWidget>(RowWidgetClass, GladiusUI::AssetPaths::LeaderboardRow);
  for (int32 Index = 0; Index < InlineRowRoots.Num(); ++Index)
  {
   if (auto* AuthoredRow = Cast<ULeaderboardRowWidget>(WidgetTree->FindWidget(InlineRowRoots[Index])))
   {
    Rows.Add(AuthoredRow);
    continue;
   }
   // Data controllers only: the visible widgets already belong to the Board tree.
   auto* Controller = NewObject<ULeaderboardRowWidget>(this, RowClass);
   Controller->BindInlineWidgets(WidgetTree, FString::Printf(TEXT("Row%02d_"), Index + 1), WidgetTree->FindWidget(InlineRowRoots[Index]));
   Rows.Add(Controller);
  }
  return;
 }

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
 // Editor-created widgets also need stable IDs before compilation.
 Tree->ForEachWidget([BP](UWidget* Widget)
 {
  if (Widget && !BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
   BP->OnVariableAdded(Widget->GetFName());
 });
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status != BS_Error;
#else
 return false;
#endif
}

bool ULeaderboardBoardWidget::EmbedEditableRows(UObject* Asset)
{
#if WITH_EDITOR
 auto* BP = Cast<UWidgetBlueprint>(Asset);
 if (!BP || !BP->GeneratedClass || !BP->WidgetTree) return false;
 auto* Defaults = Cast<ULeaderboardBoardWidget>(BP->GeneratedClass->GetDefaultObject());
 if (!Defaults) return false;
 if (!Defaults->InlineRowRoots.IsEmpty()) return Defaults->InlineRowRoots.Num() == 10;
 auto* Container = Cast<UPanelWidget>(BP->WidgetTree->FindWidget(TEXT("RowContainer")));
 if (!Container) Container = Cast<UPanelWidget>(BP->WidgetTree->FindWidget(TEXT("LB_RowContainer")));
 if (!Container) { UE_LOG(LogTemp, Warning, TEXT("Inline migration: missing RowContainer")); BP->WidgetTree->ForEachWidget([](UWidget* W) { UE_LOG(LogTemp, Warning, TEXT("Inline tree: %s %s parent=%s"), *W->GetName(), *W->GetClass()->GetName(), *GetNameSafe(W->GetParent())); }); return false; }
 const auto RowClass = GladiusUI::ResolveWidgetClass<ULeaderboardRowWidget>(Defaults->RowWidgetClass, GladiusUI::AssetPaths::LeaderboardRow);
 auto* RowBP = Cast<UWidgetBlueprint>(RowClass->ClassGeneratedBy);
 // Animated/nested user widgets require explicit migration, never silently flatten them.
 if (!RowBP || !RowBP->WidgetTree || !RowBP->WidgetTree->RootWidget || !RowBP->Animations.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Inline migration: missing row tree or animations")); return false; }
 TArray<UWidget*> SourceWidgets;
 RowBP->WidgetTree->GetAllWidgets(SourceWidgets);
 for (UWidget* W : SourceWidgets)
 {
  if (Cast<UUserWidget>(W)) { UE_LOG(LogTemp, Warning, TEXT("Inline migration: nested widget %s"), *W->GetName()); return false; }
  for (int32 I = 1; I <= 10; ++I)
   if (BP->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("Row%02d_%s"), I, *W->GetName())))) return false;
 }
 BP->Modify(); BP->WidgetTree->Modify(); Container->Modify(); Defaults->Modify();
 for (int32 I = 1; I <= 10; ++I)
 {
  auto* Copy = DuplicateObject<UWidgetTree>(RowBP->WidgetTree, GetTransientPackage());
  TArray<UWidget*> Widgets; Copy->GetAllWidgets(Widgets);
  for (UWidget* W : Widgets)
  {
   const FString Name = FString::Printf(TEXT("Row%02d_%s"), I, *W->GetName());
   W->Rename(*Name, BP->WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);
   W->SetFlags(RF_Transactional); W->bIsVariable = true;
   BP->OnVariableAdded(W->GetFName());
  }
  UWidget* Root = Copy->RootWidget;
  Root->Slot = nullptr;
  auto* Slot = Container->AddChild(Root);
  if (auto* Vertical = Cast<UVerticalBoxSlot>(Slot)) Vertical->SetPadding(FMargin(0,2));
  Defaults->InlineRowRoots.Add(Root->GetFName());
  Copy->RootWidget = nullptr;
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status != BS_Error;
#else
 return false;
#endif
}

#if WITH_EDITOR
namespace
{
// Capture Slate's original arrangement before replacing automatic layout panels.
UCanvasPanel* FreePanel(UWidgetBlueprint* BP, const TCHAR* Name, FVector2D Size)
{
 auto* Old = Cast<UPanelWidget>(BP->WidgetTree->FindWidget(Name));
 if (!Old) return nullptr;
 if (auto* Canvas = Cast<UCanvasPanel>(Old)) return Canvas;
 const auto Slate = Old->TakeWidget();
 Slate->SlatePrepass(1.0f);
 const FVector2D Desired(Slate->GetDesiredSize());
 if (Size.X <= 0) Size.X = Desired.X;
 if (Size.Y <= 0) Size.Y = Desired.Y;
 FArrangedChildren Arranged(EVisibility::All);
 Slate->ArrangeChildren(FGeometry::MakeRoot(Size,FSlateLayoutTransform()),Arranged);
 struct FPlacement { UWidget* Child; FVector2D Position; FVector2D Size; };
 TArray<FPlacement> Places;
 for (int32 I=0; I<Old->GetChildrenCount(); ++I)
 {
  UWidget* Child=Old->GetChildAt(I);
  const auto ChildSlate=Child->GetCachedWidget();
  bool bFound=false;
  for (int32 J=0; J<Arranged.Num(); ++J)
   if (ChildSlate==Arranged[J].Widget)
   {
    Places.Add({Child,FVector2D(Arranged[J].Geometry.GetAccumulatedLayoutTransform().GetTranslation()),FVector2D(Arranged[J].Geometry.GetLocalSize())});
    bFound=true; break;
   }
  // Invisible cells still need editable, nonzero geometry.
  if (!bFound) Places.Add({Child,FVector2D::ZeroVector,FVector2D(Child->GetDesiredSize())});
 }
 UPanelWidget* Parent=Old->GetParent();
 const FWidgetTransform Transform=Old->GetRenderTransform();
 const FVector2D Pivot=Old->GetRenderTransformPivot();
 const float Opacity=Old->GetRenderOpacity();
 const FName OldName=Old->GetFName();
 Old->Rename(*MakeUniqueObjectName(BP->WidgetTree,Old->GetClass(),TEXT("PreviousAutomaticLayout")).ToString(),nullptr,REN_DontCreateRedirectors);
 auto* Canvas=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),OldName);
 Canvas->SetRenderTransform(Transform); Canvas->SetRenderTransformPivot(Pivot); Canvas->SetRenderOpacity(Opacity);
 Canvas->SetVisibility(Old->GetVisibility()); Canvas->SetClipping(Old->GetClipping());
 if (Parent) Parent->ReplaceChild(Old,Canvas);
 else BP->WidgetTree->RootWidget=Canvas;
 Old->ClearChildren();
 for (const auto& P:Places)
 {
  auto* Slot=Canvas->AddChildToCanvas(P.Child);
  Slot->SetPosition(P.Position); Slot->SetSize(P.Size.ComponentMax(FVector2D(1,1)));
 }
 return Canvas;
}
void RegisterFreeWidgets(UWidgetBlueprint* BP)
{
 // The compiler also visits orphaned widgets directly owned by the tree.
 // Move obsolete copied panels/rows out of the asset before compiling.
 TArray<UWidget*> Reachable; BP->WidgetTree->GetAllWidgets(Reachable);
 TArray<UObject*> Owned; GetObjectsWithOuter(BP->WidgetTree,Owned,EGetObjectsFlags::None);
 for (auto* Object:Owned)
  if (auto* W=Cast<UWidget>(Object); W && !Reachable.Contains(W))
   W->Rename(*MakeUniqueObjectName(GetTransientPackage(),W->GetClass(),W->GetFName()).ToString(),GetTransientPackage(),REN_DontCreateRedirectors);

 TSet<FName> CurrentNames;
 for (auto* W:Reachable) CurrentNames.Add(W->GetFName());
 for (const auto& Animation:BP->Animations) if (Animation) CurrentNames.Add(Animation->GetFName());
 for (auto It=BP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
  if (!CurrentNames.Contains(It.Key())) It.RemoveCurrent();
 BP->WidgetTree->ForEachWidget([BP](UWidget* W) {
  W->bIsVariable=true;
  if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName())) BP->OnVariableAdded(W->GetFName());
 });
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
}
}
#endif

bool ULeaderboardBoardWidget::EnableFreeLayout(UObject* Asset, UObject* SharedRowAsset)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset);
 auto* RowBP=Cast<UWidgetBlueprint>(SharedRowAsset);
 if (!BP || !RowBP || !BP->WidgetTree || !RowBP->WidgetTree || !BP->GeneratedClass || !RowBP->GeneratedClass) return false;
 auto* Defaults=Cast<ULeaderboardBoardWidget>(BP->GeneratedClass->GetDefaultObject());
 if (!Defaults || Defaults->InlineRowRoots.Num()!=10) return false;
 // Rerunning must not overwrite subsequent shared-template or placement edits.
 if (Cast<ULeaderboardRowWidget>(BP->WidgetTree->FindWidget(Defaults->InlineRowRoots[0]))) return true;
 UWidget* SourceRoot=BP->WidgetTree->FindWidget(Defaults->InlineRowRoots[0]);
 if (!SourceRoot) return false;
 const FName SourceName=SourceRoot->GetFName();
 BP->Modify(); RowBP->Modify();
 // Copy the latest authored first row, including custom fonts, images and decorations.
 RowBP->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors);
 RowBP->WidgetTree=DuplicateObject<UWidgetTree>(BP->WidgetTree,RowBP,TEXT("WidgetTree"));
 auto* Template=RowBP->WidgetTree->FindWidget(SourceName);
 if (!Template) return false;
 Template->RemoveFromParent(); RowBP->WidgetTree->RootWidget=Template;
 Template->SetRenderTransform(FWidgetTransform());
 TArray<UWidget*> Widgets; RowBP->WidgetTree->GetAllWidgets(Widgets);
 for (auto* W:Widgets)
 {
  FString Name=W->GetName(); Name.RemoveFromStart(TEXT("Row01_"));
  W->Rename(*Name,nullptr,REN_DontCreateRedirectors);
 }
 FreePanel(RowBP,TEXT("RowCells"),FVector2D(508,42));
 RegisterFreeWidgets(RowBP);
 if (RowBP->Status==BS_Error) return false;
 // Freeze the current visual arrangement, then allow each row/title/header to move.
 FreePanel(BP,TEXT("BoardLayout"),FVector2D(508,0));
 FreePanel(BP,TEXT("HeaderCells"),FVector2D(508,0));
 auto* Container=FreePanel(BP,TEXT("RowContainer"),FVector2D(508,0));
 if (!Container) return false;
 for (int32 I=0; I<10; ++I)
 {
  auto* Old=BP->WidgetTree->FindWidget(Defaults->InlineRowRoots[I]);
  if (!Old || Old->GetParent()!=Container) return false;
  auto* Row=BP->WidgetTree->ConstructWidget<ULeaderboardRowWidget>(RowBP->GeneratedClass.Get(),FName(*FString::Printf(TEXT("SharedRow%02d"),I+1)));
  Row->SetRenderTransform(Old->GetRenderTransform()); Row->SetRenderTransformPivot(Old->GetRenderTransformPivot());
  Row->SetRenderOpacity(Old->GetRenderOpacity());
  Container->ReplaceChild(Old,Row);
  Defaults->InlineRowRoots[I]=Row->GetFName();
 }
 Defaults->RowWidgetClass=RowBP->GeneratedClass;
 RegisterFreeWidgets(BP);
 return BP->Status!=BS_Error;
#else
 return false;
#endif
}

bool ULeaderboardBoardWidget::ExportImageAlignedPreview(UObject* Asset,const FString& FilePath,bool bBattle,bool bLower)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset); if (!BP || !BP->GeneratedClass) return false;
 auto* Board=NewObject<ULeaderboardBoardWidget>(GetTransientPackage(),BP->GeneratedClass.Get());
 Board->AddToRoot(); Board->Initialize();
 auto* Panel=Cast<UBorder>(Board->WidgetTree->FindWidget(TEXT("BoardPanel")));
 auto* Root=Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BoardRoot")));
 // Preview in artwork coordinates; production's overall placement and scale are untouched.
 Root->SetWidthOverride(1440); Root->SetHeightOverride(1600); Panel->SetPadding(FMargin(0));
 for (const TCHAR* Name:{TEXT("BoardPanel"),TEXT("BoardLayout"),TEXT("BackgroundFrame"),TEXT("NativeBackgroundImage")})
  if (auto* W=Board->WidgetTree->FindWidget(Name)) W->SetRenderTransform(FWidgetTransform());
 TArray<FLeaderboardRow> Data;
 for (int32 I=0;I<15;++I)
 {
  FLeaderboardRow Row;Row.Rank=I+1;Row.Name=FString::Printf(TEXT("사용자 %02d"),I+1);
  Row.CharacterName=I%2==0?TEXT("글라디우스"):TEXT("세라");
  Row.Damage=987654-I*51001;Row.Health=I==3?0:5-I%5;Row.bDimmed=Row.Health==0;Row.bEmpty=false;Data.Add(Row);
 }
 Board->SetPage(bBattle?(bLower?FLeaderboardBuilder::MakeBattleLowerPage(Data):FLeaderboardBuilder::MakeBattleTopPage(Data)):FLeaderboardBuilder::MakeTotalPage(Data));
 FAssetCompilingManager::Get().FinishAllCompilation();
 FWidgetRenderer Renderer(true);
 if (auto* Image=Cast<UImage>(Board->WidgetTree->FindWidget(TEXT("NativeBackgroundImage"))))
  if (auto* Texture=Cast<UTexture2D>(Image->GetBrush().GetResourceObject()))
  { Texture->SetForceMipLevelsToBeResident(30); Texture->WaitForStreaming(); }
 const auto Slate=Board->TakeWidget();
 for (int32 Pass=0;Pass<4;++Pass)
 {
  Board->SetPage(bBattle?(bLower?FLeaderboardBuilder::MakeBattleLowerPage(Data):FLeaderboardBuilder::MakeBattleTopPage(Data)):FLeaderboardBuilder::MakeTotalPage(Data));
  Renderer.DrawWidget(Slate,FVector2D(1440,1600));
  FSlateApplication::Get().GetRenderer()->GetFontCache()->UpdateCache();
  FlushRenderingCommands();
  FSlateApplication::Get().Tick();
 }

 auto* Target=Renderer.DrawWidget(Slate,FVector2D(1440,1600));
 FlushRenderingCommands();
 FBufferArchive Bytes; const bool bOK=Target && FImageUtils::ExportRenderTarget2DAsPNG(Target,Bytes) && FFileHelper::SaveArrayToFile(Bytes,*FilePath);
 Board->RemoveFromRoot(); return bOK;
#else
 return false;
#endif
}

bool ULeaderboardBoardWidget::NormalizeEditorTree(UObject* Asset)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset); if (!BP || !BP->WidgetTree) return false;
 RegisterFreeWidgets(BP); return BP->Status!=BS_Error;
#else
 return false;
#endif
}

bool ULeaderboardBoardWidget::CopyAlignedDesigner(UObject* SourceAsset,UObject* TargetAsset,UObject* TargetRowAsset)
{
#if WITH_EDITOR
 auto* Source=Cast<UWidgetBlueprint>(SourceAsset); auto* Target=Cast<UWidgetBlueprint>(TargetAsset);
 auto* RowBP=Cast<UWidgetBlueprint>(TargetRowAsset);
 if (!Source || !Target || Source==Target || !Source->WidgetTree || !Target->WidgetTree || !Target->Animations.IsEmpty()) return false;
 Target->Modify(); Target->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors);
 Target->WidgetTree=DuplicateObject<UWidgetTree>(Source->WidgetTree,Target,TEXT("WidgetTree"));
 Target->WidgetVariableNameToGuidMap=Source->WidgetVariableNameToGuidMap;
 Target->Bindings=Source->Bindings;
 if (RowBP)
 {
  auto* Defaults=Cast<ULeaderboardBoardWidget>(Target->GeneratedClass->GetDefaultObject());
  auto* SourceDefaults=Cast<ULeaderboardBoardWidget>(Source->GeneratedClass->GetDefaultObject());
  if (!Defaults || !SourceDefaults) return false;
  Defaults->InlineRowRoots=SourceDefaults->InlineRowRoots;
  Defaults->RowWidgetClass=RowBP->GeneratedClass;
  Defaults->PreviewPage=ELeaderboardPreviewPage::BattleTop;
  for (FName Name:Defaults->InlineRowRoots)
  {
   auto* Old=Target->WidgetTree->FindWidget(Name); if (!Old || !Old->GetParent()) return false;
   auto* Parent=Old->GetParent();
   Old->Rename(*MakeUniqueObjectName(GetTransientPackage(),Old->GetClass(),Name).ToString(),GetTransientPackage(),REN_DontCreateRedirectors);
   auto* Row=Target->WidgetTree->ConstructWidget<ULeaderboardRowWidget>(RowBP->GeneratedClass.Get(),Name);
   Row->SetRenderTransform(Old->GetRenderTransform()); Row->SetRenderTransformPivot(Old->GetRenderTransformPivot());
   Parent->ReplaceChild(Old,Row); Old->Slot=nullptr;
  }
 }
 RegisterFreeWidgets(Target); return Target->Status!=BS_Error;
#else
 return false;
#endif
}
