#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Temple/TempleTypes.h"
#include "Goddess.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoddessStateChanged, EGoddessState, NewState, const FGoddessStateContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGoddessEnhanceExecuted, const FString&, UserId, const FString&, Username, int32, Tier);

// 신전의 여신. 한 사이클 동안 큐잉된 명령을 처리하는 상태 머신
// Blessing1~4(강화 1~4 처리) -> (Reading -> Speaking) 반복(대화 처리) -> Idle
// 여신은 표현 방식을 모르고 OnStateChanged만 브로드캐스트함. 텍스트 UI, 이후 모션/AnimBP는 이 델리게이트를 구독해 연출
UCLASS()
class GLADIUS_API AGoddess : public AActor
{
	GENERATED_BODY()

public:
	AGoddess();

	// ===== 기획 조정용 시간 설정 =====

	//Blessing1~4 각각의 지속 시간(초). 인덱스 0 = Blessing1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goddess|Timing", meta = (EditFixedSize))
	TArray<float> BlessingDurations;

	//Reading 기본 처리 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goddess|Timing", meta = (ClampMin = "0.0"))
	float ReadingBaseSeconds = 2.0f;

	//Speaking 기본 처리 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goddess|Timing", meta = (ClampMin = "0.0"))
	float SpeakingBaseSeconds = 2.0f;

	//글자 1개당 추가 처리 시간(초). Reading은 질문, Speaking은 응답 글자 수에 곱함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goddess|Timing", meta = (ClampMin = "0.0"))
	float ByteTimeSeconds = 0.055f;

	//사이클 시작부터 대화가 끝나야 하는 시간(초). 넘치는 대화는 우선순위가 낮은 것부터 잘림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goddess|Timing", meta = (ClampMin = "0.0"))
	float CycleBudgetSeconds = 39.0f;

	// ===== 이벤트 =====

	//상태가 바뀔 때마다 브로드캐스트 (UI/모션 연출용)
	UPROPERTY(BlueprintAssignable, Category = "Goddess")
	FOnGoddessStateChanged OnStateChanged;

	//BlessingN에서 강화 명령 하나를 실행할 때마다 브로드캐스트. 실제 강화 로직이 구독
	UPROPERTY(BlueprintAssignable, Category = "Goddess")
	FOnGoddessEnhanceExecuted OnEnhanceExecuted;

	UFUNCTION(BlueprintPure, Category = "Goddess")
	EGoddessState GetState() const { return State; }

	// ===== 사이클 제어 (ATempleCycleController가 호출) =====

	//큐잉이 끝난 명령으로 새 사이클 시작. 이 시점이 사이클 시간(CycleBudgetSeconds)의 기준
	void StartCycle(const FTempleCycleCommands& Commands);

	//AI 응답이 도착한 대화 목록 전달. Blessing이 끝났으면 바로 대화 시퀀스 시작
	void ReceiveTalkItems(TArray<FTempleTalkItem> Items);

	//이번 사이클에 처리할 대화가 없음 (대화 없음 / AI 요청 실패)
	void SetNoTalks();

	//진행 중인 사이클을 멈추고 Idle로
	void StopCycle();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class ETalkPhase : uint8
	{
		None,    //처리할 대화 없음
		Pending, //AI 응답 대기 중
		Ready,   //응답 도착, Blessing 종료 후 시작 가능
	};

	EGoddessState State = EGoddessState::Idle;
	ETalkPhase TalkPhase = ETalkPhase::None;
	bool bBlessingFinished = false;
	double CycleStartedAt = 0.0;

	TArray<FTempleEnhanceTier> EnhanceTiers;
	TArray<FTempleTalkItem> ReceivedTalkItems;
	TArray<FTempleTalkItem> TalkSequence;
	int32 TalkIndex = 0;

	FTimerHandle StateTimerHandle;

	void EnterBlessing(int32 TierIndex);
	void FinishBlessing();
	void StartTalkSequence();
	void EnterReading();
	void EnterSpeaking();
	void AdvanceTalk();
	void EnterIdle(bool bWaitingForResponse = false);

	void SetState(EGoddessState NewState, const FGoddessStateContext& Context);
	float GetBlessingDuration(int32 TierIndex) const;
	float GetCycleElapsedSeconds() const;
};
