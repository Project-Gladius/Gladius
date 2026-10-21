#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Battle/BattleBalance.h"
#include "Scripts/Battle/BattleUnit.h"
#include "BattleManager.generated.h"

class ABattleUnit;
class UTextRenderComponent;
class ACutSceneApplier;

//BattleStart -> Prepare -> Collect -> Move -> BossAction -> Resolve.
UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	BattleStart = 6,
	Prepare = 0,
	//카운트다운은 끝났지만 늦게 도착하는 채팅을 계속 받는 대기 시간
	Collect = 2,
	//회피 이동 연출. 여기부터는 입력을 받지 않는다
	Move,
	//플레이어 공격과 보스 공격 모션
	BossAction,
	//결과 표시
	Resolve,
	BattleEnd = 8
};

//전투가 끝나도 남는 유저 기록. 이 구조체가 그대로 JSON 파일이 된다.
//능력치를 추가하려면 여기에 UPROPERTY를 넣기만 하면 저장 파일에 자동으로 따라온다
USTRUCT()
struct FPlayerRecord
{
	GENERATED_BODY()

	UPROPERTY() FString UserId;
	UPROPERTY() FString DisplayName;

	//고른 직업군. abilities.json 의 characters[].id 와 같은 값이고 저장 파일에 남는다
	UPROPERTY() FString CharacterId;

	//신전에서 사는 '강화' 능력치들. 게임 오버 시 일정 비율을 잃는다
	// -1 is only the legacy-file sentinel; registration/load initializes it.
	UPROPERTY() int32 CurrentHealth = -1;
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

	//피버는 정수 스택이다.
	UPROPERTY() int32 Fever = 0;

	//연속으로 회피한 턴 수. 회피가 아닌 행동을 하면 0으로 돌아간다
	UPROPERTY() int32 EvadeStreak = 0;

	//참가할 때 기록에서 계산해 넣는 값들. 매 판정마다 다시 조회하지 않으려고 들고 있는다
	UPROPERTY() int32 MaxHP = 0;
	UPROPERTY() int32 DamageBonus = 0;
	//Classes 배열에서의 위치. 매 판정마다 문자열을 찾지 않으려고 들고 있는다
	UPROPERTY() int32 ClassIndex = 0;

	//신전 강화로 얻은 치명타. 공격별 기본 치명타에 더해진다
	UPROPERTY() float CritChanceBonus = 0.f;
	UPROPERTY() float CritDamageBonus = 0.f;

	UPROPERTY() EBattleLane Lane = EBattleLane::Mid;

	//회피로 구역을 옮기기 **직전**의 구역. 구르기가 어느 경계를 넘어야 하는지는
	//이 둘(출발·도착)로 정해진다. 좌표에서 되짚으면 가장자리에 선 사람을
	//이웃 구역으로 잘못 짚는다
	UPROPERTY() EBattleLane LaneBeforeMove = EBattleLane::Mid;

	//이번 턴에 서야 할 자리. 매 턴 구역 인원에 맞춰 다시 배분된다
	UPROPERTY() FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY() EBattleAction PendingAction = EBattleAction::Idle;

	//이번 턴에 전투 명령어가 들어왔는지. 무입력 이탈 판정에 쓴다
	UPROPERTY() bool bActedThisTurn = false;

	//연속으로 입력이 없었던 턴 수
	UPROPERTY() int32 IdleTurns = 0;

	//이번 턴에 보스에게 맞은 피해. 계산은 ResolveTurn에서 끝나지만 피격 모션은
	//보스가 실제로 휘두르는 순간에 재생해야 해서 여기 담아 둔다
	UPROPERTY() int32 DamageTakenThisTurn = 0;

	//이번 턴에 쓰러졌는지. 사망 모션도 피격과 같은 순간에 나가야 한다
	UPROPERTY() bool bDiedThisTurn = false;

	UPROPERTY() TObjectPtr<ABattleUnit> Unit;

	//0보다 크면 게임 오버 상태. 매 턴 줄어들고 0이 되면 부활한다
	UPROPERTY() int32 ReviveCountdown = 0;

	//이번 전투에 보스에게 넣은 누적 피해. Records의 TotalDamageDealt는 방송 전체
	//누적이라 초기화되지 않으므로, 판 단위 순위에는 이쪽을 써야 한다
	UPROPERTY() int32 DamageDealt = 0;
};

