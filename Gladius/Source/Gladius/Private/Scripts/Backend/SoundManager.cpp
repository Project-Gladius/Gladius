#include "Scripts/Backend/SoundManager.h"

#include "Engine/GameInstance.h"
#include "FMODStudioModule.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "fmod_errors.h"
#include "fmod_studio.hpp"

DEFINE_LOG_CATEGORY_STATIC(LogSoundManager, Log, All);

namespace
{
bool CheckFMOD(FMOD_RESULT Result, const TCHAR* Operation)
{
	if (Result != FMOD_OK)
	{
		UE_LOG(LogSoundManager, Warning, TEXT("%s: %s"), Operation, UTF8_TO_TCHAR(FMOD_ErrorString(Result)));
		return false;
	}
	return true;
}

FString GetEventPath(FMOD::Studio::EventDescription* Description)
{
	int32 RequiredSize = 0;
	const FMOD_RESULT Result = Description->getPath(nullptr, 0, &RequiredSize);
	if ((Result != FMOD_OK && Result != FMOD_ERR_TRUNCATED) || RequiredSize <= 0)
	{
		return FString();
	}
	TArray<char> Buffer;
	Buffer.SetNumZeroed(RequiredSize);
	return Description->getPath(Buffer.GetData(), RequiredSize, nullptr) == FMOD_OK
		? FString(UTF8_TO_TCHAR(Buffer.GetData())) : FString();
}

void StopAndRelease(FMOD::Studio::EventInstance* Instance)
{
	if (Instance && Instance->isValid())
	{
		CheckFMOD(Instance->stop(FMOD_STUDIO_STOP_IMMEDIATE), TEXT("Stop event"));
		CheckFMOD(Instance->release(), TEXT("Release event"));
	}
}
}

USoundManager* USoundManager::Get(const UObject* WorldContextObject)
{
	UGameInstance* GameInstance = WorldContextObject ? UGameplayStatics::GetGameInstance(WorldContextObject) : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USoundManager>() : nullptr;
}

void USoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bShuttingDown = false;
#if WITH_EDITOR
	// Release handles before the plugin destroys its runtime system at the end of PIE.
	PreEndPIEHandle = IFMODStudioModule::Get().PreEndPIEEvent().AddUObject(this, &USoundManager::ShutdownAudio);
#endif
}

void USoundManager::Deinitialize()
{
	ShutdownAudio();
#if WITH_EDITOR
	if (IFMODStudioModule::IsAvailable())
	{
		IFMODStudioModule::Get().PreEndPIEEvent().Remove(PreEndPIEHandle);
	}
	PreEndPIEHandle.Reset();
#endif
	Super::Deinitialize();
}

FMOD::Studio::System* USoundManager::GetRuntimeSystem() const
{
	return IFMODStudioModule::IsAvailable()
		? IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime) : nullptr;
}

FMOD::Studio::EventDescription* USoundManager::FindEvent(const FString& Name, const TCHAR* Category, FString& OutPath) const
{
	OutPath.Reset();
	FMOD::Studio::System* System = GetRuntimeSystem();
	if (bShuttingDown || !System || Name.IsEmpty())
	{
		UE_LOG(LogSoundManager, Warning, TEXT("Cannot play '%s': FMOD runtime is unavailable or the event name is empty."), *Name);
		return nullptr;
	}

	FMOD::Studio::EventDescription* Description = nullptr;
	TArray<FString> Candidates;
	if (Name.StartsWith(TEXT("event:/")))
	{
		Candidates.Add(Name);
	}
	else
	{
		Candidates.Add(FString::Printf(TEXT("event:/%s/%s"), Category, *Name));
		Candidates.Add(TEXT("event:/") + Name);
	}
	for (const FString& Path : Candidates)
	{
		if (System->getEvent(TCHAR_TO_UTF8(*Path), &Description) == FMOD_OK)
		{
			OutPath = GetEventPath(Description);
			return OutPath.IsEmpty() ? nullptr : Description;
		}
	}

	// Banks do not imply event folders. Also accept an unambiguous leaf name anywhere.
	if (!Name.Contains(TEXT("/")))
	{
		int32 BankCount = 0;
		System->getBankCount(&BankCount);
		TArray<FMOD::Studio::Bank*> Banks;
		Banks.SetNumZeroed(BankCount);
		if (BankCount > 0 && System->getBankList(Banks.GetData(), BankCount, &BankCount) == FMOD_OK)
		{
			for (FMOD::Studio::Bank* Bank : Banks)
			{
				if (!Bank) { continue; }
				int32 EventCount = 0;
				Bank->getEventCount(&EventCount);
				TArray<FMOD::Studio::EventDescription*> Events;
				Events.SetNumZeroed(EventCount);
				if (EventCount == 0 || Bank->getEventList(Events.GetData(), EventCount, &EventCount) != FMOD_OK) { continue; }
				for (FMOD::Studio::EventDescription* Event : Events)
				{
					if (!Event) { continue; }
					const FString Path = GetEventPath(Event);
					if (FPaths::GetCleanFilename(Path) != Name) { continue; }
					if (!OutPath.IsEmpty() && OutPath != Path)
					{
						UE_LOG(LogSoundManager, Warning, TEXT("Ambiguous FMOD event '%s'; supply its full event:/ path."), *Name);
						return nullptr;
					}
					Description = Event;
					OutPath = Path;
				}
			}
		}
		if (!OutPath.IsEmpty()) { return Description; }
	}
	UE_LOG(LogSoundManager, Warning, TEXT("FMOD event '%s' was not found. Build/load its bank and Master.strings.bank."), *Name);
	return nullptr;
}

