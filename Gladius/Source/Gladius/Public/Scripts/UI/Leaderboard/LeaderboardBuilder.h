#pragma once

#include "CoreMinimal.h"
#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"

// 순위를 매길 유저 한 명의 원본 데이터 (어디서 왔는지는 모름)
struct FLeaderboardSource
{
	FString UserId;
	FString Name;
	FString CharacterId;
	int64 Damage = 0;
	int32 Health = 0;
};

// 원본 데이터를 정렬해 리더보드 페이지로 만드는 순수 유틸리티. 상태가 없어 단독으로 테스트할 수 있음
class GLADIUS_API FLeaderboardBuilder
{
public:
	static constexpr int32 RowsPerPage = 10;
	static constexpr int32 LowerPageFirstRank = 11;
	static constexpr int32 LowerPageLastRank = 15;

	//98540 -> "98 540"
	static FString FormatDamage(int64 Damage);

	//CharacterNames(직업 id -> 이름)에 없으면 gladius/sera 기본 이름, 그래도 없으면 id 그대로
	static FString ResolveCharacterName(const FString& CharacterId, const TMap<FString, FString>& CharacterNames);

	//데미지 내림차순(동점이면 이름순)으로 순위를 매김. bUseHealth면 체력 0인 줄을 흐리게 표시
	static TArray<FLeaderboardRow> Rank(TArray<FLeaderboardSource> Sources, const TMap<FString, FString>& CharacterNames, bool bUseHealth);

	//LEADERBOARD: 1~10위
	static FLeaderboardPage MakeTotalPage(const TArray<FLeaderboardRow>& Ranked);

	//BATTLE ANALYSIS 1번: 1~10위
	static FLeaderboardPage MakeBattleTopPage(const TArray<FLeaderboardRow>& Ranked);

	//BATTLE ANALYSIS 2번: 1위, 11~15위, 나머지 빈 칸
	static FLeaderboardPage MakeBattleLowerPage(const TArray<FLeaderboardRow>& Ranked);

	//11위 이상이 있을 때만 2번 보드를 보여줌
	static bool HasBattleLowerPage(const TArray<FLeaderboardRow>& Ranked) { return Ranked.Num() >= LowerPageFirstRank; }

private:
	static FLeaderboardPage MakePage(const FString& Title, const FString& DamageHeader, bool bShowHealth, TArray<FLeaderboardRow> Rows);
};
