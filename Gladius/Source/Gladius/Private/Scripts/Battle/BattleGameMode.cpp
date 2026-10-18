#include "Scripts/Battle/BattleGameMode.h"

ABattleGameMode::ABattleGameMode()
{
	//폰 없음. PlayerController는 그대로 생성되므로 카메라 Auto Activate와 키 입력은 계속 동작한다
	DefaultPawnClass = nullptr;
}
