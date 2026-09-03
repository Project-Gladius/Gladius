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
	//유저 행동 및 보스 모션. 입력을 받지 않는다
	Resolve
};

//공격의 속성. 보스는 속성별로 다른 내성을 가진다
UENUM(BlueprintType)
enum class EDamageType : uint8
{
	Physical,
	Magical
};

//보스가 특정 행동 중에 갖는 내성. 실제 배율은 ResistanceMultiplier 참조
UENUM(BlueprintType)
enum class EResistance : uint8
{
	Enduring,	// 0.5
	Normal,		// 1.0
	Vulnerable	// 1.5
};

//보스 행동이 때리는 구역.
//Random은 '행동을 정할 때 무작위로 고른다'는 뜻이라, 예고 시점에
//ResolveHitArea로 구체 범위가 확정된다. 예고 이후에는 나타나지 않는다
UENUM(BlueprintType)
enum class EHitArea : uint8
{
	None,
	Left,
	Mid,
	Right,
	LeftMid,
	MidRight,
	LeftRight,
	All,
	Random
};

//보스의 한 가지 행동. 패턴은 이 행동들의 조합이 된다.
//지금은 ChooseBossAction이 무작위로 만들어내지만, 패턴이 정해지면
//데이터로 채워 넣기만 하면 된다
USTRUCT(BlueprintType)
struct FBossAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Damage = 1;

	//이 행동을 하는 동안의 보스 내성
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EResistance PhysicalResist = EResistance::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EResistance MagicalResist = EResistance::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EHitArea HitArea = EHitArea::Mid;

	//예고에 띄울 문구
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString NoticeText;
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
	UPROPERTY() int32 Fever = 0;

	//참가할 때 기록에서 계산해 넣는 값들. 매 판정마다 다시 조회하지 않으려고 들고 있는다
	UPROPERTY() int32 MaxHP = 0;
	UPROPERTY() int32 DamageBonus = 0;
	UPROPERTY() float CritChance = 0.f;
	UPROPERTY() float CritDamage = 0.f;

	UPROPERTY() EBattleLane Lane = EBattleLane::Mid;

	//참가할 때 한 번 받는 고정 자리 번호. 레인을 옮겨도 같은 번호를 쓴다
	UPROPERTY() int32 SlotIndex = 0;

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

	//DUMMY_MODEL이 높이 16.8cm로 임포트돼 있어 사람 크기로 올리는 보정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float UnitScale = 10.f;

	//UnitScale에 곱해지는 보스 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float BossScale = 2.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
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

	//판정 결과를 보여주는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float ResolveSeconds = 4.f;

	// ---- 전투 수치 ----

	//강화 없는 플레이어의 기본 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 PlayerBaseHP = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	float BaseCritChance = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	float BaseCritDamage = 0.4f;

	//강화 한 단계당 오르는 양
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 HPPerBonus = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 DamagePerBonus = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	float CritChancePerBonus = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	float CritDamagePerBonus = 0.1f;

	//공격 시 피버가 오르고, 회피 시 내려간다. 하한은 0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 FeverIncrement = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 FeverDecrement = 2;

	//방어 시 받는 피해를 이 값으로 나눈다 (올림)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 DefenceDivisor = 2;

	// ponytail: 원래 직업군별로 갖는 값이다. 직업군이 붙으면 그쪽으로 옮긴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 Attack1BaseDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	EDamageType Attack1Type = EDamageType::Physical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 Attack2BaseDamage = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	EDamageType Attack2Type = EDamageType::Magical;

	//보스 행동 스텁이 쓰는 피해량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 BossBaseDamage = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats")
	int32 EnemyMaxHP = 100;

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

	//유닛 머리 위 이름표로 쓸 위젯. 비워두면 C++ 기본 이름표를 쓴다.
	//UMG에서 만든 위젯 블루프린트를 지정하면 글꼴·색·배경을 에디터에서 손볼 수 있다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|UI")
	TSubclassOf<class UUserWidget> NameplateWidgetClass;

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

	//비어 있는 가장 앞 자리 번호. 만석이면 INDEX_NONE
	static int32 AllocateSlotIndex(const TSet<int32>& Used, int32 MaxSlots);

	static float ResistanceMultiplier(EResistance Resistance);

	//Random을 구체 범위로 확정한다. 나머지는 그대로 돌려준다
	static EHitArea ResolveHitArea(EHitArea Area);

	static bool HitAreaContains(EHitArea Area, EBattleLane Lane);
	static FString HitAreaLabel(EHitArea Area);

	//기획서 공식: (기본 + 강화 + 피버) * 내성 * (1 + 치명타 시 치명타 피해)
	static int32 DamageToBoss(int32 BaseDamage, int32 DamageBonus, int32 Fever,
		float Resist, bool bCrit, float CritDamage);

	//방어 시에는 나눗셈 후 올림
	static int32 DamageToPlayer(EBattleAction Action, bool bHit, int32 Base, int32 Divisor);

	//강화 단계를 실제 수치로 바꾼다
	int32 MaxHPFor(const FPlayerRecord& Record) const;

	// ---- 좌표 ----

	FVector BossLocation() const;
	FVector SlotLocation(EBattleLane Lane, int32 SlotIndex) const;

private:
	//Telegraph → Input → Collect → Resolve 순으로 타이머가 이어진다
	void StartTurn();
	void BeginInput();
	void BeginCollect();
	void ResolveTurn();

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

	//보스 행동 스텁 — 패턴이 정해지면 이 함수만 통째로 교체
	FBossAction ChooseBossAction() const;

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
