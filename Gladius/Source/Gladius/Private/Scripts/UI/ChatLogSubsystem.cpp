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

void UChatLogSubsystem::AddTempleEnhancement(const FString& UserId, const FString& DisplayName, int32 Tier)
{
	if (Tier < 1 || Tier > 4) return;
	static const TCHAR* EffectNames[] = { TEXT("체력 회복"), TEXT("공격력 증가"),
		TEXT("치명타 확률 증가"), TEXT("치명타 피해량 증가") };
	FChatLogEntry Entry;
	Entry.UserId = UserId;
	Entry.DisplayName = TEXT("신전");
	Entry.Message = FString::Printf(TEXT("%s: 강화 %d 적용 (%s)"),
		DisplayName.IsEmpty() ? *UserId : *DisplayName, Tier, EffectNames[Tier - 1]);
	Entry.Speaker = EChatLogSpeaker::Temple;
	Entry.EnhancementTier = Tier;
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
