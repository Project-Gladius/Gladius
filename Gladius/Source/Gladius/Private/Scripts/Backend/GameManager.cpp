#include "Scripts/Backend/GameManager.h"
#include "Kismet/GameplayStatics.h"
#include "Scripts/Backend/PlayerDataManager.h"

AGameManager::AGameManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGameManager::BeginPlay()
{
	Super::BeginPlay();
	// A new non-combat level ends the previous battle; combat-to-combat travel preserves it.
	SetGameState(CurrentGameState);
}

void AGameManager::SetGameState(EGameState NewState)
{
	if (NewState != EGameState::Combat)
	{
		if (UPlayerDataManager* Players = UPlayerDataManager::Get(this))
		{
			Players->EndCombat();
		}
	}
	//TODO: 신전 -> 전투로 전환되는 시점에는 CommandDispatcher::ResetLoginForNewBattle()을 호출해
	//이전 전투의 로그인 목록(LoggedInUsers)을 초기화해야 함 (CommandDispatcher 참조를 들고 있게 되면 연결)
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
