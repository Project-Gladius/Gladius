#pragma once

#include "CoreMinimal.h"
#include "BattleBalance.generated.h"

//밸런스 팀이 만드는 Balancing/Data 의 .json 세 개를 그대로 담는 자료형들.
//수치의 기준은 그 파일이고 게임은 읽기만 한다

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

//필드 영역 — 좌 / 중 / 우. 파일의 left / center / right 에 대응한다
UENUM(BlueprintType)
enum class EBattleLane : uint8
{
	Left,
	Mid,
	Right
};

//공격의 속성. 보스는 속성별로 다른 내성을 가진다
UENUM(BlueprintType)
enum class EDamageType : uint8
{
	Physical,
	Magical
};

//보스가 구역마다 갖는 내성. 배율은 파일의 resistanceMultipliers 에서 온다
UENUM(BlueprintType)
enum class EResistance : uint8
{
	Enduring,	// 0.5
	Normal,		// 1.0
	Vulnerable	// 1.5
};

//연속 회피로 피버가 줄어들 때, 기준 횟수에 닿은 뒤 어떤 주기로 줄일지
UENUM(BlueprintType)
enum class EEvadeRepeatMode : uint8
{
	//기준 도달 후 매회 — N회째부터 회피할 때마다 줄어든다
	EveryAfter,
	//N회마다 — 연속 회피 횟수가 N의 배수일 때만 줄어든다
	EachN
};

//피버 감소 결과의 소수 처리. 파일의 feverRounding 에 대응한다
UENUM(BlueprintType)
enum class EFeverRounding : uint8
{
	None,	//소수 유지
	Floor,	//내림
	Round	//반올림
};

//플레이어 공격 하나. abilities.json 의 characters[].skills 항목 하나에 대응한다
USTRUCT(BlueprintType)
struct FAttackDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseDamage = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EDamageType Type = EDamageType::Physical;

	//이 공격의 기본 치명타. 신전 강화분이 여기에 더해진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CritChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CritDamage = 0.f;
};

//플레이어가 고를 수 있는 직업군 하나. abilities.json 의 characters 항목에 대응한다.
//자리 배치에서 배열 순서가 앞뒤를 뜻한다 — 0번이 가장 앞(보스 쪽), 마지막이 가장 뒤
USTRUCT(BlueprintType)
struct FCharacterDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseHP = 7;

	//방어 시 받는 피해 비율. 올림해서 적용한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DefenseDamageRatio = 0.5f;

	//공격 1 / 공격 2. 고정 크기 배열이라 블루프린트에 노출할 수 없다
	UPROPERTY(EditAnywhere) FAttackDef Attacks[2];
};

//한 구역에서의 보스 내성 한 쌍
USTRUCT(BlueprintType)
struct FZoneResistance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EResistance Physical = EResistance::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EResistance Magical = EResistance::Normal;
};

//보스 상태가 어느 구역을 치는지 (bosses.json v2의 target.mode)
UENUM(BlueprintType)
enum class EBossTargetMode : uint8
{
	//zones에 적힌 구역을 전부 친다
	Fixed,
	//후보 구역 중 **한 곳만** 친다. 어디인지는 턴마다 뽑으므로
	//예고 문구도 '무작위 구역'이라고 알린다 (Fury)
	RandomOne,
	//비공격. 기 모으기·무방비가 여기 해당한다
	None
};

//보스의 한 가지 행동 = 한 턴. 패턴은 이 행동들을 순서대로 묶은 것이다.
//bosses.json 의 actions 항목 하나와 필드가 1:1로 대응한다
USTRUCT(BlueprintType)
struct FBossAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;

	//**파일에는 더 이상 없다** (v2에서 hitCount가 빠졌다). 항상 1이며,
	//피해식과 표시에 남아 있어 지우지 않았다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HitCount = 1;

	//어느 구역을 치는지. RandomOne이면 아래 DamageByZone에 후보가 전부 채워져 있고,
	//StartTurn에서 한 구역만 남기고 지운다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EBossTargetMode TargetMode = EBossTargetMode::Fixed;

	//구역별 피해. 0이면 그 구역은 맞지 않는다.
	//v2 파일의 totalDamage를 target.zones에 들어 있는 구역에만 채운 결과다 —
	//`damageRule`이 '구역 수로 나누지 않는다'고 못 박고 있다.
	//고정 크기 배열은 블루프린트에 노출할 수 없어 아래 셋은 에디터 전용이다
	UPROPERTY(EditAnywhere) int32 DamageByZone[3] = { 0, 0, 0 };

	//보스 공격의 구역별 속성. 플레이어 쪽 속성 저항이 생기면 그때 쓰인다
	UPROPERTY(EditAnywhere) EDamageType AttributeByZone[3];

	//플레이어가 선 구역에 따라 달라지는 보스 내성.
	//'오른쪽을 치는 동안 왼쪽에서 물리로 때리면 잘 들어간다'를 이걸로 표현한다
	UPROPERTY(EditAnywhere) FZoneResistance ResistanceByZone[3];

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TelegraphText;

	//**v2에서 motionId 두 개가 사라졌다.** 대신 상태 `Id`가 그대로 모션 키다 —
	//`SingleStrikeLeft`처럼 의미를 담고 있어 블루프린트가 바로 분기할 수 있다
};

//상태 ID를 순서대로 묶은 1~4턴 패턴.
//v2 파일에는 id가 없어 배열 순서로 만들어 넣는다 (pattern_01 …)
struct FBossPattern
{
	FString Id;
	FString Name;
	TArray<FString> Steps;
};

