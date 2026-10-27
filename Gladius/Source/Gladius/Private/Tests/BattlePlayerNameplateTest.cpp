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
#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"

struct FPlayerNameplateTestAccess
{
 static void RebuildPreview(UBattlePlayerNameplate* UI) { UI->NativePreConstruct(); }
 static void CheckStartupSize(FAutomationTestBase& Test, UBattlePlayerNameplate* UI)
 {
  for (int32 Rebuild=0;Rebuild<5;++Rebuild)
  {
   UI->SetPanelDimensions(FVector2D(600.f+Rebuild*100.f,400.f+Rebuild*100.f));
   UI->NativePreConstruct();
   Test.TestEqual(TEXT("Rebuild keeps the bottom-center scale pivot"), UI->GetRenderTransformPivot(), FVector2D(0.5f,1.f));
   Test.TestEqual(TEXT("Rebuild restores width"),UI->PanelSizeBox->GetWidthOverride(),300.f);
   Test.TestEqual(TEXT("Rebuild restores height"),UI->PanelSizeBox->GetHeightOverride(),200.f);
  }
 }

	static void Check(FAutomationTestBase& Test, UBattlePlayerNameplate* UI)
	{
		auto* DefenseHealthText=Cast<UTextBlock>(UI->WidgetTree->FindWidget(TEXT("Defense_HealthValue")));
		if (!Test.TestNotNull(TEXT("Designer Defense HP text exists"),DefenseHealthText)) return;
		const FVector2D DesignerDefenseHealthPosition=CastChecked<UCanvasPanelSlot>(DefenseHealthText->Slot)->GetPosition();
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
		Test.TestTrue(TEXT("Other actions hide or omit details"), !UI->CritChanceText || UI->CritChanceText->GetVisibility()==ESlateVisibility::Collapsed);
		Test.TestTrue(TEXT("Other actions hide or omit critical icons"), !UI->CritDamageIcon || UI->CritDamageIcon->GetVisibility()==ESlateVisibility::Collapsed);
		Test.TestEqual(TEXT("Saved Designer layout retained"), Cast<UCanvasPanelSlot>(UI->HealthText->Slot)->GetPosition(), DesignerDefenseHealthPosition);
		//행동 페이지마다 아이콘 위치는 디자이너가 따로 정함. 데이터 갱신으로 그 위치가 움직이지 않아야 함
		const FVector2D DefenseIconPosition=Cast<UCanvasPanelSlot>(UI->ActionIcon->Slot)->GetPosition();
		UI->SetDisplayData(Data);
		Test.TestEqual(TEXT("Result icon keeps designer position on refresh"), Cast<UCanvasPanelSlot>(UI->ActionIcon->Slot)->GetPosition(), DefenseIconPosition);
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
		UI->BaseDistanceScale=1.f; UI->MinimumDistanceScale=0.5f; UI->MaximumDistanceScale=1.5f;
		Test.TestEqual(TEXT("Near camera respects maximum scale"), UI->GetDistanceScale(100.f), 1.5f);
		Test.TestEqual(TEXT("Far camera respects minimum scale"), UI->GetDistanceScale(10000.f), 0.5f);
		Test.TestEqual(TEXT("Reference distance remains one"), UI->GetDistanceScale(2400.f), 1.f);
		UI->MaximumDistanceScale=0.25f;
		Test.TestEqual(TEXT("Inverted bounds are safe"), UI->GetDistanceScale(100.f), 0.5f);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerNameplateStatusValuesTest, "Gladius.BattleUI.NumericAttackAndDefenseValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerNameplateStatusValuesTest::RunTest(const FString& Parameters)
{
 TestEqual(TEXT("Nameplate component defaults to bottom center"), GetDefault<ABattleUnit>()->Nameplate->GetPivot(), FVector2D(0.5f,1.f));
 UClass* Class=LoadClass<UBattlePlayerNameplate>(nullptr,TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate.MyBattlePlayerNameplate_C"));
 if (!TestNotNull(TEXT("Nameplate Blueprint loads"),Class)) return false;
 auto* UI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class);
 UI->Initialize(); UI->TakeWidget();
 FBattlePlayerUIData Data;
 Data.BaseDamage=10; Data.BonusDamage=6; Data.Fever=3; Data.FeverAttackDamage=5;
 Data.CritChance=1.f; Data.CritDamage=9.f; Data.DefenseReduction=0.5f;
 for (EBattleAction Action : {EBattleAction::Attack1, EBattleAction::Attack2, EBattleAction::Defense})
 {
  Data.Action=Action;
  Data.BaseDamage=Action==EBattleAction::Attack2 ? 7 : 10;
  UI->SetDisplayData(Data);
  const TCHAR* Name=Action==EBattleAction::Attack1 ? TEXT("Attack1_AttackDamageValue")
   : Action==EBattleAction::Attack2 ? TEXT("Attack2_AttackDamageValue") : TEXT("Defense_DefenseReductionValue");
  auto* Value=Cast<UTextBlock>(UI->WidgetTree->FindWidget(Name));
  if (!TestNotNull(Name,Value)) return false;
  TestEqual(TEXT("Enhanced skill and fever damage are separate numbers, or defense percentage"),Value->GetText().ToString(),
   FString(Action==EBattleAction::Attack1 ? TEXT("16 + 15") : Action==EBattleAction::Attack2 ? TEXT("13 + 15") : TEXT("50%")));
  TestTrue(TEXT("Status number is visible"),Value->GetVisibility()!=ESlateVisibility::Collapsed);
  auto* Slot=Cast<UCanvasPanelSlot>(Value->Slot);
  if (!TestNotNull(TEXT("Value has an editable canvas slot"),Slot)) return false;
  Slot->SetPosition(FVector2D(91,117)); Slot->SetSize(FVector2D(123,39));
  auto Font=Value->GetFont(); Font.Size=23; Value->SetFont(Font);
  UI->SetDisplayData(Data);
  TestEqual(TEXT("Refresh keeps Designer placement"),Slot->GetPosition(),FVector2D(91,117));
  TestEqual(TEXT("Refresh keeps Designer size"),Slot->GetSize(),FVector2D(123,39));
  TestEqual(TEXT("Refresh keeps Designer font"),Value->GetFont().Size,23.f);
 }
 Data.Action=EBattleAction::Attack1; Data.Fever=0; Data.BaseDamage=10;
 UI->SetDisplayData(Data);
 auto* AttackValue=CastChecked<UTextBlock>(UI->WidgetTree->FindWidget(TEXT("Attack1_AttackDamageValue")));
 TestEqual(TEXT("No fever keeps a separate zero fever damage value"),AttackValue->GetText().ToString(),FString(TEXT("16 + 0")));
 auto* DesignerUI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class);
 DesignerUI->SetDesignerFlags(EWidgetDesignFlags::Designing);
 DesignerUI->Initialize(); DesignerUI->TakeWidget();
 auto* Pages=CastChecked<UWidgetSwitcher>(DesignerUI->WidgetTree->FindWidget(TEXT("ActionPages")));
 Pages->SetActiveWidget(DesignerUI->WidgetTree->FindWidget(TEXT("State_Defense")));
 auto* DefenseValue=CastChecked<UTextBlock>(DesignerUI->WidgetTree->FindWidget(TEXT("Defense_DefenseReductionValue")));
 for (float Reduction : {0.5f,0.25f})
 {
  DesignerUI->PreviewData.DefenseReduction=Reduction;
  FPlayerNameplateTestAccess::RebuildPreview(DesignerUI);
  TestEqual(TEXT("Defense reduction is editable in the Designer preview"),DefenseValue->GetText().ToString(),
   FString(Reduction==0.5f ? TEXT("50%") : TEXT("25%")));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerNameplateBaseScaleTest, "Gladius.BattleUI.BaseDistanceScale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerNameplateBaseScaleTest::RunTest(const FString& Parameters)
{
    auto* UI=NewObject<UBattlePlayerNameplate>();
    TestEqual(TEXT("Existing widgets retain original scale by default"),UI->BaseDistanceScale,1.f);
    const float Near=UI->GetDistanceScale(100.f);
    const float Reference=UI->GetDistanceScale(2400.f);
    const float Far=UI->GetDistanceScale(10000.f);
    for (float Base : {0.5f, 2.f})
    {
        UI->BaseDistanceScale=Base;
        TestEqual(TEXT("Reference size follows base multiplier"),UI->GetDistanceScale(2400.f),Reference*Base);
        TestEqual(TEXT("Effective maximum follows base multiplier"),UI->GetDistanceScale(100.f),Near*Base);
        TestEqual(TEXT("Effective minimum follows base multiplier"),UI->GetDistanceScale(10000.f),Far*Base);
    }
    TestEqual(TEXT("Minimum ratio stays editable and unchanged"),UI->MinimumDistanceScale,0.7f);
    TestEqual(TEXT("Maximum ratio stays editable and unchanged"),UI->MaximumDistanceScale,1.2f);
    UI->BaseDistanceScale=0.f;
    TestEqual(TEXT("Invalid zero base is clamped safely"),UI->GetDistanceScale(2400.f),0.01f);
    return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerNameplateStateIconsTest, "Gladius.BattleUI.PerStateActionIcons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerNameplateStateIconsTest::RunTest(const FString& Parameters)
{
 UClass* Class=LoadClass<UBattlePlayerNameplate>(nullptr,TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate.MyBattlePlayerNameplate_C"));
 if (!TestNotNull(TEXT("Nameplate Blueprint loads"),Class)) return false;
 auto* UI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class); UI->Initialize(); UI->TakeWidget();
 auto* Gladius=UTexture2D::CreateTransient(1,1); auto* Sera=UTexture2D::CreateTransient(1,1);
 const TCHAR* States[]={TEXT("Idle"),TEXT("Attack1"),TEXT("Attack2"),TEXT("AvoidL"),TEXT("AvoidM"),TEXT("AvoidR"),TEXT("Defense")};
 FBattlePlayerUIData Data; Data.HP=5; Data.MaxHP=5;
 for(int32 Index=0;Index<7;++Index)
 {
  const auto Action=static_cast<EBattleAction>(Index);
  const FString Prefix=States[Index];
  auto* Icon=Cast<UImage>(UI->WidgetTree->FindWidget(FName(*(Prefix+TEXT("_ActionIcon")))));
  if (!TestNotNull(*Prefix,Icon)) return false;
  TestEqual(TEXT("Icon belongs to its state"),Icon->GetParent()->GetFName(),FName(*(TEXT("State_")+Prefix)));
  auto* Slot=Cast<UCanvasPanelSlot>(Icon->Slot);
  const FVector2D Position(10.f+Index*11.f,20.f+Index*9.f); Slot->SetPosition(Position);
  UI->GladiusActionIconTextures.Add(Action,Gladius); UI->SeraActionIconTextures.Add(Action,Sera);
  Data.Action=Action; Data.CharacterId=TEXT("gladius"); UI->SetDisplayData(Data);
  TestTrue(TEXT("Gladius image applied to selected state"),Icon->GetBrush().GetResourceObject()==Gladius);
  Data.CharacterId=TEXT("sera"); UI->SetDisplayData(Data);
  TestTrue(TEXT("Sera image applied to selected state"),Icon->GetBrush().GetResourceObject()==Sera);
  TestEqual(TEXT("Refresh preserves state placement"),Slot->GetPosition(),Position);
 }
 for(int32 Index=0;Index<7;++Index)
 {
  auto* Icon=Cast<UImage>(UI->WidgetTree->FindWidget(FName(*(FString(States[Index])+TEXT("_ActionIcon")))));
  TestEqual(TEXT("Other state changes preserve independent placement"),Cast<UCanvasPanelSlot>(Icon->Slot)->GetPosition(),FVector2D(10.f+Index*11.f,20.f+Index*9.f));
 }
 TestNull(TEXT("No shared root icon remains"),UI->WidgetTree->FindWidget(TEXT("SharedActionIcon")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerNameplateDesignerStateTest, "Gladius.BattleUI.DesignerStateAndPlacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerNameplateDesignerStateTest::RunTest(const FString& Parameters)
{
 UClass* Class=LoadClass<UBattlePlayerNameplate>(nullptr,TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate.MyBattlePlayerNameplate_C"));
 if (!TestNotNull(TEXT("Nameplate Blueprint loads"),Class)) return false;
 auto* UI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class);
 UI->SetDesignerFlags(EWidgetDesignFlags::Designing);
 UI->Initialize(); UI->TakeWidget();
 auto* Pages=Cast<UWidgetSwitcher>(UI->WidgetTree->FindWidget(TEXT("ActionPages")));
 auto* Page=UI->WidgetTree->FindWidget(TEXT("State_Attack1"));
 auto* Icon=UI->WidgetTree->FindWidget(TEXT("Attack1_ActionIcon"));
 auto* Size=Cast<USizeBox>(UI->WidgetTree->FindWidget(TEXT("PlateSize")));
 if (!TestNotNull(TEXT("State switcher exists"),Pages) || !TestNotNull(TEXT("Attack1 exists"),Page)
  || !TestNotNull(TEXT("Attack1 icon exists"),Icon) || !TestNotNull(TEXT("Panel size exists"),Size)) return false;
 auto* Slot=Cast<UCanvasPanelSlot>(Icon->Slot);
 if (!TestNotNull(TEXT("Attack1 icon has a Designer slot"),Slot)) return false;
 UI->PreviewData.Action=EBattleAction::Idle;
 Pages->SetActiveWidget(Page);
 auto* OriginalTexture=UTexture2D::CreateTransient(1,1);
 auto* DesignerTexture=UTexture2D::CreateTransient(1,1);
 UI->PreviewData.CharacterId=TEXT("gladius");
 UI->HealthIconTexture=OriginalTexture; UI->FeverIconTexture=OriginalTexture;
 UI->HealthFilledTexture=OriginalTexture; UI->HealthEmptyTexture=OriginalTexture;
 UI->CritChanceIconTexture=OriginalTexture; UI->CritDamageIconTexture=OriginalTexture;
 UI->GladiusActionIconTextures.Add(EBattleAction::Attack1,OriginalTexture);
 TArray<UWidget*> Widgets; UI->WidgetTree->GetAllWidgets(Widgets);
 TArray<UImage*> DesignerIcons;
 const FLinearColor DesignerTint(0.3f,0.6f,0.9f,0.8f);
 for (UWidget* Widget : Widgets)
  if (Widget->GetName().StartsWith(TEXT("Attack1_")))
   if (UImage* Image=Cast<UImage>(Widget))
   {
    Image->SetBrushFromTexture(DesignerTexture,false);
    Image->SetColorAndOpacity(DesignerTint);
    DesignerIcons.Add(Image);
   }
 TestTrue(TEXT("Attack1 has editable icon images"),DesignerIcons.Num()>0);
 Slot->SetPosition(FVector2D(73,91)); Slot->SetSize(FVector2D(47,53));
 Size->SetWidthOverride(410); Size->SetHeightOverride(260);
 for (int32 Rebuild=0; Rebuild<3; ++Rebuild)
 {
  FPlayerNameplateTestAccess::RebuildPreview(UI);
  UI->RefreshAppearance();
  for (UImage* Image : DesignerIcons)
  {
   TestTrue(*FString::Printf(TEXT("Designer keeps edited asset: %s"),*Image->GetName()),Image->GetBrush().GetResourceObject()==DesignerTexture);
   TestEqual(TEXT("Designer keeps edited image tint"),Image->GetColorAndOpacity(),DesignerTint);
  }
  TestEqual(TEXT("Designer keeps Attack1 selected"),Pages->GetActiveWidget(),Page);
  TestTrue(TEXT("Preview data matches the edited state"),UI->Data.Action==EBattleAction::Attack1);
  TestEqual(TEXT("Designer preserves the moved Attack1 icon"),Slot->GetPosition(),FVector2D(73,91));
  TestEqual(TEXT("Designer preserves the resized Attack1 icon"),Slot->GetSize(),FVector2D(47,53));
  TestEqual(TEXT("Designer preserves panel width"),Size->GetWidthOverride(),410.f);
  TestEqual(TEXT("Designer preserves panel height"),Size->GetHeightOverride(),260.f);
 }
 auto* RuntimeUI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class);
 RuntimeUI->Initialize(); RuntimeUI->TakeWidget();
 FBattlePlayerUIData Data; Data.Action=EBattleAction::Attack2; RuntimeUI->SetDisplayData(Data);
 auto* RuntimePages=Cast<UWidgetSwitcher>(RuntimeUI->WidgetTree->FindWidget(TEXT("ActionPages")));
 TestEqual(TEXT("Runtime still selects the actual action"),RuntimePages->GetActiveWidget()->GetFName(),FName(TEXT("State_Attack2")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerNameplateStartupSizeTest, "Gladius.BattleUI.FixedStartupSize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerNameplateStartupSizeTest::RunTest(const FString& Parameters)
{
 UClass* Class=LoadClass<UBattlePlayerNameplate>(nullptr,TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate.MyBattlePlayerNameplate_C"));
 if (!TestNotNull(TEXT("Nameplate Blueprint loads"),Class)) return false;
 for (int32 Instance=0;Instance<3;++Instance)
 {
  auto* UI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class); UI->Initialize(); UI->TakeWidget();
  auto* Size=Cast<USizeBox>(UI->WidgetTree->FindWidget(TEXT("PlateSize")));
  if (!TestNotNull(TEXT("Plate size box exists"),Size)) return false;
  TestEqual(TEXT("New widget starts at width 300"),Size->GetWidthOverride(),300.f);
  TestEqual(TEXT("New widget starts at height 200"),Size->GetHeightOverride(),200.f);
  FPlayerNameplateTestAccess::CheckStartupSize(*this,UI);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameplateHealthMatrixTest,"Gladius.BattleUI.AllStatesFiveSixHealthAndNameBounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNameplateHealthMatrixTest::RunTest(const FString& Parameters)
{
 auto* Class=LoadClass<UBattlePlayerNameplate>(nullptr,TEXT("/Game/Blueprints/UI/MyBattlePlayerNameplate.MyBattlePlayerNameplate_C"));
 if(!TestNotNull(TEXT("Actual nameplate loads"),Class))return false;
 auto* UI=NewObject<UBattlePlayerNameplate>(GetTransientPackage(),Class);UI->Initialize();UI->TakeWidget();
 UI->HealthFilledTexture=UTexture2D::CreateTransient(1,1);UI->HealthEmptyTexture=UTexture2D::CreateTransient(1,1);
 FBattlePlayerUIData Data;Data.DisplayName=TEXT("패널 밖으로 벗어나면 안 되는 매우 긴 사용자 이름 WWWWWWWWWWWWWW");
 const TCHAR* States[]={TEXT("Idle"),TEXT("Attack1"),TEXT("Attack2"),TEXT("AvoidL"),TEXT("AvoidM"),TEXT("AvoidR"),TEXT("Defense")};
 for(int32 Action=0;Action<7;++Action)
 {
  Data.Action=static_cast<EBattleAction>(Action);const FString Prefix=States[Action];
  for(int32 Maximum:{6,5,6})for(int32 Health:{0,Maximum-1,Maximum})
  {
   Data.MaxHP=Maximum;Data.HP=Health;UI->SetDisplayData(Data);
   int32 Visible=0;
   for(int32 I=1;I<=6;++I)
   {
    auto* Image=Cast<UImage>(UI->WidgetTree->FindWidget(FName(*(Prefix+FString::Printf(TEXT("_HealthSegment%d"),I)))));
    if(!TestNotNull(TEXT("All six authored segment widgets exist"),Image))return false;
    const bool Shown=Image->GetVisibility()!=ESlateVisibility::Collapsed;
    TestEqual(TEXT("Exactly maximum HP number of segments is visible"),Shown,I<=Maximum);Visible+=Shown?1:0;
    if(Shown)
    {
     const auto* Slot=CastChecked<UCanvasPanelSlot>(Image->Slot);const FVector2D Pos=Slot->GetPosition(),Size=Slot->GetSize();
     TestTrue(TEXT("Health segment remains inside 300x200 panel"),Pos.X>=0 && Pos.Y>=0 && Pos.X+Size.X<=300 && Pos.Y+Size.Y<=200);
     TestTrue(TEXT("Segment fill follows current HP"),Image->GetBrush().GetResourceObject()==(I<=Health?UI->HealthFilledTexture.Get():UI->HealthEmptyTexture.Get()));
    }
   }
   TestEqual(TEXT("Visible bar count matches five or six"),Visible,Maximum);
  }
  auto* Name=CastChecked<UTextBlock>(UI->WidgetTree->FindWidget(FName(*(Prefix+TEXT("_PlayerName")))));
  TestEqual(TEXT("Name font is two thirds of original 33.75"),Name->GetFont().Size,22.5f);
  TestEqual(TEXT("Long names use ellipsis"),Name->GetTextOverflowPolicy(),ETextOverflowPolicy::Ellipsis);
  TestEqual(TEXT("Names cannot draw outside their box"),Name->GetClipping(),EWidgetClipping::ClipToBoundsAlways);
  const auto* Slot=CastChecked<UCanvasPanelSlot>(Name->Slot);const FVector2D Pos=Slot->GetPosition(),Size=Slot->GetSize();
  TestTrue(TEXT("Name box remains inside panel"),Pos.X>=0 && Pos.Y>=0 && Pos.X+Size.X<=300 && Pos.Y+Size.Y<=200);
 }
 return true;
}

#endif
