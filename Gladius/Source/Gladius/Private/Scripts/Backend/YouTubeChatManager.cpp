// Fill out your copyright notice in the Description page of Project Settings.


#include "Scripts/Backend/YouTubeChatManager.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Backend/ExternalData.h"
#include "HAL/FileManager.h"
#include "Scripts/UI/ChatLogSubsystem.h"
#include "Scripts/Battle/BattleManager.h"
#include "GameFramework/Actor.h" 
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
//이걸 왜 헤더가 아니라 여기다가 추가한거지? -> .h를 참조하는 모든 cpp에 헤더가 박히면 느려짐

// Sets default values
AYouTubeChatManager::AYouTubeChatManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AYouTubeChatManager::BeginPlay()
{
	Super::BeginPlay();
 BindLocalPatchKey();

	if (!GameManagerRef)
	{
		GameManagerRef = Cast<AGameManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGameManager::StaticClass()));
	}

 if (const auto Settings = GladiusExternalData::Broadcast())
 {
  const TSharedPtr<FJsonObject>* YouTube = nullptr;
  if (Settings->TryGetObjectField(TEXT("youtube"), YouTube) && YouTube && YouTube->IsValid())
   (*YouTube)->TryGetStringField(TEXT("videoId"), VideoId);
 }
	GetCommandDispatcher();
	InitializeKeyPool();
 RefreshBroadcastSettings();
	if (UGameInstance* Instance = GetGameInstance())
	{
		Instance->GetSubsystem<UChatLogSubsystem>()->SetYouTubeSource(CurrentApiKey(), VideoId, AdminUserIds);
	}

	IgnoreMessagesBefore = FDateTime::UtcNow();

 bBroadcastFetchingStarted = true;
	StartFetchingChat();
}

UCommandDispatcher* AYouTubeChatManager::GetCommandDispatcher()
{
	if (!CommandDispatcher)
	{
		CommandDispatcher = NewObject<UCommandDispatcher>(this);
		CommandDispatcher->Initialize();

		//전투/신전 상태 조회 및 자동 로그인 처리를 CommandDispatcher에 연결
		CommandDispatcher->GetGameStateDelegate.BindUObject(this, &AYouTubeChatManager::GetCurrentGameState);
	}
 if (ABattleManager* Battle = Cast<ABattleManager>(UGameplayStatics::GetActorOfClass(this, ABattleManager::StaticClass())))
  CommandDispatcher->BindBattleManager(Battle);
 return CommandDispatcher;
}

// Called every frame
void AYouTubeChatManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
 // PlayerController can appear after this actor, or be replaced during play.
 BindLocalPatchKey();
}

void AYouTubeChatManager::InitializeKeyPool()
{
	UGameInstance* Instance = GetGameInstance();
	UChatLogSubsystem* ChatLog = Instance ? Instance->GetSubsystem<UChatLogSubsystem>() : nullptr;
	KeyPool = ChatLog ? &ChatLog->GetApiKeyPool() : &LocalKeyPool;

	//게임 인스턴스당 한 번만 파일을 읽음 (레벨이 바뀌어도 바닥난 키 기록 유지)
	if (KeyPool->Num() == 0)
	{
		const FString Path = FYouTubeApiKeyPool::GetDefaultKeyFilePath();
		const int32 Loaded = KeyPool->LoadFromFile(Path);
		UE_LOG(LogTemp, Log, TEXT("[YouTube] API 키 %d개를 읽었습니다 (%s)"), Loaded, *Path);
	}

}

FString AYouTubeChatManager::CurrentApiKey()
{
	if (!KeyPool) InitializeKeyPool();
	return KeyPool->GetCurrentKey(FDateTime::UtcNow());
}

