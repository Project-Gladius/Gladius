#pragma once

#include "CoreMinimal.h"
#include "GladiusTypes.h"
#include "TempleTypes.generated.h"

// 강화 단계 수 (!강화 1 ~ !강화 4 / Blessing1 ~ Blessing4)
inline constexpr int32 GTempleEnhanceTierCount = 4;

// 신전 여신의 상태. 지금은 상태 텍스트 UI가, 나중에는 모션(AnimBP 등)이 이 값을 보고 연출함
UENUM(BlueprintType)
enum class EGoddessState : uint8
{
	Idle,
	Blessing1, // !강화 1 처리
	Blessing2, // !강화 2 처리
	Blessing3, // !강화 3 처리
	Blessing4, // !강화 4 처리
	Reading,   // !대화 질문을 읽는 중
	Speaking,  // AI 응답을 말하는 중
};

// 상태가 바뀔 때 함께 전달되는 부가 정보 (UI/모션이 필요한 것만 골라 쓰면 됨)
USTRUCT(BlueprintType)
struct FGoddessStateContext
{
	GENERATED_BODY()

	// Reading/Speaking 대상 유저의 표시명
	UPROPERTY(BlueprintReadOnly, Category = "Goddess")
	FString Username;

	// Reading: 유저의 질문, Speaking: AI 응답
	UPROPERTY(BlueprintReadOnly, Category = "Goddess")
	FString Text;

	// Speaking일 때 AI가 정한 감정 (neutral/sorrowful/bestow/pleased/upset)
	UPROPERTY(BlueprintReadOnly, Category = "Goddess")
	FString Emotion;

	// Blessing일 때 이번 단계에서 강화된 유저 표시명 목록
	UPROPERTY(BlueprintReadOnly, Category = "Goddess")
	TArray<FString> BlessedUsernames;

	// Idle이지만 AI 응답을 기다리는 중인지
	UPROPERTY(BlueprintReadOnly, Category = "Goddess")
	bool bWaitingForResponse = false;
};

// Temple AI 배치 요청에 들어가는 대화 한 건
USTRUCT(BlueprintType)
struct FTempleTalkBatchMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString UserId;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Username;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Message;
};

// Temple AI 배치 응답의 개별 응답 한 건
USTRUCT(BlueprintType)
struct FTempleIndividualResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Username;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 Priority = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString ResponseText;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString Emotion;
};

// Temple AI 배치 응답 전체
USTRUCT(BlueprintType)
struct FTempleTalkBatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") FString BatchId;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") TArray<FTempleIndividualResponse> Responses;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 AcceptedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Temple AI") int32 RejectedCount = 0;
};

// 강화 한 단계(!강화 N)에 쌓인 명령 목록
USTRUCT(BlueprintType)
struct FTempleEnhanceTier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple")
	TArray<FParsedCommand> Commands;
};

// 한 사이클(40초) 동안 큐잉된 신전 명령 스냅샷
USTRUCT(BlueprintType)
struct FTempleCycleCommands
{
	GENERATED_BODY()

	// 0번 = !강화 1 ... 3번 = !강화 4
	UPROPERTY(BlueprintReadOnly, Category = "Temple")
	TArray<FTempleEnhanceTier> EnhanceTiers;

	UPROPERTY(BlueprintReadOnly, Category = "Temple")
	TArray<FTempleTalkBatchMessage> Talks;

	UPROPERTY(BlueprintReadOnly, Category = "Temple")
	FDateTime WindowStartedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Temple")
	FDateTime WindowEndedAt;

	bool HasAnyEnhance() const
	{
		for (const FTempleEnhanceTier& Tier : EnhanceTiers)
		{
			if (!Tier.Commands.IsEmpty()) return true;
		}
		return false;
	}

	bool IsEmpty() const { return Talks.IsEmpty() && !HasAnyEnhance(); }
};

// 여신이 처리할 대화 한 건 (질문 + AI 응답 + 처리 시간)
USTRUCT(BlueprintType)
struct FTempleTalkItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Temple") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") FString UserId;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") FString Username;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") FString QueryMessage;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") FString ResponseText;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") FString Emotion;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") int32 Priority = 0;

	// 채팅이 들어온 순서. 커팅 후 재생 순서와 동점 우선순위 처리에 사용
	UPROPERTY(BlueprintReadOnly, Category = "Temple") int32 ArrivalIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Temple") float ReadingSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Temple") float SpeakingSeconds = 0.0f;

	float GetTotalSeconds() const { return ReadingSeconds + SpeakingSeconds; }
};
