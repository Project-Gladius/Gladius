#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Battle/BattleBalance.h"
#include "BattleUnit.generated.h"

class USkeletalMeshComponent;
class UWidgetComponent;
class UCurveFloat;

//전투에 등장하는 유닛. 메시·크기·이름표 위치는 블루프린트에서 지정한다.
//BattleManager가 PlayerUnitClass / BossUnitClass로 어떤 블루프린트를 쓸지 고른다
UCLASS()
class GLADIUS_API ABattleUnit : public AActor
{
	GENERATED_BODY()

public:
	ABattleUnit();

	//머리 위 이름과 체력 갱신. UMG라 한글·이모지 핸들도 그대로 나온다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetLabel(const FString& Name, int32 Current, int32 Max);

	//Duration 동안 Target으로 미끄러져 간다. 0 이하면 즉시 이동한다.
	//이동하는 동안만 Tick이 돌고 도착하면 다시 꺼진다.
	//bRollThrough는 OnMoveStarted로 그대로 넘어간다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void MoveTo(const FVector& Target, float Duration, bool bRollThrough = false);

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsMoving() const { return bMoving; }

	//멈춰 있을 때 바라볼 곳. 플레이어는 보스를 본다.
	//지정하지 않으면 이동이 끝나도 방향을 되돌리지 않는다 (보스 유닛이 그렇다)
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetFacingTarget(const FVector& WorldLocation);

	// ---- 블루프린트 훅 ----
	//
	//모션이 준비되면 블루프린트에서 이 이벤트들에 애니메이션을 연결한다.
	//C++은 '언제'만 알려주고 '무엇을 재생할지'는 관여하지 않는다

	//행동을 고른 순간. Idle에서 그 행동의 준비 자세로 바꾸는 자리다.
	//기획서는 선택 즉시 BattleReady로 가고, 실제 행동은 행동 턴에 일괄 재생하라고 한다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnActionSelected(EBattleAction Action);

	//이동 시작.
	//Distance가 짧으면 걷고 길면 뛴다 — 기준 거리는 블루프린트가 정한다(기획서: '구역이 아닌 거리로').
	//bRollThrough면 이동 경로의 가운데가 이번 턴 피격 범위다. 구르기로 빠져나가는 연출용
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnMoveStarted(float Duration, float Distance, bool bRollThrough);

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnArrived();

	//공격. AttackIndex는 1 또는 2, bCrit이면 치명타 연출
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnAttack(int32 AttackIndex, bool bCrit);

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnDefend();

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnHit(int32 Damage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnDied();

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnRevived();

	// ---- 보스 전용 훅 ----
	//
	//MotionId는 밸런스 파일(bosses.json)의 행동마다 적힌 값이 그대로 넘어온다.
	//어떤 애니메이션을 틀지는 블루프린트가 그 id로 고른다.
	//**비어 있으면 지금 자세를 유지하라는 뜻이다** — 기획서가 모션이 없는 행동
	//(기 모으기, 단단해지기, 그로기)을 그렇게 처리하라고 한다

	//플레이어가 행동을 고르는 동안의 대기 자세
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossWait();

	//다음 공격 예고. 플레이어가 피격 범위를 자세만 보고 짐작할 수 있어야 하므로
	//공격마다 다른 자세를 연결한다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossTelegraph(const FString& MotionId, const FString& TelegraphText);

	//플레이어들이 때리는 동안의 피격 상태. 때린 사람이 아무도 없으면 호출되지 않는다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossHit(int32 TotalDamage, int32 AttackerCount);

	//예고한 그 공격을 실제로 휘두르는 순간
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossAttack(const FString& MotionId, int32 HitCount);

	//스폰 시 스케일이 1로 덮이므로, 크기는 Mesh 컴포넌트 쪽에서 준다.
	//그래서 액터 루트는 빈 SceneComponent로 두고 Mesh를 그 아래 붙인다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	//Mesh가 아니라 Root에 붙는다. 메시 크기와 무관하게 높이를 직접 정할 수 있다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UWidgetComponent> Nameplate;

	//이동 가감속 곡선. 비워두면 등속으로 간다.
	//가로축 0~1이 이동 구간 전체, 세로축 0~1이 진행도다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	TObjectPtr<UCurveFloat> MoveCurve;

	//방향을 트는 속도(초당 각도). 달리기 모션이 옆으로 미끄러져 보이지 않으려면
	//이동이 끝나기 전에 다 돌아야 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	float TurnSpeed = 540.f;

	//이보다 짧게 움직일 때는 방향을 틀지 않는다.
	//매 턴 자리를 조금씩 다시 잡는데 그때마다 돌면 제자리에서 빙빙 도는 것처럼 보인다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	float TurnMinDistance = 150.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	//스폰 직후엔 위젯이 아직 없을 수 있어 들고 있다가 BeginPlay에서 한 번 더 적용한다
	FString PendingLabel;

	void ApplyLabel();

	// ---- 이동 상태 ----
	bool bMoving = false;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveTarget = FVector::ZeroVector;
	float MoveElapsed = 0.f;
	float MoveDuration = 0.f;

	// ---- 회전 상태 ----
	//이동과 따로 끝난다. 도착한 뒤에도 보스 쪽으로 마저 도는 동안 Tick이 살아 있어야 한다
	bool bTurning = false;
	float DesiredYaw = 0.f;
	bool bHasFaceTarget = false;
	FVector FaceTarget = FVector::ZeroVector;

	//지금 그쪽을 보도록 목표 각도를 잡는다. 이미 거의 같은 방향이면 아무것도 하지 않는다
	void TurnToward(const FVector& WorldLocation);
};