bool AYouTubeChatManager::IsKeyUnusableResponse(int32 ResponseCode, const FString& Body)
{
	if (ResponseCode != 400 && ResponseCode != 403) return false;
	//할당량 초과 / 잘못된·만료된 키 / 프로젝트에서 YouTube Data API를 켜지 않음
	static const TCHAR* Reasons[] = {
		TEXT("quotaExceeded"), TEXT("dailyLimitExceeded"), TEXT("keyInvalid"), TEXT("keyExpired"),
		TEXT("API_KEY_INVALID"), TEXT("accessNotConfigured"), TEXT("SERVICE_DISABLED") };
	for (const TCHAR* Reason : Reasons)
	{
		if (Body.Contains(Reason, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

bool AYouTubeChatManager::SwitchApiKey(const FString& UsedKey, const FString& Body)
{
	const FDateTime Now = FDateTime::UtcNow();
	const bool bHasNext = KeyPool && KeyPool->MarkExhausted(UsedKey, Now);
	const TCHAR* Why = Body.Contains(TEXT("quotaExceeded")) || Body.Contains(TEXT("dailyLimitExceeded")) ? TEXT("할당량 초과") : TEXT("사용할 수 없는 키");
	if (bHasNext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YouTube] API 키 %s %s → %s로 교체 (남은 키 %d/%d)"),
			*FYouTubeApiKeyPool::Mask(UsedKey), Why, *FYouTubeApiKeyPool::Mask(CurrentApiKey()),
			KeyPool->NumAvailable(Now), KeyPool->Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[YouTube] API 키 %s %s. 쓸 수 있는 키가 없습니다 (총 %d개). 할당량은 한국 시간 오후 4~5시에 초기화됩니다."),
			*FYouTubeApiKeyPool::Mask(UsedKey), Why, KeyPool ? KeyPool->Num() : 0);
	}
	return bHasNext;
}

void AYouTubeChatManager::StartFetchingChat()
{
 if (UGameInstance* Instance = GetGameInstance()) Instance->GetSubsystem<UAdminSubsystem>()->ResolveAdminHandles(CurrentApiKey());
	if (CurrentApiKey().IsEmpty() || VideoId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ApiKey 또는 VideoId가 설정되지 않았습니다. (키가 모두 할당량을 다 썼을 수도 있습니다)"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("유튜브 채팅 수신을 시작합니다."));
	RequestLiveChatId();
}

void AYouTubeChatManager::StopFetchingChat() {
	GetWorldTimerManager().ClearTimer(ChatFetchTimerHandle);
	UE_LOG(LogTemp, Log, TEXT("유튜브 채팅 수신 중지"));
}

void AYouTubeChatManager::SetCombatAutoPollingEnabled(bool bEnabled)
{
 // Re-entering a tutorial wait must preserve an already scheduled API interval.
 if (bEnabled && bCombatAutoPollingEnabled &&
     (bFetchInFlight || bLiveChatIdInFlight || GetWorldTimerManager().IsTimerActive(ChatFetchTimerHandle))) return;
 bCombatAutoPollingEnabled = bEnabled;
 GetWorldTimerManager().ClearTimer(ChatFetchTimerHandle);
 if (bEnabled && GetCurrentGameState() == EGameState::Combat) RequestChatFetch();
}

void AYouTubeChatManager::CancelPendingChatFetch()
{
	if (ActiveChatRequest.IsValid())
	{
		FHttpRequestPtr Cancelled = ActiveChatRequest;
		ActiveChatRequest.Reset();
		Cancelled->OnProcessRequestComplete().Unbind();
		Cancelled->CancelRequest();
	}
	bFetchInFlight = false;
}

void AYouTubeChatManager::RequestChatFetch()
{
	if (bFetchInFlight)
	{
		UE_LOG(LogTemp, Warning, TEXT("이전 채팅 조회가 아직 끝나지 않아 이번 조회를 건너뜁니다."));
		return;
	}

	GetWorldTimerManager().ClearTimer(ChatFetchTimerHandle);
	if (LiveChatId.IsEmpty() || CurrentApiKey().IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("LiveChatId 또는 쓸 수 있는 API 키가 없어 채팅을 조회하지 못했습니다."));
		//다음 조회 때는 쓸 수 있도록 LiveChatId를 다시 요청
		if (LiveChatId.IsEmpty() && !VideoId.IsEmpty() && !CurrentApiKey().IsEmpty()) RequestLiveChatId();
		ScheduleCombatPoll(30.f);
		OnChatFetchFinished.Broadcast(false);
		return;
	}

	RequestMessages();
}

void AYouTubeChatManager::OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content)
{
	OnReceiveYouTubeCommentWithUsername(CommentId, UserId, UserId, Content);
}

void AYouTubeChatManager::OnReceiveYouTubeCommentWithUsername(FString CommentId, FString UserId, FString Username, FString Content)
{
	ProcessComment(CommentId, UserId, Username, Content, FDateTime::UtcNow());
}

void AYouTubeChatManager::ProcessComment(const FString& CommentId, const FString& UserId, const FString& Username, const FString& Content, const FDateTime& PostedAt, bool bYouTubeAdmin)
{
	//여러 채팅을 한 번에 처리해도 도배 판정이 실제 채팅 시각 기준으로 되도록 처리 시각 대신 채팅이 올라온 시각을 사용
	const FDateTime Now = PostedAt;

	//공백 제거 등 명령어 전처리
	FString CleanContent = PreprocessCommand(Content);

	//받은 채팅을 그날그날 txt 파일로 저장 (도배/블랙 여부와 무관하게 원본 로그로 남김)
	SaveCommentLogToFile(UserId, CleanContent, Now);

 UAdminSubsystem* Admin = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAdminSubsystem>() : nullptr;
 const bool bPrivileged = Admin && Admin->IsAdmin(UserId);
 if (Admin)
 {
  if (!Admin->CanInput(UserId, GetCurrentGameState())) return;
  Admin->RegisterUser(UserId, Username);
 }
	FUserInfo* User = UserDatabase.Find(UserId); //왜 포인터로 받게 되는거? -> 데이터 없음 (null) 표현과 데이터 복사방지

	if (!User) //신규 유저 등록
	{
		FUserInfo NewUser;
		NewUser.UserId = UserId;
		NewUser.UserClass = TEXT("User");
		NewUser.RegTime = Now;

		UserDatabase.Add(UserId, NewUser);
		User = UserDatabase.Find(UserId);
	}

	if (!bPrivileged && User->UnbanTime > Now) //블랙리스트 진행중
	{
		FTimespan Remaining = User->UnbanTime - Now; //블랙리스트 해제까지 남은 시간
		UE_LOG(LogTemp, Warning, TEXT("블랙리스트 유저입니다. 남은 시간 %.1f초"), Remaining.GetTotalSeconds());

		SendToUserRemainingTime(UserId, Remaining); //유저에게 남은 시간 댓글로 송신하는 로직
		return;
	}

	if (!bPrivileged && IsSpamming(UserId, CleanContent, Now)) //도배 감지
	{
		UE_LOG(LogTemp, Warning, TEXT("도배 감지됨: 무시 처리"));
		HandleSpamPenalty(*User, Now);
		return;
	}

	FCommentLog Log{ CommentId, UserId, Now, CleanContent };
	CommentLogs.Add(Log);

	// 명령은 기존 디스패처에 그대로 전달하고, 로그에는 일반 채팅만 표시한다.
	if (!CleanContent.IsEmpty() && !CleanContent.StartsWith(TEXT("!")))
	{
		if (UGameInstance* Instance = GetGameInstance())
		{
			const bool bAdmin = bYouTubeAdmin || AdminUserIds.Contains(UserId)
				|| User->UserClass.Equals(TEXT("Admin"), ESearchCase::IgnoreCase);
			Instance->GetSubsystem<UChatLogSubsystem>()->AddViewerMessage(
				CommentId, UserId, Username, CleanContent, bAdmin, Now);
		}
	}

	ExecuteGameCommand(CommentId, UserId, Username, CleanContent);
}

FString AYouTubeChatManager::PreprocessCommand(const FString& Input) 
{
	return Input.TrimStartAndEnd(); //시작과 끝 공백 제거처리
}

bool AYouTubeChatManager::IsSpamming(const FString& UserId, const FString& Content, const FDateTime& CurrentTime)
{
	int32 RecentCount = 0;

	for (int32 i = CommentLogs.Num() - 1; i >= 0; i--) {
		const FCommentLog& Log = CommentLogs[i];

		if ((CurrentTime - Log.CreatedAt).GetTotalSeconds() > SpamTimeLimitSeconds)
		{
			break;
		}

		if (Log.UserId == UserId) {
			if (Log.Content == Content) //같은 유저가 같은 문자를 SpamTimeLimitSeconds 안에 보낸 경우 즉시 도배로 간주
			{
				return true;
			}
			
			if (++RecentCount >= MaxAllowedComments) //SpamTimeLimitSeconds 이내에 한 유저가 MaxAllowedComments 이상의 채팅을 친 경우 도배로 간주
			{
				return true;
			}
		}
	}

	return false;
}

void AYouTubeChatManager::RequestLiveChatId()
{
	const FString Key = CurrentApiKey();
	if (bLiveChatIdInFlight || Key.IsEmpty()) return;
	bLiveChatIdInFlight = true;

	FString Url = FString::Printf(
		TEXT("https://www.googleapis.com/youtube/v3/videos?id=%s&part=liveStreamingDetails&key=%s"),
		*VideoId,
		*Key
	);

	FHttpModule* Http = &FHttpModule::Get(); //Http를 관장하는 singleton의 주소를 Http에 받아옴 (포인터 형식으로)

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest(); //IHttpRequest형식의 포인터 변수인데, 함수가 끝나도 참조가 남아있다면 메모리에서 사라지지 않음! 대신 FHttpRequestPtr로 쓸 수 있음

	Request->OnProcessRequestComplete().BindUObject(this, &AYouTubeChatManager::OnLiveChatIdResponse, Key); //요청했던 자기 자신의 포인터, 구글이 보내준 JSON 응답 본문 및 HTTP 상태 코드(200 OK 등)가 담긴 객체, 연결 성공 여부를 매개변수로 넘김

	ActiveLiveChatRequest = Request;
	Request->SetURL(Url);
	Request->SetVerb("GET");
 Request->SetTimeout(15.f);
 if (!Request->ProcessRequest())
 {
  ActiveLiveChatRequest.Reset(); bLiveChatIdInFlight = false;
  ScheduleCombatPoll(30.f);
 }
}

void AYouTubeChatManager::OnLiveChatIdResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString UsedKey)
{
	if (Request != ActiveLiveChatRequest) return;
 ActiveLiveChatRequest.Reset();
	bLiveChatIdInFlight = false;
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LiveChatID 요청 실패 (네트워크 연결을 확인하세요)"));
  ScheduleCombatPoll(30.f);
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	if (ResponseCode != 200)
	{
		const FString Body = Response->GetContentAsString();
		if (IsKeyUnusableResponse(ResponseCode, Body))
		{
			if (SwitchApiKey(UsedKey, Body)) RequestLiveChatId();
   else ScheduleCombatPoll(30.f);
			return;
		}
		UE_LOG(LogTemp, Error, TEXT("LiveChatID 획득 실패 HTTP %d"), ResponseCode);
  ScheduleCombatPoll(30.f);
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) 
	{
  const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
  const TSharedPtr<FJsonObject>* Details = nullptr;
  if (JsonObject->TryGetArrayField(TEXT("items"), Items) && !Items->IsEmpty() && (*Items)[0].IsValid())
  {
   const auto Item = (*Items)[0]->AsObject();
   if (Item && Item->TryGetObjectField(TEXT("liveStreamingDetails"), Details) && Details && *Details
       && (*Details)->TryGetStringField(TEXT("activeLiveChatId"), LiveChatId) && !LiveChatId.IsEmpty())
   {
    GetWorldTimerManager().ClearTimer(ChatFetchTimerHandle);
    if (bCombatAutoPollingEnabled && GetCurrentGameState() == EGameState::Combat) RequestMessages();
    return;
   }
  }
 }
	UE_LOG(LogTemp, Error, TEXT("LiveChatID 획득 실패 (현재 라이브 스트리밍 중인지 확인하세요)"));
 ScheduleCombatPoll(30.f);
}

