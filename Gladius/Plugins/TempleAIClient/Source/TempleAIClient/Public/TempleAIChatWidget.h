#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TempleAITypes.h"
#include "TempleAIChatWidget.generated.h"

class ATempleAIQuickTesterActor;
class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS()
class TEMPLEAICLIENT_API UTempleAIChatWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeTester(ATempleAIQuickTesterActor* InTester);
    void SetRequestPending(bool bPending);
    void ShowResponse(const FTempleAIResponse& Response);
    void ShowError(const FString& ErrorMessage);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<ATempleAIQuickTesterActor> Tester;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> MessageInput;

    UPROPERTY(Transient)
    TObjectPtr<UButton> SendButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResponseText;

    void BuildWidgetTree();
    void SubmitCurrentMessage();

    UFUNCTION()
    void HandleSendClicked();

    UFUNCTION()
    void HandleMessageCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
