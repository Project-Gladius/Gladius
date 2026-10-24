#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Scripts/Battle/BattleManager.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

struct FBattleCameraTestAccess
{
 static void RecordsOnly(ABattleManager* Manager)
 {
  Manager->TestRecordPath = FPaths::ProjectSavedDir() / TEXT("Automation") / (FGuid::NewGuid().ToString() + TEXT(".json"));
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Manager->TestRecordPath), true);
  Manager->bAutoStartBattle = false;
  Manager->bUseBalanceFiles = false;
 }
 static const FString& Path(ABattleManager* Manager) { return Manager->TestRecordPath; }
 static void UsePath(ABattleManager* Manager, const FString& Path) { Manager->TestRecordPath=Path; }
 static void BeginPrepared(ABattleManager* Manager)
 {
  Manager->bEnableTurnCutScenes = false;
  Manager->StartBattle();
  Manager->GetWorldTimerManager().ClearTimer(Manager->PhaseTimerHandle);
  Manager->StartTurn();
 }
 static FPlayerRecord& Record(ABattleManager* Manager, const FString& Id) { return Manager->Records.FindChecked(Id); }
 //전투 종료 때 받는 축복 횟수를 직접 지급 (기록이 없으면 생성)
 static FPlayerRecord& GrantBlessings(ABattleManager* Manager, const FString& Id, const FString& Name, int32 Count)
 {
  FPlayerRecord& R = Manager->GetOrCreateRecord(Id, Name);
  R.BlessingsAvailable = Count;
  return R;
 }
 static void Prepare(ABattleManager* Manager)
 {
  Manager->TestRecordPath = FPaths::ProjectSavedDir() / TEXT("Automation") / (FGuid::NewGuid().ToString() + TEXT(".json"));
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Manager->TestRecordPath), true);
  Manager->bEnableTurnCutScenes = false;
  Manager->StartBattle();
  Manager->GetWorldTimerManager().ClearTimer(Manager->PhaseTimerHandle);
  Manager->StartTurn();
 }
 static void Cleanup(UWorld* World)
 {
  for (TActorIterator<ABattleManager> It(World); It; ++It)
  {
   const FString Path = It->TestRecordPath;
   It->StopBattle();
   It->Destroy();
   if (!Path.IsEmpty()) IFileManager::Get().Delete(*Path);
  }
 }
};
#endif
