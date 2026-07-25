// Fill out your copyright notice in the Description page of Project Settings.


#include "Scripts/Backend/YouTubeChatManager.h"
#include "HttpModule.h" 
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
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

	FCommentLog Log{ CommentId, UserId, Now, CleanContent };
	CommentLogs.Add(Log);

	if (IsSpamming(UserId, CleanContent, Now)) //도배 감지
	{
		UE_LOG(LogTemp, Warning, TEXT("도배 감지됨: 무시 처리"));
		HandleSpamPenalty(*User, Now);
		return;
	}

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
				UE_LOG(LogTemp, Log, TEXT("LiveChatID 획득 성공 %s"), *LiveChatId);

				RequestMessages();
				return;
			}
		}
	}
	UE_LOG(LogTemp, Error, TEXT("LiveChatID 획득 실패 (현재 라이브 스트리밍 중인지 확인하세요)"));
}

void AYouTubeChatManager::RequestMessages()
{
	FString Url = FString::Printf(
		TEXT("https://www.googleapis.com/youtube/v3/liveChat/messages?liveChatId=%s&part=snippet,authorDetails&key=%s"),
		*LiveChatId,
		*ApiKey
	);
}

void AYouTubeChatManager::OnChatMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{

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
	//TODO: 나중에 로직 넣을것
	UE_LOG(LogTemp, Log, TEXT("[명령어 실행] 유저: %s | 명령: %s"), *UserId, *Command);
}

void AYouTubeChatManager::SendToUserRemainingTime(const FString& UserId, FTimespan Remaining)
{
	//TODO: 채팅으로 얼마동안 블랙인지 쏴줄 것 
	UE_LOG(LogTemp, Warning, TEXT("[%s] 유저에게 남은 차단 시간 알림 송신: %.0f초"), *UserId, Remaining.GetTotalSeconds());
}

