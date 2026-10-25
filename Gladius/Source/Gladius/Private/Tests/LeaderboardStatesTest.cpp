#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardRowWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

struct FLeaderboardStateTestAccess
{
 static void Show(ULeaderboardRotatorWidget* W, int32 Page)
 {
  W->CurrentPage = static_cast<ULeaderboardRotatorWidget::EPage>(Page);
  W->RefreshCurrentPage();
 }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeaderboardStateMigrationTest,"Gladius.UI.LeaderboardStateMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLeaderboardStateMigrationTest::RunTest(const FString& Parameters)
{
 UWidgetBlueprint* BP=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(ULeaderboardRotatorWidget::StaticClass(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UWidgetBlueprint::StaticClass(),TEXT("StateMigrationTest")),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
 if (!TestNotNull(TEXT("Temporary blueprint"),BP)) return false;
 GetDefault<ULeaderboardRotatorWidget>()->BuildDefaultLayout(BP->WidgetTree);
 BP->WidgetTree->ForEachWidget([BP](UWidget* Widget) { if (Widget->bIsVariable && !BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) BP->OnVariableAdded(Widget->GetFName()); });
 UWidget* Board=BP->WidgetTree->FindWidget(TEXT("Board"));
 UCanvasPanelSlot* Slot=Cast<UCanvasPanelSlot>(Board->Slot);
 Slot->SetPosition(FVector2D(193,87)); Slot->SetSize(FVector2D(639,411)); Slot->SetAlignment(FVector2D(.2,.3)); Slot->SetAutoSize(false); Slot->SetZOrder(7);
 Board->SetRenderTranslation(FVector2D(31,19)); Board->SetRenderScale(FVector2D(.63,.71));
 const FAnchorData Layout=Slot->GetLayout(); const FWidgetTransform Transform=Board->GetRenderTransform();
 TestTrue(TEXT("Migration succeeds"),ULeaderboardRotatorWidget::AddSeparateBattleState(BP,ULeaderboardBoardWidget::StaticClass()));
 UWidgetSwitcher* Switcher=Cast<UWidgetSwitcher>(BP->WidgetTree->FindWidget(TEXT("PageSwitcher")));
 if (!TestNotNull(TEXT("State switcher"),Switcher)) return false;
 TestTrue(TEXT("Original slot object retained"),Switcher->Slot==Slot);
 TestTrue(TEXT("Original board object retained"),Switcher->GetChildAt(0)==Board);
 TestTrue(TEXT("Original offsets preserved"),Slot->GetLayout().Offsets==Layout.Offsets);
 TestTrue(TEXT("Original alignment preserved"),Slot->GetAlignment()==Layout.Alignment);
 TestEqual(TEXT("Original Z order preserved"),Slot->GetZOrder(),7);
 TestFalse(TEXT("Original autosize preserved"),Slot->GetAutoSize());
 TestTrue(TEXT("Original render transform preserved"),Board->GetRenderTransform()==Transform);
 TestTrue(TEXT("Migration rerun succeeds without rebuilding"),ULeaderboardRotatorWidget::AddSeparateBattleState(BP,ULeaderboardBoardWidget::StaticClass()));
 TestEqual(TEXT("Rerun adds no duplicate states"),Switcher->GetChildrenCount(),2);
 TestTrue(TEXT("Rerun preserves board identity"),Switcher->GetChildAt(0)==Board);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeaderboardStateRuntimeTest,"Gladius.UI.LeaderboardStateRuntime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLeaderboardStateRuntimeTest::RunTest(const FString& Parameters)
{
 UGameInstance* Instance=NewObject<UGameInstance>(GEngine); Instance->AddToRoot(); Instance->InitializeStandalone();
 ULocalPlayer* Local=NewObject<ULocalPlayer>(GEngine);
 Instance->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));
 APlayerController* Controller=Instance->GetWorld()->SpawnActor<APlayerController>();
 Controller->SetPlayer(Local);
 UClass* Class=LoadClass<ULeaderboardRotatorWidget>(nullptr,TEXT("/Game/Blueprints/UI/WBP_LeaderboardRotator.WBP_LeaderboardRotator_C"));
 ULeaderboardRotatorWidget* W=Class ? CreateWidget<ULeaderboardRotatorWidget>(Controller,Class) : nullptr;
 if (!TestNotNull(TEXT("Saved rotator loads"),W)) { Instance->Shutdown(); Instance->RemoveFromRoot(); return false; }
 W->TakeWidget();
 auto* Switcher=Cast<UWidgetSwitcher>(W->WidgetTree->FindWidget(TEXT("PageSwitcher")));
 auto* Total=Cast<ULeaderboardBoardWidget>(W->WidgetTree->FindWidget(TEXT("Board")));
 auto* Battle=Cast<ULeaderboardBoardWidget>(W->WidgetTree->FindWidget(TEXT("BattleBoard")));
 if (!TestNotNull(TEXT("Switcher"),Switcher)||!TestNotNull(TEXT("Total"),Total)||!TestNotNull(TEXT("Battle"),Battle)) { Instance->Shutdown(); Instance->RemoveFromRoot(); return false; }
 TestTrue(TEXT("Independent board classes"),Total->GetClass()!=Battle->GetClass());
 TestTrue(TEXT("Independent row classes"),Total->RowWidgetClass!=Battle->RowWidgetClass);
 FLeaderboardStateTestAccess::Show(W,0);
 FLeaderboardStateTestAccess::Show(W,1);
 TArray<UTextBlock*> Names; TArray<UImage*> Images;
 for (auto* B : {Total,Battle})
 {
  AddInfo(B->GetClass()->GetPathName()); B->WidgetTree->ForEachWidget([this](UWidget* Item){ AddInfo(Item->GetName()+TEXT(":")+Item->GetClass()->GetName()); });
  auto* Container=Cast<UPanelWidget>(B->WidgetTree->FindWidget(TEXT("RowContainer")));
  if (!TestNotNull(TEXT("Row container"),Container)||!TestEqual(TEXT("Ten generated rows"),Container->GetChildrenCount(),10)) continue;
  auto* Row=Cast<ULeaderboardRowWidget>(Container->GetChildAt(0));
  auto* Text=Cast<UTextBlock>(Row->WidgetTree->FindWidget(TEXT("NameText")));
  if (TestNotNull(TEXT("Name text"),Text)) { Text->SetRenderTranslation(FVector2D(77,33)); Names.Add(Text); }
  auto* Image=Row->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("AuthoredDecoration"));
  Container->AddChild(Image);
  Image->SetRenderTranslation(FVector2D(91,47)); Image->SetColorAndOpacity(FLinearColor(.2f,.4f,.6f,.8f)); Images.Add(Image);
 }
 for (int32 I=0;I<9;++I)
 {
  const int32 Page=I%3; FLeaderboardStateTestAccess::Show(W,Page);
  TestTrue(TEXT("Correct active state"),Switcher->GetActiveWidget()==(Page==0?Total:Battle));
  for (auto* Text:Names) TestTrue(TEXT("Text movement survives refresh and page changes"),Text->GetRenderTransform().Translation==FVector2D(77,33));
  for (auto* Image:Images) { TestTrue(TEXT("Image movement survives"),Image->GetRenderTransform().Translation==FVector2D(91,47)); TestTrue(TEXT("Image tint survives"),Image->GetColorAndOpacity()==FLinearColor(.2f,.4f,.6f,.8f)); }
 }
 Instance->Shutdown(); Instance->RemoveFromRoot();
 return true;
}
#endif

