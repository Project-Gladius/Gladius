#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"

FString FLeaderboardBuilder::FormatDamage(int64 Damage)
{
	const bool bNegative = Damage < 0;
	const FString Digits = FString::Printf(TEXT("%lld"), bNegative ? -Damage : Damage);

	FString Result;
	for (int32 Index = 0; Index < Digits.Len(); ++Index)
	{
		//뒤에서부터 세 자리마다 공백
		if (Index > 0 && (Digits.Len() - Index) % 3 == 0)
		{
			Result.AppendChar(TEXT(' '));
		}
		Result.AppendChar(Digits[Index]);
	}
	return bNegative ? TEXT("-") + Result : Result;
}

FString FLeaderboardBuilder::ResolveCharacterName(const FString& CharacterId, const TMap<FString, FString>& CharacterNames)
{
	if (const FString* Name = CharacterNames.Find(CharacterId))
	{
		if (!Name->IsEmpty())
		{
			return *Name;
		}
	}
	if (CharacterId.Equals(TEXT("gladius"), ESearchCase::IgnoreCase))
	{
		return TEXT("글라디우스");
	}
	if (CharacterId.Equals(TEXT("sera"), ESearchCase::IgnoreCase))
	{
		return TEXT("세라");
	}
	return CharacterId;
}

TArray<FLeaderboardRow> FLeaderboardBuilder::Rank(TArray<FLeaderboardSource> Sources, const TMap<FString, FString>& CharacterNames, bool bUseHealth)
{
	for (FLeaderboardSource& Source : Sources)
	{
		if (Source.Name.IsEmpty())
		{
			Source.Name = Source.UserId;
		}
	}

	Sources.StableSort([](const FLeaderboardSource& A, const FLeaderboardSource& B)
	{
		if (A.Damage != B.Damage)
		{
			return A.Damage > B.Damage;
		}
		const int32 NameOrder = A.Name.Compare(B.Name, ESearchCase::IgnoreCase);
		return NameOrder != 0 ? NameOrder < 0 : A.UserId < B.UserId;
	});

	TArray<FLeaderboardRow> Rows;
	Rows.Reserve(Sources.Num());
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		const FLeaderboardSource& Source = Sources[Index];

		FLeaderboardRow Row;
		Row.Rank = Index + 1;
		Row.Name = Source.Name;
		Row.CharacterName = ResolveCharacterName(Source.CharacterId, CharacterNames);
		Row.Damage = Source.Damage;
		Row.Health = Source.Health;
		Row.bDimmed = bUseHealth && Source.Health <= 0;
		Row.bEmpty = false;
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

FLeaderboardPage FLeaderboardBuilder::MakeTotalPage(const TArray<FLeaderboardRow>& Ranked)
{
	TArray<FLeaderboardRow> Rows(Ranked.GetData(), FMath::Min(Ranked.Num(), RowsPerPage));
	return MakePage(TEXT("LEADERBOARD"), TEXT("누적 데미지"), false, MoveTemp(Rows));
}

FLeaderboardPage FLeaderboardBuilder::MakeBattleTopPage(const TArray<FLeaderboardRow>& Ranked)
{
	TArray<FLeaderboardRow> Rows(Ranked.GetData(), FMath::Min(Ranked.Num(), RowsPerPage));
	return MakePage(TEXT("BATTLE ANALYSIS"), TEXT("데미지"), true, MoveTemp(Rows));
}

FLeaderboardPage FLeaderboardBuilder::MakeBattleLowerPage(const TArray<FLeaderboardRow>& Ranked)
{
	TArray<FLeaderboardRow> Rows;
	if (!Ranked.IsEmpty())
	{
		Rows.Add(Ranked[0]);
	}
	for (int32 Rank = LowerPageFirstRank; Rank <= LowerPageLastRank && Ranked.IsValidIndex(Rank - 1); ++Rank)
	{
		Rows.Add(Ranked[Rank - 1]);
	}
	return MakePage(TEXT("BATTLE ANALYSIS"), TEXT("데미지"), true, MoveTemp(Rows));
}

FLeaderboardPage FLeaderboardBuilder::MakePage(const FString& Title, const FString& DamageHeader, bool bShowHealth, TArray<FLeaderboardRow> Rows)
{
	FLeaderboardPage Page;
	Page.Title = Title;
	Page.DamageHeader = DamageHeader;
	Page.bShowHealth = bShowHealth;
	Page.Rows = MoveTemp(Rows);
	while (Page.Rows.Num() < RowsPerPage)
	{
		Page.Rows.AddDefaulted(); //bEmpty = true
	}
	return Page;
}
