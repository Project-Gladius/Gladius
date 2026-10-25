#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "BattleCameraTestAccess.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Scripts/Battle/BattleUnit.h"

struct FBattlePlacementTestAccess
{
 static void Spots(ABattleManager* B)
 {
  for (int32 Lane=0;Lane<3;++Lane)
  {
   B->Spots[Lane].Reset();
   for (int32 i=0;i<9;++i) B->Spots[Lane].Add(B->BossLocation()+FVector(-100.f*(i+1), 1000.f*(Lane-1),0.f));
  }
 }
 static const TMap<FString,FBattlePlayer>& Players(ABattleManager* B) { return B->Players; }
 static void NextTurn(ABattleManager* B) { B->StartTurn(); }
 static void Move(ABattleManager* B) { B->BeginMove(); }
 static bool Rear(ABattleManager* B,const FBattlePlayer& P) { return B->FormationGroup(P.ClassIndex)==1; }
 static int32 Slot(ABattleManager* B,const FBattlePlayer& P) { return B->Spots[(int32)P.Lane].IndexOfByPredicate([&](const FVector& V){return V.Equals(P.TargetLocation,1.f);}); }
 static bool Random(ABattleManager* B) { return B->bRandomInitialZone; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedFormationTest,"Gladius.Placement.FixedHalves",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFixedFormationTest::RunTest(const FString& Parameters)
{
 TArray<FVector> Spots;
 for(int32 i=0;i<9;++i) Spots.Add(FVector(100.f*(i+1),0,0));
 for (const TArray<int32>& Counts : {TArray<int32>{1,8},TArray<int32>{8,1},TArray<int32>{0,4},TArray<int32>{5,0}})
 {
  TArray<TArray<int32>> Chosen;
  ABattleManager::ChooseClassSpots(Spots,Counts,FVector::ZeroVector,Chosen);
  for(int32 G=0;G<2;++G)
  {
   TestEqual(TEXT("Requested count retained"),Chosen[G].Num(),Counts[G]);
   for(int32 I:Chosen[G]) TestTrue(TEXT("Odd split is 5 front and 4 rear regardless of population"),G==0 ? I<5 : I>=5);
  }
 }
 TArray<TArray<int32>> Spread;
 ABattleManager::ChooseClassSpots(Spots,{2,2},FVector::ZeroVector,Spread);
 TestTrue(TEXT("Front spreads to both ends of its half"),Spread[0].Contains(0)&&Spread[0].Contains(4));
 TestTrue(TEXT("Rear spreads to both ends of its half"),Spread[1].Contains(5)&&Spread[1].Contains(8));
 ABattleManager::ChooseClassSpots({FVector::ZeroVector},{1,1},FVector::ZeroVector,Spread);
 TestEqual(TEXT("One spot belongs to front"),Spread[0].Num(),1);
 TestEqual(TEXT("Rear never borrows front"),Spread[1].Num(),0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoinPlacementTest,"Gladius.Placement.JoinAndNextMove",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJoinPlacementTest::RunTest(const FString& Parameters)
{
 UGameInstance* Instance=NewObject<UGameInstance>(GEngine); Instance->AddToRoot(); Instance->InitializeStandalone();
 UWorld* W=Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
 W->EnableActorTickAndUserCallbacks(true);
#endif
 W->InitializeActorsForPlay(FURL());
 ABattleManager* B=W->SpawnActor<ABattleManager>();
 FBattleCameraTestAccess::RecordsOnly(B); B->DispatchBeginPlay();
 B->MaxPlayers=24;
 FBattlePlacementTestAccess::Spots(B);
 FBattleCameraTestAccess::BeginPrepared(B);
 TestTrue(TEXT("Random entry enabled by default"),FBattlePlacementTestAccess::Random(B));
 TSet<int32> Lanes;
 for(int32 i=0;i<24;++i)
 {
  TMap<FString,FVector> OldTargets,OldPositions;
  for(const auto& Pair:FBattlePlacementTestAccess::Players(B))
  { OldTargets.Add(Pair.Key,Pair.Value.TargetLocation); OldPositions.Add(Pair.Key,Pair.Value.Unit->GetActorLocation()); }
  const FString Id=FString::Printf(TEXT("placement_%02d"),i);
  TestTrue(TEXT("Join accepted"),B->SubmitAction(Id,Id,EBattleAction::Idle,i%2==0?TEXT("gladius"):TEXT("sera")));
  for(const auto& Pair:OldTargets)
  {
   const auto& P=FBattlePlacementTestAccess::Players(B).FindChecked(Pair.Key);
   TestTrue(TEXT("Join preserves existing target"),P.TargetLocation.Equals(Pair.Value));
   TestTrue(TEXT("Join preserves existing position"),P.Unit->GetActorLocation().Equals(OldPositions.FindChecked(Pair.Key)));
  }
  const auto* P=FBattlePlacementTestAccess::Players(B).Find(Id);
  if (!P) break;
  Lanes.Add((int32)P->Lane);
  const int32 Slot=FBattlePlacementTestAccess::Slot(B,*P);
  TestTrue(TEXT("Initial slot obeys character half"),Slot>=0&&(FBattlePlacementTestAccess::Rear(B,*P)?Slot>=5:Slot<5));
 }
 TestTrue(TEXT("Entries use multiple lanes"),Lanes.Num()>1);
 TMap<FString,FVector> BeforeMove;
 for(const auto& Pair:FBattlePlacementTestAccess::Players(B)) BeforeMove.Add(Pair.Key,Pair.Value.TargetLocation);
 FBattlePlacementTestAccess::Move(B);
 for(const auto& Pair:BeforeMove)
  TestTrue(TEXT("Current turn retains random join slot"),FBattlePlacementTestAccess::Players(B).FindChecked(Pair.Key).TargetLocation.Equals(Pair.Value));
 FBattlePlacementTestAccess::NextTurn(B);
 for(const auto& Pair:FBattlePlacementTestAccess::Players(B))
 {
  const int32 Slot=FBattlePlacementTestAccess::Slot(B,Pair.Value);
  TestTrue(TEXT("Next movement retains formation constraint"),Slot>=0&&(FBattlePlacementTestAccess::Rear(B,Pair.Value)?Slot>=5:Slot<5));
 }
 FBattleCameraTestAccess::Cleanup(W); Instance->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);Instance->RemoveFromRoot();
 return true;
}
#endif
