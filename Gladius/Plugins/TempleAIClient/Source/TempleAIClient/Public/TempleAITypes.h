#pragma once

#include "CoreMinimal.h"
#include "TempleAITypes.generated.h"

USTRUCT(BlueprintType)
struct TEMPLEAICLIENT_API FTempleAIResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Temple AI")
    FString Uid;

    UPROPERTY(BlueprintReadOnly, Category = "Temple AI")
    FString Username;

    UPROPERTY(BlueprintReadOnly, Category = "Temple AI")
    int32 Priority = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Temple AI")
    FString ResponseText;

    UPROPERTY(BlueprintReadOnly, Category = "Temple AI")
    FString Emotion;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTempleAIResponseDelegate, const FTempleAIResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTempleAIErrorDelegate, const FString&, ErrorMessage);
