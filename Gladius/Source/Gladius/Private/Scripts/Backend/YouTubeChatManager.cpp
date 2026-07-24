// Fill out your copyright notice in the Description page of Project Settings.


#include "Scripts/Backend/YouTubeChatManager.h"

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

void AYouTubeChatManager::OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content)
{
	FDateTime Now = FDateTime::UtcNow();

	//공백 제거 등 명령어 전처리
	FString CleanContent = PreprocessCommand(Content);

	FUserInfo* User = UserDatabase.Find(UserId); //왜 포인터로 받게 되는거? 

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

