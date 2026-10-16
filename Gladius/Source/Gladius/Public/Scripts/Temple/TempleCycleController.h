#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Temple/TempleTypes.h"
#include "TempleCycleController.generated.h"

class AYouTubeChatManager;
class AGameManager;
class AGoddess;
class UTempleCommandQueue;
class UTempleAIBatchClient;
class UTempleStatusWidget;

// 신전 씬의 사이클 진행자
// CycleSeconds마다: 채팅 조회 -> 큐잉 종료(스냅샷) -> 여신 사이클 시작 + 대화 배치를 AI 서버로 전송
// TurnCount번 사이클이 끝나면 GameManager를 통해 전투 레벨로 이동
UCLASS()
class GLADIUS_API ATempleCycleController : public AActor
{
	GENERATED_BODY()

public:
	ATempleCycleController();

	//한 사이클 길이(초). 신전 진입 후 이 시간이 지나면 첫 채팅 조회
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Cycle", meta = (ClampMin = "1.0"))
	float CycleSeconds = 40.0f;

	//전투 레벨로 넘어가기 전까지 진행할 사이클 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Cycle", meta = (ClampMin = "1"))
	int32 TurnCount = 5;

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
