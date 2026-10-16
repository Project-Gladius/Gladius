#include "TempleAIQuickTesterActor.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "TempleAIChatWidget.h"

ATempleAIQuickTesterActor::ATempleAIQuickTesterActor()
{
    PrimaryActorTick.bCanEverTick = false;
    TempleAIClient = CreateDefaultSubobject<UTempleAIClientComponent>(TEXT("TempleAIClient"));
}

void ATempleAIQuickTesterActor::BeginPlay()
{
    Super::BeginPlay();

    TempleAIClient->OnResponse.AddDynamic(this, &ATempleAIQuickTesterActor::HandleTempleAIResponse);
    TempleAIClient->OnError.AddDynamic(this, &ATempleAIQuickTesterActor::HandleTempleAIError);

    if (bShowInputUI)
    {
        CreateInputUI();
    }

    if (bSendOnBeginPlay)
    {
        SendTestMessage();
    }
}

void ATempleAIQuickTesterActor::CreateInputUI()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("Temple AI input UI requires a local player controller."));
        return;
    }

    TSubclassOf<UTempleAIChatWidget> WidgetType = ChatWidgetClass;
    if (!WidgetType)
    {
        WidgetType = UTempleAIChatWidget::StaticClass();
    }

    ChatWidget = CreateWidget<UTempleAIChatWidget>(PlayerController, WidgetType);
    if (!ChatWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("Temple AI input UI could not be created."));
        return;
    }

    ChatWidget->InitializeTester(this);
    ChatWidget->AddToViewport(100);

    UE_LOG(LogTemp, Log, TEXT("Temple AI input UI created and added to the viewport."));

    PlayerController->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(ChatWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
}

void ATempleAIQuickTesterActor::SendTestMessage()
{
    if (!TempleAIClient)
    {
        UE_LOG(LogTemp, Error, TEXT("TempleAIClient component is missing."));
        return;
    }

    if (bRequestInFlight)
    {
        if (ChatWidget)
        {
            ChatWidget->ShowError(TEXT("이전 응답을 기다리고 있습니다."));
        }
        return;
    }

    TestMessage = TestMessage.TrimStartAndEnd();
    if (TestMessage.IsEmpty())
    {
        if (ChatWidget)
        {
            ChatWidget->ShowError(TEXT("메시지를 입력해 주세요."));
        }
        return;
    }

    bRequestInFlight = true;
    if (ChatWidget)
    {
        ChatWidget->SetRequestPending(true);
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Temple AI request: endpoint=%s uid=%s"),
        *TempleAIClient->EndpointUrl,
        *TestUid);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            8.0f,
            FColor::Yellow,
            TEXT("Temple AI: 서버에 메시지를 보내는 중입니다..."));
    }

    TempleAIClient->SendTempleMessage(TestUid, TestUsername, TestMessage);
}

void ATempleAIQuickTesterActor::SubmitMessageFromUI(const FString& Message)
{
    TestMessage = Message;
    SendTestMessage();
}

void ATempleAIQuickTesterActor::HandleTempleAIResponse(const FTempleAIResponse& Response)
{
    bRequestInFlight = false;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Temple AI response: [%s] priority=%d emotion=%s text=%s"),
        *Response.Username,
        Response.Priority,
        *Response.Emotion,
        *Response.ResponseText);

    if (ChatWidget)
    {
        ChatWidget->ShowResponse(Response);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            20.0f,
            FColor::Green,
            FString::Printf(
                TEXT("Temple AI 응답\n감정: %s\n%s"),
                *Response.Emotion,
                *Response.ResponseText));
    }
}

void ATempleAIQuickTesterActor::HandleTempleAIError(const FString& ErrorMessage)
{
    bRequestInFlight = false;

    UE_LOG(LogTemp, Error, TEXT("Temple AI error: %s"), *ErrorMessage);

    if (ChatWidget)
    {
        ChatWidget->ShowError(ErrorMessage);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            20.0f,
            FColor::Red,
            FString::Printf(TEXT("Temple AI 오류\n%s"), *ErrorMessage));
    }
}
