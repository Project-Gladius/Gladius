#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Battle/BattleManager.h"
#include "Kismet/GameplayStatics.h"

void UCommandDispatcher::Execute(const FParsedCommand& Command)
{
 ABattleManager* Battle = ResolveBattleManager();
 EBattleAction Action;
 if (FChatCommandParser::TryGetBattleAction(Command.CommandType, Action))
 {
  if (IsValid(Battle) && Battle->IsBattleRunning() && SubmitBattleAction.IsBound())
   SubmitBattleAction.Execute(Command.UserId, Command.Username.IsEmpty() ? Command.UserId : Command.Username, Action, TEXT(""));
  return;
 }
 if (Command.CommandType == ECommandType::Logout)
 {
  if (IsValid(Battle) && Battle->IsBattleRunning()) Battle->LogoutPlayer(Command.UserId);
  return;
 }
 const EGameState State = IsValid(Battle)
  ? (Battle->IsBattleRunning() ? EGameState::Combat : EGameState::Temple)
  : (GetGameStateDelegate.IsBound() ? GetGameStateDelegate.Execute() : EGameState::Temple);
 if (State != EGameState::Temple) return;
 switch (Command.CommandType)
 {
 case ECommandType::Enhance1: case ECommandType::Enhance2:
 case ECommandType::Enhance3: case ECommandType::Enhance4:
  OnEnhanceCommand.Broadcast(Command.UserId, Command.Username, Command.Argument); break;
 case ECommandType::Talk:
  OnTalkCommand.Broadcast(Command.UserId, Command.CommentId, Command.Username, Command.Argument); break;
 case ECommandType::TravelToCombat: OnTempleCombatCommand.Broadcast(); break;
 default: break;
 }
}

void UCommandDispatcher::BindBattleManager(ABattleManager* Manager)
{
 if (BoundBattleManager.Get() == Manager && SubmitBattleAction.IsBound()) return;
 SubmitBattleAction.Unbind();
 BoundBattleManager = Manager;
 if (IsValid(Manager)) SubmitBattleAction.BindUObject(Manager, &ABattleManager::SubmitAction);
}

void UCommandDispatcher::UnbindBattleManager(ABattleManager* Manager)
{
 if (BoundBattleManager.Get() != Manager) return;
 SubmitBattleAction.Unbind();
 BoundBattleManager.Reset();
}

ABattleManager* UCommandDispatcher::ResolveBattleManager()
{
 if (BoundBattleManager.IsValid()) return BoundBattleManager.Get();
 UWorld* World = GetOuter() ? GetOuter()->GetWorld() : nullptr;
 ABattleManager* Found = World ? Cast<ABattleManager>(UGameplayStatics::GetActorOfClass(World, ABattleManager::StaticClass())) : nullptr;
 BindBattleManager(Found);
 return Found;
}
