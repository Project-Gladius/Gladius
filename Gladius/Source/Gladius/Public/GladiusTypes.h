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

// 현재 게임이 전투상태인지 신전상태인지 나타내는 값 (AGameManager가 들고 있는 값)
// 이 값에 따라 로그인 가능 여부와 어떤 ECommandType이 유효한지가 결정됨
UENUM(BlueprintType)
enum class EGameState : uint8
{
	Temple, // 신전상태 - 강화(!강화), 대화(!대화) 명령어만 유효
	Combat, // 전투상태 - 공격/방어/회피/로그아웃 명령어만 유효, 로그인도 이 상태에서만 트리거됨
};

// 채팅 명령어(!명령어 내용)로 실행 가능한 게임 명령의 종류
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
	TravelToCombat, // !전투: 신전에서 전투 이동 요청

	// ===== 전투상태 (참가) =====
	Notice, Block, Unblock, Maintenance, EndMaintenance, EventBuff,
	Login, // !로그인 글라디우스 / !로그인 세라. Argument: 캐릭터 이름
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

	// 명령이 담긴 채팅의 고유 Id (유튜브 message.id). AI 쿼리의 uid로 사용됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString CommentId;

	// 화면에 표시할 작성자명 (유튜브 authorDetails.displayName). AI 쿼리의 username으로 사용됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString Username;

	// !대화 처럼 뒤에 붙는 문자열 인자가 필요한 명령어에만 사용됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString Argument;
};