//판정이 끝난 공격 하나를 '나중에 보여주려고' 적어 둔 것.
//기획서는 전원이 동시에 때리지 않고 랜덤 순서로 조금씩 끊어 치게 한다
struct FAttackCue
{
	FString UserId;
	int32 AttackIndex = 1;
	bool bCrit = false;

	//이번 공격에 실제로 실린 피버. 피버가 오르기 전 값이라 피해 계산에 쓴 것과 같다
	int32 Fever = 0;
	//직업군별로 한 명씩 고르므로 같이 들고 있는다
	int32 ClassIndex = 0;
	//이 공격으로 보스에게 들어간 피해. 피버가 같을 때 가르는 데 쓴다
	int32 Damage = 0;

	//직업군 안에서 피버가 가장 높아 특수 연출 대상으로 뽑힌 공격.
	//**피버가 0인 사람은 뽑지 않는다** — 아무도 안 쌓았으면 아무도 특별하지 않다
	bool bFeverHighlight = false;

	//이번 턴에 나갈 피버 평타의 치명타 결과. 개수가 곧 평타 횟수다.
	//치명타는 평타마다 따로 굴리므로 미리 굴려 담아 둔다 (판정은 ResolveTurn에서 끝난다)
	TArray<bool> FeverCrits;
};

//직업군 하나의 공격 모션 길이. 밸런스 수치가 아니라 **애셋에서 재어 넣는 값**이라
//Balancing/Data가 아니라 에디터에서 관리한다.
//여기 적힌 길이만큼 공격이 걸린다고 보고 출발 시각을 역산하므로,
//실제 몬타주보다 짧게 적으면 보스가 칠 때까지 공격 모션이 안 끝난다
USTRUCT(BlueprintType)
struct FCharacterMotionTimes
{
	GENERATED_BODY()

	//피버 평타 섹션들의 길이, 몬타주에 적힌 순서대로.
	//평타가 N번이면 이 배열을 **돌려 쓴다** (3개면 1,2,3,1,2,3…).
	//길이가 섹션마다 달라 하나로 못 적는다 — 비어 있으면 평타 연출이 나가지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<float> FeverAttacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Skill1 = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Skill2 = 1.f;
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

	//직업군 id별로 쓸 블루프린트. 메시·크기·이름표 위치·이름표 위젯이
	//전부 그 블루프린트 안에 있으므로 코드를 고칠 일이 없다.
	//예: "gladius" → BP_BattleUnitG, "sera" → BP_BattleUnitS.
	//없는 id로 참가하면 C++ 기본 클래스를 쓰는데, 메시가 없어 보이지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Unit")
	TMap<FString, TSubclassOf<ABattleUnit>> UnitClassByCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Unit")
	TSubclassOf<ABattleUnit> BossUnitClass;

	// ---- 진행 ----

	//동시 참여 정원. 레인마다 이만큼의 자리가 준비된다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 MaxPlayers = 15;

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

	//자리 이동과 돌진에 쓰는 시간. 이 시간이 지나면 판정 단계로 넘어간다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float MoveSeconds = 1.5f;

	//공격이 한 번에 쏟아지면 누가 때렸는지 안 보인다. 몇 명씩 끊어서 보여준다 (기획서 AttackCount)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 AttackCount = 3;

	//그 묶음 사이의 간격 (기획서 AttackTerm). 전원이 공격해도 턴이 길어지지 않을 만큼 짧게
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float AttackTerm = 0.12f;

	//돌진했을 때 보스와 남길 거리. 구역 각도는 그대로 두고 거리만 좁힌다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float ChargeDistance = 350.f;

	//공격 뒤 자기 자리로 돌아가는 시간 (기획서 M2의 '원위치 이동')
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float ReturnSeconds = 0.8f;

	// ---- 전투 개시 ~ 보스 공격 구간 ----
	//
	//기획서: 모든 공격 모션이 '보스 공격' 순간에 끝나도록 오른쪽 정렬된다.
	//그래서 출발 시각을 BattleTime에서 역산한다 — 피버가 높을수록 때릴 게 많아
	//일찍 출발하고, 그만큼 빨리 움직인다

	//전투 개시부터 보스가 휘두르기까지의 전체 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float BattleTime = 4.f;

	//모션 길이가 BattleTime을 다 먹어도 최소한 이만큼은 이동 시간을 남긴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float MinMoveSeconds = 0.4f;

