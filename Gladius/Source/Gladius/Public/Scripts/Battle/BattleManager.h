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

UCLASS()
class GLADIUS_API ABattleManager : public AActor
{
	GENERATED_BODY()

public:
	ABattleManager();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//남은 입력 시간과 현재 선택된 행동을 레벨에 띄운다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UTextRenderComponent> StatusText;

	//이 액터의 위치가 투기장 중심. 보스는 항상 +X 끝에 선다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float ArenaRadius = 1000.f;

	//가운데 레인 기준 좌/우 레인이 벌어지는 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float LaneAngle = 35.f;

	//DUMMY_MODEL이 높이 16.8cm로 임포트돼 있어 사람 크기로 올리는 보정값.
	//정식 모델이 들어오면 1로 되돌린다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float UnitScale = 10.f;

	//UnitScale에 곱해지는 보스 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Arena")
	float BossScale = 2.f;

	//입력을 받는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float InputSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 PlayerMaxHP = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 EnemyMaxHP = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 Attack1Damage = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 Attack2Damage = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	int32 EnemyDamage = 20;

	//방어했을 때 받는 데미지 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Settings")
	float DefenseMultiplier = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 PlayerHP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 EnemyHP = 0;

	//플레이어가 현재 서 있는 영역
	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	EBattleLane PlayerLane = EBattleLane::Mid;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	int32 TurnIndex = 0;

	//입력 접수 중인지
	UPROPERTY(BlueprintReadOnly, Category = "Battle|State")
	bool bInputOpen = false;

	//채팅 쪽에서 호출할 유일한 함수. 접수 시간이 아니면 false
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool SubmitAction(EBattleAction Action);

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartBattle();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StopBattle();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Battle|Debug")
	void RunSelfTest();

	//회피 행동이면 그 영역으로 이동, 아니면 제자리
	static EBattleLane ApplyMove(EBattleAction Action, EBattleLane Current);

	//피격 시 실제로 깎이는 HP
	static int32 DamageToPlayer(EBattleAction Action, bool bHit, int32 Base, float DefMul);

	FVector BossLocation() const;
	FVector LaneLocation(EBattleLane Lane) const;

private:
	void StartTurn();
	void ResolveTurn();

	void SpawnUnits();
	void UpdateVisuals();
	void BindActionKeys();

	//투기장 테두리와 레인 경계선. 표시만 하고 통행은 막지 않는다
	void DrawArenaGuides() const;

	UPROPERTY() TObjectPtr<ABattleUnit> PlayerUnit;
	UPROPERTY() TObjectPtr<ABattleUnit> BossUnit;

	//적 행동 스텁 — 규칙이 정해지면 이 함수만 통째로 교체
	EBattleLane ChooseEnemyLane() const;

	EBattleAction PendingAction = EBattleAction::Idle;
	FTimerHandle InputTimerHandle;

	//보스가 마지막으로 친 레인. -1이면 아직 판정된 턴이 없다
	int32 LastEnemyLane = -1;
};
