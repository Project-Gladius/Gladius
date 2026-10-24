#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Scripts/Backend/YouTubeApiKeyPool.h"
#include "ChatLogSubsystem.generated.h"

UENUM(BlueprintType)
enum class EChatLogSpeaker : uint8
{
	Viewer,
	Admin,
	Goddess,
	Temple
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
	// 0이면 일반 채팅/대화, 1~4이면 실제 적용된 신전 강화 단계.
	UPROPERTY(BlueprintReadOnly, Category = "Chat Log") int32 EnhancementTier = 0;
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
	void AddTempleEnhancement(const FString& UserId, const FString& DisplayName, int32 Tier);

	void SetYouTubeSource(const FString& InApiKey, const FString& InVideoId, const TSet<FString>& InAdminUserIds);
	bool GetYouTubeSource(FString& OutApiKey, FString& OutVideoId, TSet<FString>& OutAdminUserIds) const;

	//레벨이 바뀌어도 바닥난 키를 기억하도록 게임 인스턴스에 둠
	FYouTubeApiKeyPool& GetApiKeyPool() { return ApiKeyPool; }

private:
	FYouTubeApiKeyPool ApiKeyPool;
	UPROPERTY(Transient)
	TArray<FChatLogEntry> Entries;
	TSet<FString> SeenMessageIds;
	FString ApiKey;
	FString VideoId;
	TSet<FString> AdminUserIds;

	void AddEntry(FChatLogEntry Entry);
};
