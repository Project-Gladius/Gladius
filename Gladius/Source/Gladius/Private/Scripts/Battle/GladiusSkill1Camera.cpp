#include "Scripts/Battle/GladiusSkill1Camera.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/CutSceneApplier.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGladiusSkill1, Log, All);

namespace
{
TArray<UMeshComponent*> GetBodyMeshes(const ABattleUnit& Subject, FName ComponentName)
{
    TArray<AActor*> Actors;
    Subject.GetAttachedActors(Actors, true, true);
    Actors.Add(const_cast<ABattleUnit*>(&Subject));
    TArray<UMeshComponent*> Result;
    for (AActor* Actor : Actors)
    {
        if (!IsValid(Actor) || Actor->IsHidden()) { continue; }
        TArray<UMeshComponent*> Components;
        Actor->GetComponents<UMeshComponent>(Components);
        for (UMeshComponent* Component : Components)
        {
            if (!IsValid(Component) || !Component->IsRegistered() || !Component->IsVisible() || Component->bHiddenInGame
                || (!ComponentName.IsNone() && Component->GetFName() != ComponentName)) { continue; }
            if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Component))
            {
                if (Static->GetStaticMesh()) { Result.Add(Component); }
            }
            else if (const USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Component))
            {
                if (Skinned->GetSkinnedAsset()) { Result.Add(Component); }
            }
        }
    }
    return Result;
}
}

bool AGladiusSkill1Camera::MeasureVisibleMeshTop(const ABattleUnit& Subject, FName ComponentName, float& OutHeight)
{
    bool bFound = false;
    double HighestZ = -TNumericLimits<double>::Max();
    for (UMeshComponent* Component : GetBodyMeshes(Subject, ComponentName))
    {
        const FBox Box = Component->CalcBounds(Component->GetComponentTransform()).GetBox();
        if (!Box.IsValid || !FMath::IsFinite(Box.Max.Z)) { continue; }
        HighestZ = FMath::Max(HighestZ, Box.Max.Z);
        bFound = true;
    }
    OutHeight = bFound ? static_cast<float>(HighestZ - Subject.GetGroundedVisualOrigin().Z) : 0.f;
    return bFound;
}

FTransform AGladiusSkill1Camera::CalculatePose(const FTransform& Frame, float InDistance, float InHeight,
    float InRightAngle, float InDelta, float InDuckRatio, float InTiltRatio)
{
    const FVector Origin = Frame.GetLocation();
    const float Angle = Frame.Rotator().Yaw + InRightAngle;
    const FVector Horizontal = FRotator(0.f, Angle, 0.f).Vector() * InDistance;
    const FVector Position = Origin + Horizontal + FVector(0, 0, InHeight - InDelta * InDuckRatio);
    const FVector Focus = Origin + FVector(0, 0, InHeight - InDelta * InTiltRatio);
    return FTransform((Focus - Position).Rotation(), Position, FVector::OneVector);
}

bool AGladiusSkill1Camera::SampleHeight(float& OutHeight) const
{
    const ABattleUnit* Subject = GetSubject();
    if (!Subject) { return false; }
    if (bUsingBone)
    {
        if (!BoneComponent.IsValid() || BoneComponent->GetBoneIndex(PelvisBoneName) == INDEX_NONE) { return false; }
        const FVector Position = BoneComponent->GetSocketLocation(PelvisBoneName);
        if (Position.ContainsNaN()) { return false; }
        OutHeight = static_cast<float>(Position.Z - Subject->GetGroundedVisualOrigin().Z);
        return true;
    }
    return MeasureVisibleMeshTop(*Subject, HeightSourceComponentName, OutHeight);
}

bool AGladiusSkill1Camera::InitializeCamera_Implementation()
{
    const ABattleUnit* Subject = GetSubject();
    if (!Subject || !FMath::IsFinite(Distance) || Distance < 1.f || !FMath::IsFinite(Height)
        || !FMath::IsFinite(RightAngle) || !FMath::IsFinite(DuckRatio) || DuckRatio < 0.f
        || !FMath::IsFinite(TiltRatio) || TiltRatio < 0.f) { return false; }
    BoneComponent.Reset();
    bUsingBone = false;
    for (UMeshComponent* Component : GetBodyMeshes(*Subject, HeightSourceComponentName))
    {
        USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Component);
        if (!Skinned || PelvisBoneName.IsNone() || Skinned->GetBoneIndex(PelvisBoneName) == INDEX_NONE) { continue; }
        if (BoneComponent.IsValid())
        {
            UE_LOG(LogGladiusSkill1, Warning, TEXT("Multiple pelvis sources. Set HeightSourceComponentName to select one mesh."));
            return false;
        }
        BoneComponent = Skinned;
    }
    bUsingBone = BoneComponent.IsValid();
    if (!SampleHeight(InitialCharacterHeight))
    {
        UE_LOG(LogGladiusSkill1, Warning, TEXT("No pelvis or visible mesh height found for %s."), *Subject->GetName());
        return false;
    }
    CharacterHeight = InitialCharacterHeight;
    Delta = 0.f;
    if (BoneComponent.IsValid())
    {
        AddTickPrerequisiteComponent(BoneComponent.Get());
        if (USkeletalMeshComponent* Animated = Cast<USkeletalMeshComponent>(BoneComponent.Get()))
        {
            AnimatedComponent = Animated;
            PreviousAnimTickOption = Animated->VisibilityBasedAnimTickOption;
            bPreviousUpdateRateOptimizations = Animated->bEnableUpdateRateOptimizations;
            Animated->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Animated->bEnableUpdateRateOptimizations = false;
        }
    }
    UpdatePose();
    return true;
}

void AGladiusSkill1Camera::UpdateCamera_Implementation(float RealDeltaSeconds)
{
    if (!SampleHeight(CharacterHeight))
    {
        UE_LOG(LogGladiusSkill1, Warning, TEXT("Height source disappeared during GladiusSkill1; stopping camera."));
        StopCutScene();
        OnFinished.Broadcast(true);
        return;
    }
    Delta = FMath::Max(0.f, InitialCharacterHeight - CharacterHeight);
    UpdatePose();
}

void AGladiusSkill1Camera::UpdatePose()
{
    ABattleUnit* Subject = GetSubject();
    if (!Subject) { return; }
    const ABattleUnit* Boss = GetBattleManager()->GetBossUnit();
    FTransform Frame(FRotator(0.f, Subject->GetActorRotation().Yaw, 0.f), Subject->GetGroundedVisualOrigin());
    if (IsValid(Boss)) { Frame = ACutSceneApplier::MakeGroundedOrigin(*Subject, *Boss); }
    SetActorTransform(CalculatePose(Frame, Distance, Height, RightAngle, Delta, DuckRatio, TiltRatio));
}

void AGladiusSkill1Camera::StopCutScene()
{
    if (AnimatedComponent.IsValid())
    {
        AnimatedComponent->VisibilityBasedAnimTickOption = PreviousAnimTickOption;
        AnimatedComponent->bEnableUpdateRateOptimizations = bPreviousUpdateRateOptimizations;
        AnimatedComponent.Reset();
    }
    if (BoneComponent.IsValid()) { RemoveTickPrerequisiteComponent(BoneComponent.Get()); }
    Super::StopCutScene();
}

void AGladiusSkill1Camera::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCutScene();
    Super::EndPlay(EndPlayReason);
}
