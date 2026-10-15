#include "Scripts/Backend/TurnManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Backend/PlayerDataManager.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "TimerManager.h"

namespace
{
struct FTurnTestWorld
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	UWorld* World;
	ATurnManager* Turns;
	FTurnTestWorld()
	{
		Instance->AddToRoot();
		Instance->InitializeStandalone();
		World = Instance->GetWorld();
		World->SpawnActor<AGameManager>();
		Turns = World->SpawnActor<ATurnManager>();
		Turns->bAutoStartBattle = false;
	}
	~FTurnTestWorld()
	{
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		Instance->RemoveFromRoot();
	}
};

class FWaitForPrepareDeadline : public IAutomationLatentCommand
{
public:
	FWaitForPrepareDeadline(TSharedPtr<FTurnTestWorld> InContext, FAutomationTestBase* InOwner)
		: Context(InContext), Owner(InOwner), Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		// A pending timer becomes active on the first tick and expires on a later frame.
		Context->World->GetTimerManager().Tick(0.02f);
		if (Context->Turns->GetPhase() == ETurnPhase::Waiting) { return true; }
		if (FPlatformTime::Seconds() - Started > 2.0)
		{
			Owner->AddError(TEXT("Preparation deadline did not enter Waiting."));
			return true;
		}
		return false;
	}
private:
	TSharedPtr<FTurnTestWorld> Context;
	FAutomationTestBase* Owner;
	double Started;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnManagerSequenceTest, "Gladius.TurnManager.TenTurnsAndCutScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurnManagerSequenceTest::RunTest(const FString& Parameters)
{
	FTurnTestWorld Test;
	ATurnManager* Turns = Test.Turns;
	TestEqual(TEXT("Battle defaults to ten turns"), Turns->TotalTurns, 10);
	Turns->CutSceneIds = { FName(TEXT("WideShot")), FName(TEXT("PlayerCloseUp")) };
	TestTrue(TEXT("Battle starts"), Turns->StartBattle());
	TestEqual(TEXT("Start presentation precedes turns"), Turns->GetPhase(), ETurnPhase::BattleStart);
	TestEqual(TEXT("No turn consumed by start"), Turns->GetCurrentTurn(), 0);
	TestFalse(TEXT("Cannot start an already active battle"), Turns->StartBattle());
	const int32 StartId = Turns->GetPhaseId();
	Turns->CompletePhase(StartId);
	TestFalse(TEXT("Late start callback cannot complete preparation"), Turns->CompletePhase(StartId));
	UPlayerDataManager* Players = Test.Instance->GetSubsystem<UPlayerDataManager>();
	Players->BeginPlayerCombat(TEXT("participant"));
	FPlayerCombatData Data;
	Data.Fever = 9;
	Players->UpdateCombatData(TEXT("participant"), Data);
	for (int32 Turn = 1; Turn <= 10; ++Turn)
	{
		TestEqual(TEXT("Preparation starts at correct turn"), Turns->GetCurrentTurn(), Turn);
		TestEqual(TEXT("Preparation state"), Turns->GetPhase(), ETurnPhase::BattlePrepare);
		TestTrue(TEXT("Preparation accepts actions"), Turns->IsAcceptingPlayerActions());
		Turns->CompletePhase(Turns->GetPhaseId());
		TestEqual(TEXT("Wait for final API response"), Turns->GetPhase(), ETurnPhase::Waiting);
		TestFalse(TEXT("Generic completion cannot bypass API wait"), Turns->CompletePhase(Turns->GetPhaseId()));
		Turns->ContinueWithoutFinalChat(Turns->GetPhaseId());
		for (int32 Shot = 0; Shot < 2; ++Shot)
		{
			TestEqual(TEXT("Each shot remains in the cutscene phase"), Turns->GetPhase(), ETurnPhase::CutScenes);
			TestFalse(TEXT("Actions locked for cutscenes"), Turns->IsAcceptingPlayerActions());
			const int32 ShotId = Turns->GetPhaseId();
			Turns->CompletePhase(ShotId);
			TestFalse(TEXT("Duplicate shot completion ignored"), Turns->CompletePhase(ShotId));
		}
		TestEqual(TEXT("Combat follows all shots"), Turns->GetPhase(), ETurnPhase::Battle);
		Turns->CompletePhase(Turns->GetPhaseId());
	}
	TestEqual(TEXT("Exactly ten turns lead to end presentation"), Turns->GetPhase(), ETurnPhase::BattleEnd);
	TestEqual(TEXT("No eleventh turn"), Turns->GetCurrentTurn(), 10);
	TestFalse(TEXT("Combat stats cleared before end presentation"), Players->GetCombatData(TEXT("participant"), Data));
	Turns->CompletePhase(Turns->GetPhaseId());
	TestEqual(TEXT("End presentation completion returns idle"), Turns->GetPhase(), ETurnPhase::Idle);
	TestTrue(TEXT("A new battle can start"), Turns->StartBattle());
	TestEqual(TEXT("New battle resets turn number"), Turns->GetCurrentTurn(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnManagerChatTest, "Gladius.TurnManager.WaitingAndEarlyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurnManagerChatTest::RunTest(const FString& Parameters)
{
	FTurnTestWorld Test;
	ATurnManager* Turns = Test.Turns;
	AYouTubeChatManager* Chat = Test.World->SpawnActor<AYouTubeChatManager>();
	Turns->StartBattle();
	Turns->CompletePhase(Turns->GetPhaseId());
	// No credentials/network are used. First exercise the local request-failure path.
	AddExpectedMessage(TEXT("LiveChatId"), ELogVerbosity::Warning);
	Turns->CompletePhase(Turns->GetPhaseId());
	TestEqual(TEXT("API failure keeps Waiting active"), Turns->GetPhase(), ETurnPhase::Waiting);
	const int32 WaitingId = Turns->GetPhaseId();
	Chat->OnChatFetchFinished.Broadcast(false);
	TestEqual(TEXT("Failed reply never advances battle"), Turns->GetPhase(), ETurnPhase::Waiting);
	Chat->OnChatFetchFinished.Broadcast(true);
	TestEqual(TEXT("Successful final reply advances, empty shot list skips cutscenes"), Turns->GetPhase(), ETurnPhase::Battle);
	TestFalse(TEXT("Late wait action cannot alter combat"), Turns->ContinueWithoutFinalChat(WaitingId));
	Chat->OnChatFetchFinished.Broadcast(true);
	TestEqual(TEXT("Duplicate API completion ignored outside Waiting"), Turns->GetPhase(), ETurnPhase::Battle);
	TestTrue(TEXT("Battle can end early"), Turns->EndBattle());
	TestEqual(TEXT("Early end still shows end presentation"), Turns->GetPhase(), ETurnPhase::BattleEnd);
	TestFalse(TEXT("Repeated early end ignored"), Turns->EndBattle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnManagerTimerTest, "Gladius.TurnManager.PrepareTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurnManagerTimerTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FTurnTestWorld> Context = MakeShared<FTurnTestWorld>();
	Context->Turns->PrepareDurationSeconds = 0.01f;
	Context->Turns->StartBattle();
	Context->Turns->CompletePhase(Context->Turns->GetPhaseId());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPrepareDeadline(Context, this));
	return true;
}
#endif
