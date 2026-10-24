#include "Scripts/UI/BattleDataResetWidget.h"
#include "Scripts/Battle/BattleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBattleDataResetWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Canvas;
    auto* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetPadding(FMargin(10.f));
    Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.9f));
    auto* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetPosition(FVector2D(20.f, 20.f));
    PanelSlot->SetAutoSize(true);
    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Column);
    auto* KoreanFont = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/NotoSans/NotoSansCJKkr_Regular_Font.NotoSansCJKkr_Regular_Font"));
    auto MakeButton = [&](const TCHAR* Name, const TCHAR* Label)
    {
        auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        auto Font = Text->GetFont(); Font.Size = 16; if (KoreanFont) Font.FontObject = KoreanFont; Text->SetFont(Font);
        Text->SetMargin(FMargin(12.f, 8.f));
        Button->SetContent(Text);
        Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        return Button;
    };
    MakeButton(TEXT("ResetAllUsersButton"), TEXT("전체 유저 데이터 초기화"))->OnClicked.AddDynamic(this, &UBattleDataResetWidget::ResetAllClicked);
    MakeButton(TEXT("ResetTestUsersButton"), TEXT("테스트 유저만 초기화"))->OnClicked.AddDynamic(this, &UBattleDataResetWidget::ResetTestsClicked);
    ResultText = WidgetTree->ConstructWidget<UTextBlock>();
    auto Font = ResultText->GetFont(); Font.Size = 13; if (KoreanFont) Font.FontObject = KoreanFont; ResultText->SetFont(Font);
    ResultText->SetText(FText::FromString(TEXT("임시 데이터 관리")));
    Column->AddChildToVerticalBox(ResultText);
}
void UBattleDataResetWidget::ResetAllClicked() { Reset(false); }
void UBattleDataResetWidget::ResetTestsClicked() { Reset(true); }
void UBattleDataResetWidget::Reset(bool bTestsOnly)
{
    const bool bSaved = BattleManager.IsValid() && BattleManager->ResetSavedUserData(bTestsOnly);
    ResultText->SetText(FText::FromString(bSaved
        ? (bTestsOnly ? TEXT("테스트 유저 초기화 완료") : TEXT("전체 유저 초기화 완료"))
        : TEXT("초기화 실패: 저장 파일을 확인해 주세요")));
}
