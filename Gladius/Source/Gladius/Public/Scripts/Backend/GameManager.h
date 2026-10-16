#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GladiusTypes.h"
#include "GameManager.generated.h"

// 현재 게임이 전투상태인지 신전상태인지를 관리하는 클래스
// YouTubeChatManager/CommandDispatcher는 이 클래스를 통해 현재 상태를 조회해 로그인 가능 여부와 유효한 명령어를 판단함
UCLASS()
class GLADIUS_API AGameManager : public AActor
{
	GENERATED_BODY()

public:
	AGameManager();
	virtual void BeginPlay() override;

	//현재 game이 전투상태인지 신전상태인지 나타내는 값
	//TODO: 지금은 에디터에서 수동으로 설정하거나 기본값(신전)을 사용. 실제로는 보스전 시작/종료 같은 상태 전환 로직이 구현되면
	//그 로직에서 SetGameState()를 호출해 이 값을 갱신해야 함
	UPROPERTY(EditAnywhere, BlueprintGetter = GetGameState, BlueprintSetter = SetGameState, Category = "Game State")
	EGameState CurrentGameState = EGameState::Temple;

	UFUNCTION(BlueprintGetter, Category = "Game State")
	EGameState GetGameState() const { return CurrentGameState; }

	//상태를 전환할 때 호출. 신전 -> 전투 전환 시점에 로그인 목록을 초기화해야 하므로 직접 대입하지 말고 이 함수를 통해 바꿀 것
	UFUNCTION(BlueprintSetter, Category = "Game State")
	void SetGameState(EGameState NewState);

	//신전 사이클이 끝난 뒤 이동할 전투 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game State")
	TSoftObjectPtr<UWorld> CombatLevel;

	//전투상태로 전환하고 CombatLevel을 연다
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void TravelToCombat();
};
