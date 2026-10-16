#pragma once

#include "CoreMinimal.h"
#include "Scripts/Battle/CutSceneCamera.h"
#include "GladiusRunCamera.generated.h"

USTRUCT(BlueprintType)
struct FGladiusRunWeights
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Gladius = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Sera = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Boss = 0.f;
};

// 로컬 +X는 주인공에서 무게중심을 향하는 방향. 카메라는 그 반대편에서 주인공을 본다.
UCLASS(Blueprintable)
class GLADIUS_API AGladiusRunCamera : public ACutSceneCamera
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun", meta = (ClampMin = "1.0", Units = "cm"))
    float CameraDistance = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
    float CameraPitch = -20.f;

    // 메시 원점은 대체로 발밑이다. 화면 중심에 둘 신체 지점의 높이.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun", meta = (Units = "cm"))
    float FocusHeight = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun")
    FGladiusRunWeights Weights;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun|Smoothing", meta = (ClampMin = "1.0", ClampMax = "720.0"))
    float MaxAngularSpeed = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun|Smoothing", meta = (ClampMin = "1.0", ClampMax = "3600.0"))
    float MaxAngularAcceleration = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GladiusRun|Smoothing", meta = (ClampMin = "0.1", ClampMax = "20.0"))
    float YawResponse = 4.f;

    UFUNCTION(BlueprintPure, Category = "GladiusRun")
    bool GetWeightedCenter(FVector& OutCenter) const;

    UFUNCTION(BlueprintPure, Category = "GladiusRun")
    float GetAngularVelocity() const { return AngularVelocity; }

    static bool CalculateWeightedCenter(const ABattleManager& Manager, const ABattleUnit& Subject,
        const FGladiusRunWeights& InWeights, FVector& OutCenter);

    // 각속도와 각가속도를 제한하며 ±180도 경계에서도 짧은 방향으로 회전한다.
    static void AdvanceYaw(float TargetYaw, float DeltaSeconds, float SpeedLimit, float AccelerationLimit,
        float Response, float& InOutYaw, float& InOutVelocity);

protected:
    virtual bool InitializeCamera_Implementation() override;
    virtual void UpdateCamera_Implementation(float RealDeltaSeconds) override;

private:
    bool UpdatePose();
    bool GetDesiredYaw(float& OutYaw) const;
    float CurrentYaw = 0.f;
    float AngularVelocity = 0.f;
};
