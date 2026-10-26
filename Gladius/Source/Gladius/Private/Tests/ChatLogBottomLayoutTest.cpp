#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/UI/ChatLogWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatLogBottomLayoutTest, "Gladius.UI.ChatLog.BottomLayout",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChatLogBottomLayoutTest::RunTest(const FString& Parameters)
{
 UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
 Instance->AddToRoot();
 Instance->InitializeStandalone();
 UWorld* World = Instance->GetWorld();
 ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
 Instance->AddLocalPlayer(Local, FPlatformUserId::CreateFromInternalId(0));
 APlayerController* Controller = World->SpawnActor<APlayerController>();
 Controller->SetPlayer(Local);
 UChatLogWidget* Chat = CreateWidget<UChatLogWidget>(Controller);
 if (!TestNotNull(TEXT("Chat widget"), Chat)) return false;
 TSharedRef<SWidget> Slate = Chat->TakeWidget();
 UChatLogSubsystem* Log = Instance->GetSubsystem<UChatLogSubsystem>();
 UVerticalBox* List = Cast<UVerticalBox>(Chat->WidgetTree->FindWidget(TEXT("MessageList")));
 UCanvasPanel* Viewport = Cast<UCanvasPanel>(Chat->WidgetTree->FindWidget(TEXT("MessageViewport")));
 if (!TestNotNull(TEXT("Message list"), List) || !TestNotNull(TEXT("Non-scrolling viewport"), Viewport)) return false;
 Chat->WidgetTree->ForEachWidget([this](UWidget* Widget)
 {
  TestFalse(TEXT("Chat contains no scrolling controls"), Widget->IsA<UScrollBox>());
 });
 FWidgetRenderer Renderer(true, false);
 auto CheckBottom = [this, Chat, List, Viewport, &Renderer, Slate](const TCHAR* Scenario)
 {
  // Auto-wrapped text settles after receiving its actual width from the first layout.
  for (int32 Frame = 0; Frame < 4; ++Frame)
  {
   Renderer.DrawWidget(Slate, FVector2D(1280, 720));
   FlushRenderingCommands();
  }
  UWidget* Last = List->GetChildAt(List->GetChildrenCount() - 1);
  const FGeometry& RowGeometry = Last->GetCachedGeometry();
  const FGeometry& ViewGeometry = Viewport->GetCachedGeometry();
  FVector2D Top = ViewGeometry.AbsoluteToLocal(RowGeometry.LocalToAbsolute(FVector2D::ZeroVector));
  FVector2D Bottom = ViewGeometry.AbsoluteToLocal(RowGeometry.LocalToAbsolute(RowGeometry.GetLocalSize()));
  TestTrue(*FString::Printf(TEXT("%s: latest row bottom remains 5px above viewport bottom"), Scenario),
   FMath::IsNearlyEqual(Bottom.Y, ViewGeometry.GetLocalSize().Y - 5.f, 1.f));
  TestTrue(*FString::Printf(TEXT("%s: entire latest multiline row is visible"), Scenario), Top.Y >= -1.f);
  TestTrue(*FString::Printf(TEXT("%s: latest row has a multiline height"), Scenario), RowGeometry.GetLocalSize().Y > 45.f);
  TestTrue(TEXT("Old rows are clipped to the viewport"), Viewport->GetClipping() == EWidgetClipping::ClipToBoundsAlways);
 };
 Log->AddGoddessMessage(TEXT("First line\nSecond line\nThird line"));
 CheckBottom(TEXT("Short history"));
 for (int32 Index = 0; Index < 120; ++Index) Log->AddGoddessMessage(FString::Printf(TEXT("Older message %d"), Index));
 Log->AddGoddessMessage(TEXT("A long newest message that wraps automatically across several lines at the current chat width. Its final line must stay fully visible even after the history exceeds the window height."));
 CheckBottom(TEXT("Overflowing history and automatic wrapping"));
 TestEqual(TEXT("Visible history remains capped at 100 rows"), List->GetChildrenCount(), 100);
 Chat->SetExpansionAllowed(true);
 Chat->SetExpanded(true);
 CheckBottom(TEXT("Expanded window"));
 Chat->SetExpanded(false);
 CheckBottom(TEXT("Collapsed again"));
 Chat->RemoveFromParent();
 Instance->Shutdown();
 GEngine->DestroyWorldContext(World);
 World->DestroyWorld(false);
 Instance->RemoveFromRoot();
 return true;
}
#endif
