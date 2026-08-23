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
	float MinPollIntervalSeconds = 5.0f; //채팅을 긁어오는 최소 주기(초)

	UFUNCTION(BlueprintCallable, Category = "YouTube")
	void OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content);

private:
	//현재 전투/신전 상태를 들고 있는 GameManager 참조. 에디터에서 직접 지정하지 않으면 BeginPlay에서 월드를 검색해 채움
	UPROPERTY(EditInstanceOnly, Category = "Game State")
	TObjectPtr<AGameManager> GameManagerRef;

	//CommandDispatcher의 GetGameStateDelegate에 바인딩되는 함수. GameManagerRef가 없으면 기본값(신전상태) 반환
	EGameState GetCurrentGameState() const;

	//CommandDispatcher의 OnUserLoginRequired 구독 함수
	//TODO: DB에서 플레이어 데이터를 조회하고 없으면 새로 생성한 뒤, 필드에 플레이어 캐릭터를 스폰하는 로직 연결 예정
	void HandleUserLoginRequired(const FString& UserId);

	//API 상태 저장용 변수
	FString LiveChatId;
	FString NextPageToken;
	FTimerHandle ChatFetchTimerHandle;

	//Http 요청 응답 및 콜백 함수
	void RequestLiveChatId();
	void OnLiveChatIdResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void RequestMessages();
	void OnChatMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString PreprocessCommand(const FString& Input);
	bool IsSpamming(const FString& UserId, const FString& Content, const FDateTime& CurrentTime);
	void HandleSpamPenalty(FUserInfo& User, const FDateTime& Now);
	void ExecuteGameCommand(const FString& UserId, const FString& Command);
	void SendToUserRemainingTime(const FString& UserId, FTimespan Remaining);

	//명령어(ECommandType) -> 처리 함수를 연결해주는 디스패처
	UPROPERTY()
	TObjectPtr<UCommandDispatcher> CommandDispatcher;

	//채팅을 그날그날 txt 파일로 저장하는 함수
	void SaveCommentLogToFile(const FString& UserId, const FString& Content, const FDateTime& Now);
};