void AYouTubeChatManager::RequestMessages()
{
	const FString Key = CurrentApiKey();
	if (bFetchInFlight || LiveChatId.IsEmpty() || Key.IsEmpty()) return;
	FString Url = FString::Printf(
		TEXT("https://www.googleapis.com/youtube/v3/liveChat/messages?liveChatId=%s&part=snippet,authorDetails&key=%s"),
		*LiveChatId,
		*Key
	);

	if (!NextPageToken.IsEmpty()) {
		Url += FString::Printf(TEXT("&pageToken=%s"), *NextPageToken);
	}

	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

	Request->OnProcessRequestComplete().BindUObject(this, &AYouTubeChatManager::OnChatMessagesResponse, Key);

	Request->SetURL(Url);
	Request->SetVerb("GET");

	bFetchInFlight = true;
	ActiveChatRequest = Request;
	Request->SetTimeout(15.f);
	if (!Request->ProcessRequest()) { FinishFetch(false, CombatPollIntervalSeconds); }
}

void AYouTubeChatManager::OnChatMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString UsedKey)
{
	if (Request != ActiveChatRequest) { return; }
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("채팅 수신 실패! (네트워크 연결을 확인하세요)"));
		FinishFetch(false, CombatPollIntervalSeconds);
		return;
	}

	//HTTP 오류(403 등)도 bWasSuccessful은 true로 오므로 응답 코드로 판단
	const int32 ResponseCode = Response->GetResponseCode();
	if (ResponseCode != 200)
	{
		const FString Body = Response->GetContentAsString();
		if (IsKeyUnusableResponse(ResponseCode, Body) && SwitchApiKey(UsedKey, Body))
		{
			//같은 조회를 새 키로 바로 다시 보냄 (신전 사이클은 이 조회가 끝날 때까지 기다림)
			ActiveChatRequest.Reset();
			bFetchInFlight = false;
			RequestMessages();
			return;
		}
		UE_LOG(LogTemp, Error, TEXT("채팅 수신 실패! HTTP %d: %s"), ResponseCode, *Body.Left(300));
		FinishFetch(false, CombatPollIntervalSeconds);
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("채팅 응답 JSON을 읽을 수 없습니다."));
		FinishFetch(false, CombatPollIntervalSeconds);
		return;
	}

	if (JsonObject->HasField(TEXT("nextPageToken")))
	{
		NextPageToken = JsonObject->GetStringField(TEXT("nextPageToken"));
	}

	double PollingIntervalMillis = 0.0;
	const float NextPollDelay = JsonObject->TryGetNumberField(TEXT("pollingIntervalMillis"), PollingIntervalMillis)
		? FMath::Max(1.0f, static_cast<float>(PollingIntervalMillis / 1000.0))
		: CombatPollIntervalSeconds;

	TArray<TSharedPtr<FJsonValue>> Items = JsonObject->GetArrayField(TEXT("items"));

	for (const TSharedPtr<FJsonValue>& Item : Items) {
		TSharedPtr<FJsonObject> MessageObj = Item->AsObject();
		if (!MessageObj.IsValid()) continue;

		FString CommentId = MessageObj->GetStringField(TEXT("id")); //채팅 고유 id가져오기

		TSharedPtr<FJsonObject> SnippetObj = MessageObj->GetObjectField(TEXT("snippet"));
		FString Content = SnippetObj->GetStringField(TEXT("displayMessage")); //채팅 가져오기

		//채팅이 올라온 시각. 파싱에 실패하면 지금 시각으로 대체
		FDateTime PostedAt;
		if (!FDateTime::ParseIso8601(*SnippetObj->GetStringField(TEXT("publishedAt")), PostedAt))
		{
			PostedAt = FDateTime::UtcNow();
		}
		if (PostedAt < IgnoreMessagesBefore)
		{
			continue; //씬 진입 전에 올라온 채팅은 무시
		}

		TSharedPtr<FJsonObject> AuthorObj = MessageObj->GetObjectField(TEXT("authorDetails"));
		FString UserId = AuthorObj->GetStringField(TEXT("channelId")); //채팅을 친 id를 가져오기
		FString Username = AuthorObj->GetStringField(TEXT("displayName")); //채팅을 친 유저의 표시명 가져오기 (AI 쿼리의 username)

		bool bModerator = false;
		bool bOwner = false;
		AuthorObj->TryGetBoolField(TEXT("isChatModerator"), bModerator);
		AuthorObj->TryGetBoolField(TEXT("isChatOwner"), bOwner);
		const bool bYouTubeAdmin = bModerator || bOwner;
		ProcessComment(CommentId, UserId, Username, Content, PostedAt, bYouTubeAdmin);
  if (Request != ActiveChatRequest) { OnChatFetchFinished.Broadcast(true); return; }
	}

	//UE_LOG(LogTemp, Log, TEXT(">>> [%.1f초] 뒤에 다음 채팅을 요청합니다. (NextPageToken: %s)"), NextPollDelay, *NextPageToken); //디버깅용

	FinishFetch(true, NextPollDelay);
}

