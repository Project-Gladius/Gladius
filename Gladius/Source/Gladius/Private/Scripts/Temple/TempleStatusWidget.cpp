#include "Scripts/Temple/TempleStatusWidget.h"
#include "Scripts/Temple/Goddess.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

namespace
{
	UBorder* AddPanel(UWidgetTree* WidgetTree, UCanvasPanel* Canvas, const FName Name, const FAnchors& Anchors, float Opacity)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetPadding(FMargin(14.0f, 10.0f));
		Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, Opacity));

		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(Anchors);
		PanelSlot->SetOffsets(FMargin(0.0f));
		return Panel;
	}

	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}
}

void UTempleStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultWidgetTree();
	if (CombatButton) CombatButton->OnClicked.AddUniqueDynamic(this, &UTempleStatusWidget::HandleCombatClicked);
}

void UTempleStatusWidget::NativeDestruct()
{
	if (AGoddess* BoundGoddess = Goddess.Get())
	{
		BoundGoddess->OnStateChanged.RemoveDynamic(this, &UTempleStatusWidget::HandleGoddessStateChanged);
	}
	BindToController(nullptr);
	if (CombatButton) CombatButton->OnClicked.RemoveDynamic(this, &UTempleStatusWidget::HandleCombatClicked);
	Super::NativeDestruct();
}

void UTempleStatusWidget::BindToGoddess(AGoddess* InGoddess)
{
	if (AGoddess* OldGoddess = Goddess.Get())
	{
		OldGoddess->OnStateChanged.RemoveDynamic(this, &UTempleStatusWidget::HandleGoddessStateChanged);
	}

	Goddess = InGoddess;
	if (InGoddess)
	{
		InGoddess->OnStateChanged.AddUniqueDynamic(this, &UTempleStatusWidget::HandleGoddessStateChanged);
		HandleGoddessStateChanged(InGoddess->GetState(), FGoddessStateContext());
	}
}

void UTempleStatusWidget::BuildDefaultWidgetTree()
{
	//WBP에서 레이아웃을 만든 경우에는 그대로 사용
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Canvas;

	//여신 상태: 좌상단
	UBorder* StatePanel = AddPanel(WidgetTree, Canvas, TEXT("StatePanel"), FAnchors(0.02f, 0.03f, 0.30f, 0.10f), 0.85f);
	StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StateText"));
	StateText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.82f, 0.45f)));
	SetFontSize(StateText, 20);
	StatePanel->SetContent(StateText);

	UBorder* ControlPanel = AddPanel(WidgetTree, Canvas, TEXT("ControlPanel"), FAnchors(0.02f, 0.12f, 0.30f, 0.25f), 0.85f);
	UVerticalBox* Controls = WidgetTree->ConstructWidget<UVerticalBox>();
	ControlPanel->SetContent(Controls);
	CombatButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CombatButton"));
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>();
	ButtonText->SetText(FText::FromString(TEXT("전투 씬으로 이동")));
	CombatButton->SetContent(ButtonText);
	Controls->AddChildToVerticalBox(CombatButton);

	//상태 로그: 우측 넓게, 작은 글씨
	UBorder* LogPanel = AddPanel(WidgetTree, Canvas, TEXT("LogPanel"), FAnchors(0.34f, 0.03f, 0.98f, 0.70f), 0.75f);
	LogScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LogScrollBox"));
	LogPanel->SetContent(LogScrollBox);

	LogText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LogText"));
	LogText->SetAutoWrapText(true);
	LogText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.82f, 0.88f)));
	SetFontSize(LogText, 10);
	LogScrollBox->AddChild(LogText);

	//여신 대사: 하단 중앙, 큰 글씨
	UBorder* DialoguePanel = AddPanel(WidgetTree, Canvas, TEXT("DialoguePanel"), FAnchors(0.12f, 0.76f, 0.88f, 0.96f), 0.85f);
	DialoguePanel->SetPadding(FMargin(28.0f, 18.0f));
	DialoguePanel->SetVerticalAlignment(VAlign_Center);
	DialogueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DialogueText"));
	DialogueText->SetAutoWrapText(true);
	DialogueText->SetJustification(ETextJustify::Center);
	DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetFontSize(DialogueText, 24);
	DialoguePanel->SetContent(DialogueText);
}

