#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Battle/BattleBalance.h"
#include "BattleManager.generated.h"

class ABattleUnit;
class UTextRenderComponent;

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

//턴의 진행 단계. 기획서의 4단계와 그대로 대응한다
UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	//보스 행동 예고. 이 시간의 입력도 이번 턴 행동으로 유효하다
	Telegraph,
	//플레이어 행동 선택 카운트다운
	Input,
	//카운트다운은 끝났지만 늦게 도착하는 채팅을 계속 받는 대기 시간
	Collect,
	//회피 이동 연출. 여기부터는 입력을 받지 않는다
	Move,
	//플레이어 공격과 보스 공격 모션
	BossAction,
	//결과 표시
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

	//신전에서 사는 '강화' 능력치들. 게임 오버 시 일정 비율을 잃는다
	UPROPERTY() int32 MaxHPBonus = 0;
	UPROPERTY() int32 DamageBonus = 0;
	UPROPERTY() int32 CritChanceBonus = 0;
	UPROPERTY() int32 CritDamageBonus = 0;

	//방송 전체 누적. 게임 단위로 초기화하지 않는다
	UPROPERTY() int64 TotalDamageDealt = 0;
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

	//소수 유지 모드가 있어 실수다. 피해 계산에 그대로 더해진다
	UPROPERTY() float Fever = 0.f;

	//연속으로 회피한 턴 수. 회피가 아닌 행동을 하면 0으로 돌아간다
	UPROPERTY() int32 EvadeStreak = 0;

	//참가할 때 기록에서 계산해 넣는 값들. 매 판정마다 다시 조회하지 않으려고 들고 있는다
	UPROPERTY() int32 MaxHP = 0;
	UPROPERTY() int32 DamageBonus = 0;
	//신전 강화로 얻은 치명타. 공격별 기본 치명타에 더해진다
	UPROPERTY() float CritChanceBonus = 0.f;
	UPROPERTY() float CritDamageBonus = 0.f;

	UPROPERTY() EBattleLane Lane = EBattleLane::Mid;

	//이번 턴에 서야 할 자리. 매 턴 구역 인원에 맞춰 다시 배분된다
	UPROPERTY() FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY() EBattleAction PendingAction = EBattleAction::Idle;

	//이번 턴에 전투 명령어가 들어왔는지. 무입력 이탈 판정에 쓴다
	UPROPERTY() bool bActedThisTurn = false;

	//연속으로 입력이 없었던 턴 수
	UPROPERTY() int32 IdleTurns = 0;

	UPROPERTY() TObjectPtr<ABattleUnit> Unit;

	//0보다 크면 게임 오버 상태. 매 턴 줄어들고 0이 되면 부활한다
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

