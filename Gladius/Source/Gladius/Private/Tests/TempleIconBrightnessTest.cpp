#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Scripts/Temple/Goddess.h"

struct FTempleIconBrightnessTestAccess
{
 static void State(AGoddess* G, EGoddessState S) { G->SetState(S, FGoddessStateContext()); }
 static void Update(AGoddess* G) { G->UpdateBlessingIcons(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleIconBrightnessTest, "Gladius.Temple.Icons.MotionBrightness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTempleIconBrightnessTest::RunTest(const FString& Parameters)
{
 UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
 Instance->AddToRoot(); Instance->InitializeStandalone();
 UWorld* World = Instance->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
 World->EnableActorTickAndUserCallbacks(true);
#endif
 World->InitializeActorsForPlay(FURL());
 UClass* GoddessClass = LoadClass<AGoddess>(nullptr, TEXT("/Game/Blueprints/BP_Goddess.BP_Goddess_C"));
 AGoddess* Goddess = World->SpawnActor<AGoddess>(GoddessClass);
 UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Time/i.i"));
 UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
 TArray<AStaticMeshActor*> Icons;
 TArray<UMaterialInstanceDynamic*> Originals;
 for (int32 I = 0; I < 4; ++I)
 {
  AStaticMeshActor* Icon = World->SpawnActor<AStaticMeshActor>();
  Icon->GetStaticMeshComponent()->SetStaticMesh(Shape);
  UMaterialInstanceDynamic* Original = UMaterialInstanceDynamic::Create(Material, Icon);
  Original->SetScalarParameterValue(TEXT("Glow"), 10.f * (I + 1));
  Icon->GetStaticMeshComponent()->SetMaterial(0, Original);
  Goddess->BlessingIconActors[I] = Icon;
  Icons.Add(Icon); Originals.Add(Original);
 }
 Goddess->DispatchBeginPlay();
 USkeletalMeshComponent* Mesh = Goddess->FindComponentByClass<USkeletalMeshComponent>();
 UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
 UAnimMontage* Montage = Goddess->BlessingIconMontage.LoadSynchronous();
 const auto Check = [&](int32 Lit)
 {
  for (int32 I = 0; I < 4; ++I)
  {
   float Glow = -1.f;
   Icons[I]->GetStaticMeshComponent()->GetMaterial(0)->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Glow")), Glow);
   TestEqual(*FString::Printf(TEXT("Icon %d while active %d"), I, Lit), Glow, (I + 1) * (I == Lit ? 4.f : 0.4f), KINDA_SMALL_NUMBER);
  }
 };
 Check(INDEX_NONE);
 if (TestNotNull(TEXT("Goddess animation instance"), Anim) && TestNotNull(TEXT("Bestow montage"), Montage))
 {
  for (int32 I = 0; I < 4; ++I)
  {
   TestTrue(TEXT("Bestow plays"), Anim->Montage_Play(Montage) > 0.f);
   FTempleIconBrightnessTestAccess::State(Goddess, static_cast<EGoddessState>(static_cast<int32>(EGoddessState::Blessing1) + I));
   Check(I);
  }
  Anim->Montage_Stop(0.f, Montage);
  FTempleIconBrightnessTestAccess::Update(Goddess);
  Check(INDEX_NONE); // Motion ended before the state timer.
  Anim->Montage_Play(Montage);
  FTempleIconBrightnessTestAccess::State(Goddess, EGoddessState::Reading);
  Check(INDEX_NONE);
  FTempleIconBrightnessTestAccess::State(Goddess, EGoddessState::Blessing2);
  Check(1);
  Goddess->StopCycle();
  Check(INDEX_NONE);
 }
 Goddess->Destroy();
 for (int32 I = 0; I < 4; ++I)
 {
  TestTrue(TEXT("EndPlay restores authored material"), Icons[I]->GetStaticMeshComponent()->GetMaterial(0) == Originals[I]);
  Icons[I]->Destroy();
 }
 float OriginalGlow = 0.f;
 Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Glow")), OriginalGlow);
 TestEqual(TEXT("Shared material stays at full authored brightness"), OriginalGlow, 10.f);
 Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
 return true;
}
#endif
