#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BattleCameraTestAccess.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"

struct FTestCharacterControlsAccess
{
 static bool Press(ABattleManager* B, FKey Key)
 {
  if(!B->InputComponent) return false;
  for(const FInputKeyBinding& Binding:B->InputComponent->KeyBindings)
   if(Binding.Chord.Key==Key&&Binding.KeyEvent==IE_Pressed) {Binding.KeyDelegate.Execute(Key);return true;}
  return false;
 }
 static void Bind(ABattleManager* B){B->BindActionKeys();}
 static int32 BindingCount(ABattleManager* B){return B->InputComponent?B->InputComponent->KeyBindings.Num():0;}
 static const FString& Selected(ABattleManager* B){return B->DebugUserId;}
 static bool HasRecord(ABattleManager* B,const FString& Id){return B->Records.Contains(Id);}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestCharacterKeyboardTest,"Gladius.TestControls.SpawnActionsAndSelectiveReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTestCharacterKeyboardTest::RunTest(const FString& Parameters)
{
 UGameInstance* GI=NewObject<UGameInstance>(GEngine);GI->AddToRoot();GI->InitializeStandalone();
 UWorld* W=GI->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
 W->EnableActorTickAndUserCallbacks(true);
#endif
 W->InitializeActorsForPlay(FURL());W->SpawnActor<APlayerController>();
 auto* B=W->SpawnActor<ABattleManager>();FBattleCameraTestAccess::RecordsOnly(B);B->DispatchBeginPlay();B->StartBattle();
 auto Press=[&](FKey Key){return FTestCharacterControlsAccess::Press(B,Key);};
 TestTrue(TEXT("Q is bound during battle login window"),Press(EKeys::Q));
 const FString G=FTestCharacterControlsAccess::Selected(B);
 TestTrue(TEXT("Q spawns Gladius during intro"),!G.IsEmpty()&&B->GetBattlePlayers().Contains(G));
 if(B->GetBattlePlayers().Contains(G)) TestEqual(TEXT("Q character"),B->Classes[B->GetBattlePlayers().FindChecked(G).ClassIndex].Id,FString(TEXT("gladius")));
 TestTrue(TEXT("W is bound"),Press(EKeys::W));
 const FString S=FTestCharacterControlsAccess::Selected(B);
 TestTrue(TEXT("W selects a new test account"),!S.IsEmpty()&&S!=G);
 if(B->GetBattlePlayers().Contains(S)) TestEqual(TEXT("W character"),B->Classes[B->GetBattlePlayers().FindChecked(S).ClassIndex].Id,FString(TEXT("sera")));
 const int32 Bindings=FTestCharacterControlsAccess::BindingCount(B);FTestCharacterControlsAccess::Bind(B);
 TestEqual(TEXT("Rebinding does not duplicate callbacks"),FTestCharacterControlsAccess::BindingCount(B),Bindings);
 FBattleCameraTestAccess::BeginPrepared(B); // Restart battle, retain saved records and key bindings.
 TestTrue(TEXT("Real participant joins"),B->LoginPlayer(TEXT("real_viewer"),TEXT("Viewer"),TEXT("gladius")));
 Press(EKeys::Q);const FString Current=FTestCharacterControlsAccess::Selected(B);
 const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven};
 for(int32 i=0;i<7;++i)
 {
  TestTrue(TEXT("Action key bound"),Press(Keys[i]));
  if(const auto* P=B->GetBattlePlayers().Find(Current)) TestTrue(TEXT("Selected character receives action"),P->PendingAction==static_cast<EBattleAction>(i));
  else AddError(TEXT("Selected test participant missing"));
  TestTrue(TEXT("Real viewer is not controlled"),B->GetBattlePlayers().FindChecked(TEXT("real_viewer")).PendingAction==EBattleAction::Idle);
 }
 TestTrue(TEXT("E reset key bound"),Press(EKeys::E));
 TestEqual(TEXT("Only real participant remains"),B->GetBattlePlayers().Num(),1);
 TestTrue(TEXT("Real participant retained"),B->GetBattlePlayers().Contains(TEXT("real_viewer")));
 TestTrue(TEXT("Real saved data retained"),FTestCharacterControlsAccess::HasRecord(B,TEXT("real_viewer")));
 TestFalse(TEXT("Saved inactive Gladius test deleted"),FTestCharacterControlsAccess::HasRecord(B,G));
 TestFalse(TEXT("Saved inactive Sera test deleted"),FTestCharacterControlsAccess::HasRecord(B,S));
 TestFalse(TEXT("Selected test data deleted"),FTestCharacterControlsAccess::HasRecord(B,Current));
 TestTrue(TEXT("Selection cleared"),FTestCharacterControlsAccess::Selected(B).IsEmpty());
 Press(EKeys::Three);TestEqual(TEXT("Action without selection never spawns a player"),B->GetBattlePlayers().Num(),1);
 Press(EKeys::W);TestEqual(TEXT("Controls still work after reset"),B->GetBattlePlayers().Num(),2);
 B->bEnableTestCharacterControls=false;Press(EKeys::E);Press(EKeys::Q);
 TestEqual(TEXT("Disabled controls do not reset or create players"),B->GetBattlePlayers().Num(),2);
 FBattleCameraTestAccess::Cleanup(W);GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);GI->RemoveFromRoot();
 return true;
}
#endif