	//직업군 id별 공격 모션 길이. 키는 UnitClassByCharacter와 같은 id다
	//(`abilities.json`의 `characters[].id`). 없는 id는 구조체 기본값으로 돈다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	TMap<FString, FCharacterMotionTimes> MotionTimesByCharacter;

	//ClassIndex가 가리키는 직업군의 모션 길이. 맵에 없으면 기본값
	const FCharacterMotionTimes& MotionTimesFor(int32 ClassIndex) const;

	//이 속도(초당 cm)보다 느리면 걷고, 빠르면 뛴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float RunSpeed = 350.f;

	//이보다 빨라야 하면 구르기를 섞는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float RollSpeed = 700.f;

	//구르기 한 번이 덮는 거리. 회피자는 이 거리를 남기고 미리 가 있다가
	//보스 공격 모션에 맞춰 구른다. 전체 거리가 이보다 짧으면 제자리에서 기다린다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float RollDistance = 500.f;

	//구르기 모션 길이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float RollSeconds = 0.5f;

	//보스가 휘두르기 **몇 초 전에** 구르기를 시작할지. 동시에 시작하면 피격 모션이
	//곧바로 구르기를 덮어 구른 것이 보이지 않는다. 구르기를 먼저 보여주고
	//그 끝자락에서 맞게 하는 값이다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float RollLead = 0.5f;

	//보스 공격 모션에 쓰는 시간의 **기본값**. 아래 맵에 없는 상태가 이 값을 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float BossActionSeconds = 1.5f;

	//상태 id별 공격 모션 길이. 키는 `bosses.json`의 `states[].id` 다.
	//좌우 단일 공격은 '돌기 → 때리기 → 되돌기'가 한 모션이라 8초를 넘고,
	//가운데 공격은 3초도 안 된다 — 하나로 묶으면 긴 쪽이 잘리거나 짧은 쪽이 기다린다.
	// ponytail: 밸런스 파일이 아니라 애셋에서 재어 넣는 값이라 에디터에 둔다
	//(MotionTimesByCharacter와 같은 이유)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	TMap<FString, float> BossStrikeSecondsByState;

	//판정 결과를 보여주는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float ResolveSeconds = 4.f;

	// ---- 전투 수치 ----
	//
	//'(밸런스 파일이 덮어씀)'이 붙은 항목은 게임이 시작될 때 Balancing/Data 의
	//.json 값으로 교체된다. 여기서 고쳐도 반영되지 않으므로 수치는 그 파일에서 고칠 것.
	//파일을 읽지 못했을 때만 아래 값이 쓰인다