//이동 속도에 따라 갈리는 걸음걸이. 어떤 애니메이션을 틀지는 블루프린트가 정하고,
//C++은 '이 속도로 가야 한다'에서 나온 분류만 넘긴다
UENUM(BlueprintType)
enum class EMoveGait : uint8
{
	Walk,
	Run,
	//구르기를 섞어야 할 만큼 빠른 경우
	Roll
};

//보스가 10턴을 어떻게 채울지 고르는 방식 (bosses.json의 preset.mode).
//시뮬레이터 engine.js의 schedule()과 같은 세 갈래여야 한다
UENUM(BlueprintType)
enum class EBossPresetMode : uint8
{
	//selection.pool에서 가중치로 뽑는다. 매 판 달라진다
	Random,
	//customPatterns에 적힌 패턴을 그 순서대로
	Custom,
	//directActions에 적힌 공격을 그 순서대로. 난수가 끼지 않는다
	Direct
};

//패턴 추첨 가중치
struct FPatternWeight
{
	FString PatternId;
	float Weight = 0.f;
};

//세 파일을 읽어 담은 결과. 하나라도 문제가 있으면 bLoaded가 false로 남고
//게임은 에디터에 들어 있던 값을 그대로 쓴다
struct FBattleBalance
{
	bool bLoaded = false;

	// ---- abilities.json : rules ----
	float AttackFeverGain = 5.f;
	float DefenseFeverGain = 5.f;
	float FeverDamageCoefficient = 1.f;
	int32 EvadeThreshold = 2;
	float EvadeReduction = 1.f;
	EEvadeRepeatMode EvadeRepeat = EEvadeRepeatMode::EveryAfter;
	EFeverRounding FeverRounding = EFeverRounding::Floor;
	//파일의 null(상한 없음)은 0으로 들어온다
	float FeverCap = 0.f;
	bool bAllowStayEvade = true;
	float ResistanceMultipliers[3] = { 0.5f, 1.f, 1.5f };

	// ---- abilities.json : initial / characters ----
	EBattleLane InitialZone = EBattleLane::Mid;
	float InitialFever = 0.f;
	int32 InitialEvadeStreak = 0;
	//파일에 적힌 순서 그대로. 앞에 있는 직업군이 보스에 가까운 자리를 쓴다
	TArray<FCharacterDef> Characters;

	// ---- enhancements.json ----
	int32 HPPerLevel = 1;
	int32 DamagePerLevel = 1;
	float CritChancePerLevel = 0.05f;
	float CritDamagePerLevel = 0.1f;
	float CritChanceCap = 1.f;

	//새 유저가 들고 시작하는 강화 단계 (initialLevels)
	int32 InitialHPLevel = 0;
	int32 InitialDamageLevel = 0;
	int32 InitialCritChanceLevel = 0;
	int32 InitialCritDamageLevel = 0;

	//각 파일의 revision. 어떤 개정판으로 돌았는지 로그에 남긴다
	int32 AbilitiesRevision = 0;
	int32 EnhancementsRevision = 0;
	int32 BossRevision = 0;

	// ---- bosses.json ----
	FString BossName;
	//**전투는 이 값을 쓰지 않는다.** 보스는 체력이 없고 격파로 끝나지도 않는다.
	//스키마가 밸런스 팀과의 계약이라 읽고 검증만 계속한다 (readme의 maxHp 항목)
	int32 BossMaxHP = 100;
	int32 Turns = 10;
	bool bAllowImmediateRepeat = true;
	TArray<FBossAction> Actions;
	TArray<FBossPattern> Patterns;
	TArray<FPatternWeight> Pool;

	//preset — 턴을 채우는 방식. Random이 아니면 Pool은 쓰이지 않는다.
	//preset.seed는 읽지 않는다. 시뮬레이터는 재현성이 필요해 시드를 쓰지만,
	//방송은 매번 달라야 하므로 게임에서 고정 시드를 쓰면 안 된다
	EBossPresetMode PresetMode = EBossPresetMode::Random;
	TArray<FString> PresetCustomPatterns;
	TArray<FString> PresetDirectActions;

	//CanFillExactly가 보는 표. 인덱스가 남은 턴 수이고 크기는 Turns+1이다
	TArray<bool> FillableTurns;

	//폴더 안의 abilities / enhancements / bosses .json 세 개를 읽는다.
	//실패하면 이유를 Error 로그로 남기고 false. 조용히 기본값으로 때우지 않는다
	bool LoadFromFolder(const FString& FolderPath);

	const FBossAction* FindAction(const FString& Id) const;
	//없으면 INDEX_NONE
	int32 FindCharacterIndex(const FString& Id) const;
	const FBossPattern* FindPattern(const FString& Id) const;

	//남은 턴을 가진 패턴 길이들로 **정확히** 채울 수 있는가.
	//v1에는 1턴 패턴이 있어 언제나 참이었지만, v2 패턴은 2~3턴뿐이라
	//3+3+3으로 가면 1턴이 남아 아무것도 못 넣는 막다른 길이 생긴다
	bool CanFillExactly(int32 TurnsLeft) const;

private:
	//파일 하나씩. 하나라도 false면 전체가 실패한다
	bool LoadAbilities(const FString& Path);
	bool LoadEnhancements(const FString& Path);
	bool LoadBosses(const FString& Path);
};
