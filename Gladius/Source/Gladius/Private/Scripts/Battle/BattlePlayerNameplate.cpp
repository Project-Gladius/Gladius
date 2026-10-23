#include "Scripts/Battle/BattlePlayerNameplate.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Components/WidgetSwitcher.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

namespace
{
	FString ActionPrefix(EBattleAction Action)
	{
		switch (Action)
		{
		case EBattleAction::Attack1: return TEXT("Attack1");
		case EBattleAction::Attack2: return TEXT("Attack2");
		case EBattleAction::AvoidL: return TEXT("AvoidL");
		case EBattleAction::AvoidM: return TEXT("AvoidM");
		case EBattleAction::AvoidR: return TEXT("AvoidR");
		case EBattleAction::Defense: return TEXT("Defense");
		default: return TEXT("Idle");
		}
	}
	void SetImage(UImage* Image, UTexture2D* Texture, FLinearColor Fallback = FLinearColor::Transparent)
	{
		if (!Image) return;
		Image->SetBrushFromTexture(Texture, false);
		Image->SetColorAndOpacity(Texture ? FLinearColor::White : Fallback);
	}
}

void UBattlePlayerNameplate::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RefreshAppearance();
}

void UBattlePlayerNameplate::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime()) Data = PreviewData;
	RefreshAppearance();
}

void UBattlePlayerNameplate::ResolveDesignerWidgets()
{
	if (!WidgetTree) return;
	PanelSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("PlateSize")));
	Panel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("PlatePanel")));
	const FString Prefix = ActionPrefix(Data.Action);
	if (UWidgetSwitcher* Pages = Cast<UWidgetSwitcher>(WidgetTree->FindWidget(TEXT("ActionPages"))))
		if (UWidget* Page = WidgetTree->FindWidget(FName(*(TEXT("State_") + Prefix)))) Pages->SetActiveWidget(Page);
	auto Find = [&](const TCHAR* Name) { return WidgetTree->FindWidget(FName(*(Prefix + TEXT("_") + Name))); };
	NameText = Cast<UTextBlock>(Find(TEXT("PlayerName")));
	HealthIcon = Cast<UImage>(Find(TEXT("HealthIcon")));
	HealthText = Cast<UTextBlock>(Find(TEXT("HealthValue")));
	FeverIcon = Cast<UImage>(Find(TEXT("FeverIcon")));
	FeverText = Cast<UTextBlock>(Find(TEXT("FeverValue")));
	ActionText = Cast<UTextBlock>(Find(TEXT("ActionValue")));
	CritChanceIcon = Cast<UImage>(Find(TEXT("CritChanceIcon")));
	CritChanceText = Cast<UTextBlock>(Find(TEXT("CritChanceValue")));
	CritDamageIcon = Cast<UImage>(Find(TEXT("CritDamageIcon")));
	CritDamageText = Cast<UTextBlock>(Find(TEXT("CritDamageValue")));
	ActionIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("SharedActionIcon")));
	HealthSegments.Reset();
	for (int32 Index=1; Index<=6; ++Index)
		HealthSegments.Add(Cast<UImage>(Find(*FString::Printf(TEXT("HealthSegment%d"), Index))));
}

void UBattlePlayerNameplate::SetDisplayData(const FBattlePlayerUIData& InData)
{
	Data = InData;
	Data.MaxHP = FMath::Clamp(Data.MaxHP, 5, 6);
	Data.HP = FMath::Clamp(Data.HP, 0, Data.MaxHP);
	RefreshAppearance();
	OnDisplayDataChanged(Data);
}

void UBattlePlayerNameplate::SetPanelDimensions(FVector2D InSize)
{
	PanelSize = InSize;
	ResolveDesignerWidgets();
	if (PanelSizeBox)
	{
		PanelSizeBox->SetWidthOverride(FMath::Max(0.f, InSize.X));
		PanelSizeBox->SetHeightOverride(FMath::Max(0.f, InSize.Y));
	}
}

