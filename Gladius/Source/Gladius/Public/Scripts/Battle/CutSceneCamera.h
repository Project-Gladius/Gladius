#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "CutSceneCamera.generated.h"

class ABattleManager;
class ABattleUnit;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCutSceneCameraFinished, bool, bInterrupted);

// 매 프레임 계산하는 컷신의 공통 카메라. 시간/대상/게임 카메라 복원은 Applier가 관리한다.
UCLASS(Abstract, Blueprintable)
class GLADIUS_API ACutSceneCamera : public ACameraActor
{
    GENERATED_BODY()
public:
    ACutSceneCamera();

    // 0은 자동 종료 없음. 다음 컷신 요청이나 CancelCutScene까지 유지한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CutScene", meta = (ClampMin = "0.0", Units = "s"))
    float DurationSeconds = 0.f;

    bool StartCutScene(ABattleManager* Manager, ABattleUnit* Subject);
    virtual void StopCutScene();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(BlueprintAssignable, Category = "CutScene")
    FOnCutSceneCameraFinished OnFinished;

    UFUNCTION(BlueprintPure, Category = "CutScene")
    ABattleManager* GetBattleManager() const;

    UFUNCTION(BlueprintPure, Category = "CutScene")
    ABattleUnit* GetSubject() const;

protected:
    UFUNCTION(BlueprintNativeEvent, Category = "CutScene")
    bool InitializeCamera();

    UFUNCTION(BlueprintNativeEvent, Category = "CutScene")
    void UpdateCamera(float RealDeltaSeconds);

private:
    UPROPERTY(Transient) TWeakObjectPtr<ABattleManager> BattleManager;
    UPROPERTY(Transient) TWeakObjectPtr<ABattleUnit> SubjectActor;
    float ElapsedSeconds = 0.f;
    bool bRunning = false;
};
