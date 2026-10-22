#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/UI/ChatLogSubsystem.h"
#include "ChatLogWidget.generated.h"

class UButton;
class UCanvasPanelSlot;
class UScrollBox;
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

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Chat Log", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MessageScrollBox;

	UPROPERTY(BlueprintReadOnly, Category = "Chat Log", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> MessageList;

	UPROPERTY(BlueprintReadOnly, Category = "Chat Log", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExpandButton;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExpandButtonText;

	TWeakObjectPtr<UChatLogSubsystem> BoundLog;
	TWeakObjectPtr<UCanvasPanelSlot> PanelSlot;
	bool bExpansionAllowed = false;
	bool bExpanded = false;

	void BuildDefaultWidgetTree();
	void RefreshLayout();
	void RebuildEntries();
	void AppendEntry(const FChatLogEntry& Entry);
	void HandleEntryAdded(const FChatLogEntry& Entry);

	UFUNCTION()
	void HandleExpandClicked();
};
