#include "Scripts/Backend/YouTubeApiKeyPool.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString FYouTubeApiKeyPool::GetDefaultKeyFilePath()
{
	return FPaths::ProjectConfigDir() / TEXT("YouTubeApiKeys.txt");
}

int32 FYouTubeApiKeyPool::LoadFromFile(const FString& Path)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		return 0;
	}

	const int32 Before = Keys.Num();
	for (const FString& Line : Lines)
	{
		const FString Key = Line.TrimStartAndEnd();
		if (!Key.StartsWith(TEXT("#")))
		{
			AddKey(Key);
		}
	}
	return Keys.Num() - Before;
}

void FYouTubeApiKeyPool::AddKey(const FString& Key)
{
	const FString Clean = Key.TrimStartAndEnd();
	if (!Clean.IsEmpty())
	{
		Keys.AddUnique(Clean);
	}
}

FString FYouTubeApiKeyPool::GetCurrentKey(const FDateTime& NowUtc)
{
	//지금 키부터 한 바퀴 돌며 쓸 수 있는 키를 찾음
	for (int32 Offset = 0; Offset < Keys.Num(); ++Offset)
	{
		const int32 Index = (CurrentIndex + Offset) % Keys.Num();
		if (IsAvailable(Keys[Index], NowUtc))
		{
			CurrentIndex = Index;
			return Keys[Index];
		}
	}
	return FString();
}

bool FYouTubeApiKeyPool::MarkExhausted(const FString& Key, const FDateTime& NowUtc)
{
	ExhaustedUntil.Add(Key, NextQuotaReset(NowUtc));
	const int32 Index = Keys.IndexOfByKey(Key);
	if (Index != INDEX_NONE && Index == CurrentIndex && Keys.Num() > 0)
	{
		CurrentIndex = (CurrentIndex + 1) % Keys.Num();
	}
	return !GetCurrentKey(NowUtc).IsEmpty();
}

int32 FYouTubeApiKeyPool::NumAvailable(const FDateTime& NowUtc) const
{
	int32 Count = 0;
	for (const FString& Key : Keys)
	{
		Count += IsAvailable(Key, NowUtc) ? 1 : 0;
	}
	return Count;
}

FDateTime FYouTubeApiKeyPool::NextQuotaReset(const FDateTime& MarkedUtc)
{
	//미국 태평양 자정 = 서머타임 07:00 UTC / 표준시 08:00 UTC. 늦은 쪽(08:00)을 기준으로 함
	FDateTime Reset = MarkedUtc.GetDate() + FTimespan::FromHours(8.0);
	if (Reset <= MarkedUtc)
	{
		Reset += FTimespan::FromDays(1.0);
	}
	return Reset;
}

FString FYouTubeApiKeyPool::Mask(const FString& Key)
{
	return Key.Left(8) + TEXT("****");
}

bool FYouTubeApiKeyPool::IsAvailable(const FString& Key, const FDateTime& NowUtc) const
{
	const FDateTime* Until = ExhaustedUntil.Find(Key);
	return !Until || NowUtc >= *Until;
}
