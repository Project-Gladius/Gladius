#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Temple/TempleTypes.h"
#include "TempleCycleController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTempleCombatRequested);

class UCommandDispatcher;
class AYouTubeChatManager;
class AGameManager;
class AGoddess;
class ABattleManager;
class UTempleCommandQueue;
class UTempleAIBatchClient;
class UTempleStatusWidget;
class UChatLogWidget;
class USessionLogWidget;
class UBattleTopHudWidget;

// 신전 씬의 사이클 진행자
// CycleSeconds마다: 채팅 조회 -> 큐잉 종료(스냅샷) -> 여신 사이클 시작 + 대화 배치를 AI 서버로 전송
// 사이클은 명시적으로 전투 이동을 요청할 때까지 반복.
UCLASS()
class GLADIUS_API ATempleCycleController : public AActor
{
	GENERATED_BODY()

public:
	ATempleCycleController();

	//한 사이클 길이(초). 신전 진입 후 이 시간이 지나면 첫 채팅 조회
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Cycle", meta = (ClampMin = "1.0"))
	float CycleSeconds = 25.0f;

	//구버전 씬 호환용 값. 사이클 횟수에 따른 전투 이동은 사용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Cycle", meta = (ClampMin = "0"))
	int32 CyclesBeforeCombat = 0;

	UPROPERTY(BlueprintAssignable, Category = "Temple|Travel")
	FOnTempleCombatRequested OnCombatRequested;

	// 별도 manager가 이동을 담당하게 되면 false로 설정하고 OnCombatRequested를 구독합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Travel")
	bool bAutoTravelOnCombatRequest = true;

	UFUNCTION(BlueprintCallable, Category = "Temple|Travel")
	void RequestCombatTravel();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|AI")
	FString TempleAIBatchEndpointUrl = TEXT("http://127.0.0.1:8765/temple/respond-batch");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|AI")
	FString TempleAIClientToken;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|UI")
 TSubclassOf<UBattleTopHudWidget> TopHudWidgetClass;

	//상태 UI 클래스. WBP로 교체 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|UI")
	TSubclassOf<UTempleStatusWidget> StatusWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|UI")
	TSubclassOf<UChatLogWidget> ChatLogWidgetClass;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log")
 TSubclassOf<USessionLogWidget> SessionLogWidgetClass;

	UFUNCTION(BlueprintPure, Category = "Temple|Cycle")
	int32 GetCompletedCycles() const { return CompletedCycles; }
 void SetAIMaintenance(bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FTempleFlowTestAccess;
 friend struct FTempleTopHudTestAccess;
	//에디터에서 지정하지 않으면 BeginPlay에서 월드를 검색해 채움
	UPROPERTY(EditInstanceOnly, Category = "Temple|References")
	TObjectPtr<AYouTubeChatManager> ChatManager;

	UPROPERTY(EditInstanceOnly, Category = "Temple|References")
	TObjectPtr<AGoddess> Goddess;

	UPROPERTY(EditInstanceOnly, Category = "Temple|References")
	TObjectPtr<AGameManager> GameManager;

 UPROPERTY(EditInstanceOnly, Category = "Temple|References")
 TObjectPtr<ABattleManager> BattleManager;
 TWeakObjectPtr<AGoddess> BoundGoddess;
 UFUNCTION()
 void HandleEnhanceExecuted(const FString& UserId, const FString& Username, int32 Tier);
	UFUNCTION()
	void HandleGoddessStateChanged(EGoddessState NewState, const FGoddessStateContext& Context);

	UPROPERTY(Transient)
	TObjectPtr<UTempleCommandQueue> CommandQueue;

	UPROPERTY(Transient)
	TObjectPtr<UTempleAIBatchClient> BatchClient;

	UPROPERTY(Transient)
	TObjectPtr<UTempleStatusWidget> StatusWidget;

	UPROPERTY(Transient)
	TObjectPtr<UChatLogWidget> ChatLogWidget;

 UPROPERTY(Transient)
 TObjectPtr<USessionLogWidget> SessionLogWidget;

 UPROPERTY(Transient)
 TObjectPtr<UBattleTopHudWidget> TopHudWidget;

	FTimerHandle CycleTimerHandle;
	FTimerHandle TravelTimerHandle;
	TWeakObjectPtr<UCommandDispatcher> BoundDispatcher;
	bool bLeavingTemple = false;
	bool bWasMouseCursorVisible = false;
	void HandleCombatCommand();
	int32 CompletedCycles = 0;

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