void AYouTubeChatManager::FinishFetch(bool bSuccess, float NextPollDelay)
{
	ActiveChatRequest.Reset();
	bFetchInFlight = false;

 // Transient failures must not permanently silence tutorial command input.
 ConsecutiveFetchFailures = bSuccess ? 0 : FMath::Min(ConsecutiveFetchFailures + 1, 4);
 const float RetryDelay = FMath::Min(60.f, FMath::Max(5.f, CombatPollIntervalSeconds) * float(1 << ConsecutiveFetchFailures));
 ScheduleCombatPoll(bSuccess ? NextPollDelay : RetryDelay);

	OnChatFetchFinished.Broadcast(bSuccess);
}

void AYouTubeChatManager::ScheduleCombatPoll(float Delay)
{
 if (bCombatAutoPollingEnabled && GetCurrentGameState() == EGameState::Combat)
  GetWorldTimerManager().SetTimer(ChatFetchTimerHandle, this, &AYouTubeChatManager::RequestChatFetch, FMath::Max(1.f, Delay), false);
}

//도배 유저의 차단 시간 가중 함수 
void AYouTubeChatManager::HandleSpamPenalty(FUserInfo& User, const FDateTime& Now)
{
	User.BlacklistCount++;

	int32 BanMinutes = User.BlacklistCount * 1; //블랙당한 횟수*분 만큼 패널티있음
	User.UnbanTime = Now + FTimespan::FromMinutes(BanMinutes);
	UE_LOG(LogTemp, Error, TEXT("%s 유저가 %d분간 블랙리스트로 등록되었습니다."), *User.UserId, BanMinutes);
}

