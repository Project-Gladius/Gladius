#include "Scripts/UI/SessionLogSubsystem.h"
#include "HAL/PlatformTime.h"

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
 QueueMessage(MoveTemp(Entry));
}

void USessionLogSubsystem::AddMessage(const FString& Message, ESessionLogEvent Event)
{
 FSessionLogEntry Entry; Entry.Message=Message; Entry.Event=Event;
 QueueMessage(MoveTemp(Entry));
}

void USessionLogSubsystem::AddNotice(const FString& Message)
{
 FSessionLogEntry Entry; Entry.Message=Message; Entry.Event=ESessionLogEvent::Admin; Entry.bIsNotice=true;
 QueueMessage(MoveTemp(Entry));
}

double USessionLogSubsystem::PresentationTime() const
{
 return PresentationTimeOverride>=0.0 ? PresentationTimeOverride : FPlatformTime::Seconds();
}

void USessionLogSubsystem::StoreEntry(FSessionLogEntry& Entry)
{
 Entry.Sequence=NextSequence++;
 Entries.Add(Entry);
 const int32 Overflow=Entries.Num()-FMath::Max(1,MaxEntries);
 if (Overflow>0) Entries.RemoveAt(0,Overflow);
}

void USessionLogSubsystem::QueueMessage(FSessionLogEntry Entry)
{
 AdvancePresentation();
 StoreEntry(Entry);
 PendingMessages.Add(Entry);
 const int32 Overflow=PendingMessages.Num()-FMath::Max(1,MaxEntries);
 if (Overflow>0) PendingMessages.RemoveAt(0,Overflow);
 AdvancePresentation();
 OnEntryAdded.Broadcast(Entry);
}

void USessionLogSubsystem::AdvancePresentation()
{
 const double Now=PresentationTime();
 const double Delta=LastPresentationTime>=0.0 ? FMath::Max(0.0,Now-LastPresentationTime) : 0.0;
 LastPresentationTime=Now;
 // Maintenance preempts the ticker and pauses, rather than expires, the ordinary message.
 if (!ActiveMaintenance.IsEmpty()) return;
 if (bHasActiveMessage)
 {
  RemainingSeconds-=Delta;
  if (RemainingSeconds<=0.0) bHasActiveMessage=false;
 }
 if (!bHasActiveMessage && !PendingMessages.IsEmpty())
 {
  ActiveMessage=PendingMessages[0]; PendingMessages.RemoveAt(0);
  RemainingSeconds=20.0; bHasActiveMessage=true;
 }
}

void USessionLogSubsystem::AddMaintenanceMessage(const FString& Scope, bool bEnabled, const FString& Message, ESessionLogEvent Event)
{
 // Account for elapsed ordinary display time before changing the maintenance state.
 AdvancePresentation();
 FSessionLogEntry Entry; Entry.Message=Message; Entry.Event=Event;
 if (bEnabled) Entry.MaintenanceScope=Scope;
 StoreEntry(Entry);
 if (bEnabled) ActiveMaintenance.Add(Scope,Entry);
 else
 {
  ActiveMaintenance.Remove(Scope);
  PendingMessages.Add(Entry); // completion notices are ordinary 20-second messages
  const int32 Overflow=PendingMessages.Num()-FMath::Max(1,MaxEntries);
  if (Overflow>0) PendingMessages.RemoveAt(0,Overflow);
 }
 AdvancePresentation();
 OnEntryAdded.Broadcast(Entry);
}

bool USessionLogSubsystem::GetPresentedEntry(FSessionLogEntry& OutEntry)
{
 AdvancePresentation();
 if (!ActiveMaintenance.IsEmpty())
 {
  const FSessionLogEntry* Latest=nullptr;
  TArray<FString> Scopes;
  bool bUrgent=false;
  for (const auto& Pair:ActiveMaintenance)
  {
   Scopes.Add(Pair.Key);
   bUrgent |= Pair.Value.Event==ESessionLogEvent::AdminUrgent;
   if (!Latest || Pair.Value.Sequence>Latest->Sequence) Latest=&Pair.Value;
  }
  OutEntry=*Latest;
  if (bUrgent) OutEntry.Event=ESessionLogEvent::AdminUrgent;
  if (Scopes.Num()>1) { Scopes.Sort(); OutEntry.Message+=TEXT(" (진행 중인 점검: ")+FString::Join(Scopes,TEXT(", "))+TEXT(")"); }
  return true;
 }
 if (bHasActiveMessage)
 {
  // Do not replace the fallback with a notice that is still hidden behind maintenance.
  if (ActiveMessage.bIsNotice) { LatestNotice=ActiveMessage; bHasLatestNotice=true; }
  OutEntry=ActiveMessage; return true;
 }
 if (bHasLatestNotice) { OutEntry=LatestNotice; return true; }
 return false;
}
