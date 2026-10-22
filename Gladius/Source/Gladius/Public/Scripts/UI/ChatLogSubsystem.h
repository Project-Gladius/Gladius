#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChatLogSubsystem.generated.h"

UENUM(BlueprintType)
enum class EChatLogSpeaker : uint8
{
	Viewer,
	Admin,
	Goddess
};

USTRUCT(BlueprintType)
struct FChatLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") FString MessageId;
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") FString UserId;
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") FString Message;
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") EChatLogSpeaker Speaker = EChatLogSpeaker::Viewer;
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") FDateTime CreatedAt;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatLogEntryAdded, const FChatLogEntry&);

// OpenLevel 이후에도 최근 채팅과 YouTube 연결 정보를 유지한다.
UCLASS()
class GLADIUS_API UChatLogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FOnChatLogEntryAdded OnEntryAdded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Log", meta = (ClampMin = "1"))
	int32 MaxEntries = 200;

	const TArray<FChatLogEntry>& GetEntries() const { return Entries; }

	void AddViewerMessage(const FString& MessageId, const FString& UserId, const FString& DisplayName,
		const FString& Message, bool bAdmin, const FDateTime& CreatedAt);
	void AddTempleQuestion(const FString& DisplayName, const FString& Message);
	void AddGoddessMessage(const FString& Message);

	void SetYouTubeSource(const FString& InApiKey, const FString& InVideoId, const TSet<FString>& InAdminUserIds);
	bool GetYouTubeSource(FString& OutApiKey, FString& OutVideoId, TSet<FString>& OutAdminUserIds) const;

private:
	UPROPERTY(Transient)
	TArray<FChatLogEntry> Entries;
	TSet<FString> SeenMessageIds;
	FString ApiKey;
	FString VideoId;
	TSet<FString> AdminUserIds;

	void AddEntry(FChatLogEntry Entry);
};
