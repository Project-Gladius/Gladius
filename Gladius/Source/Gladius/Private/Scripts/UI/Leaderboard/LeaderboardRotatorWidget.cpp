#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"
#include "Scripts/Backend/AdminSubsystem.h"
#include "Engine/GameInstance.h"
#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "TimerManager.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

void ULeaderboardRotatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//Designer에서 만든 레이아웃이 없을 때(C++ 클래스 단독 사용)만 기본 레이아웃 생성
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout(WidgetTree);
	}
	ResolveStateWidgets();
}

void ULeaderboardRotatorWidget::ResolveStateWidgets()
{
 if (!WidgetTree) return;
 if (!Board) Board = Cast<ULeaderboardBoardWidget>(WidgetTree->FindWidget(TEXT("Board")));
 if (!Board) Board = Cast<ULeaderboardBoardWidget>(WidgetTree->FindWidget(TEXT("LR_Board")));
 if (!BattleBoard) BattleBoard = Cast<ULeaderboardBoardWidget>(WidgetTree->FindWidget(TEXT("BattleBoard")));
 if (!PageSwitcher) PageSwitcher = Cast<UWidgetSwitcher>(WidgetTree->FindWidget(TEXT("PageSwitcher")));
}

ULeaderboardBoardWidget* ULeaderboardRotatorWidget::SelectBoard(bool bBattle)
{
 ULeaderboardBoardWidget* Selected = bBattle && BattleBoard ? BattleBoard.Get() : Board.Get();
 if (PageSwitcher && Selected && Selected->GetParent() == PageSwitcher)
 {
  PageSwitcher->SetActiveWidget(Selected);
 }
 return Selected;
}

void ULeaderboardRotatorWidget::NativePreConstruct()
{
 Super::NativePreConstruct();
 if (IsDesignTime())
 {
  ResolveStateWidgets();
  if (ULeaderboardBoardWidget* Selected = SelectBoard(PreviewPage != ELeaderboardPreviewPage::Total))
  {
   Selected->PreviewPage = PreviewPage;
   Selected->RefreshDesignerPreview();
  }
 }
}

void ULeaderboardRotatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveStateWidgets();
	RefreshCurrentPage();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SwitchTimerHandle, this, &ULeaderboardRotatorWidget::AdvancePage, SwitchSeconds, true);
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ULeaderboardRotatorWidget::RefreshCurrentPage, RefreshSeconds, true);
	}
}

void ULeaderboardRotatorWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwitchTimerHandle);
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void ULeaderboardRotatorWidget::BindToBattleManager(ABattleManager* InBattleManager)
{
	BattleManager = InBattleManager;
	CurrentPage = EPage::Total;
	RefreshCurrentPage();
}

void ULeaderboardRotatorWidget::BuildDefaultLayout(UWidgetTree* Tree) const
{
	if (!Tree)
	{
		return;
	}

	UCanvasPanel* Canvas = GladiusUI::ConstructNamed<UCanvasPanel>(Tree, TEXT("RootCanvas"));
	Tree->RootWidget = Canvas;

	const TSubclassOf<ULeaderboardBoardWidget> BoardClass = GladiusUI::ResolveWidgetClass<ULeaderboardBoardWidget>(BoardWidgetClass, GladiusUI::AssetPaths::LeaderboardBoard);
	ULeaderboardBoardWidget* BoardWidget = Tree->ConstructWidget<ULeaderboardBoardWidget>(BoardClass, TEXT("Board"));
	if (!BoardWidget)
	{
		return;
	}
	BoardWidget->bIsVariable = true;

	//좌측 상단 고정. 크기는 내용에 맞추고 배율만 조정해 글자가 잘리지 않게 함
	UCanvasPanelSlot* BoardSlot = Canvas->AddChildToCanvas(BoardWidget);
	BoardSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	BoardSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	BoardSlot->SetPosition(ScreenOffset);
	BoardSlot->SetAutoSize(true);

	BoardWidget->SetRenderTransformPivot(FVector2D(0.0f, 0.0f));
	BoardWidget->SetRenderScale(FVector2D(BoardScale, BoardScale));
}