	//직업군별 체력·방어·공격. 배열 순서가 자리 배치의 앞뒤를 뜻한다 (0번이 보스 쪽).
	//생성자에서 파일이 없을 때 쓸 기본값 두 개를 채운다
	UPROPERTY(EditAnywhere, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	TArray<FCharacterDef> Classes;

	//새로 참가하는 유저에게 줄 직업군
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	FString DefaultCharacterId = TEXT("gladius");

	//강화 한 단계당 오르는 양
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 DamagePerBonus = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float CritChancePerBonus = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float CritDamagePerBonus = 0.1f;

	//공격 시 오르는 피버 (파일의 attackFeverGain)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 FeverIncrement = 5;

	//방어 시 오르는 피버 (파일의 defenseFeverGain).
	// ponytail: 방어 행동 자체를 조건으로 본다. '실제로 피격을 막아야 오르는지'는 기획서 미정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 DefenseFeverGain = 5;

	//**더 이상 쓰이지 않는다.** 피버는 피해에 곱해지는 값이 아니라 평타 횟수로 바뀌었다.
	//`abilities.json`에 `feverDamageCoefficient`가 남아 있어 읽고 검증만 계속한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float FeverDamageCoefficient = 1.f;

	//피버 평타 한 대의 고정 피해. 스킬 피해와 달리 강화분이 붙지 않는다.
	// ponytail: 밸런스 파일에 아직 대응 항목이 없어 에디터 값이다.
	// abilities.json에 필드가 생기면 그쪽으로 옮긴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 FeverAttackDamage = 5;

	//피버 상한. 0이면 무제한 (파일의 feverCap: null)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	int32 FeverCap = 0;

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

	//치명타 확률 상한 (파일의 critChanceCap)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
	float CritChanceCap = 1.f;

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
	bool bRandomizeTestPlayers = false;

	//에디터에서 자리 배치를 점으로 미리 보여준다. Play 없이 격자를 맞출 때 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Debug")
	bool bPreviewSlots = true;

	// ---- 상태 ----

	//보스는 체력이 없다 — 목표는 격파가 아니라 정해진 턴을 버티며 피해를 쌓는 것이다.
	//그래서 깎는 값 대신 **이번 전투에 보스가 받은 누적 피해**를 들고 있고,
	//머리 위 이름표도 이걸 보여준다
	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 BossDamageTaken = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 TurnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	EBattlePhase Phase = EBattlePhase::BattleEnd;

	//이번 턴에 보스가 할 행동. 턴이 시작될 때 정해져 입력 시간 내내 예고된다
	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	FBossAction TelegraphedAction;

	//Prepare와 Collect 단계에서만 입력을 받는다
	UFUNCTION(BlueprintPure, Category = "Battle|State")
	bool IsAcceptingInput() const;

	// 컷신 등 표시 계층은 전투 상태를 읽기만 한다.
	const TMap<FString, FBattlePlayer>& GetBattlePlayers() const { return Players; }
	ABattleUnit* GetBossUnit() const { return BossUnit.Get(); }

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
 bool bAutoStartBattle = true;

 // HPGen for the temple recovery blessing; adjustable until design fixes the value.
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Temple", meta = (ClampMin = "0"))
 int32 BlessingHealAmount = 1;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Stats (밸런스 파일이 덮어씀)")
 int32 HPPerBonus = 1;

 void InitializePlayerRecords();

 UFUNCTION(BlueprintCallable, Category = "Battle|Temple")
 bool ApplyTempleEnhancement(const FString& UserId, const FString& DisplayName, int32 Tier);

 UFUNCTION(BlueprintPure, Category = "Battle|State")
 bool IsBattleRunning() const { return bBattleRunning; }

 // Damage/healing callers use this entry point so the save changes immediately.
 UFUNCTION(BlueprintCallable, Category = "Battle|Health")
 bool SetPlayerHealth(const FString& UserId, int32 CurrentHealth);

 UFUNCTION(BlueprintCallable, Category = "Battle")
 void LogoutPlayer(const FString& UserId);

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
 float BattleStartSeconds = 0.1f;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|CutScenes")
 bool bEnableTurnCutScenes = true;

 // Computed from the one selected participant when the Move phase begins.
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|CutScenes")
 TArray<FName> CutSceneIds;

 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|CutScenes")
 FString CutSceneUserId;

 static TArray<FName> CutSceneIdsFor(const FString& ClassId, EBattleAction Action);
 static FString SelectTurnCutSceneUser(const TMap<FString, FBattlePlayer>& Candidates,
  const TArray<FCharacterDef>& CharacterClasses, const FString& PreviousClassId,
  const TSet<FString>& PreviouslySelectedUsers, FRandomStream& Random);

 UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Battle|CutScenes")
 TObjectPtr<ACutSceneApplier> CutSceneApplier;

	// ---- 외부 진입점 ----

	//채팅 쪽에서 호출하는 유일한 함수.
	//SentAt은 YouTube가 찍은 작성 시각으로, 아직 쓰지 않지만 계약에 미리 넣어둔다.
	//처리되면 true, 명령어가 아니거나 정원 초과면 false
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool SubmitChat(const FString& UserId, const FString& DisplayName,
		const FString& Text, const FDateTime& SentAt);

	//이미 파싱된 행동을 넣을 때. SubmitChat이 최종적으로 이걸 호출한다
	//CharacterId를 비워두면 새 참가자는 DefaultCharacterId로 들어간다.
	//이미 참가했거나 기록이 있는 유저는 자기 직업군을 유지한다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool SubmitAction(const FString& UserId, const FString& DisplayName, EBattleAction Action,
		const FString& CharacterId = TEXT(""));

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

	//ChatCommandParser와 같은 문법으로 해석한다 (!공격 1, !회피 좌 등).
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

