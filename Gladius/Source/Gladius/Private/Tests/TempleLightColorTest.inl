#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/Time/RealTimeSunSubsystem.h"
#include "Scripts/Time/TemporaryMoonLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"

struct FTempleLightColorTestAccess
{
    static ATemporaryMoonLight* Moon(URealTimeSunSubsystem* S) { return S->TemporaryMoon.Get(); }
    static UStaticMeshComponent* MoonSphere(ATemporaryMoonLight* M) { return M->FindComponentByClass<UStaticMeshComponent>(); }
    static UDirectionalLightComponent* MoonLight(ATemporaryMoonLight* M) { return M->FindComponentByClass<UDirectionalLightComponent>(); }
    static void Prepare(UWorld* W)
    {
        W->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
        W->SetGameMode(FURL());
        W->InitializeActorsForPlay(FURL());
    }
    static void SunAt(URealTimeSunSubsystem* S, const FDateTime& Now) { S->UpdateSun(Now); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleLightColorTest,"Gladius.RealTime.TempleLightColor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleLightColorTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI=NewObject<UGameInstance>(GEngine);
    GI->AddToRoot(); GI->InitializeStandalone();
    UWorld* W=GI->GetWorld();
#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
    W->EnableActorTickAndUserCallbacks(true);
#endif
    FTempleLightColorTestAccess::Prepare(W);
    ADirectionalLight* Actor=W->SpawnActor<ADirectionalLight>();
    UDirectionalLightComponent* Light=CastChecked<UDirectionalLightComponent>(Actor->GetLightComponent());
    URealTimeSettings* Settings=GetMutableDefault<URealTimeSettings>();
    TGuardValue<TSoftObjectPtr<UWorld>> BattleGuard(Settings->BattleMap,TSoftObjectPtr<UWorld>(W));
    TGuardValue<TSoftObjectPtr<UWorld>> TempleGuard(Settings->TempleMap,Settings->TempleMap);
    TGuardValue<bool> EnabledGuard(Settings->bEnableSun,true);
    TGuardValue<bool> MoonGuard(Settings->bEnableTemporaryMoon,true);
    TGuardValue<FRealTimeSunProfile> ProfileGuard(Settings->BattleSun,Settings->BattleSun);
    TGuardValue<FRealTimeSunProfile> TempleProfileGuard(Settings->TempleSun,Settings->TempleSun);
    Settings->BattleSun.NoonAltitude=60; Settings->BattleSun.NoonDirection=35; Settings->BattleSun.SunActorTag=NAME_None;
    Settings->TempleSun.NoonAltitude=25; Settings->TempleSun.NoonDirection=150; Settings->TempleSun.SunActorTag=NAME_None;
    TArray<ALight*> TempleLights{W->SpawnActor<APointLight>(), W->SpawnActor<ASpotLight>(), W->SpawnActor<ARectLight>()};
    for (ALight* Target : TempleLights)
    {
        Target->GetLightComponent()->SetLightColor(FLinearColor::Red);
    }
    AActor* Unrelated = W->SpawnActor<AActor>();
    UPointLightComponent* DecorativeLight = NewObject<UPointLightComponent>(Unrelated);
    Unrelated->SetRootComponent(DecorativeLight);
    DecorativeLight->RegisterComponent();
    DecorativeLight->SetLightColor(FLinearColor::Blue);
    W->BeginPlay();
    URealTimeSunSubsystem* S=W->GetSubsystem<URealTimeSunSubsystem>();
    const FDateTime Noon(2026,10,24,12,0,0);
    FTempleLightColorTestAccess::SunAt(S,Noon);
    TestTrue(TEXT("Battle sun rotates actual light"),Light->GetForwardVector().Equals(-URealTimeSunSubsystem::CalculateSunDirection(12,Settings->BattleSun),0.001));
    TestEqual(TEXT("Sun supports runtime rotation"),Light->Mobility,EComponentMobility::Movable);
    ATemporaryMoonLight* Moon=FTempleLightColorTestAccess::Moon(S);
    if (TestNotNull(TEXT("Temporary moon created automatically"),Moon))
    {
        auto* Globe=FTempleLightColorTestAccess::MoonSphere(Moon);
        auto* MoonLight=FTempleLightColorTestAccess::MoonLight(Moon);
        TestTrue(TEXT("Moon color temperature enabled"), MoonLight->bUseTemperature != 0);
        TestEqual(TEXT("Moon uses configured Kelvin temperature"), MoonLight->Temperature, Settings->TemporaryMoonTemperature);
        TestEqual(TEXT("Default moon temperature is 3700K"), Settings->TemporaryMoonTemperature, 3700.f);
        TestFalse(TEXT("Moon sphere hidden below daytime horizon"),Globe->IsVisible());
        TestEqual(TEXT("No moon illumination at noon"),MoonLight->Intensity,0.f);
        FTempleLightColorTestAccess::SunAt(S,FDateTime(2026,10,24,0,0,0));
        TestTrue(TEXT("Night sphere is at zenith"),Globe->GetRelativeLocation().GetSafeNormal().Equals(FVector::UpVector,0.001));
        TestTrue(TEXT("Night moon sphere is visible"),Globe->IsVisible());
        TestTrue(TEXT("Night moon illuminates entire map"),MoonLight->Intensity > 0);
        TestTrue(TEXT("Night light shines straight down"),MoonLight->GetForwardVector().Equals(-FVector::UpVector,0.001));
        for (int32 Hour : {19, 23, 1, 5})
        {
            FTempleLightColorTestAccess::SunAt(S,FDateTime(2026,10,24,Hour,0,0));
            TestTrue(TEXT("Sphere stays overhead throughout the night"),Globe->GetRelativeLocation().GetSafeNormal().Equals(FVector::UpVector,0.001));
            TestTrue(TEXT("Night sphere remains visible"),Globe->IsVisible());
        }
        const FLinearColor Color=MoonLight->GetLightColor();
        TestTrue(TEXT("Moon emits yellow light"),Color.R > Color.B && Color.G > Color.B);
        TestTrue(TEXT("Sphere has emissive material"),Globe->GetMaterial(0) != nullptr);
        TestFalse(TEXT("Sphere never blocks gameplay"),Globe->IsCollisionEnabled());
    }
    TestTrue(TEXT("Battle map does not recolor temple targets"), TempleLights[0]->GetLightComponent()->GetLightColor().Equals(FLinearColor::Red));
    Settings->BattleMap=Settings->TempleMap;
    Settings->TempleMap=TSoftObjectPtr<UWorld>(W);
    FTempleLightColorTestAccess::SunAt(S,Noon);
    TestTrue(TEXT("Temple selects its independent profile"),Light->GetForwardVector().Equals(-URealTimeSunSubsystem::CalculateSunDirection(12,Settings->TempleSun),0.001));
    Light->SetLightColor(FLinearColor(0.25f, 0.5f, 0.75f));
    Light->SetTemperature(4200.f);
    Light->SetUseTemperature(true);
    FTempleLightColorTestAccess::SunAt(S,Noon);
    for (ALight* Target : TempleLights)
    {
        auto* Component = Target->GetLightComponent();
        TestTrue(TEXT("All three temple light types follow sun color"), Component->GetLightColor().Equals(Light->GetLightColor()));
        TestEqual(TEXT("Sun temperature copied"), Component->Temperature, Light->Temperature);
        TestTrue(TEXT("Sun temperature enabled"), Component->bUseTemperature != 0);
        TestEqual(TEXT("Runtime light is movable"), Component->Mobility, EComponentMobility::Movable);
    }
    Light->SetLightColor(FLinearColor::Green);
    FTempleLightColorTestAccess::SunAt(S,Noon);
    TestTrue(TEXT("Later sun color changes propagate"), TempleLights[0]->GetLightComponent()->GetLightColor().Equals(Light->GetLightColor()));
    FTempleLightColorTestAccess::SunAt(S,FDateTime(2026,10,24,0,0,0));
    if (Moon)
        for (ALight* Target : TempleLights)
            TestTrue(TEXT("Night targets follow moon color"), Target->GetLightComponent()->GetLightColor().Equals(FTempleLightColorTestAccess::MoonLight(Moon)->GetLightColor()));
    Settings->bEnableTemporaryMoon=false;
    FTempleLightColorTestAccess::SunAt(S,FDateTime(2026,10,24,0,0,0));
    TestTrue(TEXT("Without moon, night follows sun color"), TempleLights[0]->GetLightComponent()->GetLightColor().Equals(Light->GetLightColor()));
    TestTrue(TEXT("Unselected lighting keeps its color"), DecorativeLight->GetLightColor().Equals(FLinearColor::Blue));
    APointLight* Late = W->SpawnActor<APointLight>();
    FTempleLightColorTestAccess::SunAt(S,Noon);
    TestTrue(TEXT("Late loaded targets also synchronize"), Late->GetLightComponent()->GetLightColor().Equals(Light->GetLightColor()));
    for (ALight* Target : TempleLights) Target->Destroy();
    Unrelated->Destroy();
    Late->Destroy();
    const FQuat Before=Light->GetComponentQuat();
    Settings->bEnableSun=false;
    FTempleLightColorTestAccess::SunAt(S,FDateTime(2026,10,24,18,0,0));
    TestTrue(TEXT("Disabled sun keeps rotation"),Light->GetComponentQuat().Equals(Before,0.001));
    TestNull(TEXT("Disabling clock lighting removes temporary moon"),FTempleLightColorTestAccess::Moon(S));
    Actor->Destroy();
    W->EndPlay(EEndPlayReason::Quit);
    GI->Shutdown(); GEngine->DestroyWorldContext(W); W->DestroyWorld(false); GI->RemoveFromRoot();
    return true;
}

#endif
