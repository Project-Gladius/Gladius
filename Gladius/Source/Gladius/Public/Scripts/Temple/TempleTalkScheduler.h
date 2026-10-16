#pragma once

#include "CoreMinimal.h"
#include "Scripts/Temple/TempleTypes.h"

// 여신의 대화(Reading -> Speaking) 처리 시간을 계산하고, 사이클 시간 안에 처리할 대화를 고르는 순수 유틸리티
// 상태를 갖지 않으므로 여신(AGoddess)과 분리해 단독으로 테스트할 수 있음
class GLADIUS_API FTempleTalkScheduler
{
public:
	// Reading = ReadingBase + 질문 글자 수 * ByteTime, Speaking = SpeakingBase + 응답 글자 수 * ByteTime
	static void ComputeDurations(TArray<FTempleTalkItem>& Items, float ReadingBaseSeconds, float SpeakingBaseSeconds, float ByteTimeSeconds);

	static float GetTotalSeconds(const TArray<FTempleTalkItem>& Items);

	// 총 처리 시간이 AvailableSeconds 이하가 될 때까지 우선순위가 낮은 대화부터 잘라냄
	// (우선순위가 같으면 늦게 들어온 대화를 먼저 자름). 남은 대화는 들어온 순서로 정렬해 반환
	static TArray<FTempleTalkItem> SelectWithinBudget(TArray<FTempleTalkItem> Items, float AvailableSeconds);
};
