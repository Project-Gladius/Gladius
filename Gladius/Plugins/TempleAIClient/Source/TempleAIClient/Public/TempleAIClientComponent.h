#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HttpFwd.h"
#include "TempleAITypes.h"
#include "TempleAIClientComponent.generated.h"

UCLASS(ClassGroup = (TempleAI), meta = (BlueprintSpawnableComponent))
class TEMPLEAICLIENT_API UTempleAIClientComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTempleAIClientComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI")
    FString EndpointUrl = TEXT("http://127.0.0.1:8765/temple/respond");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI")
    FString ClientToken;

    UPROPERTY(BlueprintReadOnly, Category = "Temple AI")
    bool bIsBusy = false;

    UPROPERTY(BlueprintAssignable, Category = "Temple AI")
    FTempleAIResponseDelegate OnResponse;

    UPROPERTY(BlueprintAssignable, Category = "Temple AI")
    FTempleAIErrorDelegate OnError;

    UFUNCTION(BlueprintCallable, Category = "Temple AI")
    void SendTempleMessage(const FString& Uid, const FString& Username, const FString& Message);

    UFUNCTION(BlueprintCallable, Category = "Temple AI")
    void SetEndpointUrl(const FString& NewEndpointUrl);

    UFUNCTION(BlueprintCallable, Category = "Temple AI")
    void SetClientToken(const FString& NewClientToken);

private:
    void HandleTempleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void BroadcastError(const FString& ErrorMessage);
};
