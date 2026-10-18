#include "Scripts/Battle/GladiusRunCamera.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"

namespace
{
float ValidWeight(float Weight)
{
    return FMath::IsFinite(Weight) ? FMath::Clamp(Weight, 0.f, 1.f) : 0.f;
}
}

bool AGladiusRunCamera::CalculateWeightedCenter(const ABattleManager& Manager, const ABattleUnit& Subject,
    const FGladiusRunWeights& InWeights, FVector& OutCenter)
{
    FVector Sum = FVector::ZeroVector;
    double TotalWeight = 0.0;
    TSet<const ABattleUnit*> Visited;
    auto Add = [&](const ABattleUnit* Unit, float Weight)
    {
        Weight = ValidWeight(Weight);
        if (!IsValid(Unit) || Unit == &Subject || Weight <= 0.f || Visited.Contains(Unit)) { return; }
        const FVector Position = Unit->GetActorLocation();
        if (Position.ContainsNaN()) { return; }
        Visited.Add(Unit);
        Sum += Position * Weight;
        TotalWeight += Weight;
    };
    Add(Manager.GetBossUnit(), InWeights.Boss);
    for (const auto& Pair : Manager.GetBattlePlayers())
    {
        const FBattlePlayer& Player = Pair.Value;
        if (!Manager.Classes.IsValidIndex(Player.ClassIndex)) { continue; }
        const FString& ClassId = Manager.Classes[Player.ClassIndex].Id;
        if (ClassId == TEXT("gladius")) { Add(Player.Unit, InWeights.Gladius); }
        else if (ClassId == TEXT("sera")) { Add(Player.Unit, InWeights.Sera); }
    }
    OutCenter = TotalWeight > SMALL_NUMBER ? Sum / TotalWeight : Subject.GetActorLocation();
    return TotalWeight > SMALL_NUMBER;
}

bool AGladiusRunCamera::GetWeightedCenter(FVector& OutCenter) const
{
    const ABattleManager* Manager = GetBattleManager();
    const ABattleUnit* Subject = GetSubject();
    if (!Manager || !Subject) { OutCenter = FVector::ZeroVector; return false; }
    return CalculateWeightedCenter(*Manager, *Subject, Weights, OutCenter);
}

bool AGladiusRunCamera::GetDesiredYaw(float& OutYaw) const
{
    const ABattleUnit* Subject = GetSubject();
    FVector Center;
    if (!Subject || !GetWeightedCenter(Center)) { return false; }
    FVector Direction = Center - Subject->GetActorLocation();
    Direction.Z = 0.f;
    if (!Direction.Normalize()) { return false; }
    OutYaw = Direction.Rotation().Yaw;
    return true;
}

void AGladiusRunCamera::AdvanceYaw(float TargetYaw, float DeltaSeconds, float SpeedLimit, float AccelerationLimit,
    float Response, float& InOutYaw, float& InOutVelocity)
{
    if (!FMath::IsFinite(TargetYaw) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
    SpeedLimit = FMath::IsFinite(SpeedLimit) ? FMath::Clamp(SpeedLimit, 1.f, 720.f) : 60.f;
    AccelerationLimit = FMath::IsFinite(AccelerationLimit) ? FMath::Clamp(AccelerationLimit, 1.f, 3600.f) : 120.f;
    Response = FMath::IsFinite(Response) ? FMath::Clamp(Response, 0.1f, 20.f) : 4.f;
    // 큰 프레임 지연 뒤에 카메라가 한 번에 따라잡으려 하지 않도록 제한한다.
    const float Duration = FMath::Min(DeltaSeconds, 0.25f);
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(Duration * 120.f));
    const float Step = Duration / Steps;
    for (int32 Index = 0; Index < Steps; ++Index)
    {
        const float Error = FMath::FindDeltaAngleDegrees(InOutYaw, TargetYaw);
        const float BrakingSpeed = FMath::Sqrt(2.f * AccelerationLimit * FMath::Abs(Error));
        const float DesiredVelocity = FMath::Sign(Error) * FMath::Min3(SpeedLimit, BrakingSpeed, FMath::Abs(Error) * Response);
        const float PreviousVelocity = InOutVelocity;
        InOutVelocity = FMath::FInterpConstantTo(InOutVelocity, DesiredVelocity, Step, AccelerationLimit);
        InOutYaw = FRotator::NormalizeAxis(InOutYaw + 0.5f * (PreviousVelocity + InOutVelocity) * Step);
    }
}

bool AGladiusRunCamera::InitializeCamera_Implementation()
{
    const ABattleUnit* Subject = GetSubject();
    if (!Subject) { return false; }
    AngularVelocity = 0.f;
    CurrentYaw = Subject->GetActorRotation().Yaw;
    if (const ABattleUnit* Boss = GetBattleManager()->GetBossUnit())
    {
        FVector Direction = Boss->GetActorLocation() - Subject->GetActorLocation();
        Direction.Z = 0.f;
        if (Direction.Normalize()) { CurrentYaw = Direction.Rotation().Yaw; }
    }
    GetDesiredYaw(CurrentYaw);
    return UpdatePose();
}

void AGladiusRunCamera::UpdateCamera_Implementation(float RealDeltaSeconds)
{
    float DesiredYaw = CurrentYaw;
    GetDesiredYaw(DesiredYaw); // 무게가 전부 0이거나 중심이 겹치면 현재 방향을 유지한다.
    AdvanceYaw(DesiredYaw, RealDeltaSeconds, MaxAngularSpeed, MaxAngularAcceleration, YawResponse, CurrentYaw, AngularVelocity);
    UpdatePose();
}

bool AGladiusRunCamera::UpdatePose()
{
    const ABattleUnit* Subject = GetSubject();
    if (!Subject || !FMath::IsFinite(CameraDistance) || CameraDistance < 1.f
        || !FMath::IsFinite(CameraPitch) || !FMath::IsFinite(FocusHeight)) { return false; }
    const FVector Focus = Subject->GetActorLocation() + FVector(0, 0, FocusHeight);
    const FRotator Rotation(FMath::Clamp(CameraPitch, -89.f, 89.f), CurrentYaw, 0.f);
    SetActorLocationAndRotation(Focus - Rotation.Vector() * CameraDistance, Rotation);
    return true;
}
