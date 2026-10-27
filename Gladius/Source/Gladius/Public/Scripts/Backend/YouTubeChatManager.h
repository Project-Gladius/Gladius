#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "GladiusTypes.h"
#include "Scripts/Backend/YouTubeApiKeyPool.h"
#include "YouTubeChatManager.generated.h"

class UCommandDispatcher;
class AGameManager;
class APlayerController;
class UInputComponent;

// 채팅 조회 한 번이 끝났을 때(모든 채팅을 파싱/디스패치한 뒤) 브로드캐스트. bSuccess: 조회 성공 여부
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatFetchFinished, bool /*bSuccess*/);

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

// 유튜브 라이브 채팅을 조회해 도배/블랙리스트 필터를 거친 뒤 명령어로 파싱해 CommandDispatcher로 넘기는 클래스
// 전투상태: 스스로 주기적으로 조회 / 신전상태: ATempleCycleController가 사이클마다 RequestChatFetch()를 호출
UCLASS()
class GLADIUS_API AYouTubeChatManager : public AActor
{
	GENERATED_BODY()

public:
	AYouTubeChatManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	//키 목록은 Content/ExternalData/BroadcastSettings.json의 youtube.apiKeys에서 읽음 (할당량이 바닥나면 다음 키로 교체)
	//이 값은 목록의 맨 뒤에 붙는 예비 키. 파일이 없으면 이 키 하나만 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YouTube API");
	FString ApiKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YouTube API");
	FString VideoId;

	// 관리자 채널 ID를 수동으로 지정할 때 사용. YouTube 관리자/방송 소유자 플래그도 자동 인식한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YouTube API")
	TSet<FString> AdminUserIds;

	//채팅 수신 시작 함수 (LiveChatId 조회. 전투상태면 이후 자동으로 주기 조회)
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void StartFetchingChat();
 bool ReloadBroadcastSettings();

	//채팅 수신 중지 함수
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void StopFetchingChat();

	//채팅을 한 번 조회. 끝나면 OnChatFetchFinished가 브로드캐스트됨 (신전 사이클용)
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void RequestChatFetch();

	/** BattleManager pauses recurring polls while draining the final request. */
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void SetCombatAutoPollingEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "YouTube API")
	bool IsChatFetchInFlight() const { return bFetchInFlight; }

	/** Discards an abandoned final poll so it cannot affect a later turn. */
	UFUNCTION(BlueprintCallable, Category = "YouTube API")
	void CancelPendingChatFetch();

	FOnChatFetchFinished OnChatFetchFinished;

	//명령어 디스패처. 처음 호출될 때 생성되므로 다른 액터의 BeginPlay 순서와 무관하게 구독 가능
	UCommandDispatcher* GetCommandDispatcher();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
	TMap<FString, FUserInfo> UserDatabase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
	TArray<FCommentLog> CommentLogs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float SpamTimeLimitSeconds = 0.5f; //몇초안에 또 댓글을 입력하면 스팸인지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 MaxAllowedComments = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float CombatPollIntervalSeconds = 5.0f; //전투상태일 때 채팅을 긁어오는 주기(초). 유튜브가 pollingIntervalMillis를 주면 그 값을 사용

	UFUNCTION(BlueprintCallable, Category = "YouTube")
	void OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content);

	UFUNCTION(BlueprintCallable, Category = "YouTube")
	void OnReceiveYouTubeCommentWithUsername(FString CommentId, FString UserId, FString Username, FString Content);

	//API 응답이 '이 키로는 더 못 씀'(할당량 초과, 잘못된 키, API 미사용 설정)인지
	static bool IsKeyUnusableResponse(int32 ResponseCode, const FString& Body);

private:
	//API 키 목록 (게임 인스턴스의 ChatLogSubsystem에 있음. 없으면 LocalKeyPool)
	FYouTubeApiKeyPool* KeyPool = nullptr;
	FYouTubeApiKeyPool LocalKeyPool;
 friend struct FBroadcastReloadTestAccess;
 friend struct FBattleTutorialTestAccess;
 int32 ConsecutiveFetchFailures = 0;
 void ScheduleCombatPoll(float Delay);
 TWeakObjectPtr<APlayerController> LocalPatchController;
 TWeakObjectPtr<UInputComponent> LocalPatchInput;
 void BindLocalPatchKey();
 void HandleLocalPatchKey();
 FTimerHandle BroadcastRefreshTimerHandle;
 FString BroadcastStreamKey;
 FString BroadcastSettingsOverridePath;
 bool bBroadcastFetchingStarted = false;
 void RefreshBroadcastSettings();
 FHttpRequestPtr ActiveLiveChatRequest;
	void InitializeKeyPool();
	FString CurrentApiKey();
	//UsedKey가 쓸 수 없다고 표시. 다른 키가 남아 있으면 true
	bool SwitchApiKey(const FString& UsedKey, const FString& Body);
	bool bLiveChatIdInFlight = false;

	//현재 전투/신전 상태를 들고 있는 GameManager 참조. 에디터에서 직접 지정하지 않으면 BeginPlay에서 월드를 검색해 채움
	UPROPERTY(EditInstanceOnly, Category = "Game State")
	TObjectPtr<AGameManager> GameManagerRef;

	//CommandDispatcher의 GetGameStateDelegate에 바인딩되는 함수. GameManagerRef가 없으면 기본값(신전상태) 반환
	EGameState GetCurrentGameState() const;



	//API 상태 저장용 변수
	FString LiveChatId;
	FString NextPageToken;
	FTimerHandle ChatFetchTimerHandle;
	bool bFetchInFlight = false;
	bool bCombatAutoPollingEnabled = true;
	FHttpRequestPtr ActiveChatRequest;

	//이 시각 이전에 올라온 채팅은 무시 (첫 조회 시 유튜브가 돌려주는 과거 채팅 제외)
	FDateTime IgnoreMessagesBefore;

	//Http 요청 응답 및 콜백 함수
	void RequestLiveChatId();
	void OnLiveChatIdResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString UsedKey);

	void RequestMessages();
	void OnChatMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString UsedKey);
	void FinishFetch(bool bSuccess, float NextPollDelay);

	//채팅 한 건 처리. PostedAt: 채팅이 올라온 시각 (도배 판정 기준)
	void ProcessComment(const FString& CommentId, const FString& UserId, const FString& Username, const FString& Content, const FDateTime& PostedAt, bool bYouTubeAdmin = false);

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
};
