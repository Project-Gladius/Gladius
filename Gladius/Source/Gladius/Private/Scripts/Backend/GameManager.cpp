#include "Scripts/Backend/GameManager.h"
#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Engine/World.h"
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
	if (CombatLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("GameManager의 CombatLevel이 설정되지 않아 전투 레벨로 이동할 수 없습니다."));
		return;
	}

 if (URealTimeSettings::MatchesMap(GetWorld(), GetDefault<URealTimeSettings>()->TempleMap))
  GetWorld()->GetSubsystem<ULevelTransitionCinematics>()->PlayOutro(false, FSimpleDelegate::CreateUObject(this, &AGameManager::OpenCombatLevel));
 else OpenCombatLevel();
}

void AGameManager::OpenCombatLevel()
{
 SetGameState(EGameState::Combat);
 UGameplayStatics::OpenLevelBySoftObjectPtr(this, CombatLevel);
}
