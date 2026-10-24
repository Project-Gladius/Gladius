#include "Scripts/UI/Leaderboard/LeaderboardTypes.h"
#include "Scripts/UI/UIBrushFactory.h"

FLeaderboardStyle::FLeaderboardStyle()
{
	using GladiusUI::MakeRoundedBrush;
	using GladiusUI::MakeCircleBrush;

	const FLinearColor Gold(0.83f, 0.66f, 0.32f);

	PanelBrush = MakeRoundedBrush(FLinearColor(0.012f, 0.025f, 0.05f, 0.92f), Gold, 2.0f, 14.0f);
	HeaderBrush = MakeRoundedBrush(FLinearColor(0.03f, 0.06f, 0.11f, 1.0f), FLinearColor(0.12f, 0.2f, 0.32f), 1.0f, 4.0f);
	RowBrush = MakeRoundedBrush(FLinearColor(0.02f, 0.04f, 0.075f, 0.9f), FLinearColor(0.06f, 0.1f, 0.16f), 1.0f, 2.0f);
	FirstRowBrush = MakeRoundedBrush(FLinearColor(0.04f, 0.1f, 0.18f, 1.0f), FLinearColor(0.25f, 0.55f, 0.85f), 1.5f, 6.0f);

	MedalBrushes = {
		MakeCircleBrush(FLinearColor(0.55f, 0.4f, 0.1f), FLinearColor(1.0f, 0.8f, 0.35f)),   // 금
		MakeCircleBrush(FLinearColor(0.3f, 0.38f, 0.5f), FLinearColor(0.8f, 0.87f, 0.97f)),  // 은
		MakeCircleBrush(FLinearColor(0.42f, 0.22f, 0.1f), FLinearColor(0.95f, 0.6f, 0.35f)), // 동
	};

	TitleColor = FLinearColor(0.93f, 0.88f, 0.76f);
	HeaderTextColor = FLinearColor(0.62f, 0.68f, 0.78f);
	TextColor = FLinearColor(0.9f, 0.92f, 0.96f);
	FirstDamageColor = FLinearColor(1.0f, 0.8f, 0.35f);
}