void USoundManager::ApplyBGMState(FMOD::Studio::EventDescription* Description, FMOD::Studio::EventInstance* Instance,
	const FString& EventPath, int32 State) const
{
	const FString ParameterName = FPaths::GetCleanFilename(EventPath) + TEXT("State");
	FMOD_STUDIO_PARAMETER_DESCRIPTION Parameter = {};
	// Missing parameters are expected for BGM that does not use state transitions.
	if (Description->getParameterDescriptionByName(TCHAR_TO_UTF8(*ParameterName), &Parameter) == FMOD_OK)
	{
		CheckFMOD(Instance->setParameterByID(Parameter.id, static_cast<float>(State)), *ParameterName);
	}
}

bool USoundManager::StartBGM(const FString& Name, int32 State)
{
	FString Path;
	FMOD::Studio::EventDescription* Description = FindEvent(Name, TEXT("BGM"), Path);
	if (!Description) { return false; }

	if (FMOD::Studio::EventInstance** Existing = BGMInstances.Find(Path))
	{
		FMOD_STUDIO_PLAYBACK_STATE PlaybackState = FMOD_STUDIO_PLAYBACK_STOPPED;
		if ((*Existing)->isValid() && (*Existing)->getPlaybackState(&PlaybackState) == FMOD_OK
			&& PlaybackState != FMOD_STUDIO_PLAYBACK_STOPPED)
		{
			ApplyBGMState(Description, *Existing, Path, State);
			return true;
		}
		StopAndRelease(*Existing);
		BGMInstances.Remove(Path);
	}

	FMOD::Studio::EventInstance* Instance = nullptr;
	if (!CheckFMOD(Description->createInstance(&Instance), TEXT("Create BGM"))) { return false; }
	ApplyBGMState(Description, Instance, Path, State);
	if (!CheckFMOD(Instance->start(), TEXT("Start BGM")))
	{
		StopAndRelease(Instance);
		return false;
	}
	BGMInstances.Add(Path, Instance);
	return true;
}

void USoundManager::StopBGM(const FString& Name)
{
	if (!GetRuntimeSystem()) { BGMInstances.Reset(); return; }
	FString Path;
	// Resolve against active instances, so stopping does not depend on bank lookup.
	for (const FString& Candidate : { Name, TEXT("event:/BGM/") + Name, TEXT("event:/") + Name })
	{
		FMOD::Studio::EventInstance* Instance = nullptr;
		if (BGMInstances.RemoveAndCopyValue(Candidate, Instance))
		{
			StopAndRelease(Instance);
			return;
		}
	}
	for (const auto& Entry : BGMInstances)
	{
		if (!Name.Contains(TEXT("/")) && FPaths::GetCleanFilename(Entry.Key) == Name)
		{
			if (!Path.IsEmpty())
			{
				UE_LOG(LogSoundManager, Warning, TEXT("Ambiguous active BGM '%s'; supply its full event:/ path."), *Name);
				return;
			}
			Path = Entry.Key;
		}
	}
	FMOD::Studio::EventInstance* Instance = nullptr;
	if (BGMInstances.RemoveAndCopyValue(Path, Instance)) { StopAndRelease(Instance); }
}

void USoundManager::StopAllBGM()
{
	if (GetRuntimeSystem())
	{
		for (const auto& Entry : BGMInstances) { StopAndRelease(Entry.Value); }
	}
	BGMInstances.Reset();
}

void USoundManager::PruneFinishedSFX()
{
	for (int32 Index = SFXInstances.Num() - 1; Index >= 0; --Index)
	{
		FMOD::Studio::EventInstance* Instance = SFXInstances[Index];
		FMOD_STUDIO_PLAYBACK_STATE State = FMOD_STUDIO_PLAYBACK_STOPPED;
		if (!Instance->isValid()) { SFXInstances.RemoveAtSwap(Index); }
		else if (Instance->getPlaybackState(&State) == FMOD_OK && State == FMOD_STUDIO_PLAYBACK_STOPPED)
		{
			CheckFMOD(Instance->release(), TEXT("Release finished SFX"));
			SFXInstances.RemoveAtSwap(Index);
		}
	}
}

bool USoundManager::StartSFX(const FString& Name)
{
	FString Path;
	FMOD::Studio::EventDescription* Description = FindEvent(Name, TEXT("SFX"), Path);
	if (!Description) { return false; }
	PruneFinishedSFX();
	FMOD::Studio::EventInstance* Instance = nullptr;
	if (!CheckFMOD(Description->createInstance(&Instance), TEXT("Create SFX"))) { return false; }
	if (!CheckFMOD(Instance->start(), TEXT("Start SFX")))
	{
		StopAndRelease(Instance);
		return false;
	}
	SFXInstances.Add(Instance);
	return true;
}

void USoundManager::ShutdownAudio()
{
	bShuttingDown = true;
	StopAllBGM();
	if (GetRuntimeSystem())
	{
		for (FMOD::Studio::EventInstance* Instance : SFXInstances) { StopAndRelease(Instance); }
	}
	SFXInstances.Reset();
}
