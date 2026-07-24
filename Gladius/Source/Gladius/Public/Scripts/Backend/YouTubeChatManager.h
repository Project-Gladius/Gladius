#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YouTubeChatManager.generated.h"

USTRUCT(BlueprintType)
struct FUserInfo 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User");
	FString UserId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User");
	FString UserClass = TEXT("User");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User");
	FDateTime RegTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User");
	FDateTime UnbanTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User");
	int32 BlacklistCount = 0;
};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
	TMap<FString, FUserInfo> UserDatabase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database")
	TArray<FCommentLog> CommentLogs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float SpamTimeLimitSeconds = 0.5f; //몇초안에 또 댓글을 입력하면 스팸인지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 MaxAllowedComments = 3;

	UFUNCTION(BlueprintCallable, Category = "YouTube")
	void OnReceiveYouTubeComment(FString CommentId, FString UserId, FString Content);

private:
	FString PreprocessCommand(const FString& Input);
	bool IsSpamming(const FString& UserId, const FString& Content, const FDateTime& CurrentTime);
	void HandleSpamPenalty(FUserInfo& User, const FDateTime& Now);
	void ExecuteGameCommand(const FString& UserId, const FString& Command);
	void SendToUserRemainingTime(const FString& UserId, FTimespan Remaining);
};
