#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Scripts/Temple/TempleTypes.h"
#include "TempleCommandQueue.generated.h"

class UCommandDispatcher;

// 신전상태에서 CommandDispatcher가 브로드캐스트하는 !강화 / !대화 명령을 한 사이클 동안 쌓아두는 큐
// 사이클이 시작될 때 TakeSnapshot()으로 쌓인 명령을 넘기고 비움
UCLASS()
class GLADIUS_API UTempleCommandQueue : public UObject
{
	GENERATED_BODY()

public:
	//Dispatcher의 OnEnhanceCommand / OnTalkCommand를 구독
	void Bind(UCommandDispatcher* Dispatcher);
	void Unbind();

	//지금까지 쌓인 명령을 넘기고 큐를 비움 (다음 사이클 큐잉 시작)
	FTempleCycleCommands TakeSnapshot();

	// 유저당 신전에서 선택 가능한 축복 횟수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple|Blessing", meta = (ClampMin = "0"))
	int32 AvailableBless = 3;

	// ===== 대화 큐잉 제한 (Temple AI 서버의 배치 요청 제한과 동일) =====
	int32 MaxTalkMessageLength = 200;
	int32 MaxTalksPerCycle = 30;
	int32 MaxTotalTalkCharacters = 4000;

private:
	void AddEnhance(const FString& UserId, const FString& Username, const FString& Argument);
	void AddTalk(const FString& UserId, const FString& CommentId, const FString& Username, const FString& Message);
	void ResetWindow(const FDateTime& WindowStartedAt);

	TWeakObjectPtr<UCommandDispatcher> BoundDispatcher;
	FDelegateHandle EnhanceHandle;
	FDelegateHandle TalkHandle;

	FTempleCycleCommands Current;
	TMap<FString, int32> BlessSelectionsByUser;

	//이미 처리한 채팅 Id (같은 채팅 중복 처리 방지)
	TSet<FString> ProcessedTalkUids;

	int32 CurrentTalkCharacterCount = 0;
	bool bWindowStarted = false;
};
