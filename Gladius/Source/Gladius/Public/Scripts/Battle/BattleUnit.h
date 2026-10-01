#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	//이동하는 동안만 Tick이 돌고 도착하면 다시 꺼진다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void MoveTo(const FVector& Target, float Duration);

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsMoving() const { return bMoving; }

	// ---- 블루프린트 훅 ----
	//
	//모션이 준비되면 블루프린트에서 이 이벤트들에 애니메이션을 연결한다.
	//C++은 '언제'만 알려주고 '무엇을 재생할지'는 관여하지 않는다

	//이동 시작. Duration에 맞춰 걷기/달리기 길이를 맞추면 된다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnMoveStarted(float Duration);

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
};
