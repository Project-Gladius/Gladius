#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/UI/ChatLogWidget.h"
#include "Scripts/UI/SessionLogWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontCache.h"
#include "AssetCompilingManager.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainBattleSideLayoutTest,"Gladius.UI.MainBattleSideLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainBattleSideLayoutTest::RunTest(const FString& Parameters)
{
 UGameInstance* Instance=NewObject<UGameInstance>(GEngine); Instance->AddToRoot(); Instance->InitializeStandalone();
 ULocalPlayer* Local=NewObject<ULocalPlayer>(GEngine); Instance->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));
 APlayerController* PC=Instance->GetWorld()->SpawnActor<APlayerController>(); PC->SetPlayer(Local);
 auto* Leader=CreateWidget<ULeaderboardRotatorWidget>(PC,LoadClass<ULeaderboardRotatorWidget>(nullptr,TEXT("/Game/Blueprints/UI/WBP_LeaderboardRotator.WBP_LeaderboardRotator_C")));
 auto* Chat=CreateWidget<UChatLogWidget>(PC);
 auto* Notice=CreateWidget<USessionLogWidget>(PC);
 if (!TestNotNull(TEXT("Leaderboard"),Leader)) return false;
 UOverlay* Root=NewObject<UOverlay>(Instance);
 for (UUserWidget* W : TArray<UUserWidget*>{Leader,Chat,Notice})
 {
  auto* Slot=Root->AddChildToOverlay(W); Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
 }
 TSharedRef<SWidget> Slate=Root->TakeWidget();
 FAssetCompilingManager::Get().FinishAllCompilation();
 for (auto* W : {Leader->GetWidgetFromName(TEXT("LR_Board")),Leader->GetWidgetFromName(TEXT("BattleBoard"))})
  if(auto* B=Cast<ULeaderboardBoardWidget>(W))
   if(auto* I=Cast<UImage>(B->GetWidgetFromName(TEXT("NativeBackgroundImage"))))
    if(auto* T=Cast<UTexture2D>(I->GetBrush().GetResourceObject())) { T->SetForceMipLevelsToBeResident(30); T->WaitForStreaming(); }
 Instance->GetSubsystem<UChatLogSubsystem>()->AddGoddessMessage(TEXT("철수, 영희에게 힘의 축복을 내리노라."));
 Instance->GetSubsystem<USessionLogSubsystem>()->AddMessage(TEXT("전투 공지사항"),ESessionLogEvent::Admin);
 FWidgetRenderer Renderer(true);
 for (float Width : {1920.f,2520.f,1440.f})
 {
  const FVector2D Size(Width,1080);
  const float Side=1080.f*(16.f/9.f)/5.f;
  Notice->WindowSize=FVector2D(Side,Side/6.f); Notice->WindowOffset=FVector2D(-24,24); Notice->RefreshStyle();
  Chat->ApplyBattleSideLayout(Side,24,24+Side/6.f+12,1080*.42f);
  for(int32 Pass=0;Pass<5;++Pass)
  {
   Renderer.DrawWidget(Slate,Size); FSlateApplication::Get().GetRenderer()->GetFontCache()->UpdateCache(); FlushRenderingCommands(); FSlateApplication::Get().Tick();
   Leader->ApplyBattleSideLayout(Side,24);
  }
  Renderer.DrawWidget(Slate,Size); FlushRenderingCommands();
  auto* Panel=Chat->GetWidgetFromName(TEXT("ChatPanel"));
  TestTrue(TEXT("Chat width matches 16:9 reference"),FMath::IsNearlyEqual(Panel->GetCachedGeometry().GetLocalSize().X,Side,.1f));
  auto* Switcher=Cast<UWidgetSwitcher>(Leader->GetWidgetFromName(TEXT("PageSwitcher")));
  for (int32 Page=0;Page<3;++Page)
  {
   // The rotator normally switches state on its timer. Exercise both artwork trees here.
   Leader->CurrentPage=static_cast<ULeaderboardRotatorWidget::EPage>(Page);
   Leader->RefreshCurrentPage();
   for(int32 Pass=0;Pass<4;++Pass) { Renderer.DrawWidget(Slate,Size); FlushRenderingCommands(); Leader->ApplyBattleSideLayout(Side,24); }
   Renderer.DrawWidget(Slate,Size); FlushRenderingCommands();
   auto* Board=Cast<ULeaderboardBoardWidget>(Switcher->GetActiveWidget());
    auto* Image=Board->GetWidgetFromName(TEXT("NativeBackgroundImage"));
    const FGeometry& G=Image->GetCachedGeometry();
    const FVector2D TL=Leader->GetCachedGeometry().AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
    const FVector2D TR=Leader->GetCachedGeometry().AbsoluteToLocal(G.LocalToAbsolute(FVector2D(G.GetLocalSize().X,0)));
    TestTrue(TEXT("Leaderboard artwork width matches reference"),FMath::IsNearlyEqual(float(TR.X-TL.X),Side,.5f));
    TestTrue(TEXT("Leaderboard top left margin"),TL.Equals(FVector2D(24,24),.5));
  }
  auto* Target=Renderer.DrawWidget(Slate,Size); FlushRenderingCommands();
  FBufferArchive Bytes;
  const FString OutputDirectory=FPaths::ProjectSavedDir()/TEXT("Automation/MainBattleLayout");
  IFileManager::Get().MakeDirectory(*OutputDirectory,true);
  if(Target && FImageUtils::ExportRenderTarget2DAsPNG(Target,Bytes)) FFileHelper::SaveArrayToFile(Bytes,*(OutputDirectory/FString::Printf(TEXT("ui-%d.png"),int32(Width))));
 }
 Root->ReleaseSlateResources(true); Instance->Shutdown(); Instance->RemoveFromRoot();
 return true;
}
#endif
