#pragma once

#include "CoreMinimal.h"
#include "BattleBalance.generated.h"

//밸런스 팀이 만드는 Balancing/Data 의 .json 세 개를 그대로 담는 자료형들.
//수치의 기준은 그 파일이고 게임은 읽기만 한다

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

//한 구역에서의 보스 내성 한 쌍
USTRUCT(BlueprintType)
struct FZoneResistance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EResistance Physical = EResistance::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EResistance Magical = EResistance::Normal;
};

//보스의 한 가지 행동 = 한 턴. 패턴은 이 행동들을 순서대로 묶은 것이다.
//bosses.json 의 actions 항목 하나와 필드가 1:1로 대응한다
USTRUCT(BlueprintType)
struct FBossAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;

	//같은 행동을 몇 번 때리는지. 구역 피해가 이 횟수만큼 들어간다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HitCount = 1;

	//구역별 피해. 0이면 그 구역은 맞지 않는다.
	//고정 크기 배열은 블루프린트에 노출할 수 없어 아래 셋은 에디터 전용이다
	UPROPERTY(EditAnywhere) int32 DamageByZone[3] = { 0, 0, 0 };

	//보스 공격의 구역별 속성. 플레이어 쪽 속성 저항이 생기면 그때 쓰인다
	UPROPERTY(EditAnywhere) EDamageType AttributeByZone[3];

	//플레이어가 선 구역에 따라 달라지는 보스 내성.
	//'오른쪽을 치는 동안 왼쪽에서 물리로 때리면 잘 들어간다'를 이걸로 표현한다
	UPROPERTY(EditAnywhere) FZoneResistance ResistanceByZone[3];

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TelegraphText;

	//모션 리소스 연결용 ID. 애니메이션이 붙기 전까지는 보관만 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TelegraphMotionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AttackMotionId;
};

//행동 ID를 순서대로 묶은 1~4턴 패턴
struct FBossPattern
{
	FString Id;
	FString Name;
	TArray<FString> Steps;
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
	FString CharacterName;
	int32 BaseHP = 7;
	//방어 시 받는 피해 비율. 올림해서 적용한다
	float DefenseDamageRatio = 0.5f;
	FAttackDef Attacks[2];

	// ---- enhancements.json ----
	int32 HPPerLevel = 1;
	int32 DamagePerLevel = 1;
	float CritChancePerLevel = 0.05f;
	float CritDamagePerLevel = 0.1f;
	float CritChanceCap = 1.f;

	// ---- bosses.json ----
	FString BossName;
	int32 BossMaxHP = 100;
	int32 Turns = 10;
	bool bAllowImmediateRepeat = true;
	TArray<FBossAction> Actions;
	TArray<FBossPattern> Patterns;
	TArray<FPatternWeight> Pool;

	//폴더 안의 abilities / enhancements / bosses .json 세 개를 읽는다.
	//실패하면 이유를 Error 로그로 남기고 false. 조용히 기본값으로 때우지 않는다
	bool LoadFromFolder(const FString& FolderPath);

	const FBossAction* FindAction(const FString& Id) const;
	const FBossPattern* FindPattern(const FString& Id) const;

private:
	//파일 하나씩. 하나라도 false면 전체가 실패한다
	bool LoadAbilities(const FString& Path);
	bool LoadEnhancements(const FString& Path);
	bool LoadBosses(const FString& Path);
};
