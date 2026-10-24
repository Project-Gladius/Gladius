#pragma once

#include "CoreMinimal.h"

// 전투씬 하나 동안 쌓이는 유저 한 명의 기록
struct FBattleSessionEntry
{
	FString DisplayName;
	FString CharacterId;
	int64 DamageDealt = 0;
};

// 전투씬에 들어온 순간부터 나갈 때까지의 유저별 누적 기록 (BATTLE ANALYSIS용)
// FBattlePlayer::DamageDealt는 판마다 초기화되고 FPlayerRecord::TotalDamageDealt는 방송 전체 누적이라
// 씬 단위 기록을 따로 들고 있는다
class GLADIUS_API FBattleSessionStats
{
public:
	//참가할 때 호출. 같은 씬에서 다시 참가하면 데미지는 이어서 누적된다
	void RegisterPlayer(const FString& UserId, const FString& DisplayName, const FString& CharacterId);

	void AddDamage(const FString& UserId, int64 Damage);

	//전투 중 이탈하면 기록도 지운다 (다시 참가하면 0부터)
	void RemovePlayer(const FString& UserId);

	void Reset();

	const TMap<FString, FBattleSessionEntry>& GetEntries() const { return Entries; }

private:
	TMap<FString, FBattleSessionEntry> Entries;
};
