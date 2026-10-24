#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Battle/BattleManager.h"
#include "Kismet/GameplayStatics.h"

void UCommandDispatcher::Execute(const FParsedCommand& Command)
{
 ABattleManager* Battle = ResolveBattleManager();
 EBattleAction Action;
 const FString DisplayName = Command.Username.IsEmpty() ? Command.UserId : Command.Username;
 if (FChatCommandParser::TryGetBattleAction(Command.CommandType, Action))
 {
  //로그인(!로그인 <캐릭터>)한 유저의 전투 명령만 받는다. 명령만으로 자동 참가하지 않음
  if (IsValid(Battle) && Battle->IsBattleRunning() && Battle->IsPlayerLoggedIn(Command.UserId) && SubmitBattleAction.IsBound())
   SubmitBattleAction.Execute(Command.UserId, DisplayName, Action, TEXT(""));
  else if (IsValid(Battle) && Battle->IsBattleRunning())
   UE_LOG(LogTemp, Log, TEXT("[Battle] 로그인하지 않은 유저의 명령 무시: %s"), *Command.UserId);
  return;
 }
 if (Command.CommandType == ECommandType::Login)
 {
  OnLoginCommand.Broadcast(Command.UserId, DisplayName, Command.Argument);
  return;
 }
 if (Command.CommandType == ECommandType::Logout)
 {
  OnLogoutCommand.Broadcast(Command.UserId);
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
 OnLoginCommand.Remove(LoginHandle);
 OnLogoutCommand.Remove(LogoutHandle);
 LoginHandle.Reset();
 LogoutHandle.Reset();
 BoundBattleManager = Manager;
 if (IsValid(Manager))
 {
  SubmitBattleAction.BindUObject(Manager, &ABattleManager::SubmitAction);
  LoginHandle = OnLoginCommand.AddUObject(Manager, &ABattleManager::HandleLoginCommand);
  LogoutHandle = OnLogoutCommand.AddUObject(Manager, &ABattleManager::HandleLogoutCommand);
 }
}

void UCommandDispatcher::UnbindBattleManager(ABattleManager* Manager)
{
 if (BoundBattleManager.Get() != Manager) return;
 SubmitBattleAction.Unbind();
 OnLoginCommand.Remove(LoginHandle);
 OnLogoutCommand.Remove(LogoutHandle);
 LoginHandle.Reset();
 LogoutHandle.Reset();
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
