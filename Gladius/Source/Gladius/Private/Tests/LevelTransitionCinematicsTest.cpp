#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BattleCameraTestAccess.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
#include "TimerManager.h"
#include "Engine/Texture2D.h"

struct FLevelTransitionTestAccess
{
 static void PlayNow(ULevelTransitionCinematics* C, bool Battle) { C->Play(Battle, true); }
 static float LogoAlpha(ULevelTransitionCinematics* C) { return C->GetIntroLogoOpacity(); }
 static bool HasLogo(ULevelTransitionCinematics* C) { return C->IntroLogoOverlay.IsValid(); }
 static void DelayElapsed(ABattleManager* B) { B->HandleBattleStartDelayElapsed(); }
 static ULevelSequencePlayer* Player(ULevelTransitionCinematics* C) { return C->SequencePlayer; }
 static void Finish(ULevelTransitionCinematics* C) { C->HandleFinished(); }
 static void Complete(ULevelTransitionCinematics* C) { C->FinishPlayback(); }
 static void Cancel(ULevelTransitionCinematics* C) { C->Completion.Unbind(); C->Cleanup(); }
 static void EndBattle(ABattleManager* B) { B->FinishBattle(); }
};
namespace
{
struct FTransitionTestWorld
{
 UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
 UWorld* World;
 ABattleManager* Battle;
 FTransitionTestWorld()
 {
  Instance->AddToRoot(); Instance->InitializeStandalone(); World=Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
  World->EnableActorTickAndUserCallbacks(true);
#endif
  World->InitializeActorsForPlay(FURL());
  Battle=World->SpawnActor<ABattleManager>();
  FBattleCameraTestAccess::RecordsOnly(Battle);
  Battle->DispatchBeginPlay();
 }
 ~FTransitionTestWorld()
 {
  FBattleCameraTestAccess::Cleanup(World);
  Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntroLogoFadeTest,"Gladius.Transitions.IntroLogoFade",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIntroLogoFadeTest::RunTest(const FString& Parameters)
{
 FTransitionTestWorld T;
 auto* C=T.World->GetSubsystem<ULevelTransitionCinematics>();
 TestNotNull(TEXT("Imported logo is available"),LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Cinematics/T_GladiusIntroLogo.T_GladiusIntroLogo")));
 for (bool Battle : {false,true})
 {
  FLevelTransitionTestAccess::PlayNow(C,Battle);
  auto* Player=FLevelTransitionTestAccess::Player(C);
  if (!TestNotNull(TEXT("Intro player created"),Player)) return false;
  auto Seek=[&](double Fraction)
  {
   FMovieSceneSequencePlaybackParams Params;
   Params.PositionType=EMovieScenePositionType::Time;
   Params.UpdateMethod=EUpdatePositionMethod::Jump;
   Params.Time=Player->GetStartTime().AsSeconds()+Player->GetDuration().AsSeconds()*Fraction;
   Player->SetPlaybackPosition(Params);
   return FLevelTransitionTestAccess::LogoAlpha(C);
  };
  TestEqual(TEXT("Initially transparent"),Seek(0.),0.f);
  const float Rising=Seek(0.1);
  TestTrue(TEXT("Fade in is intermediate"),Rising>0.3f&&Rising<0.7f);
  TestEqual(TEXT("Logo holds fully visible"),Seek(0.4),1.f);
  const float Falling=Seek(0.7);
  TestTrue(TEXT("Fade out is intermediate"),Falling>0.3f&&Falling<0.7f);
  TestEqual(TEXT("Transparent before intro ends"),Seek(0.9),0.f);
  FLevelTransitionTestAccess::Cancel(C);
  TestFalse(TEXT("Cleanup removes overlay"),FLevelTransitionTestAccess::HasLogo(C));
  TestEqual(TEXT("Cleanup resets alpha"),FLevelTransitionTestAccess::LogoAlpha(C),0.f);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleLoginIntroTest,"Gladius.Transitions.LoginWindow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleLoginIntroTest::RunTest(const FString& Parameters)
{
 FTransitionTestWorld T;
 TestEqual(TEXT("Default login window is twenty seconds"),T.Battle->BattleStartSeconds,20.f);
 T.Battle->StartBattle();
 TestTrue(TEXT("Battle starts in BattleStart"),T.Battle->Phase==EBattlePhase::BattleStart);
 TestEqual(TEXT("HUD uses login duration"),T.Battle->GetInputPhaseDuration(),20.f);
 TestEqual(TEXT("HUD reports login time remaining"),T.Battle->GetInputPhaseRemainingSeconds(),20.f,KINDA_SMALL_NUMBER);
 TestFalse(TEXT("Combat input stays closed during login"),T.Battle->IsAcceptingInput());
 TestTrue(TEXT("Login accepted during intro"),T.Battle->LoginPlayer(TEXT("intro_user"),TEXT("Intro User"),TEXT("gladius")));
 TestFalse(TEXT("Attack rejected before first turn"),T.Battle->SubmitAction(TEXT("intro_user"),TEXT("Intro User"),EBattleAction::Attack1,TEXT("gladius")));
 TestFalse(TEXT("Duplicate login rejected"),T.Battle->LoginPlayer(TEXT("intro_user"),TEXT("Intro User"),TEXT("gladius")));
 FLevelTransitionTestAccess::DelayElapsed(T.Battle);
 TestTrue(TEXT("First turn starts after login delay"),T.Battle->Phase==EBattlePhase::Prepare);
 TestTrue(TEXT("Participant survives first-turn transition"),T.Battle->IsPlayerLoggedIn(TEXT("intro_user")));
 T.Battle->StopBattle();
 TestFalse(TEXT("Login rejected after end"),T.Battle->LoginPlayer(TEXT("late"),TEXT("Late"),TEXT("sera")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitionCompletionTest,"Gladius.Transitions.CompletionAndInterruption",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransitionCompletionTest::RunTest(const FString& Parameters)
{
 FTransitionTestWorld T;
 auto* C=T.World->GetSubsystem<ULevelTransitionCinematics>();
 if (!TestNotNull(TEXT("Game world owns cinematics"),C)) return false;
 int32 Intro=0, Outro=0, Duplicate=0;
 C->PlayIntro(false,FSimpleDelegate::CreateLambda([&](){++Intro;}));
 TestTrue(TEXT("Queued intro blocks first turn"),C->IsPlaying());
 C->PlayOutro(false,FSimpleDelegate::CreateLambda([&](){++Outro;}));
 TestTrue(TEXT("Outro owns playback"),C->IsPlaying());
 TestEqual(TEXT("Travel waits for outro completion"),Outro,0);
 C->PlayOutro(false,FSimpleDelegate::CreateLambda([&](){++Duplicate;}));
 TestEqual(TEXT("Interrupted intro cannot resume battle"),Intro,0);
 FLevelTransitionTestAccess::Finish(C);
 TestEqual(TEXT("Travel deferred until evaluation returns"),Outro,0);
 FLevelTransitionTestAccess::Complete(C);
 TestEqual(TEXT("Travel called once after outro"),Outro,1);
 TestEqual(TEXT("Duplicate travel request ignored"),Duplicate,0);
 TestFalse(TEXT("Finished player released"),C->IsPlaying());
 C->PlayIntro(false);
 TestFalse(TEXT("Intro cannot restart after travel requested"),C->IsPlaying());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleIntroGateTest,"Gladius.Transitions.FirstTurnWaitsForIntro",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleIntroGateTest::RunTest(const FString& Parameters)
{
 FTransitionTestWorld T;
 T.Battle->StartBattle();
 auto* C=T.World->GetSubsystem<ULevelTransitionCinematics>();
 C->PlayIntro(true);
 FLevelTransitionTestAccess::DelayElapsed(T.Battle);
 TestTrue(TEXT("An unfinished intro keeps BattleStart active"),T.Battle->Phase==EBattlePhase::BattleStart);
 FLevelTransitionTestAccess::Cancel(C);
 FLevelTransitionTestAccess::DelayElapsed(T.Battle);
 TestTrue(TEXT("Completed intro releases the first turn"),T.Battle->Phase==EBattlePhase::Prepare);
 return true;
}
#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"

namespace
{
class FVerifyRealLevelTransitions : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 int32 Phase;
 double Started=FPlatformTime::Seconds();
 FString RecordPath=FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".json"));
public:
 FVerifyRealLevelTransitions(FAutomationTestBase* InTest,bool bStartTemple):Test(InTest),Phase(bStartTemple?2:0)
 {
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(RecordPath),true);
 }
 virtual bool Update() override
 {
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
  if (FPlatformTime::Seconds()-Started>90.) { Test->AddError(FString::Printf(TEXT("Level transition timed out at phase %d"),Phase));return true; }
  if (!W || !W->HasBegunPlay()) return false;
  ABattleManager* B=nullptr;
  for (TActorIterator<ABattleManager> It(W);It;++It) { FBattleCameraTestAccess::UsePath(*It,RecordPath);B=*It; }
  auto* C=W->GetSubsystem<ULevelTransitionCinematics>();
  auto* Player=FLevelTransitionTestAccess::Player(C);
  const FString Name=Player&&Player->GetSequence()?Player->GetSequence()->GetName():TEXT("");
  auto CheckCamera=[&]()
  {
   // Wait until the first sequence evaluation has resolved the PIE camera binding.
   if (!Player || !Player->GetActiveCameraComponent()) return false;
   APlayerController* PC=W->GetFirstPlayerController();
   Test->TestTrue(TEXT("Authored camera controls the view in PIE"),PC && PC->GetViewTarget()==Player->GetActiveCameraComponent()->GetOwner());
   Player->SetPlayRate(5.f);
   return true;
  };
  if (Phase==0 && B && Name==TEXT("Battle_Intro") && CheckCamera())
  {
   Test->TestEqual(TEXT("MainBattle starts with a 20 second login window"),B->BattleStartSeconds,20.f);
   Test->TestTrue(TEXT("Battle intro is inside BattleStart"),B->Phase==EBattlePhase::BattleStart);
   FLevelTransitionTestAccess::DelayElapsed(B);
   Test->TestTrue(TEXT("First turn waits for real intro"),B->Phase==EBattlePhase::BattleStart);
   Phase=1;
  }
  else if (Phase==1 && B && !C->IsPlaying() && B->Phase==EBattlePhase::Prepare)
  {
   FLevelTransitionTestAccess::EndBattle(B);
   Test->TestTrue(TEXT("Battle outro runs in BattleEnd"),B->Phase==EBattlePhase::BattleEnd);
   auto* Outro=FLevelTransitionTestAccess::Player(C);
   if (!Test->TestNotNull(TEXT("Battle outro starts before travel"),Outro)) return true;
   Test->TestEqual(TEXT("Correct battle outro"),Outro->GetSequence()->GetName(),FString(TEXT("Battle_Outro")));
   Outro->SetPlayRate(5.f);
   Phase=2;
  }
  else if (Phase==2 && Name==TEXT("Temple_Intro") && CheckCamera()) Phase=3;
  else if (Phase==3 && !C->IsPlaying())
  {
   ATempleCycleController* Controller=nullptr;
   for (TActorIterator<ATempleCycleController> It(W);It;++It) { Controller=*It;break; }
   if (!Test->TestNotNull(TEXT("MainTemple owns its cycle controller"),Controller)) return true;
   Controller->RequestCombatTravel();
   auto* Outro=FLevelTransitionTestAccess::Player(C);
   if (!Test->TestNotNull(TEXT("Temple outro starts before travel"),Outro)) return true;
   Test->TestEqual(TEXT("Correct temple outro"),Outro->GetSequence()->GetName(),FString(TEXT("Temple_Outro")));
   Outro->SetPlayRate(5.f);
   Phase=4;
  }
  else if (Phase==4 && B && Name==TEXT("Battle_Intro") && CheckCamera())
  {
   Test->TestTrue(TEXT("Returning battle plays intro during BattleStart"),B->Phase==EBattlePhase::BattleStart);
   Phase=5;
  }
  else if (Phase==5 && !C->IsPlaying())
  {
   Test->TestTrue(TEXT("Battle login window continues after intro"),B&&B->Phase==EBattlePhase::BattleStart);
   return true;
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealBattleTransitionTest,"Gladius.Transitions.RealMaps.BattleRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealBattleTransitionTest::RunTest(const FString& Parameters)
{
 // Map fixtures contain a disconnected YouTube source; cinematic assertions are independent.
 AddExpectedError(TEXT("LiveChatID"),EAutomationExpectedErrorFlags::Contains,0);
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/Main/MainBattle")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FVerifyRealLevelTransitions(this,false));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealTempleTransitionTest,"Gladius.Transitions.RealMaps.InitialTemple",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRealTempleTransitionTest::RunTest(const FString& Parameters)
{
 // Map fixtures contain a disconnected YouTube source; cinematic assertions are independent.
 AddExpectedError(TEXT("LiveChatID"),EAutomationExpectedErrorFlags::Contains,0);
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/Main/MainTemple")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FVerifyRealLevelTransitions(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
#endif