void UBattlePlayerNameplate::RefreshAppearance()
{
	ResolveDesignerWidgets();

	const int32 MaxHP = FMath::Clamp(Data.MaxHP, 5, 6);
	const int32 HP = FMath::Clamp(Data.HP, 0, MaxHP);
	if (NameText) NameText->SetText(FText::FromString(Data.DisplayName.Len()>12 ? Data.DisplayName.Left(11)+TEXT("…") : Data.DisplayName));
	if (HealthText) HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d/%d"), HP, MaxHP)));
	if (FeverText) FeverText->SetText(FText::AsNumber(Data.Fever));
	SetImage(HealthIcon, HealthIconTexture);
	SetImage(FeverIcon, FeverIconTexture);
	for (int32 Index=0; Index<HealthSegments.Num(); ++Index)
	{
		if (!HealthSegments[Index]) continue;
		HealthSegments[Index]->SetVisibility(Index<MaxHP ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const bool Filled = Index<HP;
		SetImage(HealthSegments[Index], Filled ? HealthFilledTexture.Get() : HealthEmptyTexture.Get(),
			Filled ? FLinearColor(0.95f, 0.14f, 0.17f) : FLinearColor(0.3f, 0.34f, 0.4f));
	}
	const TMap<EBattleAction, TObjectPtr<UTexture2D>>* ClassIcons = nullptr;
	if (Data.CharacterId.Equals(TEXT("gladius"), ESearchCase::IgnoreCase)) ClassIcons = &GladiusActionIconTextures;
	else if (Data.CharacterId.Equals(TEXT("sera"), ESearchCase::IgnoreCase)) ClassIcons = &SeraActionIconTextures;
	const TObjectPtr<UTexture2D>* ResultIcon = ClassIcons ? ClassIcons->Find(Data.Action) : nullptr;
	// An explicit empty entry hides the icon; a missing entry uses the old common assignment.
	if (!ResultIcon) ResultIcon = ActionIconTextures.Find(Data.Action);
	SetImage(ActionIcon, ResultIcon ? ResultIcon->Get() : nullptr);
	SetImage(CritChanceIcon, CritChanceIconTexture);
	SetImage(CritDamageIcon, CritDamageIconTexture);
	const ESlateVisibility DetailVisibility = Data.Action==EBattleAction::Attack2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (CritChanceIcon) CritChanceIcon->SetVisibility(DetailVisibility);
	if (CritChanceText) CritChanceText->SetVisibility(DetailVisibility);
	if (CritDamageIcon) CritDamageIcon->SetVisibility(DetailVisibility);
	if (CritDamageText) CritDamageText->SetVisibility(DetailVisibility);
	if (CritChanceText) CritChanceText->SetText(FText::FromString(FString::Printf(TEXT("+%.0f%%"), Data.CritChance*100.f)));
	if (CritDamageText) CritDamageText->SetText(FText::FromString(FString::Printf(TEXT("+%.0f%%"), Data.CritDamage*100.f)));
	FString Action;
	switch (Data.Action)
	{
	case EBattleAction::Attack1:
	case EBattleAction::Attack2:
		Action = FString::Printf(TEXT("ATK %d   %d + %d"), Data.Action==EBattleAction::Attack1 ? 1 : 2, Data.BaseDamage, Data.BonusDamage); break;
	case EBattleAction::Defense:
		Action = FString::Printf(TEXT("DEFENSE   %.0f%%"), Data.DefenseReduction*100.f);
		break;
	case EBattleAction::AvoidL: case EBattleAction::AvoidM: case EBattleAction::AvoidR:
		Action = Data.Action==EBattleAction::AvoidL ? TEXT("EVADE LEFT") : Data.Action==EBattleAction::AvoidR ? TEXT("EVADE RIGHT") : TEXT("EVADE MID");
		break;
	default: Action = TEXT("IDLE"); break;
	}
	if (ActionText) ActionText->SetText(FText::FromString(Action));

}

float UBattlePlayerNameplate::GetDistanceScale(float Distance) const
{
	const float Minimum = FMath::Max(0.01f, MinimumDistanceScale);
	const float Maximum = FMath::Max(Minimum, MaximumDistanceScale);
	return FMath::Clamp(2400.f / FMath::Max(Distance, 1.f), Minimum, Maximum);
}

bool UBattlePlayerNameplate::ConvertToDesignerBlueprint(UObject* Asset)
{
#if WITH_EDITOR
	UWidgetBlueprint* BP = Cast<UWidgetBlueprint>(Asset);
	if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(StaticClass()) || !BP->WidgetTree) return false;
	UWidgetTree* Tree = BP->WidgetTree;
	if (Tree->FindWidget(TEXT("ActionPages"))) return true;
	// Never replace a user-authored hierarchy.
	if (Tree->RootWidget) return false;
	const UBattlePlayerNameplate* Defaults = BP->GeneratedClass->GetDefaultObject<UBattlePlayerNameplate>();
	BP->Modify(); Tree->Modify();
	auto Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlateSize"));
	Tree->RootWidget = Size;
	Size->SetWidthOverride(Defaults->PanelSize.X); Size->SetHeightOverride(Defaults->PanelSize.Y);
	auto Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlatePanel"));
	Border->SetPadding(FMargin(0)); Border->SetBrushColor(Defaults->PanelColor); Size->SetContent(Border);
	auto Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlateCanvas"));
	Border->SetContent(Root);
	auto Pages = Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("ActionPages"));
	auto PagesSlot = Root->AddChildToCanvas(Pages);
	PagesSlot->SetAnchors(FAnchors(0, 0, 1, 1)); PagesSlot->SetOffsets(FMargin(0));
	auto Place = [](UCanvasPanelSlot* CanvasSlot, const FBattleUIPlacement& P)
	{
		CanvasSlot->SetPosition(P.Position); CanvasSlot->SetSize(P.Size);
	};
	for (EBattleAction Action : {EBattleAction::Idle, EBattleAction::Attack1, EBattleAction::Attack2,
		EBattleAction::AvoidL, EBattleAction::AvoidM, EBattleAction::AvoidR, EBattleAction::Defense})
	{
		const FString Prefix = ActionPrefix(Action);
		auto Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*(TEXT("State_") + Prefix)));
		Pages->AddChild(Canvas);
		const auto Override = Defaults->ActionLayouts.Find(Action);
		const auto& L = Override ? *Override : Defaults->DefaultLayout;
		auto Text = [&](const TCHAR* Name, int32 FontSize, const FBattleUIPlacement& P)
		{
			auto W = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Prefix + TEXT("_") + Name)));
			auto Font = W->GetFont(); Font.Size = FontSize; W->SetFont(Font);
			W->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			W->SetShadowOffset(FVector2D(1, 1)); W->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
			Place(Canvas->AddChildToCanvas(W), P);
			return W;
		};
		auto Image = [&](const TCHAR* Name, const FBattleUIPlacement& P, UTexture2D* Texture)
		{
			auto W = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Prefix + TEXT("_") + Name)));
			Place(Canvas->AddChildToCanvas(W), P); SetImage(W, Texture); return W;
		};
		Text(TEXT("PlayerName"), 16, L.NameText)->SetText(FText::FromString(TEXT("Player")));
		Image(TEXT("HealthIcon"), L.HealthIcon, Defaults->HealthIconTexture);
		Text(TEXT("HealthValue"), 13, L.HealthText)->SetText(FText::FromString(TEXT("HP 5/5")));
		const FBattlePlayerUILayout Fallback;
		for (int32 I=0; I<6; ++I)
			Image(*FString::Printf(TEXT("HealthSegment%d"), I+1), L.HealthSegments.IsValidIndex(I) ? L.HealthSegments[I] : Fallback.HealthSegments[I], Defaults->HealthFilledTexture);
		Image(TEXT("FeverIcon"), L.FeverIcon, Defaults->FeverIconTexture);
		Text(TEXT("FeverValue"), 13, L.FeverText)->SetText(FText::FromString(TEXT("0")));
		Text(TEXT("ActionValue"), 13, L.ActionText)->SetText(FText::FromString(Prefix));
		Image(TEXT("CritChanceIcon"), L.CritChanceIcon, Defaults->CritChanceIconTexture);
		Text(TEXT("CritChanceValue"), 13, L.CritChanceText)->SetText(FText::FromString(TEXT("+25%")));
		Image(TEXT("CritDamageIcon"), L.CritDamageIcon, Defaults->CritDamageIconTexture);
		Text(TEXT("CritDamageValue"), 13, L.CritDamageText)->SetText(FText::FromString(TEXT("+50%")));
	}
	auto Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SharedActionIcon"));
	Place(Root->AddChildToCanvas(Icon), Defaults->ActionIconPlacement);
	Pages->SetActiveWidgetIndex(0);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	BP->MarkPackageDirty();
	return BP->Status != BS_Error;
#else
	return false;
#endif
}
