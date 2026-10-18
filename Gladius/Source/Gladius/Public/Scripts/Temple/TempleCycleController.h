#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Temple/TempleTypes.h"
#include "TempleCycleController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTempleTurnStarted, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTempleCombatRequested);

class UCommandDispatcher;
class AYouTubeChatManager;
class AGameManager;
class AGoddess;
class UTempleCommandQueue;
class UTempleAIBatchClient;
class UTempleStatusWidget;

// 신전 씬의 사이클 진행자
// CycleSeconds마다: 채팅 조회 -> 큐잉 종료(스냅샷) -> 여신 사이클 시작 + 대화 배치를 AI 서버로 전송
// 사이클은 무제한. 10분 단위 턴과 명시적 전투 이동 요청은 별도로 관리.
UCLASS()
class GLADIUS_API ATempleCycleController : public AActor
{
	GENERATED_BODY()

public:
	ATempleCycleController();

	//한 사이클 길이(초). 신전 진입 후 이 시간이 지나면 첫 채팅 조회
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Cycle", meta = (ClampMin = "1.0"))
	float CycleSeconds = 40.0f;

	// 신전 한 턴의 길이(초). 600초 = 10분. 사이클과 독립적으로 반복됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Turn", meta = (ClampMin = "1.0", Units = "s"))
	float TurnTime = 600.0f;

	// 첫 턴 및 매 턴 시작 시 호출. 추후 축복 초기화 시스템 연결 지점.
	UPROPERTY(BlueprintAssignable, Category = "Temple|Turn")
	FOnTempleTurnStarted OnTurnStarted;

	UPROPERTY(BlueprintAssignable, Category = "Temple|Travel")
	FOnTempleCombatRequested OnCombatRequested;

	// 별도 manager가 이동을 담당하게 되면 false로 설정하고 OnCombatRequested를 구독합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Travel")
	bool bAutoTravelOnCombatRequest = true;

	UFUNCTION(BlueprintCallable, Category = "Temple|Travel")
	void RequestCombatTravel();

	UFUNCTION(BlueprintPure, Category = "Temple|Turn")
	int32 GetCurrentTurn() const { return CurrentTurn; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|AI")
	FString TempleAIBatchEndpointUrl = TEXT("http://127.0.0.1:8765/temple/respond-batch");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|AI")
	FString TempleAIClientToken;

	//상태 UI 클래스. WBP로 교체 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|UI")
	TSubclassOf<UTempleStatusWidget> StatusWidgetClass;

	UFUNCTION(BlueprintPure, Category = "Temple|Cycle")
	int32 GetCompletedTurns() const { return CompletedTurns; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FTempleFlowTestAccess;
	//에디터에서 지정하지 않으면 BeginPlay에서 월드를 검색해 채움
	UPROPERTY(EditInstanceOnly, Category = "Temple|References")
	TObjectPtr<AYouTubeChatManager> ChatManager;

	UPROPERTY(EditInstanceOnly, Category = "Temple|References")
	TObjectPtr<AGoddess> Goddess;

	UPROPERTY(EditInstanceOnly, Category = "Temple|References")
	TObjectPtr<AGameManager> GameManager;

	UPROPERTY(Transient)
	TObjectPtr<UTempleCommandQueue> CommandQueue;

	UPROPERTY(Transient)
	TObjectPtr<UTempleAIBatchClient> BatchClient;

	UPROPERTY(Transient)
	TObjectPtr<UTempleStatusWidget> StatusWidget;

	FTimerHandle CycleTimerHandle;
	FTimerHandle TurnTimerHandle;
	FTimerHandle TravelTimerHandle;
	TWeakObjectPtr<UCommandDispatcher> BoundDispatcher;
	int32 CurrentTurn = 0;
	bool bLeavingTemple = false;
	bool bWasMouseCursorVisible = false;
	void HandleTurnTimer();
	void HandleCombatCommand();
	int32 CompletedTurns = 0;

	//현재 사이클에서 AI 응답을 기다리는 배치와 그 질문들 (응답의 uid로 원본 질문을 찾기 위함)
	FString ActiveBatchId;
	TArray<FTempleTalkBatchMessage> ActiveTalks;

	void ResolveReferences();
	void CreateStatusWidget();

	void HandleCycleTimer();
	void HandleChatFetchFinished(bool bSuccess);
	void StartCycle();

	void HandleBatchSucceeded(const FTempleTalkBatchResult& Result);
	void HandleBatchFailed(const FString& BatchId, const FString& ErrorMessage);
};
