#include "Scripts/Battle/CutSceneApplier.h"

#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/CutSceneCamera.h"
#include "Scripts/Battle/GladiusRunCamera.h"
#include "Scripts/Battle/GladiusSkill1Camera.h"
#include "Components/SceneComponent.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTimeController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCutSceneApplier, Log, All);

ACutSceneApplier::ACutSceneApplier()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    FCutSceneDefinition GladiusRun;
    GladiusRun.CameraClass = AGladiusRunCamera::StaticClass();
    CutScenes.Add(TEXT("GladiusRun"), GladiusRun);
    FCutSceneDefinition GladiusSkill1;
    GladiusSkill1.CameraClass = AGladiusSkill1Camera::StaticClass();
    CutScenes.Add(TEXT("GladiusSkill1"), GladiusSkill1);
}

ABattleUnit* ACutSceneApplier::GetCurrentSubject() const { return ActiveSubject.Get(); }

ABattleManager* ACutSceneApplier::ResolveBattleManager() const
{
    if (IsValid(BattleManager))
    {
        return BattleManager->GetWorld() == GetWorld() ? BattleManager.Get() : nullptr;
    }
    ABattleManager* Found = nullptr;
    if (!GetWorld()) { return nullptr; }
    for (TActorIterator<ABattleManager> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It)) { continue; }
        // 관리자가 여러 개라면 명시적으로 지정해야 한다. 다른 전투를 임의로 고르지 않는다.
        if (Found) { return nullptr; }
        Found = *It;
    }
    return Found;
}

ABattleUnit* ACutSceneApplier::SelectCharacter(const TMap<FString, FBattlePlayer>& Players,
    ECutSceneCharacterStrategy Strategy, const FVector& BossLocation, int32 RequiredClassIndex)
{
    const bool bClassStrategy = Strategy == ECutSceneCharacterStrategy::DefaultGladius
        || Strategy == ECutSceneCharacterStrategy::DefaultSera;
    if (bClassStrategy && RequiredClassIndex < 0) { return nullptr; }
    if (!StaticEnum<ECutSceneCharacterStrategy>()->IsValidEnumValue(static_cast<int64>(Strategy)))
    {
        return nullptr;
    }

    // TMap 순회 순서에 따라 동점 결과가 달라지지 않도록 UserId 순으로 고정한다.
    TArray<FString> Ids;
    Players.GetKeys(Ids);
    Ids.Sort();
    TArray<ABattleUnit*> Candidates;
    ABattleUnit* Best = nullptr;
    double BestScore = -TNumericLimits<double>::Max();
    for (const FString& Id : Ids)
    {
        const FBattlePlayer& Player = Players.FindChecked(Id);
        if (Player.HP <= 0 || Player.ReviveCountdown > 0 || !IsValid(Player.Unit)) { continue; }
        if (bClassStrategy && (Player.ClassIndex != RequiredClassIndex
            || (Player.PendingAction != EBattleAction::Attack1 && Player.PendingAction != EBattleAction::Attack2))) { continue; }
        if (Strategy == ECutSceneCharacterStrategy::RandomAlive)
        {
            Candidates.Add(Player.Unit);
            continue;
        }

        double Score = 0.0;
        switch (Strategy)
        {
        case ECutSceneCharacterStrategy::DefaultGladius:
        case ECutSceneCharacterStrategy::DefaultSera:
        case ECutSceneCharacterStrategy::HighestFever:
            Score = Player.Fever;
            break;
        case ECutSceneCharacterStrategy::LowestHealthRatio:
            if (Player.MaxHP <= 0) { continue; }
            Score = -static_cast<double>(Player.HP) / Player.MaxHP;
            break;
        case ECutSceneCharacterStrategy::ClosestToBoss:
            Score = -FVector::DistSquared(Player.Unit->GetActorLocation(), BossLocation);
            break;
        default:
            return nullptr;
        }
        if (FMath::IsFinite(Score) && (!Best || Score > BestScore))
        {
            Best = Player.Unit;
            BestScore = Score;
        }
    }
    return Candidates.IsEmpty() ? Best : Candidates[FMath::RandHelper(Candidates.Num())];
}

