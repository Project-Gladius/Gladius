#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Scripts/Backend/PlayerData.h"
#include "PlayerDataManager.generated.h"

/** One persistent player store per game instance, independent of combat levels. */
UCLASS()
class GLADIUS_API UPlayerDataManager final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Player Data", meta = (WorldContext = "WorldContextObject"))
	static UPlayerDataManager* Get(const UObject* WorldContextObject);

	/** Returns a copy; call SavePlayer after editing it. Does not create missing users. */
	UFUNCTION(BlueprintPure, Category = "Player Data")
	bool GetPlayer(const FString& UserId, FPlayerData& OutPlayer) const;

	/** New users receive default stats and are immediately saved to disk. */
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool GetOrCreatePlayer(const FString& UserId, FPlayerData& OutPlayer);

	/** Persists all fields. Returns false on invalid data or a disk-write failure. */
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool SavePlayer(const FPlayerData& Player);

	/** Empty CharacterClass preserves the last known class, including on logout. */
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool SetLoginState(const FString& UserId, bool bIsLoggedIn, const FString& CharacterClass = TEXT(""));

	/** Creates zeroed battle state on first entry; re-entry in the same battle preserves it. */
	UFUNCTION(BlueprintCallable, Category = "Player Data|Combat")
	bool BeginPlayerCombat(const FString& UserId);

	UFUNCTION(BlueprintPure, Category = "Player Data|Combat")
	bool GetCombatData(const FString& UserId, FPlayerCombatData& OutCombatData) const;

	/** Updates only an existing combat participant. Does not write a save file. */
	UFUNCTION(BlueprintCallable, Category = "Player Data|Combat")
	bool UpdateCombatData(const FString& UserId, const FPlayerCombatData& CombatData);

	/** Ends the battle for every participant. The next entry starts from zero. */
	UFUNCTION(BlueprintCallable, Category = "Player Data|Combat")
	void EndCombat();

	UFUNCTION(BlueprintPure, Category = "Player Data")
	bool IsStorageAvailable() const { return SaveData != nullptr; }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPlayerDataSaveGame> SaveData;

	UPROPERTY(Transient)
	TMap<FString, FPlayerCombatData> CombatPlayers;
};