void AYouTubeChatManager::ExecuteGameCommand(const FString& CommentId, const FString& UserId, const FString& Username, const FString& Command)
{
	FParsedCommand ParsedCommand;

	//"!"로 시작하는 명령어 형식이 아니거나 알 수 없는 명령어는 전부 무시
	if (!FChatCommandParser::TryParse(UserId, Command, ParsedCommand))
	{
		return;
	}

	//!대화 등에서 AI 쿼리(uid/username)로 쓰일 채팅 정보 채우기
	//신전상태의 !강화/!대화는 CommandDispatcher가 브로드캐스트하고 UTempleCommandQueue가 받아 큐잉함
	ParsedCommand.CommentId = CommentId;
	ParsedCommand.Username = Username;

	GetCommandDispatcher()->Execute(ParsedCommand);
}

void AYouTubeChatManager::SendToUserRemainingTime(const FString& UserId, FTimespan Remaining)
{
	//TODO: 채팅으로 얼마동안 블랙인지 쏴줄 것
	UE_LOG(LogTemp, Warning, TEXT("[%s] 유저에게 남은 차단 시간 알림 송신: %.0f초"), *UserId, Remaining.GetTotalSeconds());
}

EGameState AYouTubeChatManager::GetCurrentGameState() const
{
	//GameManagerRef를 찾지 못한 경우(월드에 배치 안 됨 등) 기본값으로 신전상태 취급
	if (ABattleManager* Battle = Cast<ABattleManager>(UGameplayStatics::GetActorOfClass(this, ABattleManager::StaticClass())))
	{
		return Battle->IsBattleRunning() ? EGameState::Combat : EGameState::Temple;
	}
	return IsValid(GameManagerRef) ? GameManagerRef->GetGameState() : EGameState::Temple;
}

