#pragma once

#include "CoreMinimal.h"
#include "GladiusTypes.h"
#include "CommandDispatcher.generated.h"

// 명령어 하나를 처리하는 함수의 시그니처
DECLARE_DELEGATE_OneParam(FCommandHandler, const FParsedCommand&);

// 현재 game이 전투상태인지 물어보는 델리게이트. AYouTubeChatManager(추후 GameManager)에서 바인딩해줘야 함
DECLARE_DELEGATE_RetVal(bool, FIsInCombatStateDelegate);

// 전투 관련 명령어가 특정 유저에 의해 처음 입력되어 자동 로그인이 필요할 때 브로드캐스트되는 델리게이트
// 구독한 곳(예: 플레이어 매니저)에서 DB 조회/신규 생성 및 필드에 플레이어 스폰 로직을 실행하면 됨
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserLoginRequired, const FString& /*UserId*/);

// 파싱된 명령어(ECommandType)와 이를 처리할 함수를 map으로 연결해 실행시키는 클래스
// TODO: 전투상태/신전상태에 따라 사용 가능한 명령어를 구분하는 로직은 게임 매니저가 생기면 추가 예정
UCLASS()
class GLADIUS_API UCommandDispatcher : public UObject
{
	GENERATED_BODY()

public:
	//CommandHandlers를 채움. NewObject로 생성한 직후 반드시 호출해야 함
	void Initialize();

	//파싱된 명령어를 실행 (로그인/로그아웃 판정 포함)
	void Execute(const FParsedCommand& Command);

	//TODO: 지금은 AYouTubeChatManager의 임시 bool을 바인딩하지만, 추후 GameManager의 실제 전투/신전 상태 조회 함수로 교체 예정
	FIsInCombatStateDelegate IsInCombatStateDelegate;

	//외부(플레이어 매니저 등)에서 구독해 로그인 시 DB 조회/신규 생성/필드 스폰 로직을 연결
	FOnUserLoginRequired OnUserLoginRequired;

private:
	//명령어 종류 -> 처리 함수 매핑
	TMap<ECommandType, FCommandHandler> CommandHandlers;

	void RegisterCommandHandlers();

	// ===== 전투상태 명령 처리 함수 (TODO: 실제 전투 로직 연결 필요) =====
	void HandleAttack1(const FParsedCommand& Command);
	void HandleAttack2(const FParsedCommand& Command);
	void HandleDefend(const FParsedCommand& Command);
	void HandleDodgeLeft(const FParsedCommand& Command);
	void HandleDodgeRight(const FParsedCommand& Command);
	void HandleDodgeMid(const FParsedCommand& Command);
	void HandleLogout(const FParsedCommand& Command);

	// ===== 신전상태 명령 처리 함수 (TODO: 실제 신전 로직 연결 필요) =====
	void HandleEnhance1(const FParsedCommand& Command);
	void HandleEnhance2(const FParsedCommand& Command);
	void HandleEnhance3(const FParsedCommand& Command);
	void HandleEnhance4(const FParsedCommand& Command);
	void HandleTalk(const FParsedCommand& Command);

	// ===== 로그인 / 로그아웃 =====

	//동시 로그인 가능한 최대 인원
	static constexpr int32 MaxLoggedInUsers = 30;

	//현재 로그인된 유저 목록. 매 전투 시작마다 초기화될 예정
	TSet<FString> LoggedInUsers;

	//로그인 상태에서 전투 명령을 입력하지 않은 연속 턴 수. 2가 되면 다음 턴부터 로그아웃 처리될 예정
	TMap<FString, int32> MissedTurnCounts;

	bool IsCombatCommand(ECommandType CommandType) const;

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
