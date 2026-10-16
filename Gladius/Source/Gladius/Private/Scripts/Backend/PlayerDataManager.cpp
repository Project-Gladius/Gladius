#include "Scripts/Backend/PlayerDataManager.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerData, Log, All);

namespace
{
const FString PlayerSaveSlot = TEXT("GladiusPlayerData");
constexpr int32 SaveUserIndex = 0;
}

bool FPlayerData::IsValid() const
{
	return !UserId.TrimStartAndEnd().IsEmpty()
		&& MaxHealth > 0 && CurrentHealth >= 0 && CurrentHealth <= MaxHealth
		&& FMath::IsFinite(TotalDamageDealt) && TotalDamageDealt >= 0.0
		&& FMath::IsFinite(CriticalChance) && CriticalChance >= 0.0f && CriticalChance <= 1.0f
		&& FMath::IsFinite(CriticalChanceEnhancement) && CriticalChanceEnhancement >= 0.0f && CriticalChanceEnhancement <= 1.0f
		&& FMath::IsFinite(CriticalDamage) && CriticalDamage >= 0.0f
		&& FMath::IsFinite(CriticalDamageEnhancement) && CriticalDamageEnhancement >= 0.0f;
}

UPlayerDataManager* UPlayerDataManager::Get(const UObject* WorldContextObject)
{
	UGameInstance* Instance = WorldContextObject ? UGameplayStatics::GetGameInstance(WorldContextObject) : nullptr;
	return Instance ? Instance->GetSubsystem<UPlayerDataManager>() : nullptr;
}

void UPlayerDataManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!UGameplayStatics::DoesSaveGameExist(PlayerSaveSlot, SaveUserIndex))
	{
		SaveData = Cast<UPlayerDataSaveGame>(UGameplayStatics::CreateSaveGameObject(UPlayerDataSaveGame::StaticClass()));
		return;
	}

	UPlayerDataSaveGame* Loaded = Cast<UPlayerDataSaveGame>(UGameplayStatics::LoadGameFromSlot(PlayerSaveSlot, SaveUserIndex));
	if (Loaded && Loaded->Version == 1)
	{
		bool bValid = true;
		for (const auto& Entry : Loaded->Players)
		{
			bValid &= Entry.Key == Entry.Value.UserId && Entry.Value.IsValid();
		}
		if (bValid)
		{
			SaveData = Loaded;
			return;
		}
	}
	// Do not silently replace an unreadable save with an empty database.
	UE_LOG(LogPlayerData, Error, TEXT("Cannot load %s: invalid or unsupported save. Existing file will not be overwritten."), *PlayerSaveSlot);
}

bool UPlayerDataManager::GetPlayer(const FString& UserId, FPlayerData& OutPlayer) const
{
	OutPlayer = FPlayerData();
	if (SaveData)
	{
		if (const FPlayerData* Player = SaveData->Players.Find(UserId))
		{
			OutPlayer = *Player;
			return true;
		}
	}
	return false;
}

bool UPlayerDataManager::GetOrCreatePlayer(const FString& UserId, FPlayerData& OutPlayer)
{
	if (GetPlayer(UserId, OutPlayer)) { return true; }
	FPlayerData NewPlayer;
	NewPlayer.UserId = UserId;
	if (!SavePlayer(NewPlayer)) { return false; }
	OutPlayer = NewPlayer;
	return true;
}

bool UPlayerDataManager::SavePlayer(const FPlayerData& Player)
{
	if (!SaveData || !Player.IsValid())
	{
		UE_LOG(LogPlayerData, Warning, TEXT("Cannot save player '%s': storage unavailable or invalid player data."), *Player.UserId);
		return false;
	}
	TOptional<FPlayerData> Previous;
	if (const FPlayerData* Existing = SaveData->Players.Find(Player.UserId)) { Previous = *Existing; }
	SaveData->Players.Add(Player.UserId, Player);
	if (UGameplayStatics::SaveGameToSlot(SaveData, PlayerSaveSlot, SaveUserIndex)) { return true; }
	if (Previous.IsSet()) { SaveData->Players.Add(Player.UserId, Previous.GetValue()); }
	else { SaveData->Players.Remove(Player.UserId); }
	UE_LOG(LogPlayerData, Error, TEXT("Failed to write player save '%s'."), *PlayerSaveSlot);
	return false;
}

bool UPlayerDataManager::SetLoginState(const FString& UserId, bool bIsLoggedIn, const FString& CharacterClass)
{
	FPlayerData Player;
	if (!GetOrCreatePlayer(UserId, Player)) { return false; }
	Player.bIsLoggedIn = bIsLoggedIn;
	if (bIsLoggedIn && !CharacterClass.IsEmpty()) { Player.LastLoggedInClass = CharacterClass; }
	return SavePlayer(Player);
}

bool UPlayerDataManager::BeginPlayerCombat(const FString& UserId)
{
	if (UserId.TrimStartAndEnd().IsEmpty()) { return false; }
	CombatPlayers.FindOrAdd(UserId);
	return true;
}

bool UPlayerDataManager::GetCombatData(const FString& UserId, FPlayerCombatData& OutCombatData) const
{
	OutCombatData = FPlayerCombatData();
	if (const FPlayerCombatData* Data = CombatPlayers.Find(UserId))
	{
		OutCombatData = *Data;
		return true;
	}
	return false;
}

bool UPlayerDataManager::UpdateCombatData(const FString& UserId, const FPlayerCombatData& CombatData)
{
	FPlayerCombatData* Existing = CombatPlayers.Find(UserId);
	if (!Existing || CombatData.Fever < 0 || CombatData.DodgeCount < 0) { return false; }
	*Existing = CombatData;
	return true;
}

void UPlayerDataManager::EndCombat()
{
	CombatPlayers.Reset();
}
