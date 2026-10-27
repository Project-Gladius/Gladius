#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h"
#include "Scripts/Temple/TempleTypes.h"
#include "TempleAIBatchClient.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTempleBatchSucceeded, const FTempleTalkBatchResult& /*Result*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTempleBatchFailed, const FString& /*BatchId*/, const FString& /*ErrorMessage*/);

// Temple AI 서버의 /temple/respond-batch 에 대화 배치를 보내고 응답을 검증/파싱하는 HTTP 클라이언트
// 한 번에 하나의 배치만 처리하며, 새 배치를 보내면 이전 배치 요청은 취소됨
UCLASS()
class GLADIUS_API UTempleAIBatchClient : public UObject
{
	GENERATED_BODY()

public:
	FString EndpointUrl = TEXT("http://127.0.0.1:8765/temple/respond-batch");
	FString ClientToken;
	float RequestTimeoutSeconds = 22.0f;
	int32 MaxRetryCount = 1; //연결 실패/5xx일 때 재시도 횟수

	//배치를 전송하고 BatchId를 반환. 메시지가 비어 있으면 전송하지 않고 빈 문자열 반환
	FString SendBatch(const TArray<FTempleTalkBatchMessage>& Messages, const FDateTime& WindowStartedAt, const FDateTime& WindowEndedAt);

	//진행 중인 요청을 취소 (결과 델리게이트는 호출되지 않음)
	void CancelActiveBatch();

	FOnTempleBatchSucceeded OnBatchSucceeded;
	FOnTempleBatchFailed OnBatchFailed;

private:
	struct FActiveBatch
	{
		FString BatchId;
		FDateTime WindowStartedAt;
		FDateTime WindowEndedAt;
		TArray<FTempleTalkBatchMessage> Messages;
		int32 RetryCount = 0;
	};

	FActiveBatch ActiveBatch;
	FHttpRequestPtr ActiveRequest;
	int64 BatchSequence = 0;

	void SendActiveRequest();
	void OnHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	bool TryParseResult(const FString& Body, FTempleTalkBatchResult& OutResult, FString& OutError) const;
	void Finish();
};
