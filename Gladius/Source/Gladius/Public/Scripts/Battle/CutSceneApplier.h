#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CutSceneApplier.generated.h"

class UMeshComponent;
class UMaterialInterface;
class ABattleManager;
class ABattleUnit;
class ALevelSequenceActor;
class APlayerController;
class ACutSceneCamera;
class ULevelSequence;
class ULevelSequencePlayer;
struct FBattlePlayer;

USTRUCT()
struct FCutSceneSavedMaterial
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UMeshComponent> Mesh;
    UPROPERTY() int32 Slot = 0;
    UPROPERTY() TObjectPtr<UMaterialInterface> Original;
    UPROPERTY() TObjectPtr<UMaterialInterface> Applied;
};

UENUM(BlueprintType)
enum class ECutSceneCharacterStrategy : uint8
{
    // 각 직업에서 공격을 선택한 생존자 중 최고 피버 한 명만 선정한다.
    DefaultGladius = 4,
    DefaultSera = 5,
    // 기존에 저장한 전략 값은 유지한다.
    RandomAlive = 0,
    HighestFever = 1,
    LowestHealthRatio = 2,
    ClosestToBoss = 3
};

USTRUCT(BlueprintType)
struct FCutSceneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene")
    TObjectPtr<ULevelSequence> Sequence = nullptr;

    // 동적 카메라 연출용. Sequence와 둘 중 하나만 지정한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene")
    TSubclassOf<ACutSceneCamera> CameraClass;

    // 기존 월드 배율에 곱한다. 1은 슬로모션 없음, 0.1은 10% 속도.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float WorldTimeScale = 0.1f;

    // Sequencer 바인딩 태그. 카메라만 연출한다면 해당 태그를 만들 필요 없다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene")
    FName SubjectBindingTag = TEXT("Subject");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene")
    FName BossBindingTag = TEXT("Boss");
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCutSceneApplied, FName, CutSceneType, ABattleUnit*, Subject);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCutSceneEnded, FName, CutSceneType, bool, bInterrupted);

// Dev_Battle에 배치하는 공통 재생기. 선정 규칙과 연출 이름만 받아 적용한다.
// 전투 단계 진행, 공격 판정, 캐릭터 애니메이션 선택은 호출자가 맡는다.
UCLASS()
class GLADIUS_API ACutSceneApplier : public AActor
{
    GENERATED_BODY()

public:
    ACutSceneApplier();

    // 비워 두면 현재 월드에서 단 하나의 BattleManager를 찾는다.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CutScene")
    TObjectPtr<ABattleManager> BattleManager;

    // 예: AttackCloseUp -> LS_AttackCloseUp. 실행 중인 항목의 수정은 다음 재생부터 반영.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene")
    TMap<FName, FCutSceneDefinition> CutScenes;

    // 유효한 새 요청은 현재 컷신을 교체한다. 잘못된 설정/대상 없음은 현재 컷신을 유지한다.
    UFUNCTION(BlueprintCallable, Category = "CutScene")
    bool ApplyCutScene(ECutSceneCharacterStrategy Strategy = ECutSceneCharacterStrategy::DefaultGladius,
        FName CutSceneType = NAME_None);

    // Explicit subject: the scheduler's selected user remains the subject of every shot.
    UFUNCTION(BlueprintCallable, Category = "CutScene")
    bool ApplyCutSceneForUser(const FString& UserId, FName CutSceneType);

    void EnsureDefaultCutScenes();

    UFUNCTION(BlueprintCallable, Category = "CutScene")
    void CancelCutScene();

    UFUNCTION(BlueprintPure, Category = "CutScene")
    bool IsPlaying() const { return bPlaying; }

    UFUNCTION(BlueprintPure, Category = "CutScene")
    ABattleUnit* GetCurrentSubject() const;

    UFUNCTION(BlueprintPure, Category = "CutScene")
    ABattleUnit* FindCharacter(ECutSceneCharacterStrategy Strategy = ECutSceneCharacterStrategy::DefaultGladius) const;

    // 선택한 한 명의 컷신이 시작할 때 발생한다.
    UPROPERTY(BlueprintAssignable, Category = "CutScene")
    FOnCutSceneApplied OnCutSceneApplied;

    // 선택한 컷신이 끝나거나 취소됐을 때 한 번 발생한다.
    UPROPERTY(BlueprintAssignable, Category = "CutScene")
    FOnCutSceneEnded OnCutSceneEnded;

    // C++에서도 동일한 선정 규칙/좌표 계산을 재사용할 수 있다.
    static ABattleUnit* SelectCharacter(const TMap<FString, FBattlePlayer>& Players,
        ECutSceneCharacterStrategy Strategy, const FVector& BossLocation,
        int32 RequiredClassIndex = INDEX_NONE);
    static FTransform MakeOrigin(const AActor& Subject, const AActor& Boss);
    static FTransform MakeGroundedOrigin(const ABattleUnit& Subject, const AActor& Boss);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    ABattleManager* ResolveBattleManager() const;
    bool ApplyCutSceneToSubject(ABattleUnit* Subject, FName CutSceneType);
    bool StartShot(ABattleUnit* Subject, ABattleUnit* Boss);
    void FinishCutScene(bool bInterrupted, bool bNotify = true);
    void ApplyBossOpacity(ABattleUnit* Boss);
    void RestoreBossMaterials();
    UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> BossFadeMaterials;
    UPROPERTY(Transient) TArray<FCutSceneSavedMaterial> SavedBossMaterials;

    UFUNCTION() void HandleSequenceFinished();
    UFUNCTION() void HandleSequenceStopped();
    UFUNCTION() void HandleCameraFinished(bool bInterrupted);
    UFUNCTION() void HandleActorDestroyed(AActor* DestroyedActor);

    UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
    UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> SequencePlayer;
    UPROPERTY(Transient) TObjectPtr<ACutSceneCamera> ProceduralCamera;
    UPROPERTY(Transient) TWeakObjectPtr<ABattleUnit> ActiveSubject;
    UPROPERTY(Transient) TWeakObjectPtr<ABattleUnit> ActiveBoss;
    UPROPERTY(Transient) TWeakObjectPtr<APlayerController> CameraController;
    UPROPERTY(Transient) TWeakObjectPtr<AActor> PreviousViewTarget;
    UPROPERTY(Transient) FCutSceneDefinition ActiveDefinition;

    FName ActiveType;
    float SavedGlobalDilation = 1.f;
    float SavedSubjectDilation = 1.f;
    bool bPlaying = false;
    bool bApplying = false;
    bool bFinishing = false;
    bool bEndingPlay = false;
};
