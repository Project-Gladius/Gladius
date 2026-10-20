#include "Scripts/Temple/TempleCycleController.h"
#include "Scripts/Temple/Goddess.h"
#include "Scripts/Battle/BattleManager.h"
#include "Engine/World.h"
#include "Scripts/Temple/TempleAIBatchClient.h"
#include "Scripts/Temple/TempleCommandQueue.h"
#include "Scripts/Temple/TempleStatusWidget.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "GameFramework/PlayerController.h"

ATempleCycleController::ATempleCycleController()
{
	PrimaryActorTick.bCanEverTick = false;
	StatusWidgetClass = UTempleStatusWidget::StaticClass();
}

void ATempleCycleController::BeginPlay()
{
	Super::BeginPlay();

 ResolveReferences();
 if (IsValid(Goddess))
 {
  BoundGoddess = Goddess;
  Goddess->OnEnhanceExecuted.AddUniqueDynamic(this, &ATempleCycleController::HandleEnhanceExecuted);
 }

	CommandQueue = NewObject<UTempleCommandQueue>(this);
	BatchClient = NewObject<UTempleAIBatchClient>(this);
	BatchClient->EndpointUrl = TempleAIBatchEndpointUrl;
	BatchClient->ClientToken = TempleAIClientToken;
	BatchClient->OnBatchSucceeded.AddUObject(this, &ATempleCycleController::HandleBatchSucceeded);
	BatchClient->OnBatchFailed.AddUObject(this, &ATempleCycleController::HandleBatchFailed);

	if (ChatManager)
	{
		//신전상태에서 들어오는 !강화/!대화를 큐에 쌓기 시작
		CommandQueue->Bind(ChatManager->GetCommandDispatcher());
		BoundDispatcher = ChatManager->GetCommandDispatcher();
		if (BoundDispatcher.IsValid()) BoundDispatcher->OnTempleCombatCommand.AddUObject(this, &ATempleCycleController::HandleCombatCommand);
		ChatManager->OnChatFetchFinished.AddUObject(this, &ATempleCycleController::HandleChatFetchFinished);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TempleCycleController: YouTubeChatManager를 찾지 못했습니다. 채팅 없이 사이클만 진행합니다."));
	}

	if (!Goddess)
	{
		UE_LOG(LogTemp, Error, TEXT("TempleCycleController: Goddess를 찾지 못했습니다."));
	}

	CurrentTurn = 1;
	CreateStatusWidget();
	OnTurnStarted.Broadcast(CurrentTurn);
	GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &ATempleCycleController::HandleTurnTimer, FMath::Max(TurnTime, 1.0f), true);

	//진입 후 CycleSeconds가 지나면 첫 조회, 이후 CycleSeconds마다 반복
	GetWorldTimerManager().SetTimer(CycleTimerHandle, this, &ATempleCycleController::HandleCycleTimer, FMath::Max(CycleSeconds, 1.0f), true);
}

void ATempleCycleController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CycleTimerHandle);
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().ClearTimer(TravelTimerHandle);
 if (BoundGoddess.IsValid()) BoundGoddess->OnEnhanceExecuted.RemoveDynamic(this, &ATempleCycleController::HandleEnhanceExecuted);
	if (BoundDispatcher.IsValid()) BoundDispatcher->OnTempleCombatCommand.RemoveAll(this);

	if (ChatManager)
	{
		ChatManager->OnChatFetchFinished.RemoveAll(this);
	}
	if (CommandQueue)
	{
		CommandQueue->Unbind();
	}
	if (BatchClient)
	{
		BatchClient->CancelActiveBatch();
	}
	if (StatusWidget)
	{
		StatusWidget->RemoveFromParent();
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->bShowMouseCursor = bWasMouseCursorVisible;
			PC->SetInputMode(FInputModeGameOnly());
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ATempleCycleController::ResolveReferences()
{
	if (!ChatManager)
	{
		ChatManager = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AYouTubeChatManager::StaticClass()));
	}
	if (!Goddess)
	{
		Goddess = Cast<AGoddess>(UGameplayStatics::GetActorOfClass(GetWorld(), AGoddess::StaticClass()));
	}
	if (!GameManager)
	{
		GameManager = Cast<AGameManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGameManager::StaticClass()));
	}
 if (!IsValid(BattleManager))
  BattleManager = Cast<ABattleManager>(UGameplayStatics::GetActorOfClass(this, ABattleManager::StaticClass()));
 if (!IsValid(BattleManager))
 {
  BattleManager = GetWorld()->SpawnActorDeferred<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity);
  if (IsValid(BattleManager))
  {
   BattleManager->bAutoStartBattle = false;
   BattleManager->FinishSpawning(FTransform::Identity);
  }
 }
 if (IsValid(BattleManager)) BattleManager->InitializePlayerRecords();

}

