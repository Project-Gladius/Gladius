#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/UI/BattleHud/BattleTopHudTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Scripts/Time/RealTimeClockSubsystem.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"
#include "AssetCompilingManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontCache.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleTopHudTest, "Gladius.BattleTopHud.Temple", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTempleTopHudTest::RunTest(const FString& Parameters)
{
 const FDateTime End(2026,10,26,14,0), Now(2026,10,26,14,30), Next(2026,10,26,15,0);
 const auto State = FBattleTopHudPresenter::BuildTemple(Now,End,Next,true);
 TestTrue(TEXT("Temple visible"),State.bVisible);
 TestEqual(TEXT("Left title exact"),State.TurnLabel,FString(TEXT("현재 시간")));
 TestEqual(TEXT("Local time HH:MM"),State.TurnText,FString(TEXT("14:30")));
 TestTrue(TEXT("No turn total"),State.TurnTotalText.IsEmpty());
 TestEqual(TEXT("Right title exact"),State.TimerLabel,FString(TEXT("다음 전투")));
 TestEqual(TEXT("Remaining HH:MM"),State.TimerText,FString(TEXT("00:30")));
 TestEqual(TEXT("Guide exact"),State.GuideText,FString(TEXT("!강화로 축복을 선택하세요! (인당 3회)")));
 TestEqual(TEXT("Half interval elapsed"),State.GaugePercent,0.5f);
 TestEqual(TEXT("Just ended starts empty"),FBattleTopHudPresenter::BuildTemple(End,End,Next,true).GaugePercent,0.f);
 TestEqual(TEXT("Due starts full"),FBattleTopHudPresenter::BuildTemple(Next,End,Next,true).GaugePercent,1.f);
 TestEqual(TEXT("Expired remaining clamps"),FBattleTopHudPresenter::BuildTemple(Next+FTimespan::FromMinutes(5),End,Next,true).TimerText,FString(TEXT("00:00")));
 TestEqual(TEXT("Midnight countdown"),FBattleTopHudPresenter::BuildTemple(FDateTime(2026,10,26,23,45),End,FDateTime(2026,10,27,1,15),true).TimerText,FString(TEXT("01:30")));
 TestEqual(TEXT("Partial minute stays nonzero"),FBattleTopHudPresenter::BuildTemple(Next-FTimespan::FromSeconds(1),End,Next,true).TimerText,FString(TEXT("00:01")));
 const auto Missing=FBattleTopHudPresenter::BuildTemple(Now,End,FDateTime(),false);
 TestEqual(TEXT("No schedule has placeholder"),Missing.TimerText,FString(TEXT("--:--")));
 TestEqual(TEXT("No schedule has no ratio"),Missing.GaugePercent,0.f);
 TestEqual(TEXT("Invalid interval is safe"),FBattleTopHudPresenter::BuildTemple(Now,Next,Next,true).GaugePercent,0.f);
 return true;
}
struct FTempleTopHudTestAccess
{
 static void Create(ATempleCycleController* C) { C->CreateStatusWidget(); }
 static UBattleTopHudWidget* Hud(ATempleCycleController* C) { return C->TopHudWidget; }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleTopHudIntegrationTest, "Gladius.BattleTopHud.TempleIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTempleTopHudIntegrationTest::RunTest(const FString& Parameters)
{
 UGameInstance* Instance=NewObject<UGameInstance>(GEngine);
 Instance->AddToRoot(); Instance->InitializeStandalone();
 UWorld* World=Instance->GetWorld();
 ULocalPlayer* Local=NewObject<ULocalPlayer>(GEngine); Instance->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));
 auto* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(Local);
 ATempleCycleController* Controller=World->SpawnActor<ATempleCycleController>();
 Controller->StatusWidgetClass=nullptr; Controller->ChatLogWidgetClass=nullptr; Controller->SessionLogWidgetClass=nullptr;
 auto* Schedule=Instance->GetSubsystem<UBattleScheduleSubsystem>();
 const auto Now=Instance->GetSubsystem<URealTimeClockSubsystem>()->GetLocalTime();
 Schedule->RecordBattleEnd(Now-FTimespan::FromMinutes(30));
 TestEqual(TEXT("Battle end survives repeated temple reads"),Schedule->GetTempleIntervalStart(Now+FTimespan::FromMinutes(2)),Now-FTimespan::FromMinutes(30));
 FTempleTopHudTestAccess::Create(Controller);
 UBattleTopHudWidget* Hud=FTempleTopHudTestAccess::Hud(Controller);
 TestNotNull(TEXT("Temple controller creates HUD without status panel"),Hud);
 if(Hud)
 {
  TestEqual(TEXT("Uses shared battle designer asset"),Hud->GetClass(),GladiusUI::ResolveWidgetClass<UBattleTopHudWidget>(Controller->TopHudWidgetClass,GladiusUI::AssetPaths::TopHud).Get());
  auto CheckText=[this,Hud](const TCHAR* Name,const TCHAR* Expected)
  {
   auto* Text=Cast<UTextBlock>(Hud->GetWidgetFromName(Name));
   if(TestNotNull(Name,Text)) TestEqual(Name,Text->GetText().ToString(),FString(Expected));
  };
  CheckText(TEXT("TurnLabelText"),TEXT("현재 시간"));
  CheckText(TEXT("TempleTimerLabelText"),TEXT("다음 전투"));
  CheckText(TEXT("TH_GuideText"),TEXT("!강화로 축복을 선택하세요! (인당 3회)"));
  auto* Time=Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("TH_TurnValueText")));
  if(TestNotNull(TEXT("Current time"),Time)) TestEqual(TEXT("Current time format"),Time->GetText().ToString(),Now.ToString(TEXT("%H:%M")));
  // FApp avoids a dependency on RHI globals in the gameplay module.
  if (FApp::CanEverRender())
  {
   FAssetCompilingManager::Get().FinishAllCompilation();
   const auto Slate=Hud->TakeWidget(); FWidgetRenderer Renderer(true);
   for(int32 Pass=0;Pass<6;++Pass)
   {
    Renderer.DrawWidget(Slate,FVector2D(1920,1080));
    FSlateApplication::Get().GetRenderer()->GetFontCache()->UpdateCache(); FlushRenderingCommands();
   }
   for(const TCHAR* Name : {TEXT("TurnLabelText"),TEXT("TH_TurnValueText"),TEXT("TH_GuideText"),TEXT("TempleTimerLabelText"),TEXT("TH_TimerText")})
   {
    UWidget* W=Hud->GetWidgetFromName(Name);
    if(TestNotNull(TEXT("Temple text exists"),W))
     TestTrue(*FString::Printf(TEXT("%s fits panel"),Name),W->GetDesiredSize().X <= W->GetCachedGeometry().GetLocalSize().X+1.f && W->GetDesiredSize().Y <= W->GetCachedGeometry().GetLocalSize().Y+1.f);
   }
   FString GeometryReport;
   for(const TCHAR* Name : {TEXT("TopHudFrame"),TEXT("TopHudBar"),TEXT("TurnPanel"),TEXT("CenterPanel"),TEXT("TimerPanel"),TEXT("TurnLabelText"),TEXT("TH_TurnValueText"),TEXT("TH_GuideText"),TEXT("TempleTimerLabelText"),TEXT("TH_TimerText")})
   {
    if(auto* W=Hud->GetWidgetFromName(Name))
    {
     const auto G=W->GetCachedGeometry();
     GeometryReport+=FString::Printf(TEXT("%s position=%s size=%s desired=%s\n"),Name,*G.GetAbsolutePosition().ToString(),*G.GetLocalSize().ToString(),*W->GetDesiredSize().ToString());
    }
   }
   FFileHelper::SaveStringToFile(GeometryReport,*(FPaths::ProjectSavedDir()/TEXT("Automation/TempleTopHud/geometry.txt")));
   auto* Target=Renderer.DrawWidget(Slate,FVector2D(1920,120)); FlushRenderingCommands();
   FBufferArchive Bytes; const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/TempleTopHud");
   IFileManager::Get().MakeDirectory(*Directory,true);
   if(Target && FImageUtils::ExportRenderTarget2DAsPNG(Target,Bytes)) FFileHelper::SaveArrayToFile(Bytes,*(Directory/TEXT("temple-top-hud.png")));
   Hud->ReleaseSlateResources(true);
  }
  Hud->RemoveFromParent();
 }
 World->DestroyActor(Controller); Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
 return true;
}
#endif
