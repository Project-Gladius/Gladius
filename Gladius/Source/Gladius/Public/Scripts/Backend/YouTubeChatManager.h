#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "GladiusTypes.h"
#include "YouTubeChatManager.generated.h"

class UCommandDispatcher;
class AGameManager;

USTRUCT(BlueprintType)
struct FCommentLog
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log");
	FString CommentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log");
	FString UserId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log");
	FDateTime CreatedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log");
	FString Content;
};

USTRUCT(BlueprintType)
struct FTempleTalkBatchMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString UserId;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Username;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Message;
};

USTRUCT(BlueprintType)
struct FTempleIndividualResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Username;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 Priority = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString ResponseText;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Emotion;
};

USTRUCT(BlueprintType)
struct FTempleTalkBatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString BatchId;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") TArray<FTempleIndividualResponse> Responses;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 AcceptedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 RejectedCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTempleTalkBatchResult, const FTempleTalkBatchResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTempleTalkBatchError, const FString&, ErrorMessage);

UCLASS()
class GLADIUS_API AYouTubeChatManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AYouTubeChatManager();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YouTube API");
	FString ApiKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YouTube API");
	FString VideoId;

	//채팅 수신 시작 함수
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void StartFetchingChat(); 

	//채팅 수신 중지 함수
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void StopFetchingChat();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
	TMap<FString, FUserInfo> UserDatabase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
	TArray<FCommentLog> CommentLogs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float SpamTimeLimitSeconds = 0.5f; //몇초안에 또 댓글을 입력하면 스팸인지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 MaxAllowedComments = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float TemplePollIntervalSeconds = 40.0f; //신전상태일 때 채팅을 긁어오는 주기(초)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float CombatPollIntervalSeconds = 5.0f; //전투상태일 때 채팅을 긁어오는 주기(초)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI|Batch")
	FString TempleAIBatchEndpointUrl = TEXT("http://127.0.0.1:8765/temple/respond-batch");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI|Batch")
	FString TempleAIClientToken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI|Batch", meta = (ClampMin = "1.0"))
	float TalkBatchWindowSeconds = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI|Batch", meta = (ClampMin = "1", ClampMax = "30"))
	int32 MaxTalkMessagesPerBatch = 30;

	UPROPERTY(BlueprintAssignable, Category = "Temple AI|Batch")
	FOnTempleTalkBatchResult OnTempleTalkBatchResult;

	UPROPERTY(BlueprintAssignable, Category = "Temple AI|Batch")
	FOnTempleTalkBatchError OnTempleTalkBatchError;

	UFUNCTION(BlueprintCallable, Category = "YouTube")
	void OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content);

	UFUNCTION(BlueprintCallable, Category = "YouTube")
	void OnReceiveYouTubeCommentWithUsername(FString CommentId, FString UserId, FString Username, FString Content);

	UFUNCTION(BlueprintCallable, Category = "Temple AI|Batch")
	bool DequeueNextTempleResponse(FTempleIndividualResponse& OutResponse);

private:
	//현재 전투/신전 상태를 들고 있는 GameManager 참조. 에디터에서 직접 지정하지 않으면 BeginPlay에서 월드를 검색해 채움
	UPROPERTY(EditInstanceOnly, Category = "Game State")
	TObjectPtr<AGameManager> GameManagerRef;

	//CommandDispatcher의 GetGameStateDelegate에 바인딩되는 함수. GameManagerRef가 없으면 기본값(신전상태) 반환
	EGameState GetCurrentGameState() const;

	//현재 게임 상태(신전/전투)에 맞는 채팅 폴링 주기를 반환
	float GetPollIntervalForCurrentState() const;

	//CommandDispatcher의 OnUserLoginRequired 구독 함수
	//TODO: DB에서 플레이어 데이터를 조회하고 없으면 새로 생성한 뒤, 필드에 플레이어 캐릭터를 스폰하는 로직 연결 예정
	void HandleUserLoginRequired(const FString& UserId);

	//API 상태 저장용 변수
	FString LiveChatId;
	FString NextPageToken;
	FTimerHandle ChatFetchTimerHandle;
	FTimerHandle TalkBatchTimerHandle;

	//Http 요청 응답 및 콜백 함수
	void RequestLiveChatId();
	void OnLiveChatIdResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void RequestMessages();
	void OnChatMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString PreprocessCommand(const FString& Input);
	bool IsSpamming(const FString& UserId, const FString& Content, const FDateTime& CurrentTime);
	void HandleSpamPenalty(FUserInfo& User, const FDateTime& Now);
	void ExecuteGameCommand(const FString& CommentId, const FString& UserId, const FString& Username, const FString& Command);
	void SendToUserRemainingTime(const FString& UserId, FTimespan Remaining);

	//명령어(ECommandType) -> 처리 함수를 연결해주는 디스패처
	UPROPERTY()
	TObjectPtr<UCommandDispatcher> CommandDispatcher;

	//채팅을 그날그날 txt 파일로 저장하는 함수
	void SaveCommentLogToFile(const FString& UserId, const FString& Content, const FDateTime& Now);

	struct FPendingTempleTalkBatch
	{
		FString BatchId;
		FDateTime WindowStartedAt;
		FDateTime WindowEndedAt;
		TArray<FTempleTalkBatchMessage> Messages;
	};

	struct FQueuedTempleResponse
	{
		FTempleIndividualResponse Response;
		FDateTime ReceivedAt;
	};

	FDateTime CurrentTalkWindowStartedAt;
	TArray<FTempleTalkBatchMessage> CurrentTalkMessages;
	TSet<FString> CurrentTalkUserIds;
	TSet<FString> ProcessedTalkUids;
	int32 CurrentTalkCharacterCount = 0;
	int64 TalkBatchSequence = 0;
	TArray<FPendingTempleTalkBatch> PendingTalkBatches;
	FPendingTempleTalkBatch ActiveTalkBatch;
	bool bTempleBatchRequestInFlight = false;
	int32 ActiveBatchRetryCount = 0;
	TArray<FQueuedTempleResponse> TempleResponseQueue;

	void AddTalkToCurrentBatch(const FString& CommentId, const FString& UserId, const FString& Username, const FString& Message);
	void FlushTalkBatch();
	void TrySendNextTalkBatch();
	void SendActiveTalkBatchRequest();
	void OnTempleBatchHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void FinishActiveTalkBatch();
};