//받은 채팅을 프로젝트의 Saved/ChatLogs 폴더 아래 그날 날짜의 txt 파일로 이어 붙여 저장
void AYouTubeChatManager::SaveCommentLogToFile(const FString& UserId, const FString& Content, const FDateTime& Now)
{
	//로그 폴더 경로: <프로젝트>/Saved/ChatLogs
	const FString LogDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChatLogs"));

	//폴더가 없으면 생성 (두 번째 인자 true: 상위 폴더까지 재귀적으로 생성)
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LogDir))
	{
		PlatformFile.CreateDirectoryTree(*LogDir);
	}

	//파일 이름: 그날 날짜(로컬 시간 기준) 예) 2026-08-02.txt
	const FDateTime LocalNow = FDateTime::Now();
	const FString FileName = LocalNow.ToString(TEXT("%Y-%m-%d")) + TEXT(".txt");
	const FString FilePath = FPaths::Combine(LogDir, FileName);

	//한 줄 형식: [HH:MM:SS] UserId: Content
	const FString LogLine = FString::Printf(
		TEXT("[%s] %s: %s") LINE_TERMINATOR,
		*LocalNow.ToString(TEXT("%H:%M:%S")),
		*UserId,
		*Content
	);

	//기존 파일에 이어 붙이기 (없으면 새로 생성), UTF-8로 저장
	FFileHelper::SaveStringToFile(
		LogLine,
		*FilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		FILEWRITE_Append
	);
}


