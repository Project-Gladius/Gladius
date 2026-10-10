#pragma once

#include "CoreMinimal.h"
#include "GladiusTypes.h"
#include "CommandDispatcher.generated.h"

// 명령어 하나를 처리하는 함수의 시그니처
DECLARE_DELEGATE_OneParam(FCommandHandler, const FParsedCommand&);

// 현재 game이 전투상태인지 신전상태인지 물어보는 델리게이트. AYouTubeChatManager가 AGameManager::GetGameState()에 바인딩해줌
DECLARE_DELEGATE_RetVal(EGameState, FGetGameStateDelegate);

// 전투 관련 명령어가 특정 유저에 의해 처음 입력되어 자동 로그인이 필요할 때 브로드캐스트되는 델리게이트
// 구독한 곳(예: 플레이어 매니저)에서 DB 조회/신규 생성 및 필드에 플레이어 스폰 로직을 실행하면 됨
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserLoginRequired, const FString& /*UserId*/);

// ===== 명령어별 실행 델리게이트 =====
// 외부(전투 로직, 캐릭터 스크립트 등)에서 구독해 실제 게임 동작을 처리하는 델리게이트.
// CommandType은 이미 델리게이트 종류로 구분되므로 넘기지 않고, 대신 명령의 두 번째 인자(!공격 1의 "1", !회피 좌의 "좌" 등)와
// 적용 대상 UserId만 넘겨서 외부에서 다루기 쉽게 함

// !공격 1, !공격 2 실행 시 브로드캐스트. Argument: 공격 번호("1" 또는 "2")
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttackCommand, const FString& /*UserId*/, const FString& /*Argument*/);

// !방어 실행 시 브로드캐스트. 인자를 받지 않는 명령이라 UserId만 전달됨
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDefendCommand, const FString& /*UserId*/);

// !회피 좌/우/중 실행 시 브로드캐스트. Argument: 방향("좌"/"우"/"중")
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDodgeCommand, const FString& /*UserId*/, const FString& /*Argument*/);

// !강화 1~4 실행 시 브로드캐스트. Username: 작성자 표시명, Argument: 강화 번호("1"~"4")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEnhanceCommand, const FString& /*UserId*/, const FString& /*Username*/, const FString& /*Argument*/);

// !대화 ... 실행 시 브로드캐스트. AI 쿼리(uid/username/message)에 필요한 정보를 함께 넘김
// CommentId: 채팅 고유 Id(uid), Username: 작성자 표시명(username), Argument: 대화 내용 전체(message)
DECLARE_MULTICAST_DELEGATE_FourParams(FOnTalkCommand, const FString& /*UserId*/, const FString& /*CommentId*/, const FString& /*Username*/, const FString& /*Argument*/);

// 파싱된 명령어(ECommandType)와 이를 처리할 함수를 map으로 연결해 실행시키는 클래스
UCLASS()
class GLADIUS_API UCommandDispatcher : public UObject
{
	GENERATED_BODY()

public:
	//CommandHandlers를 채움. NewObject로 생성한 직후 반드시 호출해야 함
	void Initialize();

	//파싱된 명령어를 실행 (로그인/로그아웃 판정 포함)
	void Execute(const FParsedCommand& Command);

	//AYouTubeChatManager가 AGameManager::GetGameState()를 바인딩해줘야 함
	FGetGameStateDelegate GetGameStateDelegate;

	//외부(플레이어 매니저 등)에서 구독해 로그인 시 DB 조회/신규 생성/필드 스폰 로직을 연결
	FOnUserLoginRequired OnUserLoginRequired;

	//외부(전투 로직 등)에서 구독해 공격 처리를 연결. Argument: "1" 또는 "2"
	FOnAttackCommand OnAttackCommand;

	//외부(전투 로직 등)에서 구독해 방어 처리를 연결
	FOnDefendCommand OnDefendCommand;

	//외부(전투 로직 등)에서 구독해 회피 처리를 연결. Argument: "좌"/"우"/"중"
	FOnDodgeCommand OnDodgeCommand;

	//외부(신전 로직 등)에서 구독해 강화 처리를 연결. Username: 작성자 표시명, Argument: "1"~"4"
	FOnEnhanceCommand OnEnhanceCommand;

	//외부(신전 로직 등)에서 구독해 대화(AI 쿼리) 처리를 연결. CommentId, Username, Argument(대화 내용 전체)를 받음
	FOnTalkCommand OnTalkCommand;

private:
	//명령어 종류 -> 처리 함수 매핑
	TMap<ECommandType, FCommandHandler> CommandHandlers;

	void RegisterCommandHandlers();

	// ===== 전투상태 명령 처리 함수 =====
	//공격(1/2), 회피(좌/우/중)는 각각 하나의 델리게이트로 묶여 있으므로 CommandHandlers에서 같은 함수를 공유함
	void BroadcastAttack(const FParsedCommand& Command);
	void BroadcastDefend(const FParsedCommand& Command);
	void BroadcastDodge(const FParsedCommand& Command);
	void HandleLogout(const FParsedCommand& Command);

	// ===== 신전상태 명령 처리 함수 =====
	//강화(1~4)도 하나의 델리게이트로 묶여 있으므로 CommandHandlers에서 같은 함수를 공유함
	void BroadcastEnhance(const FParsedCommand& Command);
	void BroadcastTalk(const FParsedCommand& Command);

	// ===== 로그인 / 로그아웃 =====

	//동시 로그인 가능한 최대 인원
	static constexpr int32 MaxLoggedInUsers = 30;

	//현재 로그인된 유저 목록. 매 전투 시작마다 초기화될 예정
	TSet<FString> LoggedInUsers;

	//로그인 상태에서 전투 명령을 입력하지 않은 연속 턴 수. 2가 되면 다음 턴부터 로그아웃 처리될 예정
	TMap<FString, int32> MissedTurnCounts;

	bool IsCombatCommand(ECommandType CommandType) const;

	//GetGameStateDelegate로 받아온 현재 게임 상태에서 CommandType이 유효한(실행 가능한) 명령어인지 검사
	//(전투상태면 전투 명령어만, 신전상태면 신전 명령어만 유효)
	bool IsCommandValidForCurrentState(ECommandType CommandType) const;

	//전투 관련 명령어(!공격, !방어, !회피)가 특정 유저에 의해 처음 입력되었을 때, game이 전투상태이고
	//최대 인원(30명) 이내라면 OnUserLoginRequired를 브로드캐스트해 자동 로그인 처리
	void TryAutoLogin(const FString& UserId);

	//로그인 상태에서 2턴 연속 전투 명령 미입력 시 다음 턴부터 로그아웃 처리
	//TODO: 턴 갱신 로직 구현 예정
	void UpdateLogoutStatus(const FString& UserId, bool bInputCombatCommand);

public:
	//매 전투 시작 시 로그인 목록을 초기화
	//TODO: 구현 예정
	void ResetLoginForNewBattle();
};
