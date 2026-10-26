#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Scripts/UI/SessionLogSubsystem.h"
#include "SessionLogWidget.generated.h"

class UImage;
class UTextBlock;
class UScrollBox;
class UVerticalBox;
class USizeBox;

// Shared single-line notification ticker, independent of UChatLogWidget.
UCLASS()
class GLADIUS_API USessionLogWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 USessionLogWidget(const FObjectInitializer& ObjectInitializer);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FSlateBrush BackgroundBrush;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FLinearColor BackgroundTint = FLinearColor::White;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FSlateFontInfo LogFont;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FLinearColor LogColor = FLinearColor::White;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Layout") FVector2D WindowSize = FVector2D(600, 100);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Layout") FVector2D WindowOffset = FVector2D(-24, 24);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Layout") FMargin TextPadding = FMargin(60, 20, 24, 20);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Layout", meta=(ClampMin="0")) float LineSpacing = 5;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Layout", meta=(ClampMin="1")) int32 MaxVisibleEntries = 30;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FLinearColor AdminColor = FLinearColor::Yellow;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FLinearColor AdminUrgentColor = FLinearColor::Red;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Style") FLinearColor EventColor = FLinearColor::Green;
 // Messages are held at both ends so the complete text remains readable.
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Ticker", meta=(ClampMin="1")) float ScrollSpeed = 55.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Ticker", meta=(ClampMin="0")) float StartHoldSeconds = 1.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Ticker", meta=(ClampMin="0")) float EndHoldSeconds = 1.5f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log|Ticker", meta=(ClampMin="1")) float ShortMessageSeconds = 4.f;
 UFUNCTION(BlueprintCallable, Category="Session Log") void RefreshStyle();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeOnInitialized() override;
 virtual void NativePreConstruct() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
 UPROPERTY(BlueprintReadOnly, Category="Session Log", meta=(BindWidgetOptional)) TObjectPtr<UImage> LogBackground;
 UPROPERTY(BlueprintReadOnly, Category="Session Log", meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> LogScrollBox;
 UPROPERTY(BlueprintReadOnly, Category="Session Log", meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> LogMessages;
 UPROPERTY(BlueprintReadOnly, Category="Session Log", meta=(BindWidgetOptional)) TObjectPtr<USizeBox> LogWindow;
private:
 friend class FSessionLogWidgetTest;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> CurrentText;
 TArray<FSessionLogEntry> PendingEntries;
 FSessionLogEntry CurrentEntry;
 bool bHasCurrentEntry = false;
 float MessageElapsed = 0.f;
 void ShowEntry(const FSessionLogEntry& Entry);
 void ApplyCurrentStyle();
 void AdvanceTicker(float DeltaSeconds);
 TWeakObjectPtr<USessionLogSubsystem> BoundLog;
 void BuildDefaultLayout();
 void RebuildEntries();
 void AppendMessage(const FString& Message, ESessionLogEvent Event = ESessionLogEvent::Login);
 void HandleEntryAdded(const FSessionLogEntry& Entry);
};