ABattleUnit* ACutSceneApplier::FindCharacter(ECutSceneCharacterStrategy Strategy) const
{
    const ABattleManager* Manager = ResolveBattleManager();
    if (!Manager) { return nullptr; }
    const ABattleUnit* Boss = Manager->GetBossUnit();
    if (!IsValid(Boss)) { return nullptr; }
    int32 ClassIndex = INDEX_NONE;
    if (Strategy == ECutSceneCharacterStrategy::DefaultGladius || Strategy == ECutSceneCharacterStrategy::DefaultSera)
    {
        const FString ClassId = Strategy == ECutSceneCharacterStrategy::DefaultGladius ? TEXT("gladius") : TEXT("sera");
        // 배열의 고정 위치 대신 직업 ID로 찾는다. 미등록 직업을 다른 직업으로 대체하지 않는다.
        ClassIndex = Manager->Classes.IndexOfByPredicate([&ClassId](const FCharacterDef& Definition) { return Definition.Id == ClassId; });
        if (ClassIndex == INDEX_NONE) { return nullptr; }
    }
    return SelectCharacter(Manager->GetBattlePlayers(), Strategy, Boss->GetActorLocation(), ClassIndex);
}

FTransform ACutSceneApplier::MakeOrigin(const AActor& Subject, const AActor& Boss)
{
    FVector Forward = Boss.GetActorLocation() - Subject.GetActorLocation();
    Forward.Z = 0.f;
    if (!Forward.Normalize())
    {
        Forward = Subject.GetActorForwardVector();
        Forward.Z = 0.f;
        if (!Forward.Normalize()) { Forward = FVector::ForwardVector; }
    }
    return FTransform(Forward.Rotation(), Subject.GetActorLocation(), FVector::OneVector);
}

bool ACutSceneApplier::ApplyCutScene(ECutSceneCharacterStrategy Strategy, FName CutSceneType)
{
    if (bApplying || bFinishing || bEndingPlay || !GetWorld() || !GetWorld()->IsGameWorld()) { return false; }
    // 시간 배율/카메라는 월드 공용이므로 다른 Applier와 중복 소유하지 않는다.
    for (TActorIterator<ACutSceneApplier> It(GetWorld()); It; ++It)
    {
        if (*It != this && (It->IsPlaying() || It->bApplying)) { return false; }
    }
    // 이전 컷신의 종료 콜백이 교체 도중 또 다른 요청을 끼워 넣지 않도록 한다.
    TGuardValue<bool> ApplyingGuard(bApplying, true);
    const FCutSceneDefinition* Definition = CutScenes.Find(CutSceneType);
    ABattleManager* Manager = ResolveBattleManager();
    if (!Definition || !Manager
        || (IsValid(Definition->Sequence) == (Definition->CameraClass != nullptr))
        || !FMath::IsFinite(Definition->WorldTimeScale)
        || Definition->WorldTimeScale < 0.01f || Definition->WorldTimeScale > 1.f
        || (!Definition->SubjectBindingTag.IsNone()
            && Definition->SubjectBindingTag == Definition->BossBindingTag))
    {
        UE_LOG(LogCutSceneApplier, Warning, TEXT("CutScene '%s': invalid configuration or BattleManager."), *CutSceneType.ToString());
        return false;
    }
    ABattleUnit* Boss = Manager->GetBossUnit();
    if (!IsValid(Boss)) { return false; }
    ABattleUnit* Subject = FindCharacter(Strategy);
    if (!Subject) { return false; }

    // 시간 배율을 바꾸기 전에 재생에 필요한 데이터를 먼저 확인한다.
    if (Definition->Sequence)
    {
        UMovieScene* MovieScene = Definition->Sequence->GetMovieScene();
        if (!MovieScene || MovieScene->GetPlaybackRange().IsEmpty()
            || !MovieScene->GetPlaybackRange().HasLowerBound()
            || !MovieScene->GetPlaybackRange().HasUpperBound()) { return false; }
    }
    const FCutSceneDefinition NextDefinition = *Definition;
    const TWeakObjectPtr<ABattleUnit> NextSubject = Subject;
    const TWeakObjectPtr<ABattleUnit> NextBoss = Boss;
    if (bPlaying) { FinishCutScene(true); }
    if (bEndingPlay || !NextSubject.IsValid() || !NextBoss.IsValid()) { return false; }
    ActiveDefinition = NextDefinition;
    ActiveType = CutSceneType;
    if (StartShot(NextSubject.Get(), NextBoss.Get())) { return true; }
    ActiveDefinition = FCutSceneDefinition();
    ActiveType = NAME_None;
    return false;
}

