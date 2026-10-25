#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "TimerManager.h"

bool ULevelTransitionCinematics::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void ULevelTransitionCinematics::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    // Camera auto-activation and actor BeginPlay must finish before taking the view.
    AutoIntroTimer = InWorld.GetTimerManager().SetTimerForNextTick(this, &ULevelTransitionCinematics::AutoPlayIntro);
}

void ULevelTransitionCinematics::AutoPlayIntro()
{
    const URealTimeSettings* Settings = GetDefault<URealTimeSettings>();
    if (URealTimeSettings::MatchesMap(GetWorld(), Settings->TempleMap)) PlayIntro(false);
    else if (URealTimeSettings::MatchesMap(GetWorld(), Settings->BattleMap)) PlayIntro(true);
}

void ULevelTransitionCinematics::PlayIntro(bool bBattle, FSimpleDelegate Finished)
{
    if (bIntroRequested || bOutroRequested) return;
    bIntroRequested = true;
    Completion = MoveTemp(Finished);
    bPlaybackPending = true;
    PlaybackTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, bBattle]()
    {
        Play(bBattle, true);
    }));
}

void ULevelTransitionCinematics::PlayOutro(bool bBattle, FSimpleDelegate Finished)
{
    if (bOutroRequested) return;
    bOutroRequested = true;
    // An immediate travel request can interrupt the intro, but never skip the outro.
    Completion.Unbind();
    Cleanup();
    Completion = MoveTemp(Finished);
    Play(bBattle, false);
}

bool ULevelTransitionCinematics::IsPlaying() const
{
    return bPlaybackPending || IsValid(SequencePlayer);
}

void ULevelTransitionCinematics::Play(bool bBattle, bool bIntro)
{
    bPlaybackPending = false;
    const FString Name = FString(bBattle ? TEXT("Battle_") : TEXT("Temple_")) + (bIntro ? TEXT("Intro") : TEXT("Outro"));
    const FString Path = FString(TEXT("/Game/Cinematics/")) + (bIntro ? TEXT("IntroSeq/") : TEXT("OutroSeq/")) + Name + TEXT(".") + Name;
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
    if (!Sequence || !Sequence->GetMovieScene() || Sequence->GetMovieScene()->GetPlaybackRange().IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[Transition] Missing or empty sequence: %s"), *Path);
        HandleFinished();
        return;
    }
    FMovieSceneSequencePlaybackSettings Settings;
    Settings.bAutoPlay = false;
    Settings.LoopCount.Value = 0;
    Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;
    ALevelSequenceActor* Actor = nullptr;
    SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, Actor);
    SequenceActor = Actor;
    if (!SequencePlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("[Transition] Failed to create player: %s"), *Path);
        HandleFinished();
        return;
    }
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    PreviousViewTarget = PC ? PC->GetViewTarget() : nullptr;
    SequencePlayer->OnFinished.AddUniqueDynamic(this, &ULevelTransitionCinematics::HandleFinished);
    UE_LOG(LogTemp, Log, TEXT("[Transition] Playing %s"), *Name);
    // Preserve the authored playback range (Battle_Intro starts at frame -23).
    SequencePlayer->Play();
}

void ULevelTransitionCinematics::HandleFinished()
{
    // Sequence evaluation owns the player until its callback returns.
    bPlaybackPending = true;
    PlaybackTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ULevelTransitionCinematics::FinishPlayback);
}

void ULevelTransitionCinematics::FinishPlayback()
{
    FSimpleDelegate Finished = MoveTemp(Completion);
    Completion.Unbind();
    Cleanup();
    Finished.ExecuteIfBound();
}

void ULevelTransitionCinematics::Cleanup()
{
    GetWorld()->GetTimerManager().ClearTimer(PlaybackTimer);
    bPlaybackPending = false;
    if (SequencePlayer)
    {
        SequencePlayer->OnFinished.RemoveDynamic(this, &ULevelTransitionCinematics::HandleFinished);
        SequencePlayer->Stop();
    }
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (PreviousViewTarget.IsValid()) PC->SetViewTarget(PreviousViewTarget.Get());
    PreviousViewTarget.Reset();
    if (IsValid(SequenceActor)) SequenceActor->Destroy();
    SequenceActor = nullptr;
    SequencePlayer = nullptr;
}

void ULevelTransitionCinematics::Deinitialize()
{
    Completion.Unbind();
    GetWorld()->GetTimerManager().ClearTimer(AutoIntroTimer);
    Cleanup();
    Super::Deinitialize();
}
