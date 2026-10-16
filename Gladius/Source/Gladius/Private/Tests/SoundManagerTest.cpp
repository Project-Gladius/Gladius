#include "Scripts/Backend/SoundManager.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FMODStudioModule.h"
#include "Misc/AutomationTest.h"
#include "fmod_studio.hpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundManagerLifetimeTest, "Gladius.Audio.SoundManager.LifetimeAndPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSoundManagerLifetimeTest::RunTest(const FString& Parameters)
{
	IFMODStudioModule& FMOD = IFMODStudioModule::Get();
	if (FMOD.GetStudioSystem(EFMODSystemContext::Runtime))
	{
		AddWarning(TEXT("Run outside PIE: the test needs its own FMOD runtime."));
		return true;
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(TEXT("SoundManagerTestWorld")));
	USoundManager* Manager = GameInstance->GetSubsystem<USoundManager>();
	TestNotNull(TEXT("Automatically created with game instance"), Manager);
	if (!Manager)
	{
		UWorld* World = GameInstance->GetWorld();
		GameInstance->Shutdown();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		GameInstance->RemoveFromRoot();
		return false;
	}
	TestEqual(TEXT("World lookup returns the singleton"), USoundManager::Get(GameInstance->GetWorld()), Manager);

	FMOD.SetInPIE(true, false);
	FMOD::Studio::System* System = FMOD.GetStudioSystem(EFMODSystemContext::Runtime);
	FMOD::Studio::EventDescription* Event = nullptr;
	FMOD::Studio::Bus* Master = nullptr;
	if (System)
	{
		System->flushCommands();
		if (System->getBus("bus:/", &Master) == FMOD_OK)
		{
			Master->setMute(true);
			Master->setPaused(true);
		}
	}
	// The project's TestSFX event is an optional real-bank fixture with no state parameter.
	const bool bHasFixture = System && System->getEvent("event:/TestSFX", &Event) == FMOD_OK;
	if (bHasFixture)
	{
		TestTrue(TEXT("BGM without a state parameter still starts"), Manager->StartBGM(TEXT("TestSFX"), 7));
		System->flushCommands();
		TestTrue(TEXT("Repeat BGM updates rather than duplicating"), Manager->StartBGM(TEXT("event:/TestSFX"), 3));
		System->flushCommands();
		int32 Count = 0;
		Event->getInstanceCount(&Count);
		TestEqual(TEXT("Only one BGM instance"), Count, 1);
		TestTrue(TEXT("First SFX starts"), Manager->StartSFX(TEXT("TestSFX")));
		TestTrue(TEXT("Second SFX overlaps"), Manager->StartSFX(TEXT("TestSFX")));
		System->flushCommands();
		Event->getInstanceCount(&Count);
		TestEqual(TEXT("One BGM and two independent SFX"), Count, 3);
	}
	else
	{
		AddWarning(TEXT("TestSFX bank fixture unavailable; audio assertions skipped, lifetime assertions still run."));
	}

	UWorld* OldWorld = GameInstance->GetWorld();
	UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("SoundManagerNextWorld")));
	NewWorld->SetGameInstance(GameInstance);
	GEngine->GetWorldContextFromWorldChecked(OldWorld).SetCurrentWorld(NewWorld);
	OldWorld->DestroyWorld(false);
	TestEqual(TEXT("Singleton survives replacement and destruction of its original world"), USoundManager::Get(NewWorld), Manager);
	if (bHasFixture)
	{
		int32 Count = 0;
		System->flushCommands();
		Event->getInstanceCount(&Count);
		TestEqual(TEXT("All playback survives world replacement"), Count, 3);
		Manager->StopBGM(TEXT("TestSFX"));
		System->flushCommands();
		Event->getInstanceCount(&Count);
		TestEqual(TEXT("StopBGM leaves SFX alone"), Count, 2);
		Manager->StartBGM(TEXT("TestSFX"));
		Manager->StopAllBGM();
		Manager->StopBGM(TEXT("TestSFX"));
		System->flushCommands();
		Event->getInstanceCount(&Count);
		TestEqual(TEXT("StopAllBGM is safe and leaves SFX alone"), Count, 2);
	}

	TWeakObjectPtr<USoundManager> WeakManager(Manager);
	GameInstance->Shutdown();
	if (bHasFixture)
	{
		int32 Count = -1;
		System->flushCommands();
		Event->getInstanceCount(&Count);
		TestEqual(TEXT("Game shutdown releases all managed playback"), Count, 0);
	}
	FMOD.PreEndPIE();
	FMOD.SetInPIE(false, false);
	GEngine->DestroyWorldContext(NewWorld);
	NewWorld->DestroyWorld(false);
	GameInstance->RemoveFromRoot();
	GameInstance = nullptr;
	Manager = nullptr;
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Manager can be collected after game shutdown"), WeakManager.IsValid());
	return true;
}
#endif
