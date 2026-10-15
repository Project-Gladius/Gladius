#include "Scripts/Backend/TurnManager.h"

#include "Kismet/GameplayStatics.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Backend/PlayerDataManager.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "TimerManager.h"

ATurnManager::ATurnManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATurnManager::BeginPlay()
{
	Super::BeginPlay();
	ResolveReferences();
	if (bAutoStartBattle) { StartBattle(); }
}

void ATurnManager::ResolveReferences()
{
	if (!GameManager) { GameManager = Cast<AGameManager>(UGameplayStatics::GetActorOfClass(this, AGameManager::StaticClass())); }
	if (!ChatManager) { ChatManager = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass())); }
	if (ChatManager && !ChatFetchHandle.IsValid())
	{
		ChatFetchHandle = ChatManager->OnChatFetchFinished.AddUObject(this, &ATurnManager::HandleChatFetchFinished);
	}
}

void ATurnManager::StopChatPolling()
{
	if (IsValid(ChatManager)) { ChatManager->SetCombatAutoPollingEnabled(false); }
}

bool ATurnManager::StartBattle()
{
	if (Phase != ETurnPhase::Idle || TotalTurns < 1) { return false; }
	ResolveReferences();
	CurrentTurn = 0;
	BattleTurnLimit = TotalTurns;
	if (GameManager) { GameManager->SetGameState(EGameState::Combat); }
	if (UPlayerDataManager* Players = UPlayerDataManager::Get(this)) { Players->EndCombat(); }
	StopChatPolling();
	EnterPhase(ETurnPhase::BattleStart);
	return true;
}

void ATurnManager::EnterPhase(ETurnPhase NewPhase)
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	Phase = NewPhase;
	const int32 EnteredId = ++PhaseId;
	if (Phase != ETurnPhase::BattlePrepare) { StopChatPolling(); }
	if (Phase == ETurnPhase::BattlePrepare) { ++CurrentTurn; }
	if (Phase == ETurnPhase::BattleEnd)
	{
		StopChatPolling();
		if (IsValid(ChatManager)) { ChatManager->CancelPendingChatFetch(); }
		if (UPlayerDataManager* Players = UPlayerDataManager::Get(this)) { Players->EndCombat(); }
	}
	OnPhaseStarted.Broadcast(Phase, CurrentTurn, EnteredId);
	// Event listeners may synchronously complete a phase or end the battle.
	if (PhaseId != EnteredId || IsActorBeingDestroyed()) { return; }
	switch (Phase)
	{
	case ETurnPhase::BattlePrepare:
		if (PrepareDurationSeconds > 0.0f)
		{
			FTimerDelegate Callback;
			Callback.BindWeakLambda(this, [this, EnteredId]() { CompletePhase(EnteredId); });
			GetWorldTimerManager().SetTimer(PhaseTimer, Callback, PrepareDurationSeconds, false);
		}
		if (ChatManager) { ChatManager->SetCombatAutoPollingEnabled(true); }
		break;
	case ETurnPhase::Waiting:
		StopChatPolling();
		RequestFinalChat();
		break;
	case ETurnPhase::CutScenes:
		OnCutSceneStarted.Broadcast(ActiveCutSceneIds[CutSceneIndex], CutSceneIndex, EnteredId);
		break;
	default:
		break;
	}
}

bool ATurnManager::CompletePhase(int32 ExpectedPhaseId)
{
	if (ExpectedPhaseId != PhaseId || IsActorBeingDestroyed()) { return false; }
	switch (Phase)
	{
	case ETurnPhase::BattleStart:
		EnterPhase(ETurnPhase::BattlePrepare);
		return true;
	case ETurnPhase::BattlePrepare:
		EnterPhase(ETurnPhase::Waiting);
		return true;
	case ETurnPhase::CutScenes:
		if (++CutSceneIndex < ActiveCutSceneIds.Num()) { EnterPhase(ETurnPhase::CutScenes); }
		else { EnterPhase(ETurnPhase::Battle); }
		return true;
	case ETurnPhase::Battle:
		EnterPhase(CurrentTurn >= BattleTurnLimit ? ETurnPhase::BattleEnd : ETurnPhase::BattlePrepare);
		return true;
	case ETurnPhase::BattleEnd:
		if (GameManager) { GameManager->SetGameState(EGameState::Temple); }
		EnterPhase(ETurnPhase::Idle);
		OnBattleFinished.Broadcast();
		return true;
	default:
		return false;
	}
}

void ATurnManager::BeginCutScenes()
{
	ActiveCutSceneIds = CutSceneIds;
	CutSceneIndex = ActiveCutSceneIds.IsEmpty() ? INDEX_NONE : 0;
	EnterPhase(ActiveCutSceneIds.IsEmpty() ? ETurnPhase::Battle : ETurnPhase::CutScenes);
}

void ATurnManager::RequestFinalChat()
{
	const int32 WaitingId = PhaseId;
	if (WaitingTimeoutSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer,
			FTimerDelegate::CreateUObject(this, &ATurnManager::HandleWaitingTimeout, WaitingId), WaitingTimeoutSeconds, false);
	}
	if (!IsValid(ChatManager))
	{
		GetWorldTimerManager().ClearTimer(PhaseTimer);
		OnWaitingFailed.Broadcast(WaitingId);
		return;
	}
	// If a poll is already running, that response is the final response for this turn.
	if (!ChatManager->IsChatFetchInFlight()) { ChatManager->RequestChatFetch(); }
}

void ATurnManager::HandleChatFetchFinished(bool bSuccess)
{
	if (Phase != ETurnPhase::Waiting) { return; }
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	if (bSuccess) { BeginCutScenes(); }
	else { OnWaitingFailed.Broadcast(PhaseId); }
}

void ATurnManager::HandleWaitingTimeout(int32 ExpectedPhaseId)
{
	if (Phase == ETurnPhase::Waiting && PhaseId == ExpectedPhaseId) { OnWaitingFailed.Broadcast(PhaseId); }
}

bool ATurnManager::RetryWaiting(int32 ExpectedPhaseId)
{
	if (Phase != ETurnPhase::Waiting || PhaseId != ExpectedPhaseId || !IsValid(ChatManager)) { return false; }
	RequestFinalChat();
	return true;
}

bool ATurnManager::ContinueWithoutFinalChat(int32 ExpectedPhaseId)
{
	if (Phase != ETurnPhase::Waiting || PhaseId != ExpectedPhaseId) { return false; }
	if (IsValid(ChatManager)) { ChatManager->CancelPendingChatFetch(); }
	BeginCutScenes();
	return true;
}

bool ATurnManager::EndBattle()
{
	if (Phase == ETurnPhase::Idle || Phase == ETurnPhase::BattleEnd) { return false; }
	EnterPhase(ETurnPhase::BattleEnd);
	return true;
}

void ATurnManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	StopChatPolling();
	if (IsValid(ChatManager))
	{
		ChatManager->CancelPendingChatFetch();
		ChatManager->OnChatFetchFinished.Remove(ChatFetchHandle);
	}
	Super::EndPlay(EndPlayReason);
}
