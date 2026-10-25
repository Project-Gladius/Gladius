#include "Misc/AutomationTest.h"
#include "Scripts/UI/BattleHud/BattleTopHudTypes.h"
#include "Scripts/Battle/BattleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleTopHudPresenterTest, "Gladius.BattleTopHud.Presenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleTopHudPresenterTest::RunTest(const FString& Parameters)
{
	//Prepare: 선택 안내 + 시간 + 게이지
	{
		const FBattleTopHudState S = FBattleTopHudPresenter::Build(EBattlePhase::Prepare, false, 30.0f, 60.0f, 5, 10);
		TestTrue(TEXT("Prepare 표시"), S.bVisible);
		TestEqual(TEXT("Prepare 문구"), S.GuideText, FString(TEXT("행동을 선택하세요...")));
		TestTrue(TEXT("Prepare 시간 표시"), S.bShowTimer);
		TestTrue(TEXT("Prepare 게이지 표시"), S.bShowGauge);
		TestEqual(TEXT("소수점 2자리"), S.TimerText, FString(TEXT("30.00")));
		TestEqual(TEXT("게이지 절반"), S.GaugePercent, 0.5f, KINDA_SMALL_NUMBER);
		TestEqual(TEXT("턴"), S.TurnText, FString(TEXT("5")));
		TestEqual(TEXT("전체 턴"), S.TurnTotalText, FString(TEXT("/10")));
	}

	//Collect: 대기 안내, 새 단계라 게이지가 가득 찬 상태에서 시작
	{
		const FBattleTopHudState S = FBattleTopHudPresenter::Build(EBattlePhase::Collect, false, 6.0f, 6.0f, 5, 10);
		TestEqual(TEXT("Collect 문구"), S.GuideText, FString(TEXT("대기 중...")));
		TestTrue(TEXT("Collect 시간 표시"), S.bShowTimer);
		TestEqual(TEXT("Collect 시작 시 게이지 가득"), S.GaugePercent, 1.0f, KINDA_SMALL_NUMBER);
	}

	//Move 컷신 중: 전체 숨김
	TestFalse(TEXT("컷신 중 숨김"), FBattleTopHudPresenter::Build(EBattlePhase::Move, true, 0.0f, 0.0f, 5, 10).bVisible);

	//컷신 종료 후 ~ Resolve: 전투 중, 시간·게이지 숨김
	for (const EBattlePhase Phase : { EBattlePhase::Move, EBattlePhase::BossAction, EBattlePhase::Resolve })
	{
		const FBattleTopHudState S = FBattleTopHudPresenter::Build(Phase, false, 0.0f, 0.0f, 5, 10);
		TestTrue(TEXT("전투 중 표시"), S.bVisible);
		TestEqual(TEXT("전투 중 문구"), S.GuideText, FString(TEXT("전투 중!")));
		TestFalse(TEXT("전투 중 시간 숨김"), S.bShowTimer);
		TestFalse(TEXT("전투 중 게이지 숨김"), S.bShowGauge);
	}

	//BattleStart: 로그인 안내는 인트로와 함께 표시한다.
 const FBattleTopHudState Login = FBattleTopHudPresenter::Build(EBattlePhase::BattleStart, true, 10.f, 20.f, 0, 10);
 TestTrue(TEXT("BattleStart visible during intro"), Login.bVisible);
 TestEqual(TEXT("Login prompt"), Login.GuideText, FString(TEXT("로그인해 주세요!")));
 TestTrue(TEXT("Login countdown"), Login.bShowTimer && Login.bShowGauge);
 TestEqual(TEXT("Login countdown progress"), Login.GaugePercent, 0.5f, KINDA_SMALL_NUMBER);
	TestFalse(TEXT("BattleEnd 숨김"), FBattleTopHudPresenter::Build(EBattlePhase::BattleEnd, false, 0.0f, 0.0f, 10, 10).bVisible);

	//게이지 비율은 0~1로 제한, 남은 시간은 음수로 표시하지 않음
	TestEqual(TEXT("게이지 상한"), FBattleTopHudPresenter::Build(EBattlePhase::Prepare, false, 99.0f, 10.0f, 1, 10).GaugePercent, 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("길이 0이면 게이지 0"), FBattleTopHudPresenter::Build(EBattlePhase::Prepare, false, 5.0f, 0.0f, 1, 10).GaugePercent, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("음수 시간"), FBattleTopHudPresenter::FormatSeconds(-1.0f), FString(TEXT("0.00")));
	return true;
}

#endif
