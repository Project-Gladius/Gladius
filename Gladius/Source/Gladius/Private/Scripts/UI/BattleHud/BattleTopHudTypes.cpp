#include "Scripts/UI/BattleHud/BattleTopHudTypes.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/UI/UIBrushFactory.h"

FBattleTopHudStyle::FBattleTopHudStyle()
{
	using GladiusUI::MakeRoundedBrush;
	using GladiusUI::MakeCircleBrush;

	const FLinearColor Gold(0.83f, 0.66f, 0.32f);
	const FLinearColor Navy(0.01f, 0.025f, 0.05f, 0.92f);

	TurnPanelBrush = MakeRoundedBrush(Navy, Gold, 2.0f, 18.0f);
	CenterBarBrush = MakeRoundedBrush(FLinearColor(0.015f, 0.03f, 0.06f, 0.88f), Gold.CopyWithNewOpacity(0.7f), 1.0f, 8.0f);
	TimerPanelBrush = MakeRoundedBrush(Navy, Gold, 2.0f, 18.0f);

	//명세: 임시로 회색/흰색 막대
	GaugeBackgroundBrush = MakeRoundedBrush(FLinearColor(0.35f, 0.35f, 0.38f, 1.0f), FLinearColor(0.2f, 0.2f, 0.22f), 0.0f, 3.0f);
	GaugeFillBrush = MakeRoundedBrush(FLinearColor::White, FLinearColor::White, 0.0f, 3.0f);

	TimerIconBrush = MakeCircleBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FLinearColor(0.35f, 0.75f, 1.0f), 2.5f);

	TextColor = FLinearColor(0.95f, 0.93f, 0.88f);
	AccentColor = FLinearColor(1.0f, 0.82f, 0.45f);
}

FString FBattleTopHudPresenter::FormatSeconds(float Seconds)
{
	return FString::Printf(TEXT("%.2f"), FMath::Max(0.0f, Seconds));
}

FBattleTopHudState FBattleTopHudPresenter::Build(EBattlePhase Phase, bool bCutScenePlaying, float RemainingSeconds, float DurationSeconds, int32 TurnIndex, int32 Turns)
{
	FBattleTopHudState State;
	State.TurnText = FString::FromInt(FMath::Max(0, TurnIndex));
	State.TurnTotalText = FString::Printf(TEXT("/%d"), FMath::Max(0, Turns));
	State.TimerText = FormatSeconds(RemainingSeconds);
	State.GaugePercent = DurationSeconds > 0.0f ? FMath::Clamp(RemainingSeconds / DurationSeconds, 0.0f, 1.0f) : 0.0f;

	switch (Phase)
	{
	case EBattlePhase::BattleStart:
		State.bVisible = true;
		State.GuideText = TEXT("로그인해 주세요!");
		State.bShowTimer = true;
		State.bShowGauge = true;
		break;

	case EBattlePhase::Prepare:
		State.bVisible = true;
		State.GuideText = TEXT("행동을 선택하세요...");
		State.bShowTimer = true;
		State.bShowGauge = true;
		break;

	case EBattlePhase::Collect:
		State.bVisible = true;
		State.GuideText = TEXT("대기 중...");
		State.bShowTimer = true;
		State.bShowGauge = true;
		break;

	case EBattlePhase::Move:
	case EBattlePhase::BossAction:
	case EBattlePhase::Resolve:
		//컷신이 화면을 쓰는 동안은 상단 UI를 숨김
		State.bVisible = !bCutScenePlaying;
		State.GuideText = TEXT("전투 중!");
		break;

	default: //BattleEnd
		break;
	}
	return State;
}