#if WITH_EDITOR
	//프로퍼티를 바꾸거나 액터를 옮길 때마다 불린다. 자리 미리보기를 다시 그린다
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UTextRenderComponent> StatusText;

	// ---- 투기장 배치 ----

	//이 액터의 위치가 투기장 중심. 보스는 항상 +X 끝에 선다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float ArenaRadius = 1600.f;

	//가운데 레인 기준 좌/우 레인이 벌어지는 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float LaneAngle = 35.f;

	//레인 안 격자의 가로 칸 수. 세로 줄 수는 MaxPlayers에서 자동으로 나온다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	int32 SlotColumns = 6;

	//아래 넷은 전부 ArenaRadius에 곱해지는 비율. 격자 모양을 결정한다.
	//bPreviewSlots를 켜두면 값을 바꾸는 즉시 뷰포트의 점들이 움직인다

	//보스가 중심에서 떨어진 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float BossDistanceRatio = 0.8f;

	//보스에서 첫 줄까지의 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float LaneDistanceRatio = 0.7f;

	//줄과 줄 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float RowSpacingRatio = 0.12f;

	//LaneAngle 중 격자가 실제로 쓰는 비율. 1이면 레인 폭을 꽉 채운다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float ColumnSpreadRatio = 0.8f;

	// ---- 유닛 ----

	//플레이어와 보스로 쓸 블루프린트. 메시·크기·이름표 위치·이름표 위젯이
	//전부 이 블루프린트 안에 있으므로 코드를 고칠 일이 없다.
	//비워두면 C++ 기본 클래스를 쓰는데, 메시가 없어 보이지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Unit")
	TSubclassOf<ABattleUnit> PlayerUnitClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Unit")
	TSubclassOf<ABattleUnit> BossUnitClass;

	// ---- 진행 ----

	//동시 참여 정원. 레인마다 이만큼의 자리가 준비된다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 MaxPlayers = 30;

	//게임 오버 후 부활까지 기다리는 턴 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 ReviveTurns = 3;

	//부활할 때 잃는 강화 능력치 비율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float GameOverLoss = 0.5f;

	//한 게임을 이루는 턴 수. 이만큼 버티면 무승부로 끝난다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings (밸런스 파일이 덮어씀)")
	int32 Turns = 10;

	//전투 명령어를 이만큼의 턴 동안 연속으로 안 치면 전장에서 빠진다.
	//능력치는 기록에 남고 자리와 인원수만 반환된다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 IdleTurnsBeforeKick = 2;

	//보스 행동 예고 시간. 보스 모션 길이에 맞춰야 하므로 임시값이다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float TelegraphSeconds = 5.f;

	//플레이어 행동 선택 카운트다운. 방송 지연이 있으므로 짧으면 반응이 안 온다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float PlayerSelectCountdown = 90.f;

	//카운트다운이 끝난 뒤 늦게 도착하는 채팅을 더 받는 시간.
	//방송 지연 + API 폴링 간격보다 길어야 한다. 로그의 실측 지연을 보고 맞출 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float CollectSeconds = 6.f;

	//회피 이동에 쓰는 시간. 이동 모션 길이에 맞춘다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float MoveSeconds = 1.5f;

	//플레이어 공격과 보스 공격 모션에 쓰는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float BossActionSeconds = 1.5f;

	//판정 결과를 보여주는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float ResolveSeconds = 4.f;

	// ---- 전투 수치 ----
	//
	//'(밸런스 파일이 덮어씀)'이 붙은 항목은 게임이 시작될 때 Balancing/Data 의
	//.json 값으로 교체된다. 여기서 고쳐도 반영되지 않으므로 수치는 그 파일에서 고칠 것.
	//파일을 읽지 못했을 때만 아래 값이 쓰인다

	//강화 없는 플레이어의 기본 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 PlayerBaseHP = 5;

	//강화 한 단계당 오르는 양
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 HPPerBonus = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 DamagePerBonus = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float CritChancePerBonus = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float CritDamagePerBonus = 0.1f;

	//공격 시 오르는 피버 (파일의 attackFeverGain)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float FeverIncrement = 5.f;

	//방어 시 오르는 피버 (파일의 defenseFeverGain).
	// ponytail: 방어 행동 자체를 조건으로 본다. '실제로 피격을 막아야 오르는지'는 기획서 미정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float DefenseFeverGain = 5.f;

	//피해 계산에서 피버에 곱하는 계수 (파일의 feverDamageCoefficient)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float FeverDamageCoefficient = 1.f;

	//피버 상한. 0이면 무제한 (파일의 feverCap: null)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float FeverCap = 0.f;

	//연속 회피 감소. N회 연속 회피하면 피버가 비율만큼 줄어든다.
	//회피가 아닌 행동을 하면 연속 횟수가 0으로 돌아간다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 EvadeStreakN = 2;

	//0~1 비율. 1이면 피버가 0이 된다 (파일의 evadeReduction)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float EvadeFeverReduction = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	EEvadeRepeatMode EvadeRepeatMode = EEvadeRepeatMode::EveryAfter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	EFeverRounding FeverRounding = EFeverRounding::Floor;

	//서 있는 구역으로 다시 회피하는 것을 허용할지 (파일의 allowStayEvade)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	bool bAllowStayEvade = true;

	//방어 시 받는 피해 비율. 올림해서 적용한다 (파일의 defenseDamageRatio)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float DefenseDamageRatio = 0.5f;

	//치명타 확률 상한 (파일의 critChanceCap)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float CritChanceCap = 1.f;

	// ponytail: 원래 직업군별로 갖는 값이다. 직업군이 붙으면 그쪽으로 옮긴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	FAttackDef Attack1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	FAttackDef Attack2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 EnemyMaxHP = 100;

	// ---- 밸런스 파일 ----

	//Balancing/Data 의 abilities / enhancements / bosses .json 을 읽어
	//위 수치들을 덮어쓴다. 파일에 문제가 있으면 읽지 않고 에디터 값을 그대로 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance")
	bool bUseBalanceFiles = true;

	//프로젝트 폴더(.uproject가 있는 곳) 기준 상대 경로.
	//저장소에서는 Gladius/ 바깥에 Balancing/ 이 있어 한 단계 올라간다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Balance")
	FString BalanceDataDir = TEXT("../Balancing/Data");

	//채팅이 붙기 전까지 더미 유저들이 매 턴 무작위 행동을 고르게 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Debug")
	bool bRandomizeTestPlayers = true;

	//에디터에서 자리 배치를 점으로 미리 보여준다. Play 없이 격자를 맞출 때 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Debug")
	bool bPreviewSlots = true;

	// ---- 상태 ----

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 EnemyHP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 TurnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	EBattlePhase Phase = EBattlePhase::Resolve;

	//이번 턴에 보스가 할 행동. 턴이 시작될 때 정해져 입력 시간 내내 예고된다
	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	FBossAction TelegraphedAction;

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

	//현재 격자 좌표에 BattleSpot을 만들어 준다. 백지에서 시작하지 않도록 하는 용도라
	//이미 스팟이 있으면 아무것도 하지 않는다
	UFUNCTION(CallInEditor, Category = "Battle|Arena")
	void GenerateSpotsFromGrid();

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

	//세 회피 중 하나인지. 연속 회피 판정과 제자리 회피 금지에 쓴다
	static bool IsEvade(EBattleAction Action);

	//후보 자리 중 Count개를 서로 최대한 멀리 떨어지게 고른다.
	//첫 자리는 Anchor(보스)에 가장 가까운 곳이고, 그다음부터는
	//이미 고른 자리들에서 가장 먼 곳을 하나씩 더한다.
	//자리가 인원보다 적으면 앞에서부터 다시 쓴다(겹침)
	static void ChooseSpreadSpots(const TArray<FVector>& Candidates, int32 Count,
		const FVector& Anchor, TArray<int32>& OutIndices);

	float ResistanceMultiplier(EResistance Resistance) const;

	//이동 후 플레이어 구역과 공격 속성으로 내성 배율을 구한다 (기획서 2절)
	float ZoneResistMultiplier(const FBossAction& Action, EBattleLane Lane, EDamageType Type) const;

	//로그와 표시판에 쓸 피격 구역 문자열
	static FString ZonesLabel(const FBossAction& Action);

	//시뮬레이터와 같은 식: (기본 + 강화 + 피버×계수) * 내성 * (1 + 치명타 시 치명타 피해).
	//강화 피해는 시뮬레이터가 기본 피해에 더해 넣는 것과 같은 자리다
	static int32 DamageToBoss(int32 BaseDamage, int32 DamageBonus, float Fever, float FeverCoefficient,
		float Resist, bool bCrit, float CritDamage);

	//회피한 턴의 피버. EvadeStreak은 이번 회피까지 센 값을 넘긴다.
	//기준에 닿지 않았으면 그대로 돌려준다
	static float DecayFever(float Fever, int32 EvadeStreak, int32 StreakN,
		float Reduction, EEvadeRepeatMode Repeat, EFeverRounding Rounding);

	//구역 피해 0이면 안 맞는다. 방어는 비율을 곱한 뒤 올림, 그 뒤 타격 횟수만큼 곱한다
	static int32 DamageToPlayer(EBattleAction Action, int32 ZoneDamage, int32 HitCount, float DefenseRatio);

	//강화 단계를 실제 수치로 바꾼다
	int32 MaxHPFor(const FPlayerRecord& Record) const;

	// ---- 좌표 ----

	FVector BossLocation() const;

	//스팟과 무관한 순수 격자 계산. 스팟을 만들어 줄 때와 미리보기에 쓴다
	FVector GridSlotLocation(EBattleLane Lane, int32 SlotIndex) const;