void UTempleStatusWidget::HandleGoddessStateChanged(EGoddessState NewState, const FGoddessStateContext& Context)
{
	const FString StateLabel = GetStateLabel(NewState, Context);
	if (StateText)
	{
		StateText->SetText(FText::FromString(FString::Printf(TEXT("여신 상태: %s"), *StateLabel)));
	}

	AppendLog(FString::Printf(TEXT("[상태] %s"), *StateLabel));

	switch (NewState)
	{
	case EGoddessState::Blessing1:
	case EGoddessState::Blessing2:
	case EGoddessState::Blessing3:
	case EGoddessState::Blessing4:
	{
		const int32 Tier = GetBlessingTier(NewState);
		for (const FString& Username : Context.BlessedUsernames)
		{
			AppendLog(FString::Printf(TEXT("[대사] %s 의 강화요청 (강화%d)"), *Username, Tier));
		}
		SetDialogue(BuildBlessingDialogue(Context));
		break;
	}
	case EGoddessState::Reading:
		AppendLog(FString::Printf(TEXT("[질문] %s : %s"), *Context.Username, *Context.Text));
		SetDialogue(FString());
		break;
	case EGoddessState::Speaking:
	{
		const FString Dialogue = FString::Printf(TEXT("%s에게 : %s"), *Context.Username, *Context.Text);
		AppendLog(FString::Printf(TEXT("[대사] %s"), *Dialogue));
		SetDialogue(Dialogue);
		break;
	}
	case EGoddessState::Idle:
	default:
		SetDialogue(FString());
		break;
	}
}

void UTempleStatusWidget::SetDialogue(const FString& Dialogue)
{
	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(Dialogue));
	}
}

void UTempleStatusWidget::AppendLog(const FString& Line)
{
	LogLines.Add(FString::Printf(TEXT("[%s] %s"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Line));
	while (LogLines.Num() > FMath::Max(MaxLogLines, 1))
	{
		LogLines.RemoveAt(0);
	}

	if (LogText)
	{
		LogText->SetText(FText::FromString(FString::Join(LogLines, LINE_TERMINATOR)));
	}
	if (LogScrollBox)
	{
		LogScrollBox->ScrollToEnd();
	}
}

int32 UTempleStatusWidget::GetBlessingTier(EGoddessState State)
{
	return static_cast<int32>(State) - static_cast<int32>(EGoddessState::Blessing1) + 1;
}

FString UTempleStatusWidget::GetStateLabel(EGoddessState State, const FGoddessStateContext& Context)
{
	FString Label = UEnum::GetDisplayValueAsText(State).ToString();
	if (State == EGoddessState::Idle && Context.bWaitingForResponse)
	{
		Label += TEXT(" (AI 응답 대기 중)");
	}
	return Label;
}

FString UTempleStatusWidget::BuildBlessingDialogue(const FGoddessStateContext& Context) const
{
	const TArray<FString>& Names = Context.BlessedUsernames;
	const int32 ShownCount = FMath::Min(Names.Num(), FMath::Max(MaxBlessingNamesInDialogue, 1));

	TArray<FString> Lines;
	for (int32 Index = 0; Index < ShownCount; ++Index)
	{
		Lines.Add(FString::Printf(TEXT("%s 의 강화요청"), *Names[Index]));
	}
	if (Names.Num() > ShownCount)
	{
		Lines.Add(FString::Printf(TEXT("외 %d명"), Names.Num() - ShownCount));
	}
	return FString::Join(Lines, LINE_TERMINATOR);
}

void UTempleStatusWidget::BindToController(ATempleCycleController* InController)
{
	if (Controller.IsValid())
	{
		Controller->OnCombatRequested.RemoveDynamic(this, &UTempleStatusWidget::HandleCombatRequested);
	}
	Controller = InController;
	if (InController)
	{
		InController->OnCombatRequested.AddUniqueDynamic(this, &UTempleStatusWidget::HandleCombatRequested);
	}
}

void UTempleStatusWidget::HandleCombatClicked()
{
	if (Controller.IsValid()) Controller->RequestCombatTravel();
}

void UTempleStatusWidget::HandleCombatRequested()
{
	if (CombatButton) CombatButton->SetIsEnabled(false);
}
