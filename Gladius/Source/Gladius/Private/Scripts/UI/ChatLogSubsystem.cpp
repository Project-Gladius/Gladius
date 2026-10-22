#include "Scripts/UI/ChatLogSubsystem.h"

void UChatLogSubsystem::AddViewerMessage(const FString& MessageId, const FString& UserId,
	const FString& DisplayName, const FString& Message, bool bAdmin, const FDateTime& CreatedAt)
{
	FChatLogEntry Entry;
	Entry.MessageId = MessageId;
	Entry.UserId = UserId;
	Entry.DisplayName = DisplayName.IsEmpty() ? UserId : DisplayName;
	Entry.Message = Message;
	Entry.Speaker = bAdmin ? EChatLogSpeaker::Admin : EChatLogSpeaker::Viewer;
	Entry.CreatedAt = CreatedAt;
	AddEntry(MoveTemp(Entry));
}

void UChatLogSubsystem::AddTempleQuestion(const FString& DisplayName, const FString& Message)
{
	FChatLogEntry Entry;
	Entry.DisplayName = DisplayName;
	Entry.Message = Message;
	Entry.CreatedAt = FDateTime::UtcNow();
	AddEntry(MoveTemp(Entry));
}

void UChatLogSubsystem::AddGoddessMessage(const FString& Message)
{
	FChatLogEntry Entry;
	Entry.DisplayName = TEXT("아우렐리아");
	Entry.Message = Message;
	Entry.Speaker = EChatLogSpeaker::Goddess;
	Entry.CreatedAt = FDateTime::UtcNow();
	AddEntry(MoveTemp(Entry));
}

void UChatLogSubsystem::AddEntry(FChatLogEntry Entry)
{
	Entry.Message.TrimStartAndEndInline();
	if (Entry.Message.IsEmpty() || (!Entry.MessageId.IsEmpty() && SeenMessageIds.Contains(Entry.MessageId))) return;

	if (!Entry.MessageId.IsEmpty()) SeenMessageIds.Add(Entry.MessageId);
	Entries.Add(MoveTemp(Entry));
	while (Entries.Num() > FMath::Max(1, MaxEntries))
	{
		if (!Entries[0].MessageId.IsEmpty()) SeenMessageIds.Remove(Entries[0].MessageId);
		Entries.RemoveAt(0);
	}
	OnEntryAdded.Broadcast(Entries.Last());
}

void UChatLogSubsystem::SetYouTubeSource(const FString& InApiKey, const FString& InVideoId,
	const TSet<FString>& InAdminUserIds)
{
	if (InApiKey.IsEmpty() || InVideoId.IsEmpty()) return;
	ApiKey = InApiKey;
	VideoId = InVideoId;
	AdminUserIds = InAdminUserIds;
}

bool UChatLogSubsystem::GetYouTubeSource(FString& OutApiKey, FString& OutVideoId,
	TSet<FString>& OutAdminUserIds) const
{
	if (ApiKey.IsEmpty() || VideoId.IsEmpty()) return false;
	OutApiKey = ApiKey;
	OutVideoId = VideoId;
	OutAdminUserIds = AdminUserIds;
	return true;
}