private:
	//Telegraph → Input → Collect → Move → BossAction → Resolve 순으로 타이머가 이어진다
	void StartTurn();
	void BeginInput();
	void BeginCollect();
	//회피 이동을 반영하고 유닛을 새 자리로 보낸다
	void BeginMove();
	//피해를 계산하고 공격 모션을 띄운다
	void ResolveTurn();
	//결과를 반영해 이름표를 갱신하고 종료 조건을 본다
	void EndTurn();

	//구역별 인원에 맞춰 설 자리를 다시 배분한다
	void AssignSlots();

	//전장에서 빼되 기록은 남긴다
	void RemovePlayer(const FString& UserId);

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

	//기록의 강화 수치를 전투용 스냅샷에 반영한다
	void ApplyRecordToPlayer(const FPlayerRecord& Record, FBattlePlayer& Player) const;

	//밸런스 파일을 읽어 위 수치들에 반영한다. 실패하면 아무것도 바꾸지 않는다
	void ApplyBalanceFiles();

	//BalanceDataDir을 절대 경로로
	FString BalanceFolderPath() const;

	//이번 턴 보스 행동. 패턴이 끝났으면 새 패턴을 먼저 뽑는다
	FBossAction NextBossAction();

	//남은 턴에 들어가는 패턴 중 가중 랜덤으로 하나. 못 고르면 nullptr.
	//PreviousId는 직전에 끝난 패턴 (연속 재선택 금지에 쓴다)
	const FBossPattern* ChoosePattern(int32 TurnsLeft, const FString& PreviousId) const;

	//밸런스 파일 내용. bLoaded가 false면 에디터 값으로 돈다
	FBattleBalance Balance;

	//진행 중인 패턴과 그 안에서의 위치. 패턴이 끝나야 다음 패턴을 뽑는다
	FString CurrentPatternId;
	int32 PatternStep = 0;

	//레벨에 놓인 BattleSpot을 전부 찾아 구역별로 모으고 보스에서 가까운 순으로 정렬한다
	void GatherSpots();

	//구역별 자리 좌표. 비어 있으면 SlotLocation이 격자 계산으로 넘어간다.
	//FVector라 GC 대상이 아니어서 UPROPERTY가 필요 없다
	TArray<FVector> Spots[3];

	//지금 전투 중인 참여자
	UPROPERTY() TMap<FString, FBattlePlayer> Players;

	//전투 참여 여부와 무관하게 남는 유저 기록. 저장 대상은 이쪽이다
	UPROPERTY() TMap<FString, FPlayerRecord> Records;

	UPROPERTY() TObjectPtr<ABattleUnit> BossUnit;

	//한 번에 한 단계만 도므로 타이머 하나를 돌려 쓴다
	FTimerHandle PhaseTimerHandle;

	//Resolve 단계에 띄울 직전 턴 요약. 디버그 표시용이라 문자열로 들고 있는다
	FString LastTurnSummary;
};
