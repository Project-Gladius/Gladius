#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurnManager.generated.h"

class AGameManager;
class AYouTubeChatManager;

UENUM(BlueprintType)
enum class ETurnPhase : uint8
{
	Idle,
	BattleStart,
	BattlePrepare,
	Waiting,
	CutScenes,
	Battle,
	BattleEnd
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTurnPhaseStarted, ETurnPhase, Phase, int32, TurnNumber, int32, PhaseId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTurnCutSceneStarted, FName, CutSceneId, int32, CutSceneIndex, int32, PhaseId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnWaitingFailed, int32, PhaseId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTurnBattleFinished);

/** Battle-level coordinator. UI, sequences and combat report completion using PhaseId. */
UCLASS()
class GLADIUS_API ATurnManager : public AActor
{
	GENERATED_BODY()

public:
	ATurnManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle", meta = (ClampMin = "1"))
	int32 TotalTurns = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	bool bAutoStartBattle = true;

	/** Zero means the UI/controller explicitly completes the preparation phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle", meta = (ClampMin = "0"))
	float PrepareDurationSeconds = 40.0f;

	/** Timeout notifies the UI; it never silently skips the final chat request. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle", meta = (ClampMin = "0"))
	float WaitingTimeoutSeconds = 5.0f;

	/** Ordered per-turn shots. Empty means go directly from Waiting to Battle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|CutScenes")
	TArray<FName> CutSceneIds;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Battle|References")
	TObjectPtr<AGameManager> GameManager;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Battle|References")
	TObjectPtr<AYouTubeChatManager> ChatManager;

	UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
	FOnTurnPhaseStarted OnPhaseStarted;

	UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
	FOnTurnCutSceneStarted OnCutSceneStarted;

	UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
	FOnTurnWaitingFailed OnWaitingFailed;

	/** Fired only after the BattleEnd presentation has completed. */
	UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
	FOnTurnBattleFinished OnBattleFinished;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool StartBattle();

	/** Completes start, preparation, each cutscene, combat, or end. Waiting has its own APIs. */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool CompletePhase(int32 ExpectedPhaseId);

	/** Allows victory/defeat or an external controller to end the battle early. */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool EndBattle();

	UFUNCTION(BlueprintCallable, Category = "Battle|Waiting")
	bool RetryWaiting(int32 ExpectedPhaseId);

	UFUNCTION(BlueprintCallable, Category = "Battle|Waiting")
	bool ContinueWithoutFinalChat(int32 ExpectedPhaseId);

	UFUNCTION(BlueprintPure, Category = "Battle")
	ETurnPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetCurrentTurn() const { return CurrentTurn; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetPhaseId() const { return PhaseId; }

	/** Final API replies are accepted during Waiting, then the action set should be frozen. */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsAcceptingPlayerActions() const { return Phase == ETurnPhase::BattlePrepare || Phase == ETurnPhase::Waiting; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveReferences();
	void EnterPhase(ETurnPhase NewPhase);
	void BeginCutScenes();
	void HandleChatFetchFinished(bool bSuccess);
	void RequestFinalChat();
	void HandleWaitingTimeout(int32 ExpectedPhaseId);
	void StopChatPolling();

	UPROPERTY(Transient)
	ETurnPhase Phase = ETurnPhase::Idle;

	int32 PhaseId = 0;
	int32 CurrentTurn = 0;
	int32 BattleTurnLimit = 10;
	int32 CutSceneIndex = INDEX_NONE;
	TArray<FName> ActiveCutSceneIds;
	FTimerHandle PhaseTimer;
	FDelegateHandle ChatFetchHandle;
};