void ULeaderboardRotatorWidget::AdvancePage()
{
	switch (CurrentPage)
	{
	case EPage::Total:
		CurrentPage = EPage::BattleTop;
		break;
	case EPage::BattleTop:
		//11위 이상이 없으면 2번 보드는 건너뜀
		CurrentPage = FLeaderboardBuilder::HasBattleLowerPage(RankBattle()) ? EPage::BattleLower : EPage::Total;
		break;
	default:
		CurrentPage = EPage::Total;
		break;
	}
	RefreshCurrentPage();
}

void ULeaderboardRotatorWidget::RefreshCurrentPage()
{
	ULeaderboardBoardWidget* ActiveBoard = SelectBoard(CurrentPage != EPage::Total);
	if (!ActiveBoard)
	{
		return;
	}

	switch (CurrentPage)
	{
	case EPage::Total:
		ActiveBoard->SetPage(FLeaderboardBuilder::MakeTotalPage(RankTotal()));
		break;
	case EPage::BattleTop:
		ActiveBoard->SetPage(FLeaderboardBuilder::MakeBattleTopPage(RankBattle()));
		break;
	case EPage::BattleLower:
	{
		const TArray<FLeaderboardRow> Ranked = RankBattle();
		//보여주는 사이 인원이 줄어 11위가 없어지면 1번 보드로 대체
		ActiveBoard->SetPage(FLeaderboardBuilder::HasBattleLowerPage(Ranked)
			? FLeaderboardBuilder::MakeBattleLowerPage(Ranked)
			: FLeaderboardBuilder::MakeBattleTopPage(Ranked));
		break;
	}
	}
}

TMap<FString, FString> ULeaderboardRotatorWidget::CollectCharacterNames() const
{
	TMap<FString, FString> Names;
	if (const ABattleManager* Manager = BattleManager.Get())
	{
		for (const FCharacterDef& Class : Manager->GetCharacterClasses())
		{
			Names.Add(Class.Id, Class.Name);
		}
	}
	return Names;
}

TArray<FLeaderboardRow> ULeaderboardRotatorWidget::RankTotal() const
{
	TArray<FLeaderboardSource> Sources;
	if (const ABattleManager* Manager = BattleManager.Get())
	{
		//Saved/PlayerData.json에 저장되는 기록과 같은 데이터 (메모리 쪽이 항상 최신)
		for (const TPair<FString, FPlayerRecord>& Pair : Manager->GetPlayerRecords())
		{
			const auto* Admin = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAdminSubsystem>() : nullptr;
			if (Admin && Admin->IsLeaderboardExcluded(Pair.Value.UserId.IsEmpty() ? Pair.Key : Pair.Value.UserId)) continue;
			FLeaderboardSource Source;
			Source.UserId = Pair.Value.UserId.IsEmpty() ? Pair.Key : Pair.Value.UserId;
			Source.Name = Pair.Value.DisplayName;
			Source.CharacterId = Pair.Value.CharacterId;
			Source.Damage = Pair.Value.TotalDamageDealt;
			Sources.Add(MoveTemp(Source));
		}
	}
	return FLeaderboardBuilder::Rank(MoveTemp(Sources), CollectCharacterNames(), false);
}

TArray<FLeaderboardRow> ULeaderboardRotatorWidget::RankBattle() const
{
	TArray<FLeaderboardSource> Sources;
	if (const ABattleManager* Manager = BattleManager.Get())
	{
		const TMap<FString, FBattleSessionEntry>& Session = Manager->GetSessionStats().GetEntries();

		//현재 전장에 있는 유저만. 데미지는 이번 전투씬 누적
		for (const TPair<FString, FBattlePlayer>& Pair : Manager->GetBattlePlayers())
		{
			const FBattleSessionEntry* Entry = Session.Find(Pair.Key);

			FLeaderboardSource Source;
			Source.UserId = Pair.Key;
			Source.Name = Pair.Value.DisplayName;
			Source.CharacterId = Entry ? Entry->CharacterId : FString();
			Source.Damage = Entry ? Entry->DamageDealt : 0;
			Source.Health = Pair.Value.HP;
			Sources.Add(MoveTemp(Source));
		}
	}
	return FLeaderboardBuilder::Rank(MoveTemp(Sources), CollectCharacterNames(), true);
}

