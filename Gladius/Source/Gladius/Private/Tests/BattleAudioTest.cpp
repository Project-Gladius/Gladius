#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Scripts/Backend/SoundManager.h"
#include "Scripts/Battle/BattleTutorialController.h"
#include "BattleCameraTestAccess.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FMODStudioModule.h"
#include "Misc/AutomationTest.h"
#include "fmod_studio.hpp"

struct FBattleAudioTestAccess
{
 static void StartTurn(ABattleManager* M) { M->StartTurn(); }
 static void Input(ABattleManager* M) { M->BeginInput(); }
 static void Collect(ABattleManager* M) { M->BeginCollect(); }
 static void Move(ABattleManager* M) { M->BeginMove(); }
 static void Offer(ABattleManager* M)
 {
  M->Tutorial=NewObject<UBattleTutorialController>(M);
  M->Tutorial->Manager=M;
  M->Tutorial->EntryTime=M->GetWorld()->GetTimeSeconds();
  M->Tutorial->BeginOffer();
 }
 static void Explanation(ABattleManager* M) { M->Tutorial->EnterTurn(2); }
 static void Complete(ABattleManager* M) { M->Tutorial->CompleteToMain(); }
 static void Announcement(ABattleManager* M) { M->Tutorial->BeginBattleAnnouncement(); }
 static int32 FadeCount(USoundManager* A) { return A->BGMFades.Num(); }
 static void AdvanceFade(USoundManager* A, double Seconds)
 {
  for (auto& E:A->BGMFades) E.Value.StartedAt-=Seconds;
  const auto Handle=A->FadeTickerHandle;
  if (!A->TickBGMFades(0.f)) FTSTicker::RemoveTicker(Handle);
 }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleAudioFlowTest,"Gladius.Audio.BattleMusicAndFade",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBattleAudioFlowTest::RunTest(const FString& Parameters)
{
 auto& FMOD=IFMODStudioModule::Get();
 if(FMOD.GetStudioSystem(EFMODSystemContext::Runtime)) { AddError(TEXT("Run outside PIE")); return false; }
 auto* GI=NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
 auto* World=GI->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
 World->EnableActorTickAndUserCallbacks(true);
#endif
 World->InitializeActorsForPlay(FURL());
 FMOD.SetInPIE(true,false);
 auto* System=FMOD.GetStudioSystem(EFMODSystemContext::Runtime);
 auto* Audio=GI->GetSubsystem<USoundManager>();
 TestNotNull(TEXT("Runtime available"),System);
 if(!System) { GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot(); return false; }
 System->flushCommands();
 FMOD::Studio::Bus* Master=nullptr;
 if(System->getBus("bus:/",&Master)==FMOD_OK) { Master->setMute(true); Master->setPaused(true); }
 auto Event=[&](const char* Name) { FMOD::Studio::EventDescription* E=nullptr; System->getEvent(Name,&E); return E; };
 auto* Temple=Event("event:/Temple1"); auto* Wait=Event("event:/BattleWait"); auto* Battle=Event("event:/Battle");
 TestNotNull(TEXT("Temple1 bank event"),Temple); TestNotNull(TEXT("BattleWait bank event"),Wait); TestNotNull(TEXT("Battle bank event"),Battle);
 auto Count=[&](FMOD::Studio::EventDescription* E) { System->flushCommands(); int N=0; if(E) E->getInstanceCount(&N); return N; };
 auto* M=World->SpawnActor<ABattleManager>(); FBattleCameraTestAccess::RecordsOnly(M);
 M->bEnableTurnCutScenes=false; M->StartBattle();
 Audio->StartBGM(TEXT("Temple1")); TestEqual(TEXT("Temple music starts"),Count(Temple),1);
 M->StartBattle(); TestEqual(TEXT("Battle entry stops temple music"),Count(Temple),0);
 FBattleAudioTestAccess::StartTurn(M);
 TestEqual(TEXT("Turn input duration includes telegraph and totals fifteen seconds"),M->GetInputPhaseDuration(),15.f);
 FBattleAudioTestAccess::Input(M);
 TestTrue(TEXT("Remaining input timer does not add another fifteen seconds"),M->GetInputPhaseRemainingSeconds()<=15.f);
 TestEqual(TEXT("Main battle input uses Battle"),Count(Battle),1);
 TestEqual(TEXT("Main battle input has no BattleWait"),Count(Wait),0);
 FMOD::Studio::EventInstance* MainInstance=nullptr;
 if(Battle) { int N=0; Battle->getInstanceList(&MainInstance,1,&N); }
 FBattleAudioTestAccess::Collect(M);
 TestEqual(TEXT("Main collection does not fade music"),FBattleAudioTestAccess::FadeCount(Audio),0);
 TestEqual(TEXT("Battle continues through main collection"),Count(Battle),1);
 TestEqual(TEXT("Main collection has no BattleWait"),Count(Wait),0);
 FBattleAudioTestAccess::Move(M);
 TestEqual(TEXT("Battle continues through main action"),Count(Battle),1);
 FBattleAudioTestAccess::StartTurn(M);
 TestEqual(TEXT("Next main turn keeps one Battle instance"),Count(Battle),1);
 if(Battle)
 {
  FMOD::Studio::EventInstance* NextInstance=nullptr; int N=0;
  Battle->getInstanceList(&NextInstance,1,&N);
  TestTrue(TEXT("Main music instance is preserved across turns"),NextInstance==MainInstance);
 }
 if(Battle)
 {
  FMOD::Studio::EventInstance* Instance=nullptr; int N=0; Battle->getInstanceList(&Instance,1,&N);
  if(Instance) Instance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
  System->flushCommands();
  FBattleAudioTestAccess::AdvanceFade(Audio,0.);
  System->flushCommands();
  FMOD_STUDIO_PLAYBACK_STATE State=FMOD_STUDIO_PLAYBACK_STOPPED;
  if(Instance) Instance->getPlaybackState(&State);
  TestTrue(TEXT("Completed one-shot BGM is restarted"),State!=FMOD_STUDIO_PLAYBACK_STOPPED);
 }
 FBattleAudioTestAccess::Offer(M);
 TestEqual(TEXT("Turn-zero offer starts BattleWait"),Count(Wait),1);
 TestEqual(TEXT("Offer stops action music"),Count(Battle),0);
 FBattleAudioTestAccess::Explanation(M);
 TestEqual(TEXT("Tutorial explanation retains a single BattleWait"),Count(Wait),1);
 FBattleAudioTestAccess::Collect(M);
 TestEqual(TEXT("Tutorial collection fades too"),FBattleAudioTestAccess::FadeCount(Audio),1);
 FBattleAudioTestAccess::AdvanceFade(Audio,1.5);
 System->flushCommands();
 if(Wait)
 {
  FMOD::Studio::EventInstance* Instance=nullptr; int N=0; Wait->getInstanceList(&Instance,1,&N);
  float Volume=1.f; if(Instance) Instance->getVolume(&Volume);
  TestTrue(TEXT("Tutorial wait fade is half volume after 1.5 seconds"),Volume>0.4f&&Volume<0.6f);
 }
 FBattleAudioTestAccess::AdvanceFade(Audio,1.6);
 TestEqual(TEXT("Tutorial wait fades out in three seconds"),Count(Wait),0);
 FBattleAudioTestAccess::Move(M);
 TestEqual(TEXT("Tutorial action starts Battle"),Count(Battle),1);
 FBattleAudioTestAccess::Announcement(M);
 TestEqual(TEXT("Tutorial final text uses BattleWait"),Count(Wait),1);
 TestEqual(TEXT("Tutorial text stops Battle"),Count(Battle),0);
 FBattleAudioTestAccess::Complete(M);
 TestEqual(TEXT("Completing tutorial immediately begins main Battle BGM"),Count(Battle),1);
 TestEqual(TEXT("Completing tutorial stops BattleWait"),Count(Wait),0);
 FBattleAudioTestAccess::Collect(M);
 TestEqual(TEXT("Post-tutorial main collection keeps Battle"),Count(Battle),1);
 TestEqual(TEXT("Post-tutorial main collection does not fade"),FBattleAudioTestAccess::FadeCount(Audio),0);
 M->StopBattle();
 TestEqual(TEXT("Stop clears BattleWait"),Count(Wait),0);
 TestEqual(TEXT("Stop clears Battle"),Count(Battle),0);
 TestEqual(TEXT("Stop cancels every fade"),FBattleAudioTestAccess::FadeCount(Audio),0);
 FBattleCameraTestAccess::Cleanup(World);
 GI->Shutdown(); FMOD.PreEndPIE(); FMOD.SetInPIE(false,false);
 GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
 return true;
}
#endif
