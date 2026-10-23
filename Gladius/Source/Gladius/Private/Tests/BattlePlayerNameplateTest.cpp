#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scripts/Battle/BattlePlayerNameplate.h"
#include "Components/Image.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"

struct FPlayerNameplateTestAccess
{
	static void Check(FAutomationTestBase& Test, UBattlePlayerNameplate* UI)
	{
		UI->GladiusActionIconTextures.Reset();
		UI->SeraActionIconTextures.Reset();
		UI->ActionIconTextures.Reset();
		UI->HealthFilledTexture = UTexture2D::CreateTransient(1, 1);
		UI->HealthEmptyTexture = UTexture2D::CreateTransient(1, 1);
		UI->GladiusActionIconTextures.Add(EBattleAction::Attack2, UI->HealthFilledTexture);
		UI->SeraActionIconTextures.Add(EBattleAction::Attack2, UI->HealthEmptyTexture);
		FBattlePlayerUIData Data;
		Data.CharacterId=TEXT("gladius"); Data.MaxHP=5; Data.HP=3; Data.Fever=8; Data.Action=EBattleAction::Attack2;
		Data.CritChance=0.25f; Data.CritDamage=0.5f;
		UI->SetDisplayData(Data);
		if (!Test.TestNotNull(TEXT("Designer HP text exists"), UI->HealthText.Get())) return;
		Cast<UCanvasPanelSlot>(UI->HealthText->Slot)->SetPosition(FVector2D(90, 10));
		Cast<UCanvasPanelSlot>(UI->HealthSegments[4]->Slot)->SetPosition(FVector2D(20, 80));
		UI->SetDisplayData(Data);
		Test.TestEqual(TEXT("HP format"), UI->HealthText->GetText().ToString(), FString(TEXT("HP 3/5")));
		Test.TestEqual(TEXT("Fever text contains only number"), UI->FeverText->GetText().ToString(), FString(TEXT("8")));
		Test.TestEqual(TEXT("Six fixed icon slots"), UI->HealthSegments.Num(), 6);
		Test.TestEqual(TEXT("Sixth slot hidden at max five"), UI->HealthSegments[5]->GetVisibility(), ESlateVisibility::Collapsed);
		Test.TestTrue(TEXT("Third HP icon is full"), UI->HealthSegments[2]->GetBrush().GetResourceObject()==UI->HealthFilledTexture);
		Test.TestTrue(TEXT("Fourth HP icon is empty"), UI->HealthSegments[3]->GetBrush().GetResourceObject()==UI->HealthEmptyTexture);
		Test.TestEqual(TEXT("Data refresh preserves dragged HP text position"), Cast<UCanvasPanelSlot>(UI->HealthText->Slot)->GetPosition(), FVector2D(90, 10));
		Test.TestEqual(TEXT("Each HP slot has independent position"), Cast<UCanvasPanelSlot>(UI->HealthSegments[4]->Slot)->GetPosition(), FVector2D(20, 80));
		Test.TestEqual(TEXT("Critical chance format"), UI->CritChanceText->GetText().ToString(), FString(TEXT("+25%")));
		Test.TestEqual(TEXT("Critical damage format"), UI->CritDamageText->GetText().ToString(), FString(TEXT("+50%")));
		Test.TestTrue(TEXT("Attack2 shows detail"), UI->CritChanceText->GetVisibility()!=ESlateVisibility::Collapsed);
		Test.TestTrue(TEXT("Action texture selected"), UI->ActionIcon->GetBrush().GetResourceObject()==UI->HealthFilledTexture);
		const FVector2D IconPosition=Cast<UCanvasPanelSlot>(UI->ActionIcon->Slot)->GetPosition();
		Data.CharacterId=TEXT("Sera"); UI->SetDisplayData(Data);
		Test.TestTrue(TEXT("Same action uses Sera icon"), UI->ActionIcon->GetBrush().GetResourceObject()==UI->HealthEmptyTexture);
		Test.TestEqual(TEXT("Class switch keeps icon position"), Cast<UCanvasPanelSlot>(UI->ActionIcon->Slot)->GetPosition(), IconPosition);
		Data.CharacterId=TEXT("Gladius"); UI->SetDisplayData(Data);
		Test.TestTrue(TEXT("Class switch restores Gladius icon"), UI->ActionIcon->GetBrush().GetResourceObject()==UI->HealthFilledTexture);
		Data.CharacterId=TEXT("unknown"); UI->SetDisplayData(Data);
		Test.TestNull(TEXT("Unknown class does not retain previous class icon"), UI->ActionIcon->GetBrush().GetResourceObject());
		UI->ActionIconTextures.Add(EBattleAction::Attack2, UI->HealthEmptyTexture);
		UI->SetDisplayData(Data);
		Test.TestTrue(TEXT("Legacy common assignments retained"), UI->ActionIcon->GetBrush().GetResourceObject()==UI->HealthEmptyTexture);
		UI->SeraActionIconTextures.Add(EBattleAction::Attack2, nullptr);
		Data.CharacterId=TEXT("sera"); UI->SetDisplayData(Data);
		Test.TestNull(TEXT("Explicit empty class icon overrides legacy fallback"), UI->ActionIcon->GetBrush().GetResourceObject());
		Data.CharacterId=TEXT("gladius");
		Data.Action=EBattleAction::Defense; Data.MaxHP=6; Data.HP=6;
		UI->SetDisplayData(Data);
		Test.TestEqual(TEXT("Sixth slot visible at max six"), UI->HealthSegments[5]->GetVisibility(), ESlateVisibility::HitTestInvisible);
		Test.TestTrue(TEXT("Sixth HP full at six"), UI->HealthSegments[5]->GetBrush().GetResourceObject()==UI->HealthFilledTexture);
		Test.TestEqual(TEXT("Other actions hide details"), UI->CritChanceText->GetVisibility(), ESlateVisibility::Collapsed);
		Test.TestEqual(TEXT("Other actions hide critical icons"), UI->CritDamageIcon->GetVisibility(), ESlateVisibility::Collapsed);
		Test.TestEqual(TEXT("Migrated default layout retained"), Cast<UCanvasPanelSlot>(UI->HealthText->Slot)->GetPosition(), UI->DefaultLayout.HealthText.Position);
		Test.TestEqual(TEXT("Result icon position is invariant"), Cast<UCanvasPanelSlot>(UI->ActionIcon->Slot)->GetPosition(), IconPosition);
		Test.TestNull(TEXT("No stale result texture"), UI->ActionIcon->GetBrush().GetResourceObject());
		Data.HP=100; Data.MaxHP=100; UI->SetDisplayData(Data);
		Test.TestEqual(TEXT("Invalid legacy HP is clamped"), UI->HealthText->GetText().ToString(), FString(TEXT("HP 6/6")));
		Data.HP=0; UI->SetDisplayData(Data);
		Test.TestTrue(TEXT("Zero HP shows empty first slot"), UI->HealthSegments[0]->GetBrush().GetResourceObject()==UI->HealthEmptyTexture);
		Test.TestEqual(TEXT("Default panel width"), UI->PanelSizeBox->GetWidthOverride(), 300.f);
		Test.TestEqual(TEXT("Default panel height"), UI->PanelSizeBox->GetHeightOverride(), static_cast<float>(UI->PanelSize.Y));
		UI->SetPanelDimensions(FVector2D(100, 50));
		Test.TestEqual(TEXT("Panel width has no minimum restriction"), UI->PanelSizeBox->GetWidthOverride(), 100.f);
		Test.TestEqual(TEXT("Panel height has no minimum restriction"), UI->PanelSizeBox->GetHeightOverride(), 50.f);
		UI->MinimumDistanceScale=0.5f; UI->MaximumDistanceScale=1.5f;
		Test.TestEqual(TEXT("Near camera respects maximum scale"), UI->GetDistanceScale(100.f), 1.5f);
		Test.TestEqual(TEXT("Far camera respects minimum scale"), UI->GetDistanceScale(10000.f), 0.5f);
		Test.TestEqual(TEXT("Reference distance remains one"), UI->GetDistanceScale(2400.f), 1.f);
		UI->MaximumDistanceScale=0.25f;
		Test.TestEqual(TEXT("Inverted bounds are safe"), UI->GetDistanceScale(100.f), 0.5f);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerNameplateLayoutTest, "Gladius.BattleUI.LayoutAndHealthIcons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerNameplateLayoutTest::RunTest(const FString& Parameters)
{
	UBattlePlayerNameplate* Native=NewObject<UBattlePlayerNameplate>();
	Native->Initialize(); Native->TakeWidget();
	TestNull(TEXT("C++ does not generate a widget tree"), Native->WidgetTree->RootWidget.Get());
	Native->SetDisplayData(FBattlePlayerUIData()); // Missing designer children must be safe.
	UClass* Class=LoadClass<UBattlePlayerNameplate>(nullptr, TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate.MyBattlePlayerNameplate_C"));
	if (!TestNotNull(TEXT("Designer Blueprint loads"), Class)) return false;
	TestTrue(TEXT("Units default to the Designer Blueprint"), GetDefault<ABattleUnit>()->PlayerUIClass.Get() == Class);
	UBattlePlayerNameplate* UI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(), Class);
	UI->Initialize();
	UI->TakeWidget();
	auto Pages=Cast<UWidgetSwitcher>(UI->WidgetTree->FindWidget(TEXT("ActionPages")));
	if (!TestNotNull(TEXT("Persistent Designer action switcher"), Pages)) return false;
	TestEqual(TEXT("Seven editable state pages"), Pages->GetChildrenCount(), 7);
	FPlayerNameplateTestAccess::Check(*this, UI);
	TestEqual(TEXT("Defense selects its Designer page"), Pages->GetActiveWidget()->GetFName(), FName(TEXT("State_Defense")));
	return true;
}
#endif
