#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleManager.generated.h"

class ABattleUnit;
class UTextRenderComponent;

//필드 영역 — 좌 / 중 / 우
UENUM(BlueprintType)
enum class EBattleLane : uint8
{
	Left,
	Mid,
	Right
};

//플레이어가 한 턴에 고를 수 있는 행동
UENUM(BlueprintType)
enum class EBattleAction : uint8
{
	Idle,
	Attack1,
	Attack2,
	AvoidL,
	AvoidM,
	AvoidR,
	Defense
};

//턴의 진행 단계
UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	//시청자가 화면을 보고 채팅을 치는 시간
	Input,
	//카운트다운은 끝났지만 늦게 도착하는 채팅을 계속 받는 시간
	Collect,
	//판정 결과를 보여주는 시간. 입력을 받지 않는다
	Resolve
};

//전투가 끝나도 남는 유저 기록. 이 구조체가 그대로 JSON 파일이 된다.
//능력치를 추가하려면 여기에 UPROPERTY를 넣기만 하면 저장 파일에 자동으로 따라온다
USTRUCT()
struct FPlayerRecord
{
	GENERATED_BODY()

	UPROPERTY() FString UserId;
	UPROPERTY() FString DisplayName;

	//공격력 강화 단계
	UPROPERTY() int32 AttackBonus = 0;

	//최대 체력 강화 단계
	UPROPERTY() int32 MaxHPBonus = 0;
};

//저장 파일의 최상위 형태. { "records": [ ... ] }
USTRUCT()
struct FPlayerRecordFile
{
	GENERATED_BODY()

	UPROPERTY() TArray<FPlayerRecord> Records;
};

USTRUCT()
struct FBattlePlayer
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() int32 HP = 0;

	//참가할 때 기록에서 계산해 넣는 값들. 매 판정마다 다시 조회하지 않으려고 들고 있는다
	UPROPERTY() int32 MaxHP = 0;
	UPROPERTY() int32 AttackBonus = 0;
	UPROPERTY() EBattleLane Lane = EBattleLane::Mid;

	//참가할 때 한 번 받는 고정 자리 번호. 레인을 옮겨도 같은 번호를 쓴다
	UPROPERTY() int32 SlotIndex = 0;

	UPROPERTY() EBattleAction PendingAction = EBattleAction::Idle;
	UPROPERTY() TObjectPtr<ABattleUnit> Unit;

	//0보다 크면 사망 상태. 매 턴 줄어들고 0이 되면 부활한다
	UPROPERTY() int32 ReviveCountdown = 0;
};

UCLASS()
class GLADIUS_API ABattleManager : public AActor
{
	GENERATED_BODY()

public:
	ABattleManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UTextRenderComponent> StatusText;

	// ---- 투기장 배치 ----

	//이 액터의 위치가 투기장 중심. 보스는 항상 +X 끝에 선다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float ArenaRadius = 1000.f;

	//가운데 레인 기준 좌/우 레인이 벌어지는 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float LaneAngle = 35.f;

	//레인 안 격자의 가로 칸 수. 세로 줄 수는 MaxPlayers에서 자동으로 나온다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	int32 SlotColumns = 6;

	//DUMMY_MODEL이 높이 16.8cm로 임포트돼 있어 사람 크기로 올리는 보정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float UnitScale = 10.f;

	//UnitScale에 곱해지는 보스 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float BossScale = 2.f;

	// ---- 규칙 ----

	//동시 참여 정원. 레인마다 이만큼의 자리가 준비된다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 MaxPlayers = 30;

	//사망 후 부활까지 기다리는 턴 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 ReviveTurns = 3;

	//시청자에게 보이는 입력 시간. 방송 지연이 있으므로 짧으면 반응이 안 온다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float InputSeconds = 30.f;

	//카운트다운이 끝난 뒤 늦게 도착하는 채팅을 더 받는 시간.
	//방송 지연 + API 폴링 간격보다 길어야 한다. 로그의 실측 지연을 보고 맞출 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float CollectSeconds = 6.f;

	//판정 결과를 보여주는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float ResolveSeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 PlayerMaxHP = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 EnemyMaxHP = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 Attack1Damage = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 Attack2Damage = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 EnemyDamage = 20;

	//방어했을 때 받는 데미지 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float DefenseMultiplier = 0.5f;

	//최대 체력 강화 한 단계당 늘어나는 HP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 HPPerBonus = 10;