bool ULeaderboardRotatorWidget::AddSeparateBattleState(UObject* Asset, TSubclassOf<ULeaderboardBoardWidget> BattleClass)
{
#if WITH_EDITOR
 UWidgetBlueprint* BP = Cast<UWidgetBlueprint>(Asset);
 if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(StaticClass()) || !BP->WidgetTree || !BattleClass) return false;
 UWidgetTree* Tree = BP->WidgetTree;
 // Never rebuild an existing state layout or overwrite subsequent designer edits.
 if (Tree->FindWidget(TEXT("PageSwitcher")))
  return Tree->FindWidget(TEXT("Board")) && Tree->FindWidget(TEXT("BattleBoard"));
 ULeaderboardBoardWidget* Existing = Cast<ULeaderboardBoardWidget>(Tree->FindWidget(TEXT("Board")));
 if (!Existing || !Existing->GetParent() || Tree->FindWidget(TEXT("BattleBoard"))) return false;
 UPanelWidget* Parent = Existing->GetParent();
 const int32 Index = Parent->GetChildIndex(Existing);
 if (Index == INDEX_NONE) return false;
 BP->Modify(); Tree->Modify(); Parent->Modify(); Existing->Modify();
 UWidgetSwitcher* Switcher = GladiusUI::ConstructNamed<UWidgetSwitcher>(Tree, TEXT("PageSwitcher"));
 // Reuse the actual original slot: anchors, offsets, autosize, alignment and Z order survive.
 if (!Parent->ReplaceChildAt(Index, Switcher)) return false;
 Existing->Slot = nullptr;
 Switcher->AddChild(Existing);
 ULeaderboardBoardWidget* Battle = Tree->ConstructWidget<ULeaderboardBoardWidget>(BattleClass, TEXT("BattleBoard"));
 Battle->bIsVariable = true;
 Battle->SetRenderTransform(Existing->GetRenderTransform());
 Battle->SetRenderTransformPivot(Existing->GetRenderTransformPivot());
 Switcher->AddChild(Battle);
 Switcher->SetActiveWidgetIndex(0);
 for (const FName Name : {FName(TEXT("PageSwitcher")), FName(TEXT("BattleBoard"))})
  if (!BP->WidgetVariableNameToGuidMap.Contains(Name)) BP->OnVariableAdded(Name);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status != BS_Error;
#else
 return false;
#endif
}

void ULeaderboardRotatorWidget::ApplyBattleSideLayout(float Width, float Margin)
{
 ULeaderboardBoardWidget* Active = SelectBoard(CurrentPage != EPage::Total);
 UWidget* Container = PageSwitcher ? static_cast<UWidget*>(PageSwitcher.Get()) : Active;
 if (!Active || !Container) return;
 // Fit the visible artwork, including the authored transforms inside each board.
 UWidget* Frame = Active->GetWidgetFromName(TEXT("NativeBackgroundImage"));
 if (!Frame) Frame = Active->GetRootWidget();
 if (!Frame) return;
 const FGeometry& Geometry = Frame->GetCachedGeometry();
 const FGeometry& RootGeometry = GetCachedGeometry();
 const FVector2D TopLeft = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
 const FVector2D TopRight = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D(Geometry.GetLocalSize().X, 0)));
 const float CurrentWidth = TopRight.X - TopLeft.X;
 if (CurrentWidth <= KINDA_SMALL_NUMBER) return; // Wait for the first layout pass.
 const float Ratio = Width / CurrentWidth;
 const FVector2D Target(Margin, Margin);
 if (FMath::IsNearlyEqual(CurrentWidth, Width, .1f) && TopLeft.Equals(Target, .1f)) return;
 const auto* Layout = Cast<UCanvasPanelSlot>(Container->Slot);
 if (!Layout) return;
 const FVector2D Position = Layout->GetPosition();
 FWidgetTransform Transform = Container->GetRenderTransform();
 Container->SetRenderTransformPivot(FVector2D::ZeroVector);
 Transform.Translation = Transform.Translation * Ratio + Target - Position - (TopLeft - Position) * Ratio;
 Transform.Scale *= Ratio;
 Container->SetRenderTransform(Transform);
}
