#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Scripts/Battle/BattleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "TimerManager.h"

void ULeaderboardRotatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void ULeaderboardRotatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

void ULeaderboardRotatorWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Canvas;

	if (!BoardWidgetClass)
	{
		BoardWidgetClass = ULeaderboardBoardWidget::StaticClass();
	}
	Board = CreateWidget<ULeaderboardBoardWidget>(this, BoardWidgetClass);
	if (!Board)
	{
		return;
	}

	//좌측 상단 고정. 크기는 내용에 맞추고 배율만 조정해 글자가 잘리지 않게 함
	UCanvasPanelSlot* BoardSlot = Canvas->AddChildToCanvas(Board);
	BoardSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	BoardSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	BoardSlot->SetPosition(ScreenOffset);
	BoardSlot->SetAutoSize(true);

	Board->SetRenderTransformPivot(FVector2D(0.0f, 0.0f));
	Board->SetRenderScale(FVector2D(BoardScale, BoardScale));
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
	if (!Board)
	{
		return;
	}

	switch (CurrentPage)
	{
	case EPage::Total:
		Board->SetPage(FLeaderboardBuilder::MakeTotalPage(RankTotal()));
		break;
	case EPage::BattleTop:
		Board->SetPage(FLeaderboardBuilder::MakeBattleTopPage(RankBattle()));
		break;
	case EPage::BattleLower:
	{
		const TArray<FLeaderboardRow> Ranked = RankBattle();
		//보여주는 사이 인원이 줄어 11위가 없어지면 1번 보드로 대체
		Board->SetPage(FLeaderboardBuilder::HasBattleLowerPage(Ranked)
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
