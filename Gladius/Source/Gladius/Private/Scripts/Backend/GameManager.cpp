#include "Scripts/Backend/GameManager.h"

AGameManager::AGameManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGameManager::SetGameState(EGameState NewState)
{
	//TODO: 신전 -> 전투로 전환되는 시점에는 CommandDispatcher::ResetLoginForNewBattle()을 호출해
	//이전 전투의 로그인 목록(LoggedInUsers)을 초기화해야 함 (CommandDispatcher 참조를 들고 있게 되면 연결)
	CurrentGameState = NewState;
}
