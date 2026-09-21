#include "TempleAIClientComponent.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UTempleAIClientComponent::UTempleAIClientComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTempleAIClientComponent::SetEndpointUrl(const FString& NewEndpointUrl)
{
    EndpointUrl = NewEndpointUrl.TrimStartAndEnd();
}

void UTempleAIClientComponent::SetClientToken(const FString& NewClientToken)
{
    ClientToken = NewClientToken.TrimStartAndEnd();
}

void UTempleAIClientComponent::SendTempleMessage(
    const FString& Uid,
    const FString& Username,
    const FString& Message)
{
    if (bIsBusy)
    {
        BroadcastError(TEXT("Temple AI request is already running."));
        return;
    }

    const FString CleanEndpointUrl = EndpointUrl.TrimStartAndEnd();
    const FString CleanUid = Uid.TrimStartAndEnd();
    const FString CleanUsername = Username.TrimStartAndEnd();
    const FString CleanMessage = Message.TrimStartAndEnd();

    if (CleanEndpointUrl.IsEmpty())
    {
        BroadcastError(TEXT("Temple AI endpoint URL is empty."));
        return;
    }

    if (CleanUid.IsEmpty() || CleanUsername.IsEmpty() || CleanMessage.IsEmpty())
    {
        BroadcastError(TEXT("uid, username, and message are required."));
        return;
    }

    TSharedRef<FJsonObject> RequestObject = MakeShared<FJsonObject>();
    RequestObject->SetStringField(TEXT("uid"), CleanUid);
    RequestObject->SetStringField(TEXT("username"), CleanUsername);
    RequestObject->SetStringField(TEXT("message"), CleanMessage);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    if (!FJsonSerializer::Serialize(RequestObject, Writer))
    {
        BroadcastError(TEXT("Temple AI request JSON serialization failed."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(CleanEndpointUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

    const FString CleanClientToken = ClientToken.TrimStartAndEnd();
    if (!CleanClientToken.IsEmpty())
    {
        Request->SetHeader(TEXT("X-Temple-Client-Token"), CleanClientToken);
    }

    Request->SetContentAsString(RequestBody);
    Request->OnProcessRequestComplete().BindUObject(
        this,
        &UTempleAIClientComponent::HandleTempleResponse);

    bIsBusy = true;
    if (!Request->ProcessRequest())
    {
        bIsBusy = false;
        BroadcastError(TEXT("Temple AI HTTP request could not be started."));
    }
}

void UTempleAIClientComponent::HandleTempleResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful)
{
    bIsBusy = false;

    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastError(TEXT("Temple AI request failed before receiving a response."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        BroadcastError(FString::Printf(
            TEXT("Temple AI request failed: HTTP %d\n%s"),
            ResponseCode,
            *ResponseBody));
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        BroadcastError(FString::Printf(
            TEXT("Temple AI response JSON parse failed.\n%s"),
            *ResponseBody));
        return;
    }

    FTempleAIResponse ParsedResponse;
    double PriorityNumber = 0.0;

    RootObject->TryGetStringField(TEXT("uid"), ParsedResponse.Uid);
    RootObject->TryGetStringField(TEXT("username"), ParsedResponse.Username);
    RootObject->TryGetNumberField(TEXT("priority"), PriorityNumber);
    RootObject->TryGetStringField(TEXT("responseText"), ParsedResponse.ResponseText);
    RootObject->TryGetStringField(TEXT("emotion"), ParsedResponse.Emotion);
    ParsedResponse.Priority = FMath::RoundToInt(PriorityNumber);

    if (ParsedResponse.ResponseText.TrimStartAndEnd().IsEmpty())
    {
        BroadcastError(FString::Printf(
            TEXT("Temple AI response is empty or invalid.\n%s"),
            *ResponseBody));
        return;
    }

    OnResponse.Broadcast(ParsedResponse);
}

void UTempleAIClientComponent::BroadcastError(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);
    OnError.Broadcast(ErrorMessage);
}
