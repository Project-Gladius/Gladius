#include "Misc/AutomationTest.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FLeaderboardSource MakeSource(const FString& UserId, int64 Damage, int32 Health = 5, const FString& CharacterId = TEXT("gladius"))
	{
		FLeaderboardSource Source;
		Source.UserId = UserId;
		Source.Name = UserId;
		Source.CharacterId = CharacterId;
		Source.Damage = Damage;
		Source.Health = Health;
		return Source;
	}

	TArray<FLeaderboardSource> MakeSources(int32 Count)
	{
		TArray<FLeaderboardSource> Sources;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			//u00이 가장 높은 데미지
			Sources.Add(MakeSource(FString::Printf(TEXT("u%02d"), Index), 1000 - Index * 10));
		}
		return Sources;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeaderboardFormatDamageTest, "Gladius.Leaderboard.FormatDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLeaderboardFormatDamageTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("0"), FLeaderboardBuilder::FormatDamage(0), FString(TEXT("0")));
	TestEqual(TEXT("999"), FLeaderboardBuilder::FormatDamage(999), FString(TEXT("999")));
	TestEqual(TEXT("1000"), FLeaderboardBuilder::FormatDamage(1000), FString(TEXT("1 000")));
	TestEqual(TEXT("98540"), FLeaderboardBuilder::FormatDamage(98540), FString(TEXT("98 540")));
	TestEqual(TEXT("1234567"), FLeaderboardBuilder::FormatDamage(1234567), FString(TEXT("1 234 567")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeaderboardCharacterNameTest, "Gladius.Leaderboard.CharacterName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLeaderboardCharacterNameTest::RunTest(const FString& Parameters)
{
	const TMap<FString, FString> Empty;
	TestEqual(TEXT("gladius 기본 이름"), FLeaderboardBuilder::ResolveCharacterName(TEXT("gladius"), Empty), FString(TEXT("글라디우스")));
	TestEqual(TEXT("sera 기본 이름"), FLeaderboardBuilder::ResolveCharacterName(TEXT("sera"), Empty), FString(TEXT("세라")));

	const TMap<FString, FString> FromBalance = { { TEXT("sera"), TEXT("세라(밸런스)") } };
	TestEqual(TEXT("밸런스 이름 우선"), FLeaderboardBuilder::ResolveCharacterName(TEXT("sera"), FromBalance), FString(TEXT("세라(밸런스)")));
	TestEqual(TEXT("모르는 id는 그대로"), FLeaderboardBuilder::ResolveCharacterName(TEXT("knight"), Empty), FString(TEXT("knight")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeaderboardRankTest, "Gladius.Leaderboard.Rank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLeaderboardRankTest::RunTest(const FString& Parameters)
{
	const TMap<FString, FString> Names;

	TArray<FLeaderboardSource> Sources = { MakeSource(TEXT("b"), 50), MakeSource(TEXT("a"), 50), MakeSource(TEXT("c"), 90, 0) };
	Sources[0].Name.Empty(); //이름이 없으면 UserId로 표시

	const TArray<FLeaderboardRow> Battle = FLeaderboardBuilder::Rank(Sources, Names, true);
	TestEqual(TEXT("1위는 데미지 최고"), Battle[0].Name, FString(TEXT("c")));
	TestEqual(TEXT("동점은 이름순"), Battle[1].Name, FString(TEXT("a")));
	TestEqual(TEXT("이름 없으면 UserId"), Battle[2].Name, FString(TEXT("b")));
	TestEqual(TEXT("순위 번호"), Battle[2].Rank, 3);
	TestTrue(TEXT("체력 0은 흐리게"), Battle[0].bDimmed);
	TestFalse(TEXT("체력 있으면 정상"), Battle[1].bDimmed);

	const TArray<FLeaderboardRow> Total = FLeaderboardBuilder::Rank(Sources, Names, false);
	TestFalse(TEXT("누적 보드는 흐림 없음"), Total[0].bDimmed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeaderboardPagesTest, "Gladius.Leaderboard.Pages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLeaderboardPagesTest::RunTest(const FString& Parameters)
{
	const TMap<FString, FString> Names;

	//누적: 상위 10명, 체력 열 없음
	{
		const FLeaderboardPage Page = FLeaderboardBuilder::MakeTotalPage(FLeaderboardBuilder::Rank(MakeSources(12), Names, false));
		TestEqual(TEXT("LEADERBOARD 제목"), Page.Title, FString(TEXT("LEADERBOARD")));
		TestFalse(TEXT("체력 열 없음"), Page.bShowHealth);
		TestEqual(TEXT("10칸"), Page.Rows.Num(), FLeaderboardBuilder::RowsPerPage);
		TestEqual(TEXT("마지막 칸 10위"), Page.Rows[9].Rank, 10);
	}

	//인원이 적으면 남는 칸은 빈 줄
	{
		const FLeaderboardPage Page = FLeaderboardBuilder::MakeBattleTopPage(FLeaderboardBuilder::Rank(MakeSources(3), Names, true));
		TestTrue(TEXT("체력 열 표시"), Page.bShowHealth);
		TestEqual(TEXT("10칸 유지"), Page.Rows.Num(), FLeaderboardBuilder::RowsPerPage);
		TestFalse(TEXT("3위까지 채움"), Page.Rows[2].bEmpty);
		TestTrue(TEXT("4번째 칸부터 빈 줄"), Page.Rows[3].bEmpty);
	}

	//11명 미만이면 2번 보드 없음
	TestFalse(TEXT("10명이면 2번 보드 없음"), FLeaderboardBuilder::HasBattleLowerPage(FLeaderboardBuilder::Rank(MakeSources(10), Names, true)));

	//2번 보드: 1위 + 11~15위, 나머지 빈 칸
	{
		const TArray<FLeaderboardRow> Ranked = FLeaderboardBuilder::Rank(MakeSources(17), Names, true);
		TestTrue(TEXT("11명 이상이면 2번 보드"), FLeaderboardBuilder::HasBattleLowerPage(Ranked));

		const FLeaderboardPage Page = FLeaderboardBuilder::MakeBattleLowerPage(Ranked);
		TestEqual(TEXT("첫 줄 1위"), Page.Rows[0].Rank, 1);
		TestEqual(TEXT("둘째 줄 11위"), Page.Rows[1].Rank, 11);
		TestEqual(TEXT("여섯째 줄 15위"), Page.Rows[5].Rank, 15);
		TestTrue(TEXT("16위부터는 표시 안 함"), Page.Rows[6].bEmpty);
		TestEqual(TEXT("10칸 유지"), Page.Rows.Num(), FLeaderboardBuilder::RowsPerPage);
	}
	return true;
}

#endif
