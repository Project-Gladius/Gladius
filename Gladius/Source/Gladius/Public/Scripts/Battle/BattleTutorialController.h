#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Scripts/UI/BattleHud/BattleTopHudTypes.h"
#include "BattleTutorialController.generated.h"
class ABattleManager;
enum class ETutorialFlow : uint8 { Inactive, Intro, Offer, LoginExplanation, LoginWait, Explanation, Input, Action, StartingBattle, Complete };
struct FTutorialMessage
{
 FString Text;
 float Seconds = 10.f;
};
UCLASS()
class GLADIUS_API UBattleTutorialController : public UObject
{
 GENERATED_BODY()
public:
 UBattleTutorialController();
 UPROPERTY() TObjectPtr<UObject> EmphasisFont;
 void BeginEntry(ABattleManager* InManager);
 void OnIntroFinished();
 bool RequestTutorial();
 bool IsActive() const { return Flow != ETutorialFlow::Inactive && Flow != ETutorialFlow::Complete; }
 bool IsPracticing() const { return Flow != ETutorialFlow::Inactive && Flow != ETutorialFlow::Intro && Flow != ETutorialFlow::Offer && Flow != ETutorialFlow::Complete; }
 bool AcceptsAction() const { return Flow == ETutorialFlow::Explanation || Flow == ETutorialFlow::Input; }
 bool AcceptsLogin() const { return Flow != ETutorialFlow::StartingBattle && Flow != ETutorialFlow::Action && Flow != ETutorialFlow::Inactive && Flow != ETutorialFlow::Complete; }
 bool SuppressesBossAttack() const { return IsPracticing() && TutorialTurn != 3; }
 void OnPlayerRegistered(const FString& UserId);
 void OnLoginSucceeded();
 void OnActionAccepted();
 void OnInputClosed();
 void OnTurnFinished();
 void Shutdown();
 FBattleTopHudState BuildHud() const;
 static FString FormatRichText(const FString& Source);
private:
 friend struct FBattleTutorialTestAccess;
 friend struct FBattleAudioTestAccess;
 UPROPERTY() TObjectPtr<ABattleManager> Manager;
 ETutorialFlow Flow = ETutorialFlow::Inactive;
 TArray<FTutorialMessage> Messages;
 TMap<FString,int32> SavedHealth;
 int32 MessageIndex = 0;
 int32 TutorialTurn = 0;
 int32 EmptyTurns = 0;
 bool bTurnHadInput = false;
 bool bInputStarted = false;
 bool bReturnAfterTurn = false;
 double EntryTime = 0.;
 FString Guide;
 float FrozenSeconds = 0.f;
 FTimerHandle MessageTimer, DeadlineTimer;
 FDelegateHandle IntroHandle;
 void ResumeChatPolling();
 void BeginOffer();
 void ReturnToOffer();
 void ExpireOffer();
 void Queue(TArray<FTutorialMessage> InMessages);
 void ShowNextMessage();
 void MessagesFinished();
 void BeginLoginWait();
 void RevealBoss();
 void EnterTurn(int32 Turn);
 void BeginInput(float Seconds);
 void BeginBattleAnnouncement();
 void CompleteToMain();
 void RestoreHealth();
 void ClearTimers();
};
