#include "Scripts/Battle/BattlePlayerNameplate.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Parent, const FName Name, int32 FontSize,
		const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Parent->AddChildToVerticalBox(Text);
		return Text;
	}
}

void UBattlePlayerNameplate::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultWidgetTree();
	Refresh();
}

void UBattlePlayerNameplate::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlateSize"));
	Size->SetWidthOverride(180.f);
	Size->SetHeightOverride(104.f);
	WidgetTree->RootWidget = Size;

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlatePanel"));
	Panel->SetPadding(FMargin(8.f, 5.f));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.065f, 0.90f));
	Size->SetContent(Panel);

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlateRows"));
	Panel->SetContent(Rows);
	NameText = AddText(WidgetTree, Rows, TEXT("PlayerName"), 16, FLinearColor::White);

	UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HealthRow"));
	Rows->AddChildToVerticalBox(HealthRow);
	UTextBlock* HPLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HPLabel"));
	HPLabel->SetText(FText::FromString(TEXT("HP ")));
	HPLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.32f, 0.32f)));
	HealthRow->AddChildToHorizontalBox(HPLabel);

	for (int32 Index = 0; Index < 6; ++Index)
	{
		USizeBox* SegmentSize = WidgetTree->ConstructWidget<USizeBox>();
		SegmentSize->SetWidthOverride(15.f);
		SegmentSize->SetHeightOverride(10.f);
		UBorder* Segment = WidgetTree->ConstructWidget<UBorder>();
		Segment->SetPadding(FMargin(0.f));
		SegmentSize->SetContent(Segment);
		UHorizontalBoxSlot* HealthSlot = HealthRow->AddChildToHorizontalBox(SegmentSize);
		HealthSlot->SetPadding(FMargin(1.f, 3.f, 1.f, 0.f));
		HealthSegments.Add(Segment);
	}

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthValue"));
	HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HealthRow->AddChildToHorizontalBox(HealthText);
	FeverText = AddText(WidgetTree, Rows, TEXT("FeverValue"), 11, FLinearColor(1.f, 0.64f, 0.16f));
	ActionText = AddText(WidgetTree, Rows, TEXT("ActionValue"), 13, FLinearColor(0.98f, 0.83f, 0.35f));
	DetailText = AddText(WidgetTree, Rows, TEXT("ActionDetail"), 10, FLinearColor(0.8f, 0.86f, 0.95f));
}

void UBattlePlayerNameplate::SetDisplayData(const FBattlePlayerUIData& InData)
{
	Data = InData;
	Refresh();
	OnDisplayDataChanged(Data);
}

void UBattlePlayerNameplate::Refresh()
{
	if (!NameText || !HealthText || !FeverText || !ActionText || !DetailText) return;

	const FString ShortName = Data.DisplayName.Len() > 12
		? Data.DisplayName.Left(11) + TEXT("…") : Data.DisplayName;
	NameText->SetText(FText::FromString(ShortName));
	HealthText->SetText(FText::FromString(FString::Printf(TEXT(" %d/%d"), Data.HP, Data.MaxHP)));
	FeverText->SetText(FText::FromString(FString::Printf(TEXT("FEVER  %d"), Data.Fever)));
	const float HealthRatio = FMath::Clamp(static_cast<float>(Data.HP) / FMath::Max(Data.MaxHP, 1), 0.f, 1.f);
	const int32 Filled = FMath::CeilToInt(HealthRatio * HealthSegments.Num());
	for (int32 Index = 0; Index < HealthSegments.Num(); ++Index)
	{
		HealthSegments[Index]->SetBrushColor(Index < Filled
			? FLinearColor(0.95f, 0.14f, 0.17f, 1.f)
			: FLinearColor(0.30f, 0.34f, 0.40f, 1.f));
	}

	FString Action;
	FString Detail;
	FLinearColor Accent(0.98f, 0.83f, 0.35f);
	switch (Data.Action)
	{
	case EBattleAction::Attack1:
		Action = FString::Printf(TEXT("ATK 1   %d + %d"), Data.BaseDamage, Data.BonusDamage);
		Detail = TEXT("기본 + 강화/피버  ·  스킬 1");
		break;
	case EBattleAction::Attack2:
		Action = FString::Printf(TEXT("ATK 2   %d + %d"), Data.BaseDamage, Data.BonusDamage);
		Detail = FString::Printf(TEXT("치명 %.0f%% / 추가 %.0f%%"), Data.CritChance * 100.f, Data.CritDamage * 100.f);
		break;
	case EBattleAction::Defense:
		Action = FString::Printf(TEXT("DEFENSE   %.0f%%"), Data.DefenseReduction * 100.f);
		Detail = TEXT("받는 피해 감소  ·  방어 스킬");
		Accent = FLinearColor(0.55f, 0.78f, 1.f);
		break;
	case EBattleAction::AvoidL:
	case EBattleAction::AvoidM:
	case EBattleAction::AvoidR:
		Action = Data.Action == EBattleAction::AvoidL ? TEXT("EVADE   LEFT")
			: Data.Action == EBattleAction::AvoidR ? TEXT("EVADE   RIGHT") : TEXT("EVADE   MID");
		Detail = Data.bFeverWillDrop ? TEXT("! 연속 회피: 피버 감소 예상") : TEXT("회피 스킬 선택");
		Accent = Data.bFeverWillDrop ? FLinearColor(1.f, 0.36f, 0.25f)
			: FLinearColor(0.55f, 0.92f, 0.68f);
		break;
	default:
		Action = TEXT("IDLE");
		Detail = TEXT("행동 선택 대기 중");
		break;
	}
	ActionText->SetText(FText::FromString(Action));
	ActionText->SetColorAndOpacity(FSlateColor(Accent));
	DetailText->SetText(FText::FromString(Detail));
}