bool ACutSceneApplier::StartShot(ABattleUnit* Subject, ABattleUnit* Boss)
{
    if (!IsValid(Subject) || !IsValid(Boss) || bEndingPlay) { return false; }
    const FCutSceneDefinition& Definition = ActiveDefinition;
    const FName CutSceneType = ActiveType;
    const float PreviousEffectiveDilation = GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
    if (!FMath::IsFinite(PreviousEffectiveDilation) || PreviousEffectiveDilation <= SMALL_NUMBER) { return false; }
    ULevelSequencePlayer* NewPlayer = nullptr;
    if (Definition.CameraClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ProceduralCamera = GetWorld()->SpawnActor<ACutSceneCamera>(Definition.CameraClass, FTransform::Identity, SpawnParams);
        if (!IsValid(ProceduralCamera) || !ProceduralCamera->StartCutScene(ResolveBattleManager(), Subject))
        {
            if (IsValid(ProceduralCamera)) { ProceduralCamera->Destroy(); }
            ProceduralCamera = nullptr;
            return false;
        }
        ProceduralCamera->AddTickPrerequisiteActor(Subject);
    }
    else
    {
        FMovieSceneSequencePlaybackSettings Settings;
        Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;
        ALevelSequenceActor* NewSequenceActor = nullptr;
        NewPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Definition.Sequence, Settings, NewSequenceActor);
        if (!IsValid(NewPlayer) || !IsValid(NewSequenceActor))
        {
            if (IsValid(NewSequenceActor)) { NewSequenceActor->Destroy(); }
            return false;
        }
        SequenceActor = NewSequenceActor;
        SequencePlayer = NewPlayer;
        SetActorTransform(MakeOrigin(*Subject, *Boss));
        SequenceActor->bOverrideInstanceData = true;
        UDefaultLevelSequenceInstanceData* InstanceData = NewObject<UDefaultLevelSequenceInstanceData>(SequenceActor);
        InstanceData->TransformOriginActor = this;
        SequenceActor->DefaultInstanceData = InstanceData;
        if (!Definition.SubjectBindingTag.IsNone())
        {
            SequenceActor->SetBindingByTag(Definition.SubjectBindingTag, TArray<AActor*>{ Subject }, false);
        }
        if (!Definition.BossBindingTag.IsNone())
        {
            SequenceActor->SetBindingByTag(Definition.BossBindingTag, TArray<AActor*>{ Boss }, false);
        }
        SequencePlayer->SetTimeController(MakeShared<FMovieSceneTimeController_Tick>());
    }
    CameraController = UGameplayStatics::GetPlayerController(this, 0);
    PreviousViewTarget = CameraController.IsValid() ? CameraController->GetViewTarget() : nullptr;
    ActiveSubject = Subject;
    ActiveBoss = Boss;
    SavedGlobalDilation = UGameplayStatics::GetGlobalTimeDilation(this);
    SavedSubjectDilation = Subject->CustomTimeDilation;
    bPlaying = true;

    UGameplayStatics::SetGlobalTimeDilation(this, SavedGlobalDilation * Definition.WorldTimeScale);
    const float EffectiveDilation = GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
    if (!FMath::IsFinite(EffectiveDilation) || EffectiveDilation <= SMALL_NUMBER)
    {
        FinishCutScene(true, false);
        return false;
    }
    Subject->CustomTimeDilation = SavedSubjectDilation * PreviousEffectiveDilation / EffectiveDilation;
    Subject->OnDestroyed.AddDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
    Boss->OnDestroyed.AddDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
    if (ProceduralCamera)
    {
        ProceduralCamera->CustomTimeDilation = 1.f / EffectiveDilation;
        ProceduralCamera->OnFinished.AddDynamic(this, &ACutSceneApplier::HandleCameraFinished);
        ProceduralCamera->OnDestroyed.AddDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
        if (CameraController.IsValid()) { CameraController->SetViewTarget(ProceduralCamera); }
    }
    else
    {
        SequencePlayer->SetPlayRate(1.f / EffectiveDilation);
        SequencePlayer->OnFinished.AddDynamic(this, &ACutSceneApplier::HandleSequenceFinished);
        SequencePlayer->OnStop.AddDynamic(this, &ACutSceneApplier::HandleSequenceStopped);
        SequenceActor->OnDestroyed.AddDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
    }
    OnCutSceneApplied.Broadcast(CutSceneType, Subject);
    if (bPlaying && NewPlayer && SequencePlayer == NewPlayer) { SequencePlayer->Play(); }
    return true;
}


