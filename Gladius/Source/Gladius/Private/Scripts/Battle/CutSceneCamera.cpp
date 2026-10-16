#include "Scripts/Battle/CutSceneCamera.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"

ACutSceneCamera::ACutSceneCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

ABattleManager* ACutSceneCamera::GetBattleManager() const { return BattleManager.Get(); }
ABattleUnit* ACutSceneCamera::GetSubject() const { return SubjectActor.Get(); }

bool ACutSceneCamera::StartCutScene(ABattleManager* Manager, ABattleUnit* InSubject)
{
    if (!IsValid(Manager) || !IsValid(InSubject) || Manager->GetWorld() != GetWorld()
        || InSubject->GetWorld() != GetWorld() || !FMath::IsFinite(DurationSeconds) || DurationSeconds < 0.f) { return false; }
    BattleManager = Manager;
    SubjectActor = InSubject;
    ElapsedSeconds = 0.f;
    if (!InitializeCamera()) { return false; }
    bRunning = true;
    SetActorTickEnabled(true);
    return true;
}

void ACutSceneCamera::StopCutScene()
{
    bRunning = false;
    SetActorTickEnabled(false);
}

bool ACutSceneCamera::InitializeCamera_Implementation() { return true; }
void ACutSceneCamera::UpdateCamera_Implementation(float RealDeltaSeconds) {}

void ACutSceneCamera::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bRunning) { return; }
    if (!BattleManager.IsValid() || !SubjectActor.IsValid())
    {
        StopCutScene();
        OnFinished.Broadcast(true);
        return;
    }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
    if (DurationSeconds > 0.f) { ElapsedSeconds += DeltaSeconds; }
    UpdateCamera(DeltaSeconds);
    if (bRunning && DurationSeconds > 0.f && ElapsedSeconds >= DurationSeconds)
    {
        StopCutScene();
        OnFinished.Broadcast(false);
    }
}
