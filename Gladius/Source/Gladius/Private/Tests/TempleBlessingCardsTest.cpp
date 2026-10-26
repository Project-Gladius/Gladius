#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Scripts/Temple/TempleBlessingDisplay.h"
#include "Scripts/Battle/BattleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempleBlessingCardsTest,"Gladius.Temple.UI.BlessingCards",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTempleBlessingCardsTest::RunTest(const FString& Parameters)
{
 auto* Instance=NewObject<UGameInstance>(GEngine); Instance->AddToRoot(); Instance->InitializeStandalone();
 UWorld* World=Instance->GetWorld();
 UClass* WidgetClass=LoadClass<UTempleBlessingCardWidget>(nullptr,TEXT("/Game/Blueprints/UI/WBP_TempleBlessingCard.WBP_TempleBlessingCard_C"));
 if (!TestNotNull(TEXT("Designer card asset"),WidgetClass)) return false;
 auto* Card=CreateWidget<UTempleBlessingCardWidget>(World,WidgetClass);
 auto* Manager=World->SpawnActor<ABattleManager>();
 Manager->BlessingHealAmount=3; Manager->DamagePerBonus=2; Manager->CritChancePerBonus=.07f; Manager->CritDamagePerBonus=.15f;
 auto* Title=Cast<UTextBlock>(Card->WidgetTree->FindWidget(TEXT("BlessingTitle")));
 auto* Description=Cast<UTextBlock>(Card->WidgetTree->FindWidget(TEXT("BlessingDescription")));
 auto* Background=Cast<UImage>(Card->WidgetTree->FindWidget(TEXT("CardBackground")));
 TestNotNull(TEXT("Imported original background"),Background ? Background->GetBrush().GetResourceObject() : nullptr);
 if (TestNotNull(TEXT("Title"),Title) && TestNotNull(TEXT("Description"),Description))
 {
  TestEqual(TEXT("MaruBuri Bold title"),Title->GetFont().FontObject->GetName(),FString(TEXT("MaruBuri_Bold_Font")));
  TestEqual(TEXT("MaruBuri Regular description"),Description->GetFont().FontObject->GetName(),FString(TEXT("MaruBuri_Regular_Font")));
  TestTrue(TEXT("Title is above divider"),Cast<UCanvasPanelSlot>(Title->Slot)->GetPosition().Y+Cast<UCanvasPanelSlot>(Title->Slot)->GetSize().Y<200.f);
  TestTrue(TEXT("Body is below divider"),Cast<UCanvasPanelSlot>(Description->Slot)->GetPosition().Y>200.f);
  const TCHAR* Expected[]={TEXT("3"),TEXT("2"),TEXT("7%p"),TEXT("15%p")};
  FString PreviewDir; FParse::Value(FCommandLine::Get(),TEXT("BlessingPreviewDir="),PreviewDir);
  for(int32 Tier=1;Tier<=4;++Tier)
  {
   Card->ShowBlessing(Tier,Manager);
   TestEqual(TEXT("Matching blessing title"),Title->GetText().ToString(),UTempleBlessingCardWidget::BlessingTitle(Tier).ToString());
   TestTrue(TEXT("Description uses actual game values"),Description->GetText().ToString().Contains(Expected[Tier-1]));
   if(!PreviewDir.IsEmpty())
   {
    Card->ShowBlessing(Tier,nullptr);
    FWidgetRenderer Renderer(true,false);
    UTextureRenderTarget2D* Target=Renderer.DrawWidget(Card->TakeWidget(),FVector2D(1000,529));
    FlushRenderingCommands();
    FBufferArchive Png;
    TestTrue(TEXT("Preview exported"),FImageUtils::ExportRenderTarget2DAsPNG(Target,Png));
    FFileHelper::SaveArrayToFile(Png,*(PreviewDir/FString::Printf(TEXT("blessing-%d.png"),Tier)));
   }
  }
 }
 Manager->Destroy(); Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
 return true;
}
#endif

