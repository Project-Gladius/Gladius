#include "Misc/AutomationTest.h"
#include "Scripts/Temple/TempleTalkScheduler.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FTempleTalkItem MakeItem(int32 ArrivalIndex, int32 Priority, float ReadingSeconds, float SpeakingSeconds)
	{
		FTempleTalkItem Item;
		Item.Uid = FString::Printf(TEXT("uid-%d"), ArrivalIndex);
		Item.ArrivalIndex = ArrivalIndex;
		Item.Priority = Priority;
		Item.ReadingSeconds = ReadingSeconds;
		Item.SpeakingSeconds = SpeakingSeconds;
		return Item;
	}

	TArray<int32> GetArrivalIndices(const TArray<FTempleTalkItem>& Items)
	{
		TArray<int32> Indices;
		for (const FTempleTalkItem& Item : Items)
		{
			Indices.Add(Item.ArrivalIndex);
		}
		return Indices;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleTalkSchedulerDurationTest, "Gladius.Temple.TalkScheduler.ComputeDurations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTempleTalkSchedulerDurationTest::RunTest(const FString& Parameters)
{
	TArray<FTempleTalkItem> Items;
	FTempleTalkItem Item;
	Item.QueryMessage = TEXT("마수가 무서워요");      // 8글자
	Item.ResponseText = TEXT("두려움은 증거란다.");   // 10글자
	Items.Add(Item);

	FTempleTalkScheduler::ComputeDurations(Items, 2.0f, 2.0f, 0.055f);

	TestEqual(TEXT("Reading = 2 + 8 * 0.055"), Items[0].ReadingSeconds, 2.0f + 8 * 0.055f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Speaking = 2 + 10 * 0.055"), Items[0].SpeakingSeconds, 2.0f + 10 * 0.055f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleTalkSchedulerSelectTest, "Gladius.Temple.TalkScheduler.SelectWithinBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTempleTalkSchedulerSelectTest::RunTest(const FString& Parameters)
{
	//(a) 예산 안이면 전부, 들어온 순서대로
	{
		TArray<FTempleTalkItem> Items = { MakeItem(2, 10, 3, 3), MakeItem(0, 90, 3, 3), MakeItem(1, 50, 3, 3) };
		const TArray<FTempleTalkItem> Selected = FTempleTalkScheduler::SelectWithinBudget(Items, 27.0f);
		TestTrue(TEXT("(a) 전부 선택"), GetArrivalIndices(Selected) == TArray<int32>{ 0, 1, 2 });
	}

	//(b) 예산 초과면 우선순위 낮은 것부터 제거
	{
		TArray<FTempleTalkItem> Items = { MakeItem(0, 80, 5, 5), MakeItem(1, 20, 5, 5), MakeItem(2, 60, 5, 5) };
		const TArray<FTempleTalkItem> Selected = FTempleTalkScheduler::SelectWithinBudget(Items, 20.0f);
		TestTrue(TEXT("(b) 우선순위 20 제거"), GetArrivalIndices(Selected) == TArray<int32>{ 0, 2 });
	}

	//(b) 우선순위가 같으면 늦게 들어온 것부터 제거
	{
		TArray<FTempleTalkItem> Items = { MakeItem(0, 50, 5, 5), MakeItem(1, 50, 5, 5), MakeItem(2, 50, 5, 5) };
		const TArray<FTempleTalkItem> Selected = FTempleTalkScheduler::SelectWithinBudget(Items, 20.0f);
		TestTrue(TEXT("(b) 동점이면 마지막 제거"), GetArrivalIndices(Selected) == TArray<int32>{ 0, 1 });
	}

	//단일 항목이 예산을 넘으면 아무것도 처리하지 않음
	{
		TArray<FTempleTalkItem> Items = { MakeItem(0, 100, 20, 20) };
		TestTrue(TEXT("예산 초과 단일 항목 제거"), FTempleTalkScheduler::SelectWithinBudget(Items, 27.0f).IsEmpty());
	}

	//남은 시간이 음수(응답이 너무 늦음)면 전부 제거
	{
		TArray<FTempleTalkItem> Items = { MakeItem(0, 100, 1, 1) };
		TestTrue(TEXT("남은 시간 음수"), FTempleTalkScheduler::SelectWithinBudget(Items, -1.0f).IsEmpty());
	}

	//빈 목록
	TestTrue(TEXT("빈 목록"), FTempleTalkScheduler::SelectWithinBudget({}, 27.0f).IsEmpty());
	return true;
}

#endif
