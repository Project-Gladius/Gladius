#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TempleAIClientComponent.h"
#include "TempleAIQuickTesterActor.generated.h"

UCLASS()
class TEMPLEAICLIENT_API ATempleAIQuickTesterActor : public AActor
{
    GENERATED_BODY()

public:
    ATempleAIQuickTesterActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Temple AI")
    UTempleAIClientComponent* TempleAIClient;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI")
    FString TestUid = TEXT("unreal-editor-001");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI")
    FString TestUsername = TEXT("언리얼테스터");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI")
    FString TestMessage = TEXT("여신님, 제 목소리가 들리시나요?");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temple AI")
    bool bSendOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, Category = "Temple AI")
    void SendTestMessage();

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleTempleAIResponse(const FTempleAIResponse& Response);

    UFUNCTION()
    void HandleTempleAIError(const FString& ErrorMessage);
};
