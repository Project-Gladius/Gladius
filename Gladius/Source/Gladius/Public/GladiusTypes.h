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

// 채팅 명령어(!명령어 내용)로 실행 가능한 게임 명령의 종류
// TODO: 전투상태/신전상태 구분은 게임 매니저가 추가되면 적용 예정. 지금은 상태 구분 없이 전체 명령을 나열만 함
UENUM(BlueprintType)
enum class ECommandType : uint8
{
	None, // 명령어가 아니거나 인식할 수 없는 명령어

	// ===== 전투상태 =====
	Attack1,
	Attack2,
	Defend,
	DodgeLeft,
	DodgeRight,
	DodgeMid,
	Logout,

	// ===== 신전상태 =====
	Enhance1,
	Enhance2,
	Enhance3,
	Enhance4,
	Talk,
};

// 채팅 한 줄을 파싱한 결과
USTRUCT(BlueprintType)
struct FParsedCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	ECommandType CommandType = ECommandType::None;

	// 명령을 입력한(=캐릭터에 적용될) 유저 Id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString UserId;

	// !대화 처럼 뒤에 붙는 문자열 인자가 필요한 명령어에만 사용됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString Argument;
};
