#include "Scripts/Backend/PlayerData.h"
#include "Scripts/Backend/CommandDispatcher.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Backend/PlayerDataManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDataPersistenceTest, "Gladius.PlayerData.DefaultsAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerDataPersistenceTest::RunTest(const FString& Parameters)
{
	FPlayerData Player;
	TestEqual(TEXT("Initial health"), Player.CurrentHealth, 5);
	TestEqual(TEXT("Initial maximum health"), Player.MaxHealth, 5);
	TestEqual(TEXT("Initial critical chance"), Player.CriticalChance, 0.25f);
	TestEqual(TEXT("Initial critical chance enhancement"), Player.CriticalChanceEnhancement, 0.0f);
	TestEqual(TEXT("Initial critical damage"), Player.CriticalDamage, 0.4f);
	TestEqual(TEXT("Initial critical damage enhancement"), Player.CriticalDamageEnhancement, 0.0f);
	TestFalse(TEXT("Not logged in by default"), Player.bIsLoggedIn);
	TestFalse(TEXT("Shop permission is explicit"), Player.bCanPurchaseFromShop);
	TestTrue(TEXT("Unknown class starts empty"), Player.LastLoggedInClass.IsEmpty());
	TestEqual(TEXT("Initial cumulative damage"), Player.TotalDamageDealt, 0.0);
	TestFalse(TEXT("Empty user identifiers rejected"), Player.IsValid());
	Player.UserId = TEXT("youtube-channel-test");
	TestTrue(TEXT("Identified default record is valid"), Player.IsValid());
	Player.CurrentHealth = 6;
	TestFalse(TEXT("Health above maximum rejected"), Player.IsValid());
	Player.CurrentHealth = 0;
	TestTrue(TEXT("Dead players can be saved"), Player.IsValid());

	Player.bIsLoggedIn = true;
	Player.bCanPurchaseFromShop = true;
	Player.LastLoggedInClass = TEXT("Gladius");
	Player.TotalDamageDealt = 12345.5;
	Player.CurrentHealth = 3;
	Player.MaxHealth = 8;
	Player.CriticalChance = 0.35f;
	Player.CriticalChanceEnhancement = 0.1f;
	Player.CriticalDamage = 0.6f;
	Player.CriticalDamageEnhancement = 0.2f;
	UPlayerDataSaveGame* Save = NewObject<UPlayerDataSaveGame>();
	Save->Players.Add(Player.UserId, Player);
	FPlayerData Second;
	Second.UserId = TEXT("another-user");
	Save->Players.Add(Second.UserId, Second);
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Serialize player database"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) { return false; }
	UPlayerDataSaveGame* Loaded = Cast<UPlayerDataSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Restore save class"), Loaded)) { return false; }
	TestEqual(TEXT("Both independent users restored"), Loaded->Players.Num(), 2);
	const FPlayerData* Restored = Loaded->Players.Find(Player.UserId);
	if (!TestNotNull(TEXT("Lookup by YouTube identifier"), Restored)) { return false; }
	TestEqual(TEXT("Identifier restored"), Restored->UserId, Player.UserId);
	TestEqual(TEXT("Login restored"), Restored->bIsLoggedIn, Player.bIsLoggedIn);
	TestEqual(TEXT("Shop permission restored"), Restored->bCanPurchaseFromShop, Player.bCanPurchaseFromShop);
	TestEqual(TEXT("Last class restored"), Restored->LastLoggedInClass, Player.LastLoggedInClass);
	TestEqual(TEXT("Cumulative damage restored"), Restored->TotalDamageDealt, Player.TotalDamageDealt);
	TestEqual(TEXT("Health restored"), Restored->CurrentHealth, Player.CurrentHealth);
	TestEqual(TEXT("Maximum health restored"), Restored->MaxHealth, Player.MaxHealth);
	TestEqual(TEXT("Critical chance restored"), Restored->CriticalChance, Player.CriticalChance);
	TestEqual(TEXT("Chance enhancement restored"), Restored->CriticalChanceEnhancement, Player.CriticalChanceEnhancement);
	TestEqual(TEXT("Critical damage restored"), Restored->CriticalDamage, Player.CriticalDamage);
	TestEqual(TEXT("Damage enhancement restored"), Restored->CriticalDamageEnhancement, Player.CriticalDamageEnhancement);
	TestEqual(TEXT("Second user's default health remains independent"), Loaded->Players.FindChecked(Second.UserId).CurrentHealth, 5);
	// Use a unique slot so the real player database is never touched by the test.
	const FString Slot = TEXT("GladiusPlayerData_Automation_") + FGuid::NewGuid().ToString();
	if (TestTrue(TEXT("Write an actual save file"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0)))
	{
		UPlayerDataSaveGame* DiskSave = Cast<UPlayerDataSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (TestNotNull(TEXT("Read save file back"), DiskSave))
		{
			const FPlayerData* DiskPlayer = DiskSave->Players.Find(Player.UserId);
			if (TestNotNull(TEXT("Disk record retains user key"), DiskPlayer))
			{
				TestEqual(TEXT("Disk record retains health"), DiskPlayer->CurrentHealth, Player.CurrentHealth);
				TestEqual(TEXT("Disk record retains damage enhancement"), DiskPlayer->CriticalDamageEnhancement, Player.CriticalDamageEnhancement);
			}
		}
	}
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		TestTrue(TEXT("Remove only the temporary test save"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDataLoginEventsTest, "Gladius.PlayerData.LoginEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerDataLoginEventsTest::RunTest(const FString& Parameters)
{
	UCommandDispatcher* Dispatcher = NewObject<UCommandDispatcher>();
	Dispatcher->Initialize();
	Dispatcher->GetGameStateDelegate.BindLambda([]() { return EGameState::Combat; });
	int32 Logins = 0;
	int32 Logouts = 0;
	Dispatcher->OnUserLoginRequired.AddLambda([&](const FString& UserId) { ++Logins; });
	Dispatcher->OnUserLoggedOut.AddLambda([&](const FString& UserId) { ++Logouts; });
	FParsedCommand Command;
	Command.UserId = TEXT("test-user");
	Command.CommandType = ECommandType::Defend;
	Dispatcher->Execute(Command);
	Dispatcher->Execute(Command);
	TestEqual(TEXT("Repeated combat commands log in once"), Logins, 1);
	Command.CommandType = ECommandType::Logout;
	Dispatcher->Execute(Command);
	TestEqual(TEXT("Logout notifies the player store"), Logouts, 1);
	Command.CommandType = ECommandType::Defend;
	Dispatcher->Execute(Command);
	TestEqual(TEXT("Logout removes membership so login can occur again"), Logins, 2);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerCombatDataTest, "Gladius.PlayerData.CombatLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerCombatDataTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone(FName(TEXT("PlayerCombatDataTestWorld")));
	UWorld* World = Instance->GetWorld();
	UPlayerDataManager* Players = Instance->GetSubsystem<UPlayerDataManager>();
	AGameManager* Game = World->SpawnActor<AGameManager>();
	if (TestNotNull(TEXT("Player manager exists"), Players) && TestNotNull(TEXT("Game manager exists"), Game))
	{
		Game->SetGameState(EGameState::Combat);
		FPlayerCombatData Data;
		TestFalse(TEXT("Non-participants have no combat record"), Players->GetCombatData(TEXT("first"), Data));
		TestFalse(TEXT("Empty identifier rejected"), Players->BeginPlayerCombat(TEXT("")));
		TestFalse(TEXT("Cannot update before combat entry"), Players->UpdateCombatData(TEXT("first"), Data));
		TestTrue(TEXT("Player enters combat"), Players->BeginPlayerCombat(TEXT("first")));
		TestTrue(TEXT("Combat record available"), Players->GetCombatData(TEXT("first"), Data));
		TestEqual(TEXT("Fever starts at zero"), Data.Fever, 0);
		TestEqual(TEXT("Dodge count starts at zero"), Data.DodgeCount, 0);
		Data.Fever = 8;
		Data.DodgeCount = 2;
		TestTrue(TEXT("Combat values can change"), Players->UpdateCombatData(TEXT("first"), Data));
		Players->BeginPlayerCombat(TEXT("first"));
		Players->GetCombatData(TEXT("first"), Data);
		TestEqual(TEXT("Same-battle re-entry preserves fever"), Data.Fever, 8);
		Players->BeginPlayerCombat(TEXT("second"));
		FPlayerCombatData Other;
		Players->GetCombatData(TEXT("second"), Other);
		TestEqual(TEXT("Players have independent combat values"), Other.Fever, 0);
		Data.DodgeCount = -1;
		TestFalse(TEXT("Negative combat counters rejected"), Players->UpdateCombatData(TEXT("first"), Data));
		Game->SetGameState(EGameState::Combat);
		Players->GetCombatData(TEXT("first"), Data);
		TestEqual(TEXT("Remaining in combat does not reset fever"), Data.Fever, 8);
		Game->SetGameState(EGameState::Temple);
		TestFalse(TEXT("Leaving combat removes first player's battle data"), Players->GetCombatData(TEXT("first"), Data));
		TestEqual(TEXT("No stale fever returned after combat"), Data.Fever, 0);
		TestEqual(TEXT("No stale dodge count returned after combat"), Data.DodgeCount, 0);
		TestFalse(TEXT("Leaving combat clears all participants"), Players->GetCombatData(TEXT("second"), Other));
		Game->SetGameState(EGameState::Combat);
		Players->BeginPlayerCombat(TEXT("first"));
		Players->GetCombatData(TEXT("first"), Data);
		TestEqual(TEXT("Next battle starts with zero fever"), Data.Fever, 0);
		TestEqual(TEXT("Next battle starts with zero dodges"), Data.DodgeCount, 0);
		Data.Fever = 3;
		Players->UpdateCombatData(TEXT("first"), Data);
		Game->CurrentGameState = EGameState::Temple;
		Game->DispatchBeginPlay();
		TestFalse(TEXT("Entering a non-combat level clears prior battle data"), Players->GetCombatData(TEXT("first"), Data));
	}
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	Instance->RemoveFromRoot();
	return true;
}
#endif
