// Fill out your copyright notice in the Description page of Project Settings.


#include "Scripts/Backend/YouTubeChatManager.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/GameManager.h"
#include "GameFramework/Actor.h" 
#include "Kismet/GameplayStatics.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
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

	if (!GameManagerRef)
	{
		GameManagerRef = Cast<AGameManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGameManager::StaticClass()));
	}

	CommandDispatcher = NewObject<UCommandDispatcher>(this);
	CommandDispatcher->Initialize();

	//전투/신전 상태 조회 및 자동 로그인 처리를 CommandDispatcher에 연결
	CommandDispatcher->GetGameStateDelegate.BindUObject(this, &AYouTubeChatManager::GetCurrentGameState);
	CommandDispatcher->OnUserLoginRequired.AddUObject(this, &AYouTubeChatManager::HandleUserLoginRequired);

	StartFetchingChat();
}

// Called every frame
void AYouTubeChatManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AYouTubeChatManager::StartFetchingChat()
{
	if (ApiKey.IsEmpty() || VideoId.IsEmpty()) 
	{
		UE_LOG(LogTemp, Error, TEXT("ApiKey 또는 VideoId가 설정되지 않았습니다."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("유튜브 채팅 수신을 시작합니다."));
	RequestLiveChatId();
}

void AYouTubeChatManager::StopFetchingChat() {
	GetWorldTimerManager().ClearTimer(ChatFetchTimerHandle);
	UE_LOG(LogTemp, Log, TEXT("유튜브 채팅 수신 중지"));
}


void AYouTubeChatManager::OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content)
{
	FDateTime Now = FDateTime::UtcNow();

	//공백 제거 등 명령어 전처리
	FString CleanContent = PreprocessCommand(Content);

	//받은 채팅을 그날그날 txt 파일로 저장 (도배/블랙 여부와 무관하게 원본 로그로 남김)
	SaveCommentLogToFile(UserId, CleanContent, Now);

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

	if (User->UnbanTime > Now) //블랙리스트 진행중
	{
		FTimespan Remaining = User->UnbanTime - Now; //블랙리스트 해제까지 남은 시간
		UE_LOG(LogTemp, Warning, TEXT("블랙리스트 유저입니다. 남은 시간 %.1f초"), Remaining.GetTotalSeconds());

		SendToUserRemainingTime(UserId, Remaining); //유저에게 남은 시간 댓글로 송신하는 로직
		return;
	}

	if (IsSpamming(UserId, CleanContent, Now)) //도배 감지
	{
		UE_LOG(LogTemp, Warning, TEXT("도배 감지됨: 무시 처리"));
		HandleSpamPenalty(*User, Now);
		return;
	}

	FCommentLog Log{ CommentId, UserId, Now, CleanContent };
	CommentLogs.Add(Log);

	ExecuteGameCommand(UserId, CleanContent);
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
	FString Url = FString::Printf(
		TEXT("https://www.googleapis.com/youtube/v3/videos?id=%s&part=liveStreamingDetails&key=%s"),
		*VideoId,
		*ApiKey
	);

	FHttpModule* Http = &FHttpModule::Get(); //Http를 관장하는 singleton의 주소를 Http에 받아옴 (포인터 형식으로)

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest(); //IHttpRequest형식의 포인터 변수인데, 함수가 끝나도 참조가 남아있다면 메모리에서 사라지지 않음! 대신 FHttpRequestPtr로 쓸 수 있음

	Request->OnProcessRequestComplete().BindUObject(this, &AYouTubeChatManager::OnLiveChatIdResponse); //요청했던 자기 자신의 포인터, 구글이 보내준 JSON 응답 본문 및 HTTP 상태 코드(200 OK 등)가 담긴 객체, 연결 성공 여부를 매개변수로 넘김

	Request->SetURL(Url);
	Request->SetVerb("GET");
	Request->ProcessRequest();
}

void AYouTubeChatManager::OnLiveChatIdResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid()) return;

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) 
	{
		TArray<TSharedPtr<FJsonValue>> Items = JsonObject->GetArrayField(TEXT("items")); 
		if (Items.Num() > 0)
		{
			TSharedPtr<FJsonObject> Details = Items[0]->AsObject()->GetObjectField(TEXT("liveStreamingDetails"));  

			if (Details.IsValid() && Details->HasField(TEXT("activeLiveChatId"))) 
			{
				LiveChatId = Details->GetStringField(TEXT("activeLiveChatId"));
				UE_LOG(LogTemp, Log, TEXT("LiveChatID 획득 성공 %s"), *LiveChatId); //LivechatID 가져오기

				RequestMessages();
				return;
			}
		}
	}
	UE_LOG(LogTemp, Error, TEXT("LiveChatID 획득 실패 (현재 라이브 스트리밍 중인지 확인하세요)"));
}