void ACutSceneApplier::CancelCutScene()
{
    FinishCutScene(true);
}

void ACutSceneApplier::HandleSequenceFinished() { FinishCutScene(false); }
void ACutSceneApplier::HandleCameraFinished(bool bInterrupted) { FinishCutScene(bInterrupted); }
void ACutSceneApplier::HandleSequenceStopped()
{
    // UE는 자연 종료에서도 OnStop을 먼저 보내고 OnFinished를 뒤이어 보낸다.
    // 다음 틱까지 기다린 후, 그 시퀀스가 여전히 현재 시퀀스일 때만 외부 중단으로 처리한다.
    if (bFinishing || !bPlaying || !GetWorld()) { return; }
    const TWeakObjectPtr<ULevelSequencePlayer> StoppedPlayer = SequencePlayer;
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, StoppedPlayer]()
    {
        if (bPlaying && SequencePlayer == StoppedPlayer.Get()) { FinishCutScene(true); }
    }));
}
void ACutSceneApplier::HandleActorDestroyed(AActor* DestroyedActor) { FinishCutScene(true); }

void ACutSceneApplier::FinishCutScene(bool bInterrupted, bool bNotify)
{
    if (!bPlaying || bFinishing) { return; }
    bFinishing = true; // 정리 중의 Stop / Destroy 이벤트와 새 재생 요청을 막는다.
    const FName FinishedType = ActiveType;
    if (SequencePlayer)
    {
        SequencePlayer->OnFinished.RemoveDynamic(this, &ACutSceneApplier::HandleSequenceFinished);
        SequencePlayer->OnStop.RemoveDynamic(this, &ACutSceneApplier::HandleSequenceStopped);
        SequencePlayer->Stop();
    }
    if (IsValid(ProceduralCamera))
    {
        ProceduralCamera->OnFinished.RemoveDynamic(this, &ACutSceneApplier::HandleCameraFinished);
        ProceduralCamera->OnDestroyed.RemoveDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
        ProceduralCamera->StopCutScene();
    }
    if (ActiveSubject.IsValid())
    {
        ActiveSubject->OnDestroyed.RemoveDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
        ActiveSubject->CustomTimeDilation = SavedSubjectDilation;
    }
    if (ActiveBoss.IsValid()) { ActiveBoss->OnDestroyed.RemoveDynamic(this, &ACutSceneApplier::HandleActorDestroyed); }
    if (GetWorld()) { UGameplayStatics::SetGlobalTimeDilation(this, SavedGlobalDilation); }
    if (CameraController.IsValid() && PreviousViewTarget.IsValid())
    {
        CameraController->SetViewTarget(PreviousViewTarget.Get());
    }
    if (IsValid(SequenceActor))
    {
        SequenceActor->OnDestroyed.RemoveDynamic(this, &ACutSceneApplier::HandleActorDestroyed);
        SequenceActor->Destroy();
    }
    if (IsValid(ProceduralCamera)) { ProceduralCamera->Destroy(); }
    ProceduralCamera = nullptr;
    SequencePlayer = nullptr;
    SequenceActor = nullptr;
    ActiveSubject.Reset();
    ActiveBoss.Reset();
    CameraController.Reset();
    PreviousViewTarget.Reset();
    bFinishing = false;
    ActiveDefinition = FCutSceneDefinition();
    ActiveType = NAME_None;
    bPlaying = false;
    if (bNotify) { OnCutSceneEnded.Broadcast(FinishedType, bInterrupted); }
}

void ACutSceneApplier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    FinishCutScene(true, false);
    Super::EndPlay(EndPlayReason);
}