	//공격력 강화 한 단계당 늘어나는 피해
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 DamagePerBonus = 2;

	//채팅이 붙기 전까지 더미 유저들이 매 턴 무작위 행동을 고르게 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Debug")
	bool bRandomizeTestPlayers = true;

	// ---- 상태 ----

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 EnemyHP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 TurnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	EBattlePhase Phase = EBattlePhase::Resolve;

	//Input과 Collect 단계에서만 입력을 받는다
	UFUNCTION(BlueprintPure, Category = "Battle|State")
	bool IsAcceptingInput() const;

	// ---- 외부 진입점 ----

	//채팅 쪽에서 호출하는 유일한 함수.
	//SentAt은 YouTube가 찍은 작성 시각으로, 아직 쓰지 않지만 계약에 미리 넣어둔다.
	//처리되면 true, 명령어가 아니거나 정원 초과면 false
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool SubmitChat(const FString& UserId, const FString& DisplayName,
		const FString& Text, const FDateTime& SentAt);

	//이미 파싱된 행동을 넣을 때. SubmitChat이 최종적으로 이걸 호출한다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool SubmitAction(const FString& UserId, const FString& DisplayName, EBattleAction Action);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartBattle();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StopBattle();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Battle|Debug")
	void RunSelfTest();

	//유저 기록 저장/불러오기. 저장 위치는 Saved/PlayerData.json.
	//턴이 끝날 때와 종료 시에 자동으로 불린다. 상점 상호작용처럼
	//기록이 바뀌는 다른 지점이 생기면 거기서도 호출하면 된다
	UFUNCTION(BlueprintCallable, Category = "Battle|Save")
	void SaveRecords();

	UFUNCTION(BlueprintCallable, Category = "Battle|Save")
	void LoadRecords();

	static FString SaveFilePath();

	// ---- 순수 함수 (상태 없음, 자체 테스트 대상) ----

	//채팅 한 줄을 행동으로. 앞의 '!'는 있어도 되고 없어도 된다
	static bool ParseCommand(const FString& Text, EBattleAction& OutAction);

	//회피 행동이면 그 영역으로 이동, 아니면 제자리
	static EBattleLane ApplyMove(EBattleAction Action, EBattleLane Current);

	//피격 시 실제로 깎이는 HP
	static int32 DamageToPlayer(EBattleAction Action, bool bHit, int32 Base, float DefMul);

	//비어 있는 가장 앞 자리 번호. 만석이면 INDEX_NONE
	static int32 AllocateSlotIndex(const TSet<int32>& Used, int32 MaxSlots);

	//강화 단계를 실제 수치로 바꾼다
	int32 MaxHPFor(const FPlayerRecord& Record) const;

	// ---- 좌표 ----

	FVector BossLocation() const;
	FVector SlotLocation(EBattleLane Lane, int32 SlotIndex) const;

private:
	void StartTurn();
	void BeginCollect();
	void ResolveTurn();

	void SpawnBoss();
	void UpdateVisuals();
	void BindActionKeys();

	//투기장 테두리와 레인 경계선. 표시만 하고 통행은 막지 않는다
	void DrawArenaGuides() const;

	//더미 유저 한 명을 무작위 행동으로 참가시킨다
	void AddTestPlayer();

	// ponytail: 성장 규칙이 미정이라 디버그 키로만 올린다.
	// 실제 규칙이 정해지면 이 함수를 지우고 그 자리에 넣으면 된다
	void GrantBonusToAll();

	//적 행동 스텁 — 규칙이 정해지면 이 함수만 통째로 교체
	EBattleLane ChooseEnemyLane() const;

	//지금 전투 중인 참여자
	UPROPERTY() TMap<FString, FBattlePlayer> Players;

	//전투 참여 여부와 무관하게 남는 유저 기록. 저장 대상은 이쪽이다
	UPROPERTY() TMap<FString, FPlayerRecord> Records;

	UPROPERTY() TObjectPtr<ABattleUnit> BossUnit;

	//한 번에 한 단계만 도므로 타이머 하나를 돌려 쓴다
	FTimerHandle PhaseTimerHandle;

	//이번 턴에 보스가 칠 레인. 턴이 시작될 때 정해져 입력 시간 내내 예고된다
	EBattleLane TelegraphedLane = EBattleLane::Mid;

	//Resolve 단계에 띄울 직전 턴 요약. 디버그 표시용이라 문자열로 들고 있는다
	FString LastTurnSummary;
};
