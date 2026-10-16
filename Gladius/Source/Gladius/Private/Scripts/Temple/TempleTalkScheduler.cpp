#include "Scripts/Temple/TempleTalkScheduler.h"

void FTempleTalkScheduler::ComputeDurations(TArray<FTempleTalkItem>& Items, float ReadingBaseSeconds, float SpeakingBaseSeconds, float ByteTimeSeconds)
{
	for (FTempleTalkItem& Item : Items)
	{
		Item.ReadingSeconds = ReadingBaseSeconds + Item.QueryMessage.Len() * ByteTimeSeconds;
		Item.SpeakingSeconds = SpeakingBaseSeconds + Item.ResponseText.Len() * ByteTimeSeconds;
	}
}

float FTempleTalkScheduler::GetTotalSeconds(const TArray<FTempleTalkItem>& Items)
{
	float Total = 0.0f;
	for (const FTempleTalkItem& Item : Items)
	{
		Total += Item.GetTotalSeconds();
	}
	return Total;
}

TArray<FTempleTalkItem> FTempleTalkScheduler::SelectWithinBudget(TArray<FTempleTalkItem> Items, float AvailableSeconds)
{
	float Total = GetTotalSeconds(Items);

	if (Total > AvailableSeconds + KINDA_SMALL_NUMBER)
	{
		//top에 "가장 먼저 잘라낼 대화"가 오는 최소 힙: 우선순위가 낮을수록, 같으면 늦게 들어올수록 먼저
		auto CutFirst = [](const FTempleTalkItem& A, const FTempleTalkItem& B)
		{
			if (A.Priority != B.Priority)
			{
				return A.Priority < B.Priority;
			}
			return A.ArrivalIndex > B.ArrivalIndex;
		};

		Items.Heapify(CutFirst);
		while (!Items.IsEmpty() && Total > AvailableSeconds + KINDA_SMALL_NUMBER)
		{
			FTempleTalkItem Cut;
			Items.HeapPop(Cut, CutFirst);
			Total -= Cut.GetTotalSeconds();
		}
	}

	Items.Sort([](const FTempleTalkItem& A, const FTempleTalkItem& B)
	{
		return A.ArrivalIndex < B.ArrivalIndex;
	});
	return Items;
}
