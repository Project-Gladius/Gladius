#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelTransitionCinematics.generated.h"

class ALevelSequenceActor;
class ULevelSequencePlayer;

// Per-world ownership prevents duplicate intros and overlapping level travel requests.
UCLASS()
class GLADIUS_API ULevelTransitionCinematics : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    void PlayIntro(bool bBattle, FSimpleDelegate Finished = FSimpleDelegate());
    void PlayOutro(bool bBattle, FSimpleDelegate Finished);
    bool IsPlaying() const;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
    friend struct FLevelTransitionTestAccess;
    UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
    UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> SequencePlayer;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    FSimpleDelegate Completion;
    FTimerHandle PlaybackTimer;
    FTimerHandle AutoIntroTimer;
    bool bIntroRequested = false;
    bool bOutroRequested = false;
    bool bPlaybackPending = false;
    void AutoPlayIntro();
    void Play(bool bBattle, bool bIntro);
    void Cleanup();
    void FinishPlayback();
    UFUNCTION() void HandleFinished();
};
