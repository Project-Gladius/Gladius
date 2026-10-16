#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerData.generated.h"

/** Persistent player information, keyed by the YouTube UserDatabase UserId. */
USTRUCT(BlueprintType)
struct GLADIUS_API FPlayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Identity")
	FString UserId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Identity")
	bool bIsLoggedIn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Identity")
	bool bCanPurchaseFromShop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Identity")
	FString LastLoggedInClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Identity", meta = (ClampMin = "0"))
	double TotalDamageDealt = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Stats", meta = (ClampMin = "0"))
	int32 CurrentHealth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Stats", meta = (ClampMin = "1"))
	int32 MaxHealth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Stats", meta = (ClampMin = "0", ClampMax = "1"))
	float CriticalChance = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Stats", meta = (ClampMin = "0", ClampMax = "1"))
	float CriticalChanceEnhancement = 0.0f;

	/** Additional damage ratio: 0.4 means a critical hit deals 1.4 times damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Stats", meta = (ClampMin = "0"))
	float CriticalDamage = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Player|Stats", meta = (ClampMin = "0"))
	float CriticalDamageEnhancement = 0.0f;

	bool IsValid() const;
};

/** Per-battle state; deliberately kept outside the persistent save payload. */
USTRUCT(BlueprintType)
struct GLADIUS_API FPlayerCombatData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Combat", meta = (ClampMin = "0"))
	int32 Fever = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Combat", meta = (ClampMin = "0"))
	int32 DodgeCount = 0;
};

/** Local save-file payload. Runtime access goes through UPlayerDataManager. */
UCLASS()
class GLADIUS_API UPlayerDataSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 Version = 1;

	UPROPERTY(SaveGame)
	TMap<FString, FPlayerData> Players;
};
