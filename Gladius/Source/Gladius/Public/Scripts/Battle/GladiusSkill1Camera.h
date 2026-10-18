#pragma once

#include "CoreMinimal.h"
#include "Scripts/Battle/CutSceneCamera.h"
#include "Components/SkinnedMeshComponent.h"
#include "GladiusSkill1Camera.generated.h"

class USkeletalMeshComponent;

UCLASS(Blueprintable)
class GLADIUS_API AGladiusSkill1Camera : public ACutSceneCamera
{
    GENERATED_BODY()
public:
    // 캐릭터 원점에서 카메라까지의 수평 거리.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1", meta = (ClampMin = "1.0", Units = "cm"))
    float Distance = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1", meta = (Units = "cm"))
    float Height = 170.f;

    // 캐릭터->보스를 향하는 정면에서 오른쪽으로 잰 카메라 위치 각도.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1", meta = (Units = "deg"))
    float RightAngle = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1", meta = (ClampMin = "0.0"))
    float DuckRatio = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1", meta = (ClampMin = "0.0"))
    float TiltRatio = 1.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1|Height Detection")
    FName PelvisBoneName = TEXT("pelvis");

    // 비우면 모든 보이는 신체 메시를 사용한다. 이름을 지정하면 그 컴포넌트만 사용.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusSkill1|Height Detection")
    FName HeightSourceComponentName;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GladiusSkill1|Height Detection")
    float InitialCharacterHeight = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GladiusSkill1|Height Detection")
    float CharacterHeight = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GladiusSkill1|Height Detection")
    float Delta = 0.f;

    UFUNCTION(BlueprintPure, Category = "GladiusSkill1|Height Detection")
    bool IsUsingPelvis() const { return BoneComponent.IsValid(); }

    virtual void StopCutScene() override;
    static bool MeasureVisibleMeshTop(const ABattleUnit& Subject, FName ComponentName, float& OutHeight);
    static FTransform CalculatePose(const FTransform& Frame, float InDistance, float InHeight,
        float InRightAngle, float InDelta, float InDuckRatio, float InTiltRatio);

protected:
    virtual bool InitializeCamera_Implementation() override;
    virtual void UpdateCamera_Implementation(float RealDeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool SampleHeight(float& OutHeight) const;
    void UpdatePose();
    UPROPERTY(Transient) TWeakObjectPtr<USkinnedMeshComponent> BoneComponent;
    UPROPERTY(Transient) TWeakObjectPtr<USkeletalMeshComponent> AnimatedComponent;
    EVisibilityBasedAnimTickOption PreviousAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    bool bPreviousUpdateRateOptimizations = false;
    bool bUsingBone = false;
};
