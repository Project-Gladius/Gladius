#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/ChatLogSubsystem.h"
#include "ChatLogWidget.generated.h"

class UButton;
class UCanvasPanelSlot;
class UScrollBox;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;

// 최종 아트 없이 사용하는 공통 채팅 로그. WBP 자식에서 같은 이름의 위젯으로 교체할 수 있다.
UCLASS()
class GLADIUS_API UChatLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetExpansionAllowed(bool bAllowed);
	void SetExpanded(bool bInExpanded);
	void SetFixedExpanded();
	void ApplyBattleSideLayout(float Width, float Margin, float Top, float Height);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// Retained only to migrate existing Blueprint layouts to a non-scrolling viewport.
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MessageScrollBox;

	UPROPERTY(BlueprintReadOnly, Category = "Chat Log", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> MessageList;

	UPROPERTY(BlueprintReadOnly, Category = "Chat Log", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExpandButton;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MessageViewport;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExpandButtonText;

	TWeakObjectPtr<UChatLogSubsystem> BoundLog;
	TWeakObjectPtr<UCanvasPanelSlot> PanelSlot;
	bool bExpansionAllowed = false;
	bool bExpanded = false;
	bool bFitBelowBlessings = false;

	void BuildDefaultWidgetTree();
	void ConfigureMessageViewport();
	void RefreshLayout();
	void RebuildEntries();
	void AppendEntry(const FChatLogEntry& Entry);
	void HandleEntryAdded(const FChatLogEntry& Entry);

	UFUNCTION()
	void HandleExpandClicked();
};
