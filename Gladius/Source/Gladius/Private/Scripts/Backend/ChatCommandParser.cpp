#include "Scripts/Backend/ChatCommandParser.h"

namespace
{
	// "로그인"과 "로그아웃"은 앞 두 글자가 같지만, 셋째 글자부터 달라 서로의 접두어가 아니므로 검사 순서는 결과에 영향을 주지 않음
	const TArray<FString>& GetKnownKeywords()
	{
		static const TArray<FString> Keywords = {
			TEXT("공격"), TEXT("방어"), TEXT("회피"), TEXT("로그아웃"), TEXT("로그인"), TEXT("강화"), TEXT("대화"), TEXT("전투")
		};
		return Keywords;
	}
}

bool FChatCommandParser::TryParse(const FString& UserId, const FString& RawContent, FParsedCommand& OutCommand)
{
	const FString Trimmed = RawContent.TrimStartAndEnd();

	if (!Trimmed.StartsWith(TEXT("!")))
	{
		return false; //명령어가 아닌 채팅은 전부 무시
	}

	//'!' 뒤 공백 유무 모두 허용
	const FString Body = Trimmed.Mid(1).TrimStart();

	for (const FString& Keyword : GetKnownKeywords())
	{
		if (!Body.StartsWith(Keyword))
		{
			continue;
		}

		const FString Remainder = Body.Mid(Keyword.Len()).TrimStartAndEnd();

		FParsedCommand Result;
		Result.UserId = UserId;
		Result.Argument = Remainder; //명령어 뒤에 붙은 인자(ex. "1", "좌", 대화 내용)를 그대로 보관. CommandDispatcher가 외부 델리게이트로 넘길 때 사용

		if (!ResolveCommand(Keyword, Remainder, Result))
		{
			return false; //키워드는 맞지만 뒤에 붙은 내용이 알 수 없는 경우
		}

		OutCommand = Result;
		return true;
	}

	return false; //알 수 없는 명령어
}

bool FChatCommandParser::ResolveCommand(const FString& Keyword, const FString& Remainder, FParsedCommand& OutCommand)
{
	if (Keyword == TEXT("공격"))
	{
		if (Remainder == TEXT("1")) { OutCommand.CommandType = ECommandType::Attack1; return true; }
		if (Remainder == TEXT("2")) { OutCommand.CommandType = ECommandType::Attack2; return true; }
		return false;
	}

	if (Keyword == TEXT("방어"))
	{
		if (!Remainder.IsEmpty()) return false; //인자를 받지 않는 명령어라 뒤에 뭔가 더 붙으면 실패
		OutCommand.CommandType = ECommandType::Defend;
		return true;
	}

	if (Keyword == TEXT("회피"))
	{
		if (Remainder == TEXT("좌")) { OutCommand.CommandType = ECommandType::DodgeLeft; return true; }
		if (Remainder == TEXT("우")) { OutCommand.CommandType = ECommandType::DodgeRight; return true; }
		if (Remainder == TEXT("중")) { OutCommand.CommandType = ECommandType::DodgeMid; return true; }
		return false;
	}

	if (Keyword == TEXT("로그아웃"))
	{
		if (!Remainder.IsEmpty()) return false; //인자를 받지 않는 명령어라 뒤에 뭔가 더 붙으면 실패
		OutCommand.CommandType = ECommandType::Logout;
		return true;
	}

	if (Keyword == TEXT("로그인"))
	{
		//캐릭터 이름(글라디우스/세라)이 있어야 함. 이름이 맞는지는 BattleManager가 밸런스 데이터로 확인
		if (Remainder.IsEmpty()) return false;
		OutCommand.CommandType = ECommandType::Login;
		return true;
	}

	if (Keyword == TEXT("강화"))
	{
		if (Remainder == TEXT("1")) { OutCommand.CommandType = ECommandType::Enhance1; return true; }
		if (Remainder == TEXT("2")) { OutCommand.CommandType = ECommandType::Enhance2; return true; }
		if (Remainder == TEXT("3")) { OutCommand.CommandType = ECommandType::Enhance3; return true; }
		if (Remainder == TEXT("4")) { OutCommand.CommandType = ECommandType::Enhance4; return true; }
		return false;
	}

	if (Keyword == TEXT("전투"))
	{
		if (!Remainder.IsEmpty()) return false;
		OutCommand.CommandType = ECommandType::TravelToCombat;
		return true;
	}

	if (Keyword == TEXT("대화"))
	{
		if (Remainder.IsEmpty()) return false;
		//대화는 뒤에 남은 문자열 전체를 인자로 사용 (Argument는 TryParse에서 이미 채워둠)
		OutCommand.CommandType = ECommandType::Talk;
		return true;
	}

	return false;
}

bool FChatCommandParser::TryGetBattleAction(ECommandType Type, EBattleAction& OutAction)
{
 switch (Type)
 {
 case ECommandType::Attack1: OutAction = EBattleAction::Attack1; return true;
 case ECommandType::Attack2: OutAction = EBattleAction::Attack2; return true;
 case ECommandType::Defend: OutAction = EBattleAction::Defense; return true;
 case ECommandType::DodgeLeft: OutAction = EBattleAction::AvoidL; return true;
 case ECommandType::DodgeMid: OutAction = EBattleAction::AvoidM; return true;
 case ECommandType::DodgeRight: OutAction = EBattleAction::AvoidR; return true;
 default: return false;
 }
}