	//직업군별 인원을 받아 자리를 앞뒤로 나눠 고른다.
	//후보를 Anchor(보스)에서 가까운 순으로 정렬한 뒤 인원 비율로 구간을 쪼개고,
	//앞 직업군은 자기 구간의 앞에서부터, 맨 뒤 직업군은 뒤에서부터 채운다.
	//결과는 Candidates의 인덱스이며 직업군끼리 자리가 섞이지 않는다
	static void ChooseClassSpots(const TArray<FVector>& Candidates, const TArray<int32>& CountsByClass,
		const FVector& Anchor, TArray<TArray<int32>>& OutIndicesByClass);

	//직업군 id를 Classes 배열의 위치로. 모르는 id면 0번
	int32 ClassIndexFor(const FString& CharacterId) const;

	float ResistanceMultiplier(EResistance Resistance) const;

	//이동 후 플레이어 구역과 공격 속성으로 내성 배율을 구한다 (기획서 2절)
	float ZoneResistMultiplier(const FBossAction& Action, EBattleLane Lane, EDamageType Type) const;

	//로그와 표시판에 쓸 피격 구역 문자열
	static FString ZonesLabel(const FBossAction& Action);

	//시뮬레이터와 같은 식: (기본 + 강화 + 피버×계수) * 내성 * (1 + 치명타 시 치명타 피해).
	//강화 피해는 시뮬레이터가 기본 피해에 더해 넣는 것과 같은 자리다
	//**피버는 들어가지 않는다.** 피버는 피해에 더해지는 값이 아니라 평타 횟수다 —
	//평타는 이 함수에 BaseDamage = FeverAttackDamage로 다시 들어온다
	static int32 DamageToBoss(int32 BaseDamage, int32 DamageBonus,
		float Resist, bool bCrit, float CritDamage);

