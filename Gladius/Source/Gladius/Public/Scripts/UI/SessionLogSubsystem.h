#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SessionLogSubsystem.generated.h"

UENUM(BlueprintType)
enum class ESessionLogEvent : uint8 { Login, Logout, LoginFailed, InactivityLogout, Admin, AdminUrgent, Event };

USTRUCT(BlueprintType)
struct FSessionLogEntry
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly, Category="Session Log") int64 Sequence = 0;
 UPROPERTY(BlueprintReadOnly, Category="Session Log") bool bIsNotice = false;
 UPROPERTY(BlueprintReadOnly, Category="Session Log") FString MaintenanceScope;
 UPROPERTY(BlueprintReadOnly, Category="Session Log") FString UserId;
 UPROPERTY(BlueprintReadOnly, Category="Session Log") FString Message;
 UPROPERTY(BlueprintReadOnly, Category="Session Log") ESessionLogEvent Event = ESessionLogEvent::Login;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSessionLogAdded, const FSessionLogEntry&);

// Dedicated notification history survives map travel and never enters the chat history.
UCLASS()
class GLADIUS_API USessionLogSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 FOnSessionLogAdded OnEntryAdded;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session Log", meta=(ClampMin="1")) int32 MaxEntries = 100;
 const TArray<FSessionLogEntry>& GetEntries() const { return Entries; }
 void AddEvent(const FString& UserId, const FString& DisplayName, ESessionLogEvent Event);
 void AddMessage(const FString& Message, ESessionLogEvent Event);
 void AddNotice(const FString& Message);
 void AddMaintenanceMessage(const FString& Scope, bool bEnabled, const FString& Message, ESessionLogEvent Event);
 bool GetPresentedEntry(FSessionLogEntry& OutEntry);
private:
 friend struct FSessionPresentationTestAccess;
 UPROPERTY(Transient) TArray<FSessionLogEntry> Entries;
 UPROPERTY(Transient) TArray<FSessionLogEntry> PendingMessages;
 UPROPERTY(Transient) TMap<FString,FSessionLogEntry> ActiveMaintenance;
 UPROPERTY(Transient) FSessionLogEntry ActiveMessage;
 UPROPERTY(Transient) FSessionLogEntry LatestNotice;
 bool bHasActiveMessage = false;
 bool bHasLatestNotice = false;
 int64 NextSequence = 1;
 double RemainingSeconds = 0.0;
 double LastPresentationTime = -1.0;
 double PresentationTimeOverride = -1.0; // deterministic automation clock; runtime uses monotonic real time
 double PresentationTime() const;
 void AdvancePresentation();
 void StoreEntry(FSessionLogEntry& Entry);
 void QueueMessage(FSessionLogEntry Entry);
};
