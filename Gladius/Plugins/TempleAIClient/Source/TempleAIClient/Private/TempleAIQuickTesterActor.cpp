#include "TempleAIQuickTesterActor.h"

#include "Engine/Engine.h"

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

    if (bSendOnBeginPlay)
    {
        SendTestMessage();
    }
}

void ATempleAIQuickTesterActor::SendTestMessage()
{
    if (!TempleAIClient)
    {
        UE_LOG(LogTemp, Error, TEXT("TempleAIClient component is missing."));
        return;
    }

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

void ATempleAIQuickTesterActor::HandleTempleAIResponse(const FTempleAIResponse& Response)
{
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Temple AI response: [%s] priority=%d emotion=%s text=%s"),
        *Response.Username,
        Response.Priority,
        *Response.Emotion,
        *Response.ResponseText);

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
    UE_LOG(LogTemp, Error, TEXT("Temple AI error: %s"), *ErrorMessage);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            20.0f,
            FColor::Red,
            FString::Printf(TEXT("Temple AI 오류\n%s"), *ErrorMessage));
    }
}
