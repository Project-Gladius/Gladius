#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "LeaderboardTypes.generated.h"

UENUM(BlueprintType)
enum class ELeaderboardPreviewPage : uint8
{
 Total,
 BattleTop,
 BattleLower
};

// 리더보드 한 줄
USTRUCT(BlueprintType)
struct FLeaderboardRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") int32 Rank = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") FString CharacterName;
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") int64 Damage = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") int32 Health = 0;

	//체력이 0이라 흐리게 표시할 줄
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") bool bDimmed = false;

	//표시할 사람이 없는 빈 줄
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") bool bEmpty = true;
};

// 화면에 한 번에 보이는 보드 한 장
USTRUCT(BlueprintType)
struct FLeaderboardPage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") FString DamageHeader;
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") bool bShowHealth = false;

	//항상 FLeaderboardBuilder::RowsPerPage 칸. 남는 칸은 bEmpty
	UPROPERTY(BlueprintReadOnly, Category = "Leaderboard") TArray<FLeaderboardRow> Rows;
};

// 리더보드 외형. 지금은 기본 도형(라운드 박스)이지만, UI 에셋이 오면 각 브러시에 텍스처만 지정하면 된다
USTRUCT(BlueprintType)
struct GLADIUS_API FLeaderboardStyle
{
	GENERATED_BODY()

	FLeaderboardStyle();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Brush") FSlateBrush PanelBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Brush") FSlateBrush HeaderBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Brush") FSlateBrush RowBrush;

	//1위 줄 강조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Brush") FSlateBrush FirstRowBrush;

	//1~3위 순위 배경(메달). 인덱스 0 = 1위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Brush", meta = (EditFixedSize)) TArray<FSlateBrush> MedalBrushes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Color") FLinearColor TitleColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Color") FLinearColor HeaderTextColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Color") FLinearColor TextColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Color") FLinearColor FirstDamageColor;

	//체력 0인 줄의 불투명도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Color", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DimmedOpacity = 0.35f;

	//지정하면 이 폰트를 사용 (비우면 엔진 기본 폰트)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Font", meta = (AllowedClasses = "/Script/Engine.Font")) TObjectPtr<UObject> FontFamily;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Font") int32 TitleFontSize = 26;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Font") int32 HeaderFontSize = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Font") int32 RowFontSize = 15;

	// ===== 레이아웃 (px, 보드 기준) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Layout") float BoardWidth = 540.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Layout") float RowHeight = 42.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Layout") float RankColumnWidth = 64.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Layout") float CharacterColumnWidth = 110.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Layout") float HealthColumnWidth = 56.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard|Layout") float DamageColumnWidth = 120.0f;
};
