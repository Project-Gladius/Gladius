#include "Scripts/Battle/BattleTutorialController.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UBattleTutorialController::UBattleTutorialController()
{
 EmphasisFont=LoadObject<UObject>(nullptr,TEXT("/Game/UI/Fonts/MaruBuri/MaruBuri_Bold_Font.MaruBuri_Bold_Font"));
}
void UBattleTutorialController::BeginEntry(ABattleManager* InManager)
{
 Manager = InManager; Flow = ETutorialFlow::Intro;
 EntryTime = Manager->GetWorld()->GetTimeSeconds();
 Manager->GetWorldTimerManager().ClearTimer(Manager->PhaseTimerHandle);
 auto* Cinematics=Manager->GetWorld()->GetSubsystem<ULevelTransitionCinematics>();
 if (Cinematics->HasIntroFinished()) OnIntroFinished();
 else
 {
  IntroHandle=Cinematics->OnIntroFinished.AddUObject(this,&UBattleTutorialController::OnIntroFinished);
  Cinematics->PlayIntro(true);
 }
}
void UBattleTutorialController::OnIntroFinished()
{
 if (Flow != ETutorialFlow::Intro) return;
 if (!Manager->Players.IsEmpty()) CompleteToMain(); else BeginOffer();
}
void UBattleTutorialController::ClearTimers()
{
 if (!Manager || !Manager->GetWorld()) return;
 Manager->GetWorldTimerManager().ClearTimer(MessageTimer);
 Manager->GetWorldTimerManager().ClearTimer(DeadlineTimer);
 if (auto* Cinematics=Manager->GetWorld()->GetSubsystem<ULevelTransitionCinematics>()) Cinematics->OnIntroFinished.Remove(IntroHandle);
 IntroHandle.Reset();
}
void UBattleTutorialController::ResumeChatPolling()
{
 if (auto* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(Manager, AYouTubeChatManager::StaticClass())))
  Chat->SetCombatAutoPollingEnabled(true);
}
void UBattleTutorialController::BeginOffer()
{
 ResumeChatPolling();
 ClearTimers(); Flow = ETutorialFlow::Offer; TutorialTurn = 0; EmptyTurns = 0;
 Guide = TEXT("캐릭터를 소환하세요!"); FrozenSeconds = 0;
 Manager->Phase = EBattlePhase::BattleStart; Manager->TurnIndex = 0;
 Manager->InputPhaseDuration = 0; Manager->InputPhaseEndTime = 0;
 if (Manager->BossUnit) { Manager->BossUnit->SetActorHiddenInGame(true); Manager->BossUnit->SetActorEnableCollision(false); }
 const float Remaining = 600.f - float(Manager->GetWorld()->GetTimeSeconds() - EntryTime);
 if (Remaining <= 0) { ExpireOffer(); return; }
 Manager->GetWorldTimerManager().SetTimer(DeadlineTimer,this,&UBattleTutorialController::ExpireOffer,Remaining,false);
 Queue({{TEXT("콜로세움에 오신 것을 환영합니다."),5},
  {TEXT("튜토리얼 진행을 원하신다면 채팅창에 \"!튜토리얼\" 을 입력하세요."),5},
  {TEXT("튜토리얼 진행을 원하지 않는다면 \"캐릭터를 소환\"해 주세요."),5}});
}
void UBattleTutorialController::ExpireOffer()
{
 if (Flow != ETutorialFlow::Offer) return;
 ClearTimers(); Flow = ETutorialFlow::Complete;
 if (Manager->BossTelegraphWidget) Manager->BossTelegraphWidget->EndTutorial();
 Manager->StopBattle();
 Manager->GetWorld()->GetSubsystem<ULevelTransitionCinematics>()->PlayOutro(true,
  FSimpleDelegate::CreateUObject(Manager,&ABattleManager::TravelToTemple));
}
bool UBattleTutorialController::RequestTutorial()
{
 if (Flow != ETutorialFlow::Offer) return false;
 ClearTimers(); Flow = ETutorialFlow::LoginExplanation; TutorialTurn = 1; EmptyTurns = 0;
 Guide = TEXT("튜토리얼"); FrozenSeconds = 0;
 Manager->TurnIndex = 1; Manager->SessionStats.Reset();
 Queue({{TEXT("\"제국의 전사들이여, 영광스러운 전투에 참여하라!\""),5},
  {TEXT("채팅창에 \"!로그인 글라디우스\" 또는 \"!로그인 세라\" 를 입력해 \"캐릭터를 소환\"하세요."),5}});
 return true;
}
void UBattleTutorialController::Queue(TArray<FTutorialMessage> InMessages)
{
 Manager->GetWorldTimerManager().ClearTimer(MessageTimer);
 Messages = MoveTemp(InMessages); MessageIndex = 0; ShowNextMessage();
}
void UBattleTutorialController::ShowNextMessage()
{
 if (!IsActive()) return;
 if (MessageIndex >= Messages.Num()) { MessagesFinished(); return; }
 const int32 Index = MessageIndex++;
 const FTutorialMessage Message = Messages[Index];
 if (TutorialTurn == 3 && Flow == ETutorialFlow::Explanation && Index == Messages.Num()-1) BeginInput(10.f);
 if (Manager->BossTelegraphWidget) Manager->BossTelegraphWidget->ShowTutorialMessage(FormatRichText(Message.Text),Message.Seconds);
 Manager->GetWorldTimerManager().SetTimer(MessageTimer,this,&UBattleTutorialController::ShowNextMessage,Message.Seconds,false);
}
void UBattleTutorialController::MessagesFinished()
{
 if (Flow == ETutorialFlow::Offer) { MessageIndex = 0; ShowNextMessage(); }
 else if (Flow == ETutorialFlow::LoginExplanation) BeginLoginWait();
 else if (Flow == ETutorialFlow::Explanation && !bInputStarted) BeginInput(TutorialTurn == 1 ? 15.f : Manager->PlayerSelectCountdown);
}
void UBattleTutorialController::BeginLoginWait()
{
 ResumeChatPolling();
 Flow = ETutorialFlow::LoginWait; Guide = TEXT("\"!로그인\" 명령어를 입력하세요!"); FrozenSeconds = 0;
 if (!Manager->Players.IsEmpty()) { RevealBoss(); return; }
 Manager->GetWorldTimerManager().SetTimer(DeadlineTimer,this,&UBattleTutorialController::ReturnToOffer,60.f,false);
}
void UBattleTutorialController::OnPlayerRegistered(const FString& UserId)
{
 if (!IsPracticing()) return;
 auto* P = Manager->Players.Find(UserId); if (!P) return;
 if (!SavedHealth.Contains(UserId)) SavedHealth.Add(UserId,P->HP);
 Manager->SetPlayerHealth(UserId, P->MaxHP);
 P->Lane = EBattleLane::Left;
 TGuardValue<bool> RandomGuard(Manager->bRandomInitialZone,false);
 Manager->PickInitialSlot(*P);
 if (P->Unit) { P->Unit->SetActorLocation(P->TargetLocation); P->Unit->SnapMeshToGround(); P->Unit->SetLabel(P->DisplayName,P->HP,P->MaxHP); }
}
void UBattleTutorialController::OnLoginSucceeded()
{
 if (Flow == ETutorialFlow::Offer) CompleteToMain();
 else if (Flow == ETutorialFlow::LoginWait) RevealBoss();
}
void UBattleTutorialController::RevealBoss()
{
 ClearTimers(); EnterTurn(1);
 if (Manager->BossUnit) Manager->BossUnit->OnBossTelegraph(TEXT("StrongStrikeLeft"),TEXT(""));
 Queue({{TEXT("이 강력한 짐승은 \"마족 트루돈\"입니다."),5},
  {TEXT("여러분의 목표는 이 괴수로부터 \"10턴을 생존\"하는 것입니다."),5},
  {TEXT("턴마다 하나의 행동을 선택할 수 있습니다."),5},
  {TEXT("채팅창에 \"!공격 1\" 명령어를 입력해 트루돈을 공격해 보세요."),5}});
}
void UBattleTutorialController::EnterTurn(int32 Turn)
{
 ResumeChatPolling();
 TutorialTurn = Turn; Flow = ETutorialFlow::Explanation;
 bTurnHadInput = false; bInputStarted = false; bReturnAfterTurn = false;
 Guide = TEXT("튜토리얼"); FrozenSeconds = Turn == 1 || Turn == 3 ? 15.f : 0.f;
 Manager->TurnIndex = Turn; Manager->Phase = EBattlePhase::Prepare;
 Manager->InputPhaseDuration = 0; Manager->InputPhaseEndTime = 0;
 Manager->StopTurnCutScenes();
 if (Manager->BossUnit) { Manager->BossUnit->SetActorHiddenInGame(false); Manager->BossUnit->SetActorEnableCollision(true); }
 Manager->TelegraphedAction = FBossAction();
 if (Turn == 3)
 {
  if (const auto* Left = Manager->Balance.FindAction(TEXT("SingleStrikeLeft"))) Manager->TelegraphedAction = *Left;
  else { Manager->TelegraphedAction.Id=TEXT("SingleStrikeLeft"); Manager->TelegraphedAction.DamageByZone[0]=2; }
  Manager->TelegraphedAction.TargetMode = EBossTargetMode::Fixed;
  Manager->TelegraphedAction.DamageByZone[1] = Manager->TelegraphedAction.DamageByZone[2] = 0;
  if (Manager->BossUnit) Manager->BossUnit->OnBossTelegraph(Manager->TelegraphedAction.TelegraphMotion(),TEXT(""));
 }
 else Manager->TelegraphedAction.TargetMode = EBossTargetMode::None;
 for (auto& Pair : Manager->Players)
 {
  auto& P = Pair.Value; P.PendingAction = EBattleAction::Idle; P.bActedThisTurn = false; P.EarlyHitReports = 0; P.IdleTurns = 0;
  if (P.Unit) { P.Unit->NotifyActionSelected(EBattleAction::Idle); P.Unit->SetDefending(false); }
 }
 Manager->RefreshPlayerUI();
 if (Turn == 2) Queue({{TEXT("채팅창에 \"!공격 1\"를 입력하세요."),10},
  {TEXT("머리 위에 보이는 불꽃 안의 숫자는 \"피버\"입니다."),5},
  {TEXT("공격을 할 때마다 \"피버\"가 늘어납니다."),5},
  {TEXT("\"피버\" 수치만큼 \"추가 타격\"을 진행합니다."),5}});
 if (Turn == 3) Queue({{TEXT("\"트루돈이 왼쪽을 공격할 준비를 합니다!\""),10},
  {TEXT("\"!방어\" 를 통해 받는 데미지를 줄일 수 있습니다."),5},
  {TEXT("\"방어\" 시에도 공격할 때처럼 \"피버\"가 늘어납니다."),5},
  {TEXT("\"!회피 좌/중/우\" 를 통해 원하는 칸으로 회피할 수 있습니다."),5},
  {TEXT("\"회피\"를 \"연속으로 사용\"하면 \"피버\"가 \"줄어들\" 수 있으니 조심하세요!"),5},
  {TEXT("트루돈이 여러분을 공격합니다. 명령어를 통해 대처하세요!"),5}});
 if (Turn == 4) Queue({{TEXT("채팅창에 \"!공격 2\"를 입력하세요."),10},
  {TEXT("\"공격 2\"는 약하지만, \"공격\"과 \"후속 타격\"에 \"치명타\"가 적용될 수 있습니다."),5},
  {TEXT("\"후속 타격\"을 잊지 않았겠죠? \"피버\" 수만큼 \"후속 타격\"이 진행됩니다!"),5},
  {TEXT("\"피버\" 또는 \"치명타 능력치\"가 높다면, \"공격 2\"를 적극적으로 사용하세요!"),5}});
}
void UBattleTutorialController::BeginInput(float Seconds)
{
 ResumeChatPolling();
 bInputStarted = true; Flow = ETutorialFlow::Input;
 Guide = TutorialTurn == 1 ? TEXT("트루돈을 공격하세요!") : TEXT("행동을 선택하세요!");
 Manager->Phase = EBattlePhase::Prepare;
 Manager->InputPhaseDuration = Seconds;
 Manager->InputPhaseEndTime = Manager->GetWorld()->GetTimeSeconds()+Seconds;
 if (Manager->BossUnit) Manager->BossUnit->OnBossWait();
 Manager->GetWorldTimerManager().SetTimer(Manager->PhaseTimerHandle,Manager.Get(),&ABattleManager::BeginCollect,FMath::Max(.001f,Seconds),false);
}
void UBattleTutorialController::OnActionAccepted() { if (AcceptsAction()) bTurnHadInput = true; }
void UBattleTutorialController::OnInputClosed()
{
 if (!IsPracticing()) return;
 EmptyTurns = bTurnHadInput ? 0 : EmptyTurns+1;
 bReturnAfterTurn = EmptyTurns >= 2; Flow = ETutorialFlow::Action;
 Manager->GetWorldTimerManager().ClearTimer(MessageTimer);
 if (Manager->BossTelegraphWidget) Manager->BossTelegraphWidget->HideTelegraph();
}
void UBattleTutorialController::OnTurnFinished()
{
 if (!IsPracticing()) return;
 if (bReturnAfterTurn) { ReturnToOffer(); return; }
 if (TutorialTurn >= 4) CompleteToMain(); else EnterTurn(TutorialTurn+1);
}
void UBattleTutorialController::RestoreHealth()
{
 for (const auto& Pair : SavedHealth)
 {
  if (auto* R = Manager->Records.Find(Pair.Key)) R->CurrentHealth = Pair.Value;
  if (auto* P = Manager->Players.Find(Pair.Key))
  {
   Manager->ApplyRecordToPlayer(Manager->Records.FindChecked(Pair.Key),*P);
   // The same transition restores death animation, visibility and UI after practice.
   Manager->SetPlayerHealth(Pair.Key, Pair.Value);
  }
 }
 SavedHealth.Reset();
}
void UBattleTutorialController::Shutdown()
{
 ClearTimers(); if (Manager) RestoreHealth(); Flow=ETutorialFlow::Inactive;
 if (Manager && Manager->BossTelegraphWidget) Manager->BossTelegraphWidget->EndTutorial();
}
void UBattleTutorialController::ReturnToOffer()
{
 if (!IsPracticing()) return;
 Manager->StopBattle();
 TArray<FString> Ids; Manager->Players.GetKeys(Ids);
 for (const auto& Id:Ids) Manager->RemovePlayer(Id);
 Manager->SessionStats.Reset(); Manager->bBattleRunning=true;
 if (auto* Chat=Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(Manager,AYouTubeChatManager::StaticClass()))) Chat->SetCombatAutoPollingEnabled(true);
 Manager->BossDamageTaken=Manager->BossDamageShown=0;
 BeginOffer(); Manager->SaveRecords();
}
void UBattleTutorialController::CompleteToMain()
{
 ClearTimers(); RestoreHealth(); Flow=ETutorialFlow::Complete;
 if (Manager->BossTelegraphWidget) Manager->BossTelegraphWidget->EndTutorial();
 Manager->SessionStats.Reset(); Manager->BossDamageTaken=Manager->BossDamageShown=0;
 Manager->CurrentPatternId.Reset(); Manager->PatternStep=0; Manager->PresetPatternIndex=0;
 Manager->TurnIndex=0; Manager->PreviousCutSceneClassId.Reset(); Manager->SelectedCutSceneUsers.Reset();
 if (Manager->BossUnit) { Manager->BossUnit->SetActorHiddenInGame(false); Manager->BossUnit->SetActorEnableCollision(true); Manager->BossUnit->OnBossWait(); }
 for (auto& Pair:Manager->Players)
 {
  auto& P=Pair.Value; P.Fever=Manager->Balance.InitialFever; P.EvadeStreak=Manager->Balance.InitialEvadeStreak; P.IdleTurns=0;
  P.DamageDealt=0; P.PendingAction=EBattleAction::Idle; P.bActedThisTurn=false; P.HitsToShow.Reset(); P.EarlyHitReports=0;
  if (const auto* R=Manager->Records.Find(Pair.Key)) Manager->SessionStats.RegisterPlayer(Pair.Key,P.DisplayName,R->CharacterId);
 }
 Manager->UpdateVisuals(); Manager->SaveRecords(); Manager->StartTurn();
}
FBattleTopHudState UBattleTutorialController::BuildHud() const
{
 auto State=FBattleTopHudPresenter::Build(Manager->Phase,Manager->IsTurnCutScenePlaying(),Manager->GetInputPhaseRemainingSeconds(),Manager->GetInputPhaseDuration(),TutorialTurn,Flow==ETutorialFlow::Offer||Flow==ETutorialFlow::Intro?Manager->Turns:4);
 State.bVisible=Flow!=ETutorialFlow::Intro && !Manager->IsTurnCutScenePlaying();
 if (Flow!=ETutorialFlow::Action) State.GuideText=Guide;
 const bool Counting=Flow==ETutorialFlow::Input;
 const float Seconds=Counting?Manager->GetInputPhaseRemainingSeconds():FrozenSeconds;
 State.TimerText=FBattleTopHudPresenter::FormatSeconds(Seconds);
 State.bShowTimer=Seconds>0 && (Counting||Flow==ETutorialFlow::Explanation);
 State.bShowGauge=State.bShowTimer;
 State.GaugePercent=Counting?State.GaugePercent:(Seconds>0?1.f:0.f);
 return State;
}
FString UBattleTutorialController::FormatRichText(const FString& Source)
{
 FString Out; bool Emphasis=false;
 for (TCHAR C:Source)
 {
  if (C==TEXT('"')) { Out+=Emphasis?TEXT("</>"):TEXT("<TutorialEmphasis>"); Emphasis=!Emphasis; }
  else if (C==TEXT('\'')) continue;
  else if (C==TEXT('&')) Out+=TEXT("&amp;");
  else if (C==TEXT('<')) Out+=TEXT("&lt;");
  else if (C==TEXT('>')) Out+=TEXT("&gt;");
  else Out.AppendChar(C);
 }
 if (Emphasis) Out+=TEXT("</>");
 return Out;
}
