#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BattleGameMode.generated.h"

//전투 씬은 고정 카메라 + 채팅 입력이라 조종할 폰이 없다.
//기본 폰을 두면 WASD로 굴러다니는 구체가 생기고, 그 폰의 방향이 카메라와 달라 좌우가 반대로 느껴진다
UCLASS()
class GLADIUS_API ABattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABattleGameMode();
};
