#include "Scripts/Backend/GameManager.h"
#include "Kismet/GameplayStatics.h"


AGameManager::AGameManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGameManager::BeginPlay()
{
	Super::BeginPlay();
	// Scene routing only; battle state and player records belong to BattleManager.
	SetGameState(CurrentGameState);
}

void AGameManager::SetGameState(EGameState NewState)
{
	CurrentGameState = NewState;
}

void AGameManager::TravelToCombat()
{
	SetGameState(EGameState::Combat);

	if (CombatLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("GameManager의 CombatLevel이 설정되지 않아 전투 레벨로 이동할 수 없습니다."));
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, CombatLevel);
}
