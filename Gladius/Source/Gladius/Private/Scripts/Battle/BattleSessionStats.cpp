#include "Scripts/Battle/BattleSessionStats.h"

void FBattleSessionStats::RegisterPlayer(const FString& UserId, const FString& DisplayName, const FString& CharacterId)
{
	FBattleSessionEntry& Entry = Entries.FindOrAdd(UserId);
	Entry.DisplayName = DisplayName;
	Entry.CharacterId = CharacterId;
}

void FBattleSessionStats::AddDamage(const FString& UserId, int64 Damage)
{
	if (FBattleSessionEntry* Entry = Entries.Find(UserId))
	{
		Entry->DamageDealt += Damage;
	}
}

void FBattleSessionStats::RemovePlayer(const FString& UserId)
{
	Entries.Remove(UserId);
}

void FBattleSessionStats::Reset()
{
	Entries.Reset();
}
