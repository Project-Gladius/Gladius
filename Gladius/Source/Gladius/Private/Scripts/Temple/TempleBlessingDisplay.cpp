#include "Scripts/Temple/TempleBlessingDisplay.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Cinematics/LevelTransitionCinematics.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

FText UTempleBlessingCardWidget::BlessingTitle(int32 Tier)
{
 static const TCHAR* Titles[] = { TEXT("회복의 축복"), TEXT("힘의 축복"), TEXT("민첩의 축복"), TEXT("예리함의 축복") };
 return FText::FromString(Titles[FMath::Clamp(Tier,1,4)-1]);
}

FText UTempleBlessingCardWidget::BlessingDescription(int32 Tier, const ABattleManager* Manager)
{
 const ABattleManager* Values = Manager ? Manager : GetDefault<ABattleManager>();
 const auto Number = [](double Value) { FNumberFormattingOptions Options; Options.SetMaximumFractionalDigits(2); return FText::AsNumber(Value,&Options); };
 switch (Tier)
 {
 case 1: return FText::Format(NSLOCTEXT("TempleBlessing", "Recovery", "체력을 {0}만큼\n회복합니다."), Number(Values->BlessingHealAmount));
 case 2: return FText::Format(NSLOCTEXT("TempleBlessing", "Strength", "기본 공격력이 {0}만큼\n증가합니다."), Number(Values->DamagePerBonus));
 case 3: return FText::Format(NSLOCTEXT("TempleBlessing", "Agility", "치명타 확률이 {0}%p\n증가합니다."), Number(Values->CritChancePerBonus * 100.0));
 default: return FText::Format(NSLOCTEXT("TempleBlessing", "Sharpness", "치명타 데미지가 {0}%p\n증가합니다."), Number(Values->CritDamagePerBonus * 100.0));
 }
}

void UTempleBlessingCardWidget::BuildLayout(UWidgetTree* Tree)
{
 if (!Tree) return;
 if (Tree->RootWidget)
 {
  if (auto* ExistingCanvas=Cast<UCanvasPanel>(Tree->RootWidget))
  {
   auto* Size=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("CardDesignSize"));
   Size->SetWidthOverride(1000); Size->SetHeightOverride(529); Size->SetContent(ExistingCanvas);
   auto* Scale=Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("CardScale"));
   Scale->SetStretch(EStretch::ScaleToFit); Scale->SetContent(Size); Tree->RootWidget=Scale;
  }
  return;
 }
 UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BlessingCard"));
 Tree->RootWidget = Root;
 UImage* Background = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CardBackground"));
 UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Temple/T_TempleBlessingPanel.T_TempleBlessingPanel"));
 Background->SetBrushFromTexture(Texture);
 FSlateBrush Brush = Background->GetBrush();
 // Keep the original imported PNG. Exclude its transparent outer margin in the brush UVs.
 Brush.SetUVRegion(FBox2f(FVector2f(300.f/3351.f,300.f/2055.f),FVector2f(3050.f/3351.f,1754.f/2055.f)));
 Brush.ImageSize = FVector2D(1000.f,529.f);
 Background->SetBrush(Brush);
 auto* BackgroundSlot = Root->AddChildToCanvas(Background);
 BackgroundSlot->SetAnchors(FAnchors(0,0,1,1)); BackgroundSlot->SetOffsets(FMargin(0));
 const auto Text = [Tree,Root](const TCHAR* Name, FVector2D Position, FVector2D Size, const TCHAR* FontPath, int32 FontSize)
 {
  UTextBlock* Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  Widget->SetFont(FSlateFontInfo(LoadObject<UObject>(nullptr,FontPath),FontSize));
  Widget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
  Widget->SetJustification(ETextJustify::Center);
  Widget->SetAutoWrapText(true);
  auto* Slot = Root->AddChildToCanvas(Widget); Slot->SetPosition(Position); Slot->SetSize(Size);
  return Widget;
 };
 auto* Title = Text(TEXT("BlessingTitle"), FVector2D(80,82), FVector2D(840,96), TEXT("/Game/UI/Fonts/MaruBuri/MaruBuri_Bold_Font.MaruBuri_Bold_Font"),48);
 auto* Description = Text(TEXT("BlessingDescription"), FVector2D(72,247), FVector2D(856,220), TEXT("/Game/UI/Fonts/MaruBuri/MaruBuri_Regular_Font.MaruBuri_Regular_Font"),38);
 Title->SetText(BlessingTitle(1)); Description->SetText(BlessingDescription(1,nullptr));
 BuildLayout(Tree);
}

void UTempleBlessingCardWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 BuildLayout(WidgetTree);
 RefreshText();
}
void UTempleBlessingCardWidget::NativePreConstruct()
{
 Super::NativePreConstruct();
 RefreshText();
}
void UTempleBlessingCardWidget::ShowBlessing(int32 Tier, ABattleManager* Manager)
{
 BlessingTier=FMath::Clamp(Tier,1,4); BattleManager=Manager; RefreshText();
}
void UTempleBlessingCardWidget::RefreshText()
{
 if (!WidgetTree) return;
 if (auto* Title=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("BlessingTitle")))) { Title->SetText(BlessingTitle(BlessingTier)); Title->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }
 if (auto* Description=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("BlessingDescription")))) { Description->SetText(BlessingDescription(BlessingTier,BattleManager.Get())); Description->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }
}

bool UTempleBlessingCardWidget::BuildDesignerLayout(UObject* WidgetBlueprintAsset)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
 if (!BP || !BP->WidgetTree) return false;
 BuildLayout(BP->WidgetTree);
 BP->WidgetTree->ForEachWidget([BP](UWidget* Widget){ BP->OnVariableAdded(Widget->GetFName()); });
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 BP->MarkPackageDirty();
 return BP->Status != BS_Error;
#else
 return false;
#endif
}

ATempleBlessingDisplay::ATempleBlessingDisplay()
{
 PrimaryActorTick.bCanEverTick=false;
 Card=CreateDefaultSubobject<UWidgetComponent>(TEXT("BlessingCard"));
 SetRootComponent(Card);
 CardClass=UTempleBlessingCardWidget::StaticClass();
 Card->SetWidgetClass(CardClass);
 Card->SetWidgetSpace(EWidgetSpace::Screen);
 Card->SetDrawSize(FVector2D(343,182));
 Card->SetPivot(FVector2D(0.5f,0.f));
 Card->SetTwoSided(true);
 Card->SetTickWhenOffscreen(true);
 Card->SetBlendMode(EWidgetBlendMode::Transparent);
 Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Card->SetGenerateOverlapEvents(false);
 Card->SetRelativeScale3D(FVector(0.62f));
}
void ATempleBlessingDisplay::PostRegisterAllComponents()
{
 Super::PostRegisterAllComponents(); RefreshCard();
}
void ATempleBlessingDisplay::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform); RefreshCard();
}
void ATempleBlessingDisplay::BeginPlay()
{
 Super::BeginPlay();
 Card->SetVisibility(false, true);
 auto* Cinematics = GetWorld()->GetSubsystem<ULevelTransitionCinematics>();
 if (Cinematics->HasIntroFinished()) ShowAfterIntro();
 else Cinematics->OnIntroFinished.AddUObject(this, &ATempleBlessingDisplay::ShowAfterIntro);
 // The controller creates/loads the battle data during BeginPlay.
 GetWorldTimerManager().SetTimerForNextTick(this,&ATempleBlessingDisplay::RefreshCard);
}
void ATempleBlessingDisplay::RefreshCard()
{
 if (!Card) return;
 if (Card->GetWidgetClass()!=CardClass) Card->SetWidgetClass(CardClass);
 Card->InitWidget();
 if (auto* Widget=Cast<UTempleBlessingCardWidget>(Card->GetUserWidgetObject()))
  Widget->ShowBlessing(BlessingTier,Cast<ABattleManager>(UGameplayStatics::GetActorOfClass(this,ABattleManager::StaticClass())));
}



void ATempleBlessingDisplay::DrawEditorPreview()
{
#if WITH_EDITOR
 RefreshCard();
 if (Card && GetWorld() && !GetWorld()->IsGameWorld())
 {
  Card->TickComponent(0.f, LEVELTICK_All, nullptr);
  GetWorld()->SendAllEndOfFrameUpdates();
 }
#endif
}



void ATempleBlessingDisplay::ShowAfterIntro()
{
 if (Card) Card->SetVisibility(true, true);
 RefreshCard();
}