void AYouTubeChatManager::RefreshBroadcastSettings()
{
 if (!KeyPool) InitializeKeyPool();
 const FString Path = BroadcastSettingsOverridePath.IsEmpty() ? FYouTubeApiKeyPool::GetDefaultKeyFilePath() : BroadcastSettingsOverridePath;
 FString Json; TSharedPtr<FJsonObject> Root;
 const TSharedPtr<FJsonObject>* YouTube = nullptr;
 const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
 TArray<FString> Keys; FString StreamKey;
 bool bValid = FFileHelper::LoadFileToString(Json, *Path)
  && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root
  && Root->TryGetObjectField(TEXT("youtube"), YouTube) && YouTube && YouTube->IsValid();
 if (bValid && (*YouTube)->HasField(TEXT("apiKeys")))
 {
  bValid = (*YouTube)->TryGetArrayField(TEXT("apiKeys"), Entries);
  if (bValid) for (const auto& Entry : *Entries)
  {
   FString Key;
   if (!Entry || Entry->Type != EJson::String || !Entry->TryGetString(Key)) { bValid = false; break; }
   Key.TrimStartAndEndInline();
   if (!Key.IsEmpty()) Keys.AddUnique(Key);
  }
 }
 if (bValid && (*YouTube)->HasField(TEXT("streamKey")))
  bValid = (*YouTube)->HasTypedField<EJson::String>(TEXT("streamKey")) && (*YouTube)->TryGetStringField(TEXT("streamKey"), StreamKey);
 StreamKey.TrimStartAndEndInline();
 const bool bNeedsRefresh = !bValid || Keys.IsEmpty() || StreamKey.IsEmpty();
 auto& Timers = GetWorldTimerManager();
 if (bNeedsRefresh)
 {
  if (!Timers.IsTimerActive(BroadcastRefreshTimerHandle))
   Timers.SetTimer(BroadcastRefreshTimerHandle, this, &AYouTubeChatManager::RefreshBroadcastSettings, 30.f, true);
 }
 else Timers.ClearTimer(BroadcastRefreshTimerHandle);
 if (!bValid) return; // Keep the last valid values while the file is being edited.
 FString NextVideo = VideoId;
 if ((*YouTube)->HasField(TEXT("videoId")) && !(*YouTube)->TryGetStringField(TEXT("videoId"), NextVideo)) return;
 NextVideo.TrimStartAndEndInline();
 const bool bVideoChanged = NextVideo != VideoId;
 if (bVideoChanged)
 {
  StopFetchingChat(); CancelPendingChatFetch();
  if (ActiveLiveChatRequest.IsValid())
  {
   ActiveLiveChatRequest->OnProcessRequestComplete().Unbind(); ActiveLiveChatRequest->CancelRequest();
   ActiveLiveChatRequest.Reset();
  }
  bLiveChatIdInFlight = false;
  LiveChatId.Reset(); NextPageToken.Reset(); VideoId = NextVideo;
  IgnoreMessagesBefore = FDateTime::UtcNow();
 }
 const bool bKeysChanged = KeyPool->ReplaceKeys(Keys);
 BroadcastStreamKey = MoveTemp(StreamKey);
 if (Keys.IsEmpty()) { StopFetchingChat(); return; }
 if (UGameInstance* Instance = GetGameInstance())
  Instance->GetSubsystem<UChatLogSubsystem>()->SetYouTubeSource(CurrentApiKey(), VideoId, AdminUserIds);
 if (!bBroadcastFetchingStarted || (!bKeysChanged && !bVideoChanged)) return;
 if (LiveChatId.IsEmpty())
 {
  if (ActiveLiveChatRequest.IsValid())
  {
   ActiveLiveChatRequest->OnProcessRequestComplete().Unbind();
   ActiveLiveChatRequest->CancelRequest();
   ActiveLiveChatRequest.Reset(); bLiveChatIdInFlight = false;
  }
  StartFetchingChat();
 }
 else if (bCombatAutoPollingEnabled && GetCurrentGameState() == EGameState::Combat && !bFetchInFlight)
  RequestChatFetch();
}

void AYouTubeChatManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 DisableInput(LocalPatchController.Get());
 LocalPatchController.Reset(); LocalPatchInput.Reset();
 bBroadcastFetchingStarted = false;
 GetWorldTimerManager().ClearTimer(BroadcastRefreshTimerHandle);
 GetWorldTimerManager().ClearTimer(ChatFetchTimerHandle);
 if (ActiveLiveChatRequest.IsValid())
 {
  ActiveLiveChatRequest->OnProcessRequestComplete().Unbind();
  ActiveLiveChatRequest->CancelRequest(); ActiveLiveChatRequest.Reset();
 }
 CancelPendingChatFetch();
 Super::EndPlay(EndPlayReason);
}

bool AYouTubeChatManager::ReloadBroadcastSettings()
{
 const FString Path = BroadcastSettingsOverridePath.IsEmpty() ? FYouTubeApiKeyPool::GetDefaultKeyFilePath() : BroadcastSettingsOverridePath;
 FString Json; TSharedPtr<FJsonObject> Root;
 const TSharedPtr<FJsonObject>* YouTube = nullptr;
 if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
  || !Root || !Root->TryGetObjectField(TEXT("youtube"), YouTube) || !YouTube || !YouTube->IsValid()) return false;
 for (const TCHAR* Field : {TEXT("videoId"), TEXT("streamKey")})
  if (!(*YouTube)->HasTypedField<EJson::String>(Field)) return false;
 const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
 if (!(*YouTube)->TryGetArrayField(TEXT("apiKeys"), Keys)) return false;
 for (const auto& Key : *Keys) if (!Key || Key->Type != EJson::String) return false;
 RefreshBroadcastSettings();
 if (GetGameInstance()) GetGameInstance()->GetSubsystem<UAdminSubsystem>()->ResolveAdminHandles(CurrentApiKey());
 return true;
}

void AYouTubeChatManager::BindLocalPatchKey()
{
 auto* Controller = UGameplayStatics::GetPlayerController(this, 0);
 if (!Controller || !Controller->IsLocalController()) return;
 if (LocalPatchController.Get() == Controller && LocalPatchInput.IsValid() && LocalPatchInput.Get() == InputComponent.Get()) return;
 if (LocalPatchController.IsValid()) DisableInput(LocalPatchController.Get());
 EnableInput(Controller);
 if (!InputComponent) return;
 // Reusing an input component must not register P a second time.
 for (int32 Index=InputComponent->KeyBindings.Num()-1; Index>=0; --Index)
  if (InputComponent->KeyBindings[Index].Chord.Key == EKeys::P) InputComponent->KeyBindings.RemoveAt(Index);
 FInputKeyBinding Binding(FInputChord(EKeys::P), IE_Pressed);
 Binding.bConsumeInput = false;
 Binding.bExecuteWhenPaused = true;
 Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AYouTubeChatManager::HandleLocalPatchKey);
 InputComponent->KeyBindings.Emplace(MoveTemp(Binding));
 LocalPatchController = Controller;
 LocalPatchInput = InputComponent;
}

void AYouTubeChatManager::HandleLocalPatchKey()
{
 // Local keyboard access is an operator action; no chat-user impersonation or permission edit is needed.
 if (!GetGameInstance()) return;
 if (auto* Admin = GetGameInstance()->GetSubsystem<UAdminSubsystem>())
 {
  auto* Battle = Cast<ABattleManager>(UGameplayStatics::GetActorOfClass(this, ABattleManager::StaticClass()));
  Admin->ExecutePatch(Battle);
 }
}