	//회피한 턴의 피버. EvadeStreak은 이번 회피까지 센 값을 넘긴다.
	//기준에 닿지 않았으면 그대로 돌려준다
	static int32 DecayFever(int32 Fever, int32 EvadeStreak, int32 StreakN,
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
 friend struct FBattleUnificationTestAccess;
 friend struct FBattleCameraTestAccess;
 bool bRecordLoadFailed = false;
 bool bRecordsInitialized = false;
 FPlayerRecord& GetOrCreateRecord(const FString& UserId, const FString& DisplayName, const FString& CharacterId = TEXT(""));
#if WITH_DEV_AUTOMATION_TESTS
 FString TestRecordPath;
#endif
 FString RecordFilePath() const
 {
#if WITH_DEV_AUTOMATION_TESTS
  if (!TestRecordPath.IsEmpty()) return TestRecordPath;
#endif
  return SaveFilePath();
 }
 bool bBattleRunning = false;
 TArray<FName> ActiveCutSceneIds;
 FString PreviousCutSceneClassId;
 TSet<FString> SelectedCutSceneUsers;
 FRandomStream CutSceneRandom;
 int32 CutSceneIndex = 0;
 void StartMoveCutScenes();
 void StopTurnCutScenes();
 void PlayNextCutScene();
 UFUNCTION()
 void HandleCutSceneEnded(FName Type, bool bInterrupted);
	//Prepare contains the telegraph and player selection intervals.
	void StartTurn();
	void BeginInput();
	void BeginCollect();
	//회피 이동을 반영하고 유닛을 새 자리로 보낸다
	void BeginMove();
	//피해를 계산하고 공격 모션을 띄운다
	void ResolveTurn();

	//직업군마다 피버가 가장 높았던 공격 하나에 표시를 남긴다.
	//피버 → 피해 → UserId 순으로 비교해 같은 입력이면 항상 같은 결과가 나온다
	static void MarkFeverHighlights(TArray<FAttackCue>& Cues);

	//예약해 둔 공격 연출을 AttackCount명씩 꺼내 재생한다. 다 쓰면 타이머를 끈다
	void FlushAttackCues();

	//이 상태의 공격 모션 길이. 맵에 없으면 BossActionSeconds
	float BossStrikeSecondsFor(const FString& StateId) const;

	//회피자가 구르기 시작점까지 달려간다. 도착하는 순간이 구르기 시작이다
	void BeginEvadeApproach();

	//From에서 To로 갈 때 **마지막으로 넘는** 경계를 돌려준다.
	//좌→우면 중/우 경계다 — 목적 구역 바로 앞의 선을 넘어야 '들어갔다'가 된다.
	//레벨에 그 표식이 없으면 false (회피는 목표 자리에 내려놓는 식으로 돈다)
	bool TryGetLaneBoundary(EBattleLane From, EBattleLane To, FVector& OutBoundary) const;

	//회피자 전원이 구른다. 보스가 휘두르기 RollLead초 전이다
	void BeginEvadeRolls();

	//플레이어 공격이 다 끝나고 보스가 치는 순간 (기획서 M2의 BossAttack 단계)
	void BossAttackBeat();

	//거리와 시간에서 나온 속도로 걸음걸이를 고른다. 시간이 0 이하면 가장 빠른 것으로 본다
	static EMoveGait ChooseGait(float Distance, float Seconds, float RunSpeed, float RollSpeed);

	//공격 모션에 필요한 시간. 스킬 한 번 + 평타 FeverStacks회
	static float AttackMotionSeconds(float SkillSeconds, float FlurrySeconds);

	//평타 Hits번의 총 길이. 섹션 길이 배열을 앞에서부터 돌려 쓰며 더한다
	static float FeverFlurrySeconds(const TArray<float>& SectionSeconds, int32 Hits);

	//정수 피버에 상한을 적용해 평타 횟수로 쓴다
	int32 FeverStacksFor(int32 Fever) const;

	//판정 단계 시작부터 보스가 칠 때까지의 시간.
	//전투 개시부터 재면 BattleTime이 되어야 하지만, 플레이어 공격 연출보다 빨라서는 안 된다
	static float BossStrikeDelay(float FlurrySeconds, float RemainingToBattleTime);

	//그 자리에서 보스 쪽으로 ChargeDistance까지 당긴 좌표.
	//보스에서 본 각도를 유지하므로 구역을 벗어나지 않는다
	FVector ChargeLocation(const FVector& SlotLocation) const;

	//Classes 배열 순서가 곧 앞뒤다. 맨 뒤 직업군만 원거리로 보고 돌진시키지 않는다
	bool IsRangedClass(int32 ClassIndex) const;
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
	//CharacterId를 비워두면 DefaultCharacterId로 들어간다
	void AddTestPlayer(const FString& CharacterId = TEXT(""));

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

	//custom 모드에서 몇 번째 패턴까지 썼는지. 추첨이 없으므로 순서만 들고 있으면 된다
	int32 PresetPatternIndex = 0;

	//레벨에 놓인 BattleSpot을 전부 찾아 구역별로 모으고 보스에서 가까운 순으로 정렬한다
	void GatherSpots();

	//구역별 자리 좌표. 비어 있으면 SlotLocation이 격자 계산으로 넘어간다.
	//FVector라 GC 대상이 아니어서 UPROPERTY가 필요 없다
	TArray<FVector> Spots[3];

	//구역 경계 표식의 좌표. [0] = 좌/중, [1] = 중/우.
	//`ABattleSpot`에 `bIsBoundary`를 켜서 레벨에 놓은 것들이다
	FVector BoundaryPoints[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bHasBoundary[2] = { false, false };

	//지금 전투 중인 참여자
	UPROPERTY() TMap<FString, FBattlePlayer> Players;

	//전투 참여 여부와 무관하게 남는 유저 기록. 저장 대상은 이쪽이다
	UPROPERTY() TMap<FString, FPlayerRecord> Records;

	UPROPERTY() TObjectPtr<ABattleUnit> BossUnit;

	//한 번에 한 단계만 도므로 타이머 하나를 돌려 쓴다
	FTimerHandle PhaseTimerHandle;

	//공격 연출을 끊어 보여주는 타이머. 단계 타이머와 동시에 돌기 때문에 따로 둔다
	FTimerHandle AttackCueHandle;

	//구르기는 보스 공격보다 먼저 시작하므로 단계 타이머와 겹친다
	FTimerHandle EvadeRollHandle;

	//접근 달리기는 구르기보다 더 먼저 시작한다
	FTimerHandle EvadeApproachHandle;

	//그 달리기에 쓸 시간. BeginEvadeApproach가 꺼내 쓴다
	float EvadeApproachSeconds = 0.f;

	//이번 턴에 재생할 공격 연출. 피해 계산은 ResolveTurn에서 이미 끝났고 보여주기만 남았다.
	//UObject를 들지 않으므로 UPROPERTY가 필요 없다
	TArray<FAttackCue> AttackCues;
	int32 AttackCueIndex = 0;

	//Resolve 단계에 띄울 직전 턴 요약. 디버그 표시용이라 문자열로 들고 있는다
	FString LastTurnSummary;
};
