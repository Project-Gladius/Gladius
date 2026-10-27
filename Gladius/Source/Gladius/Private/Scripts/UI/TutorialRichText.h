#pragma once
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/PanelWidget.h"
#include "Engine/DataTable.h"
#include "Styling/SlateTypes.h"
#include "Scripts/Battle/BattleTutorialController.h"

namespace TutorialText
{
// Called during NativeOnInitialized, before the live Slate tree is constructed.
inline URichTextBlock* Replace(UWidgetTree* Tree, UTextBlock* Original)
{
 if (!Tree || !Original || !Original->GetParent()) return nullptr;
 auto* Parent=Original->GetParent();
 auto* Rich=Tree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass());
 FTextBlockStyle Style;
 Style.SetFont(Original->GetFont()).SetColorAndOpacity(Original->GetColorAndOpacity())
  .SetShadowOffset(Original->GetShadowOffset()).SetShadowColorAndOpacity(Original->GetShadowColorAndOpacity());
 auto* Table=NewObject<UDataTable>(Rich); Table->RowStruct=FRichTextStyleRow::StaticStruct();
 FRichTextStyleRow Normal; Normal.TextStyle=Style; Table->AddRow(TEXT("Default"),Normal);
 FRichTextStyleRow Bold=Normal;
 auto Font=Style.Font;
 if (auto* FontObject=GetDefault<UBattleTutorialController>()->EmphasisFont.Get()) { Font.FontObject=FontObject; Font.TypefaceFontName=NAME_None; }
 Bold.TextStyle.SetFont(Font).SetColorAndOpacity(FSlateColor(FLinearColor(1.f,1.f,0.f)));
 Table->AddRow(TEXT("TutorialEmphasis"),Bold);
 Rich->SetTextStyleSet(Table); Rich->SetDefaultTextStyle(Style);
 if (auto* Property=FindFProperty<FByteProperty>(Original->GetClass(),TEXT("Justification")))
  Rich->SetJustification(static_cast<ETextJustify::Type>(Property->GetPropertyValue_InContainer(Original)));
 Rich->SetAutoWrapText(Original->GetAutoWrapText()); Rich->SetWrapTextAt(Original->GetWrapTextAt());
 Rich->SetRenderTransform(Original->GetRenderTransform());
 Rich->SetRenderTransformPivot(Original->GetRenderTransformPivot());
 Rich->SetRenderOpacity(Original->GetRenderOpacity());
 Rich->SetVisibility(Original->GetVisibility());
 const int32 Index=Parent->GetChildIndex(Original);
 auto* SlotTemplate=Original->Slot.Get();
 Parent->RemoveChildAt(Index);
 if (!Parent->InsertChildAt(Index,Rich,SlotTemplate)) return nullptr;
 return Rich;
}
inline void Set(URichTextBlock* Rich,const FString& Text)
{
 if (!Rich) return;
 const auto Formatted=UBattleTutorialController::FormatRichText(Text);
 if (Rich->GetText().ToString()!=Formatted) Rich->SetText(FText::FromString(Formatted));
}
}
