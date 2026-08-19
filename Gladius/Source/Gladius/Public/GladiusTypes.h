#pragma once

#include "CoreMinimal.h"
#include "GladiusTypes.generated.h"

// 게임 전역에서 사용하는 유저 정보 (시청자/플레이어 stat 등은 추후 확장 예정)
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
