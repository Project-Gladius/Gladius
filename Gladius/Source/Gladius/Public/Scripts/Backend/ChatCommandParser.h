#pragma once

#include "CoreMinimal.h"
#include "GladiusTypes.h"
#include "Scripts/Battle/BattleUnit.h"

// 유튜브 채팅 한 줄을 게임 명령어로 파싱하는 정적 유틸리티 클래스
// 형식: !<명령어><공백?><내용>  (ex. "!공격 1", "! 공격1", "!  공격  1" 모두 허용)
// 명령어 형식이 아니거나 알 수 없는 명령어는 전부 무시 대상이므로 TryParse가 false를 반환함
class GLADIUS_API FChatCommandParser
{
public:
	static bool TryParse(const FString& UserId, const FString& RawContent, FParsedCommand& OutCommand);

static bool TryGetBattleAction(ECommandType Type, EBattleAction& OutAction);

private:
	// Keyword 뒤에 남은 문자열(Remainder)을 보고 세부 CommandType과 Argument를 채움
	static bool ResolveCommand(const FString& Keyword, const FString& Remainder, FParsedCommand& OutCommand);
};
