#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Battle/BattleBalance.h"
#include "BattleUnit.generated.h"

class USkeletalMeshComponent;
class UWidgetComponent;
class UCurveFloat;
class UBattlePlayerNameplate;
struct FBattlePlayerUIData;

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

	//이름표에 넣을 글자를 그대로 지정한다. 'N / M' 꼴이 아닌 것을 띄울 때 쓴다 —
	//보스는 체력이 없어 받은 피해만 보여준다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetLabelText(const FString& Text);

	// Install and update the temporary player HUD on the existing screen-space nameplate.
	void SetPlayerUI(const FBattlePlayerUIData& Data);
	void SetPlayerUIVisible(bool bVisible);
	void UpdatePlayerUIScale(const FVector& CameraLocation);

	//Duration 동안 Target으로 미끄러져 간다. 0 이하면 즉시 이동한다.
	//이동하는 동안만 Tick이 돌고 도착하면 다시 꺼진다.
	//Gait는 OnMoveStarted로 그대로 넘어간다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void MoveTo(const FVector& Target, float Duration, EMoveGait Gait = EMoveGait::Walk);

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsMoving() const { return bMoving; }

	// ---- ABP가 읽어 가는 상태 ----
	//
	//**애님 블루프린트는 이것들을 BlueprintUpdateAnimation에서 직접 읽는다.**
	//캐릭터 블루프린트가 ABP로 캐스팅해 변수를 밀어 넣던 방식을 대체한 것이다 —
	//직업군이 늘어도 같은 배선을 다시 만들 필요가 없고, 캐릭터 BP에는
	//'어떤 몬타주를 트는가'만 남는다

	//이번 턴에 고른 행동. 무입력이면 Idle이다
	UFUNCTION(BlueprintPure, Category = "Battle")
	EBattleAction GetSelectedAction() const { return SelectedAction; }

	//무입력이 아니면 준비 자세를 잡는다
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsReady() const { return SelectedAction != EBattleAction::Idle; }

	//마지막 이동의 걸음걸이가 걷기보다 빠른가
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsRunning() const { return LastGait != EMoveGait::Walk; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsDead() const { return bDead; }

	// ---- BattleManager가 부르는 알림 ----
	//
	//상태를 적어 두고 같은 이름의 블루프린트 훅을 이어서 띄운다.
	//매니저가 훅만 부르면 상태 갱신을 빠뜨리므로 한 함수로 묶었다

	void NotifyActionSelected(EBattleAction Action);
	void NotifyDied();
	void NotifyRevived();

	//막을 들고 버티는 중인가. BattleManager가 켜고 끄며, ABP가 이것을 읽어
	//Defense 상태로 들어간다 — IsMoving과 같은 방식이다.
	//버티는 자세를 몬타주가 아니라 상태로 두어야 앞뒤 모션과 블렌드된다
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsDefending() const { return bDefending; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetDefending(bool bInDefending) { bDefending = bInDefending; }

	//피버 평타를 FirstDelay 뒤부터 Crits.Num()회 재생한다.
	//SectionSeconds는 섹션 길이들이고 **돌려 쓴다** — 섹션마다 길이가 달라
	//고정 간격으로는 맞지 않는다. BattleManager가 한 번만 불러 주고
	//이어 가는 것은 유닛이 스스로 한다 (공격자마다 평타 수가 다르다)
	void PlayFeverFlurry(const TArray<bool>& Crits, float FirstDelay,
		const TArray<float>& SectionSeconds);

	//끊기면 안 되는 몬타주를 틀 때 블루프린트가 같이 부른다 (Seconds는 보통 Get Play Length).
	//이 시간 동안 들어온 MoveTo는 **버리지 않고 미뤘다가** 끝난 뒤에 실행한다 —
	//쓰러져 있는 동안 자리 재배치에 끌려가 미끄러지는 것을 막는 장치다.
	//전투 박자에 묶인 이동(돌진·구르기)에는 쓰지 말 것. 늦게 출발하면 타이밍이 어긋난다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void HoldMotion(float Seconds);

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

	//이동 시작. Gait는 '이 거리를 이 시간에 가려면 얼마나 빨라야 하는가'로 정해진다.
	//기준 속도(RunSpeed / RollSpeed)는 BattleManager가 들고 있다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnMoveStarted(float Duration, float Distance, EMoveGait Gait);

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnArrived();

	//회피한 전원이 보스의 공격 모션에 맞춰 구른다.
	//RemainingSeconds만큼 더 가야 하므로, 0이면 제자리에서 구르는 것이고
	//크면 구르면서 그만큼 더 이동한다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnEvadeRoll(float RemainingSeconds, float RemainingDistance);

	//공격. AttackIndex는 1 또는 2, bCrit이면 치명타 연출.
	//bFeverHighlight면 이번 턴 자기 직업군에서 피버가 가장 높았던 한 명이다 —
	//평소 공격 대신 전용 모션을 재생한다 (특수 컷신이 붙을 자리)
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnAttack(int32 AttackIndex, bool bCrit, bool bFeverHighlight);

	//피버 평타 한 대. 스킬 모션이 끝난 뒤 Count회 이어서 온다.
	//Index는 0부터고, 마지막 대는 Index == Count - 1 이다 —
	//마무리 연출을 붙이고 싶으면 그걸로 가른다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnFeverAttack(int32 Index, int32 Count, bool bCrit);

	//방어 자세를 잡는다. 피해 판정이 아니라 **행동 턴이 시작될 때** 불린다.
	//막이 실제로 맞았는지는 뒤이어 오는 OnHit으로 알 수 있다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnDefend();

	//맞는 순간. bLethal이면 이 타격으로 쓰러진다 — 뒤이어 OnDied가 온다.
	//쓰러진 뒤 일어날지는 이 값으로 갈린다. OnDied를 기다려서는 늦는다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnHit(int32 Damage, bool bLethal);

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnDied();

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnRevived();

	// ---- 보스 전용 훅 ----
	//
	//StateId는 `bosses.json`의 `states[].id` 가 그대로 넘어온다 —
	//`SingleStrikeLeft` · `AllStrike` · `Fury` · `Defense` · `Defenseless` 같은 값이다.
	//어떤 애니메이션을 틀지는 블루프린트가 이 id로 고른다.
	//**비어 있으면 지금 자세를 유지하라는 뜻이다** (밸런스 파일을 못 읽은 경우)

	//플레이어가 행동을 고르는 동안의 대기 자세
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossWait();

	//다음 공격 예고. 플레이어가 피격 범위를 자세만 보고 짐작할 수 있어야 하므로
	//공격마다 다른 자세를 연결한다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossTelegraph(const FString& StateId, const FString& TelegraphText);

	//플레이어들이 때리는 동안의 피격 상태. 때린 사람이 아무도 없으면 호출되지 않는다
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossHit(int32 TotalDamage, int32 AttackerCount);

	//예고한 그 공격을 실제로 휘두르는 순간
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Motion")
	void OnBossAttack(const FString& StateId, int32 HitCount);

	//스폰 시 스케일이 1로 덮이므로, 크기는 Mesh 컴포넌트 쪽에서 준다.
	//그래서 액터 루트는 빈 SceneComponent로 두고 Mesh를 그 아래 붙인다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	//Mesh가 아니라 Root에 붙는다. 메시 크기와 무관하게 높이를 직접 정할 수 있다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UWidgetComponent> Nameplate;

	// Temporary C++ widget by default; replace with a derived WBP when art is ready.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UBattlePlayerNameplate> PlayerUIClass;

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

	bool bDefending = false;
	bool bDead = false;

	//이번 턴에 고른 행동과 마지막 이동의 걸음걸이. ABP가 읽어 간다
	EBattleAction SelectedAction = EBattleAction::Idle;
	EMoveGait LastGait = EMoveGait::Walk;

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

	// ---- 모션 보호 ----
	//이 시각까지는 이동을 미룬다 (월드 시간). HoldMotion이 정한다
	float MotionHoldUntil = 0.f;

	//미뤄 둔 이동. 겹쳐 들어오면 마지막 것만 남는다 — 어차피 가야 할 곳은 한 곳이다
	bool bHasPendingMove = false;
	FVector PendingMoveTarget = FVector::ZeroVector;
	float PendingMoveDuration = 0.f;
	EMoveGait PendingMoveGait = EMoveGait::Walk;
	FTimerHandle MotionHoldHandle;

	void FlushPendingMove();

	// ---- 피버 평타 ----
	//미리 굴려 받은 치명타 결과. 앞에서부터 하나씩 꺼내 쓴다
	TArray<bool> FlurryCrits;
	int32 FlurryIndex = 0;
	//섹션 길이. 평타 수보다 짧으면 앞에서부터 다시 쓴다
	TArray<float> FlurrySections;
	FTimerHandle FlurryHandle;

	void FireNextFeverAttack();
};
