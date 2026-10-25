#include "Scripts/UI/SessionLogSubsystem.h"

void USessionLogSubsystem::AddEvent(const FString& UserId, const FString& DisplayName, ESessionLogEvent Event)
{
 FSessionLogEntry Entry;
 Entry.UserId = UserId;
 Entry.Event = Event;
 const FString Name = DisplayName.IsEmpty() ? UserId : DisplayName;
 switch (Event)
 {
 case ESessionLogEvent::Login: Entry.Message = Name + TEXT(" 님이 로그인하셨습니다"); break;
 case ESessionLogEvent::Logout: Entry.Message = Name + TEXT(" 님이 로그아웃하셨습니다"); break;
 case ESessionLogEvent::LoginFailed: Entry.Message = Name + TEXT("님의 로그인이 정상 처리되지 못했습니다"); break;
 case ESessionLogEvent::InactivityLogout: Entry.Message = Name + TEXT(" 님의 입력이 없어 자동 로그아웃되었습니다"); break;
 default: return;
 }
 Entries.Add(Entry);
 const int32 Overflow = Entries.Num() - FMath::Max(1, MaxEntries);
 if (Overflow > 0) Entries.RemoveAt(0, Overflow);
 OnEntryAdded.Broadcast(Entry);
}

void USessionLogSubsystem::AddMessage(const FString& Message, ESessionLogEvent Event)
{
 FSessionLogEntry Entry; Entry.Message = Message; Entry.Event = Event;
 Entries.Add(Entry);
 const int32 Overflow = Entries.Num() - FMath::Max(1, MaxEntries);
 if (Overflow > 0) Entries.RemoveAt(0, Overflow);
 OnEntryAdded.Broadcast(Entry);
}
