#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SoundManager.generated.h"

namespace FMOD::Studio
{
class EventDescription;
class EventInstance;
class System;
}

/** One sound manager per game instance. Playback survives ordinary level travel. */
UCLASS()
class GLADIUS_API USoundManager final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Audio", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Sound Manager"))
	static USoundManager* Get(const UObject* WorldContextObject);

	/** Starts one instance per BGM event, or updates its optional <event name>State parameter. */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	bool StartBGM(const FString& Name = TEXT("DummyBGM"), int32 State = 0);

	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void StopBGM(const FString& Name);

	/** Linear fade in real seconds, independent of cutscene time dilation. */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void FadeOutBGM(const FString& Name, float Seconds = 3.f);

	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void StopAllBGM();

	/** SFX calls create independent instances, so repeated calls can overlap. */
	UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
	bool StartSFX(const FString& Name = TEXT("DummySFX"));

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FMOD::Studio::System* GetRuntimeSystem() const;
	FMOD::Studio::EventDescription* FindEvent(const FString& Name, const TCHAR* Category, FString& OutPath) const;
	void ApplyBGMState(FMOD::Studio::EventDescription* Description, FMOD::Studio::EventInstance* Instance,
		const FString& EventPath, int32 State) const;
	void PruneFinishedSFX();
	void ShutdownAudio();

	// FMOD owns these handles; we retain and release each instance explicitly.
	TMap<FString, FMOD::Studio::EventInstance*> BGMInstances;
	TArray<FMOD::Studio::EventInstance*> SFXInstances;
	bool bShuttingDown = false;
	struct FBGMFade
	{
		double StartedAt = 0.;
		float Seconds = 3.f;
		float InitialVolume = 1.f;
	};
	TMap<FString, FBGMFade> BGMFades;
	FTSTicker::FDelegateHandle FadeTickerHandle;
	bool TickBGMFades(float DeltaSeconds);
	friend struct FBattleAudioTestAccess;

#if WITH_EDITOR
	FDelegateHandle PreEndPIEHandle;
#endif
};
