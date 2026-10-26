#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SViewport.h"

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
    bActiveIntro = bIntro;
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
    if (bIntro) ShowIntroLogo();
}

void ULevelTransitionCinematics::HandleFinished()
{
    // Sequence evaluation owns the player until its callback returns.
    bPlaybackPending = true;
    PlaybackTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ULevelTransitionCinematics::FinishPlayback);
}

void ULevelTransitionCinematics::FinishPlayback()
{
    const bool bFinishedIntro = bActiveIntro;
    FSimpleDelegate Finished = MoveTemp(Completion);
    Completion.Unbind();
    Cleanup();
    if (bFinishedIntro)
    {
        bIntroFinished = true;
        OnIntroFinished.Broadcast();
    }
    Finished.ExecuteIfBound();
}

void ULevelTransitionCinematics::Cleanup()
{
    RemoveIntroLogo();
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

void ULevelTransitionCinematics::ShowIntroLogo()
{
    RemoveIntroLogo();
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport) return;
    IntroLogoTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Cinematics/T_GladiusIntroLogo.T_GladiusIntroLogo"));
    if (!IntroLogoTexture)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Transition] Intro logo texture missing"));
        return;
    }
    IntroLogoBrush.SetResourceObject(IntroLogoTexture);
    IntroLogoBrush.ImageSize = FVector2D(IntroLogoTexture->GetSizeX(), IntroLogoTexture->GetSizeY());
    IntroLogoBrush.DrawAs = ESlateBrushDrawType::Image;
    // Both scenes use the same explicit viewport-relative logo width.
    IntroLogoOverlay = SNew(SBox)
        .Visibility(EVisibility::HitTestInvisible)
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride_UObject(this, &ULevelTransitionCinematics::GetIntroLogoWidth)
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFitX)
                [
                    SNew(SBox).WidthOverride(IntroLogoBrush.ImageSize.X).HeightOverride(IntroLogoBrush.ImageSize.Y)
                    [ SNew(SImage).Image(&IntroLogoBrush)
                      .ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f,1.f,1.f,GetIntroLogoOpacity()); }) ]
                ]
            ]
        ];
    IntroLogoViewport = Viewport;
    Viewport->AddViewportWidgetContent(IntroLogoOverlay.ToSharedRef(), 1000);
}

float ULevelTransitionCinematics::GetIntroLogoOpacity() const
{
    if (!SequencePlayer) return 0.f;
    const double Duration = SequencePlayer->GetDuration().AsSeconds();
    if (Duration <= 0.) return 0.f;
    const double Elapsed = SequencePlayer->GetCurrentTime().AsSeconds() - SequencePlayer->GetStartTime().AsSeconds();
    // Follow sequence time, including pause, seeking and playback-rate changes.
    const float Progress = static_cast<float>(Elapsed / Duration);
    if (Progress <= 0.f || Progress >= 0.85f) return 0.f;
    if (Progress < 0.2f) return FMath::SmoothStep(0.f, 0.2f, Progress);
    if (Progress <= 0.55f) return 1.f;
    return 1.f - FMath::SmoothStep(0.55f, 0.85f, Progress);
}

void ULevelTransitionCinematics::RemoveIntroLogo()
{
    if (IntroLogoOverlay.IsValid() && IntroLogoViewport.IsValid())
        IntroLogoViewport->RemoveViewportWidgetContent(IntroLogoOverlay.ToSharedRef());
    IntroLogoOverlay.Reset();
    IntroLogoViewport.Reset();
    IntroLogoBrush.SetResourceObject(nullptr);
    IntroLogoTexture = nullptr;
}

FOptionalSize ULevelTransitionCinematics::GetIntroLogoWidth() const
{
 if (IntroLogoViewport.IsValid())
  if (auto View = IntroLogoViewport->GetGameViewportWidget())
   return FOptionalSize(View->GetCachedGeometry().GetLocalSize().X * .5f);
 return FOptionalSize(960.f);
}