void AYouTubeChatManager::RequestMessages()
{
	if (LiveChatId.IsEmpty() || ApiKey.IsEmpty()) return;
	FString Url = FString::Printf(
		TEXT("https://www.googleapis.com/youtube/v3/liveChat/messages?liveChatId=%s&part=snippet,authorDetails&key=%s"),
		*LiveChatId,
		*ApiKey
	);

	if (!NextPageToken.IsEmpty()) {
		Url += FString::Printf(TEXT("&pageToken=%s"), *NextPageToken);
	}

	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

	Request->OnProcessRequestComplete().BindUObject(this, &AYouTubeChatManager::OnChatMessagesResponse);

	Request->SetURL(Url);
	Request->SetVerb("GET");
	Request->ProcessRequest();
}

void AYouTubeChatManager::OnChatMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid()) 
	{
		int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG(LogTemp, Error, TEXT("채팅 수신 실패! HTTP Response Code: %d"), ResponseCode);

		if (ResponseCode == 403)
		{
			UE_LOG(LogTemp, Error, TEXT("API 일일 할당량을 모두 소모했거나 권한이 없습니다!"));
		}

		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) {
		if (JsonObject->HasField(TEXT("nextPageToken")))
		{
			NextPageToken = JsonObject->GetStringField(TEXT("nextPageToken"));
		}

		//유튜브가 알려주는 pollingIntervalMillis는 무시하고, 신전(40초)/전투(5초) 상태별 고정 주기만 사용
		const float NextPollDelay = GetPollIntervalForCurrentState();

		TArray<TSharedPtr<FJsonValue>> Items = JsonObject->GetArrayField(TEXT("items"));

		for (const TSharedPtr<FJsonValue>& Item : Items) { 
			TSharedPtr<FJsonObject> MessageObj = Item->AsObject();
			if (!MessageObj.IsValid()) continue;

			FString CommentId = MessageObj->GetStringField(TEXT("id")); //채팅 고유 id가져오기

			TSharedPtr<FJsonObject> SnippetObj = MessageObj->GetObjectField(TEXT("snippet"));
			FString Content = SnippetObj->GetStringField(TEXT("displayMessage")); //채팅 가져오기

			TSharedPtr<FJsonObject> AuthorObj = MessageObj->GetObjectField(TEXT("authorDetails"));
			FString UserId = AuthorObj->GetStringField(TEXT("channelId")); //채팅을 친 id를 가져오기

			OnReceiveYouTubeComment(CommentId, UserId, Content);
		}

		//UE_LOG(LogTemp, Log, TEXT(">>> [%.1f초] 뒤에 다음 채팅을 요청합니다. (NextPageToken: %s)"), NextPollDelay, *NextPageToken); //디버깅용

		GetWorldTimerManager().SetTimer(
			ChatFetchTimerHandle,
			this,
			&AYouTubeChatManager::RequestMessages,
			NextPollDelay,
			false
		);
	}
}

//도배 유저의 차단 시간 가중 함수 
void AYouTubeChatManager::HandleSpamPenalty(FUserInfo& User, const FDateTime& Now)
{
	User.BlacklistCount++;

	int32 BanMinutes = User.BlacklistCount * 1; //블랙당한 횟수*분 만큼 패널티있음
	User.UnbanTime = Now + FTimespan::FromMinutes(BanMinutes);
	UE_LOG(LogTemp, Error, TEXT("%s 유저가 %d분간 블랙리스트로 등록되었습니다."), *User.UserId, BanMinutes);
}

void AYouTubeChatManager::ExecuteGameCommand(const FString& UserId, const FString& Command)
{
	FParsedCommand ParsedCommand;

	//"!"로 시작하는 명령어 형식이 아니거나 알 수 없는 명령어는 전부 무시
	if (!FChatCommandParser::TryParse(UserId, Command, ParsedCommand))
	{
		return;
	}

	if (CommandDispatcher)
	{
		CommandDispatcher->Execute(ParsedCommand);
	}
}

void AYouTubeChatManager::SendToUserRemainingTime(const FString& UserId, FTimespan Remaining)
{
	//TODO: 채팅으로 얼마동안 블랙인지 쏴줄 것
	UE_LOG(LogTemp, Warning, TEXT("[%s] 유저에게 남은 차단 시간 알림 송신: %.0f초"), *UserId, Remaining.GetTotalSeconds());
}

EGameState AYouTubeChatManager::GetCurrentGameState() const
{
	//GameManagerRef를 찾지 못한 경우(월드에 배치 안 됨 등) 기본값으로 신전상태 취급
	return GameManagerRef ? GameManagerRef->GetGameState() : EGameState::Temple;
}

float AYouTubeChatManager::GetPollIntervalForCurrentState() const
{
	return GetCurrentGameState() == EGameState::Combat ? CombatPollIntervalSeconds : TemplePollIntervalSeconds;
}

void AYouTubeChatManager::HandleUserLoginRequired(const FString& UserId)
{
	//TODO: DB에서 UserId에 해당하는 플레이어 데이터를 조회하고, 없으면 새로 생성한 뒤 필드에 플레이어 캐릭터를 스폰하는 로직 연결 예정
	UE_LOG(LogTemp, Log, TEXT("[로그인] 유저 %s 의 전투 참가 로그인 처리 필요"), *UserId);
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