void ATempleCycleController::CreateStatusWidget()
{
	if (!StatusWidgetClass)
	{
		return;
	}

	StatusWidget = CreateWidget<UTempleStatusWidget>(GetWorld(), StatusWidgetClass);
	if (StatusWidget)
	{
		StatusWidget->AddToViewport();
		StatusWidget->BindToGoddess(Goddess);
		StatusWidget->BindToController(this);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			bWasMouseCursorVisible = PC->bShowMouseCursor;
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
}

void ATempleCycleController::HandleCycleTimer()
{
	if (bLeavingTemple) return;

	if (ChatManager)
	{
		ChatManager->RequestChatFetch(); //완료되면 HandleChatFetchFinished에서 사이클 시작
	}
	else
	{
		StartCycle();
	}
}

void ATempleCycleController::HandleChatFetchFinished(bool bSuccess)
{
	//조회에 실패해도 그동안 쌓인 명령으로 사이클은 진행 (턴 시간이 밀리지 않도록)
	if (!bLeavingTemple) StartCycle();
}

void ATempleCycleController::StartCycle()
{
	if (bLeavingTemple) return;
	++CompletedTurns;

	//큐잉 종료: 이번 사이클에 처리할 명령을 가져가고 다음 사이클 큐잉 시작
	FTempleCycleCommands Commands = CommandQueue->TakeSnapshot();

	UE_LOG(LogTemp, Log, TEXT("[신전] 사이클 %d 시작: 대화 %d건"), CompletedTurns, Commands.Talks.Num());

	ActiveTalks = Commands.Talks;
	ActiveBatchId.Reset();

	if (Goddess)
	{
		Goddess->StartCycle(Commands);
	}

	//대화는 여신과 별개로 바로 AI 서버에 요청
	if (!Commands.Talks.IsEmpty())
	{
		ActiveBatchId = BatchClient->SendBatch(Commands.Talks, Commands.WindowStartedAt, Commands.WindowEndedAt);
	}
}

void ATempleCycleController::HandleBatchSucceeded(const FTempleTalkBatchResult& Result)
{
	if (bLeavingTemple || Result.BatchId != ActiveBatchId || !Goddess)
	{
		return; //이전 사이클의 응답
	}
	ActiveBatchId.Reset();

	//응답의 uid로 원본 질문을 찾아 여신이 처리할 대화 항목을 만듦 (서버가 검열로 뺀 질문은 응답이 없으므로 자연히 제외)
	TArray<FTempleTalkItem> Items;
	for (const FTempleIndividualResponse& Response : Result.Responses)
	{
		const int32 TalkIndex = ActiveTalks.IndexOfByPredicate([&Response](const FTempleTalkBatchMessage& Talk)
		{
			return Talk.Uid == Response.Uid;
		});
		if (TalkIndex == INDEX_NONE)
		{
			continue;
		}

		const FTempleTalkBatchMessage& Talk = ActiveTalks[TalkIndex];

		FTempleTalkItem Item;
		Item.Uid = Talk.Uid;
		Item.UserId = Talk.UserId;
		Item.Username = Response.Username.IsEmpty() ? Talk.Username : Response.Username;
		Item.QueryMessage = Talk.Message;
		Item.ResponseText = Response.ResponseText;
		Item.Emotion = Response.Emotion;
		Item.Priority = Response.Priority;
		Item.ArrivalIndex = TalkIndex;
		Items.Add(MoveTemp(Item));
	}

	Goddess->ReceiveTalkItems(MoveTemp(Items));
}

void ATempleCycleController::HandleBatchFailed(const FString& BatchId, const FString& ErrorMessage)
{
	if (bLeavingTemple || BatchId != ActiveBatchId || !Goddess)
	{
		return;
	}
	ActiveBatchId.Reset();

	Goddess->SetNoTalks();
}

void ATempleCycleController::HandleTurnTimer()
{
	if (bLeavingTemple) return;
	++CurrentTurn;
	OnTurnStarted.Broadcast(CurrentTurn);
}

void ATempleCycleController::HandleCombatCommand()
{
	// 채팅 목록 처리 콜백을 빠져나온 다음 프레임에 이동합니다.
	if (!bLeavingTemple && !GetWorldTimerManager().TimerExists(TravelTimerHandle))
		TravelTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &ATempleCycleController::RequestCombatTravel);
}

void ATempleCycleController::RequestCombatTravel()
{
	if (bLeavingTemple) return;
	ResolveReferences();
	if (bAutoTravelOnCombatRequest && (!GameManager || GameManager->CombatLevel.IsNull()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[신전] 전투 이동 불가: GameManager와 CombatLevel을 지정하세요."));
		return;
	}
	bLeavingTemple = true;
	GetWorldTimerManager().ClearTimer(CycleTimerHandle);
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().ClearTimer(TravelTimerHandle);
	ActiveBatchId.Reset();
	if (BatchClient) BatchClient->CancelActiveBatch();
	if (Goddess) Goddess->StopCycle();
	OnCombatRequested.Broadcast();
	if (bAutoTravelOnCombatRequest && IsValid(GameManager)) GameManager->TravelToCombat();
}

void ATempleCycleController::HandleEnhanceExecuted(const FString& UserId, const FString& Username, int32 Tier)
{
 if (bLeavingTemple) return;
 if (!IsValid(BattleManager)) ResolveReferences();
 if (IsValid(BattleManager)) BattleManager->ApplyTempleEnhancement(UserId, Username, Tier);
}
