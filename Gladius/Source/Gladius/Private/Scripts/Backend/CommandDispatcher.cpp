#include "Scripts/Backend/CommandDispatcher.h"

void UCommandDispatcher::Initialize()
{
	RegisterCommandHandlers();
}

void UCommandDispatcher::RegisterCommandHandlers()
{
	CommandHandlers.Add(ECommandType::Attack1, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleAttack1));
	CommandHandlers.Add(ECommandType::Attack2, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleAttack2));
	CommandHandlers.Add(ECommandType::Defend, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleDefend));
	CommandHandlers.Add(ECommandType::DodgeLeft, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleDodgeLeft));
	CommandHandlers.Add(ECommandType::DodgeRight, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleDodgeRight));
	CommandHandlers.Add(ECommandType::DodgeMid, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleDodgeMid));
	CommandHandlers.Add(ECommandType::Logout, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleLogout));

	CommandHandlers.Add(ECommandType::Enhance1, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleEnhance1));
	CommandHandlers.Add(ECommandType::Enhance2, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleEnhance2));
	CommandHandlers.Add(ECommandType::Enhance3, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleEnhance3));
	CommandHandlers.Add(ECommandType::Enhance4, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleEnhance4));
	CommandHandlers.Add(ECommandType::Talk, FCommandHandler::CreateUObject(this, &UCommandDispatcher::HandleTalk));
}

void UCommandDispatcher::Execute(const FParsedCommand& Command)
{
	if (IsCombatCommand(Command.CommandType))
	{
		//로그아웃 자체는 로그인을 트리거하는 명령이 아니므로 제외 (공격/방어/회피 3종만 자동 로그인 트리거)
		if (Command.CommandType != ECommandType::Logout)
		{
			TryAutoLogin(Command.UserId);
		}

		UpdateLogoutStatus(Command.UserId, true);
	}

	if (!IsCommandValidForCurrentState(Command.CommandType))
	{
		return; //현재 게임 상태(전투/신전)와 맞지 않는 명령어는 무시 (ex. 전투중 !강화, 신전에서 !공격)
	}

	if (const FCommandHandler* Handler = CommandHandlers.Find(Command.CommandType))
	{
		Handler->ExecuteIfBound(Command);
	}
}

bool UCommandDispatcher::IsCombatCommand(ECommandType CommandType) const
{
	switch (CommandType)
	{
	case ECommandType::Attack1:
	case ECommandType::Attack2:
	case ECommandType::Defend:
	case ECommandType::DodgeLeft:
	case ECommandType::DodgeRight:
	case ECommandType::DodgeMid:
	case ECommandType::Logout:
		return true;
	default:
		return false;
	}
}

bool UCommandDispatcher::IsCommandValidForCurrentState(ECommandType CommandType) const
{
	//GetGameStateDelegate가 아직 바인딩되지 않았다면(GameManager 연결 전) 기본값으로 신전상태 취급
	const EGameState CurrentState = GetGameStateDelegate.IsBound() ? GetGameStateDelegate.Execute() : EGameState::Temple;

	return IsCombatCommand(CommandType)
		? (CurrentState == EGameState::Combat)
		: (CurrentState == EGameState::Temple);
}

void UCommandDispatcher::TryAutoLogin(const FString& UserId)
{
	//GetGameStateDelegate가 바인딩되어있지 않거나(아직 GameManager 연결 전) 전투상태가 아니면 로그인 트리거 안함
	if (!GetGameStateDelegate.IsBound() || GetGameStateDelegate.Execute() != EGameState::Combat)
	{
		return;
	}

	if (LoggedInUsers.Contains(UserId))
	{
		return; //이미 로그인된 유저
	}

	if (LoggedInUsers.Num() >= MaxLoggedInUsers)
	{
		return; //최대 인원 초과로 로그인 불가
	}

	LoggedInUsers.Add(UserId);

	//실제 DB 조회/신규 생성/필드 스폰 로직은 이 델리게이트를 구독하는 쪽(플레이어 매니저 등)에서 처리
	OnUserLoginRequired.Broadcast(UserId);
}

void UCommandDispatcher::UpdateLogoutStatus(const FString& UserId, bool bInputCombatCommand)
{
	//TODO: 로그인 상태에서 전투 명령 미입력 턴을 누적시키고, 2턴 연속 미입력 시 다음 턴부터 로그아웃 처리
}

void UCommandDispatcher::ResetLoginForNewBattle()
{
	//TODO: 매 전투 시작마다 LoggedInUsers, MissedTurnCounts 초기화
}

// ===== 전투상태 명령 처리 함수 =====

void UCommandDispatcher::HandleAttack1(const FParsedCommand& Command)
{
	//TODO: 캐릭터 공격(1번) 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[공격 1] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleAttack2(const FParsedCommand& Command)
{
	//TODO: 캐릭터 공격(2번) 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[공격 2] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleDefend(const FParsedCommand& Command)
{
	//TODO: 캐릭터 방어 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[방어] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleDodgeLeft(const FParsedCommand& Command)
{
	//TODO: 캐릭터 좌측 회피 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[회피 좌] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleDodgeRight(const FParsedCommand& Command)
{
	//TODO: 캐릭터 우측 회피 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[회피 우] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleDodgeMid(const FParsedCommand& Command)
{
	//TODO: 캐릭터 중앙 회피 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[회피 중] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleLogout(const FParsedCommand& Command)
{
	//TODO: 로그아웃 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[로그아웃] 유저: %s"), *Command.UserId);
}

// ===== 신전상태 명령 처리 함수 =====

void UCommandDispatcher::HandleEnhance1(const FParsedCommand& Command)
{
	//TODO: 강화(1번) 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[강화 1] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleEnhance2(const FParsedCommand& Command)
{
	//TODO: 강화(2번) 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[강화 2] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleEnhance3(const FParsedCommand& Command)
{
	//TODO: 강화(3번) 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[강화 3] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleEnhance4(const FParsedCommand& Command)
{
	//TODO: 강화(4번) 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[강화 4] 유저: %s"), *Command.UserId);
}

void UCommandDispatcher::HandleTalk(const FParsedCommand& Command)
{
	//TODO: 신전 대화 로직 연결
	UE_LOG(LogTemp, Log, TEXT("[대화] 유저: %s | 내용: %s"), *Command.UserId, *Command.Argument);
}
