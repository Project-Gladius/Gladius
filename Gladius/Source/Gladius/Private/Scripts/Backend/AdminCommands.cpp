#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/UI/SessionLogSubsystem.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Engine/GameInstance.h"

bool UAdminSubsystem::IsAdminCommand(ECommandType Type)
{
 return Type == ECommandType::Notice || Type == ECommandType::Block || Type == ECommandType::Unblock
  || Type == ECommandType::TravelToCombat || Type == ECommandType::Maintenance
  || Type == ECommandType::Patch || Type == ECommandType::EndMaintenance || Type == ECommandType::EventBuff;
}
bool UAdminSubsystem::HandleAdminCommand(const FParsedCommand& C, ABattleManager* Battle)
{
 if (!IsAdminCommand(C.CommandType)) return false;
 const bool bExhibitCommand = IsExhibit(C.UserId)
  && (C.CommandType == ECommandType::TravelToCombat || C.CommandType == ECommandType::EventBuff);
 if (!IsAdmin(C.UserId) && !bExhibitCommand) return true;
 auto* Log = GetGameInstance()->GetSubsystem<USessionLogSubsystem>();
 const FString Name = IsAdmin(C.UserId) ? GetAdminName(C.UserId) : (C.Username.IsEmpty() ? GetAdminName(C.UserId) : C.Username);
 auto Emit = [Log](const FString& Text, ESessionLogEvent Style = ESessionLogEvent::Admin) { Log->AddMessage(Text, Style); };
 auto Error = [&]() { Emit(TEXT("[운영자] 명령을 처리하지 못했습니다. 대상 닉네임, 인자 및 저장 상태를 확인하세요."), ESessionLogEvent::AdminUrgent); };
 const FString Arg = C.Argument.TrimStartAndEnd();
 TArray<FString> Words; Arg.ParseIntoArrayWS(Words);
 auto TargetError = [&](const FString& Target, bool bAmbiguous)
 {
  Emit(bAmbiguous
   ? FString::Printf(TEXT("[운영자] '%s' 닉네임이 여러 계정과 일치합니다. 정확한 대상 ID를 입력하세요."), *Target)
   : FString::Printf(TEXT("[운영자] '%s' 대상을 찾지 못했습니다. 표시 닉네임 또는 정확한 대상 ID를 확인하세요."), *Target), ESessionLogEvent::AdminUrgent);
 };
 auto Resolve = [&](const FString& Target, FString& Id, FString& DisplayName)
 {
  bool bAmbiguous=false;
  if (ResolveUserTarget(Target,Battle,Id,DisplayName,bAmbiguous)) return true;
  TargetError(Target,bAmbiguous); return false;
 };
 switch (C.CommandType)
 {
 case ECommandType::Patch:
  ExecutePatch(Battle);
  return true;
 case ECommandType::Notice:
  if (!Arg.IsEmpty()) Log->AddNotice(FString::Printf(TEXT("[%s] %s"), *Name, *Arg));
  return true;
 case ECommandType::Block:
 {
  if (Arg.IsEmpty()) { Error(); return true; }
  FString TargetId,TargetName;
  bool bAmbiguous=false,bTimed=false;
  double Hours=0;
  // Match the whole nickname first, so a name ending in a number remains valid.
  if (!ResolveUserTarget(Arg,Battle,TargetId,TargetName,bAmbiguous))
  {
   if (bAmbiguous) { TargetError(Arg,true); return true; }
   if (Words.Num()<2 || !LexTryParseString(Hours,*Words.Last())) { TargetError(Arg,false); return true; }
   const FString Nickname=Arg.Left(Arg.Len()-Words.Last().Len()).TrimStartAndEnd();
   if (!Resolve(Nickname,TargetId,TargetName)) return true;
   bTimed=true;
  }
  if (IsReservedAdmin(TargetId)) return true;
  const FDateTime Now=FDateTime::UtcNow();
  FDateTime Until;
  if (!bTimed)
   Until=FDateTime(Now.GetYear()+1,Now.GetMonth(),FMath::Min(Now.GetDay(),FDateTime::DaysInMonth(Now.GetYear()+1,Now.GetMonth())),Now.GetHour(),Now.GetMinute(),Now.GetSecond());
  else
  {
   if (!FMath::IsFinite(Hours) || Hours<=0 || Hours>=(FDateTime::MaxValue()-Now).GetTotalHours()) { Error(); return true; }
   Until=Now+FTimespan::FromHours(Hours);
  }
  if (!Block(TargetId,Until)) { Error(); return true; }
  const FDateTime LocalUntil=Until+(FDateTime::Now()-FDateTime::UtcNow());
  Emit(!bTimed ? FString::Printf(TEXT("[%s] %s이 차단되었습니다. 차단 기한 : 1년."),*Name,*TargetName)
   : FString::Printf(TEXT("[%s] %s이 차단되었습니다. 차단 시한 : %s."),*Name,*TargetName,*LocalUntil.ToString(TEXT("%Y-%m-%d %H:%M:%S"))),ESessionLogEvent::AdminUrgent);
  return true;
 }
 case ECommandType::Unblock:
 {
  FString TargetId,TargetName;
  if (!Resolve(Arg,TargetId,TargetName)) return true;
  if (IsReservedAdmin(TargetId)) return true;
  if (!Unblock(TargetId)) { Error(); return true; }
  Emit(FString::Printf(TEXT("[%s] %s의 차단이 해제되었습니다."),*Name,*TargetName),ESessionLogEvent::AdminUrgent);
  return true;
 }
 case ECommandType::TravelToCombat:
 {
  auto* Schedule = GetGameInstance()->GetSubsystem<UBattleScheduleSubsystem>();
  const FDateTime Now = FDateTime::Now();
  if (IsExhibit(C.UserId) && !Arg.IsEmpty()) { Error(); return true; }
  if (Arg == TEXT("취소"))
  {
   FDateTime Cancelled, Next;
   if (!Schedule->CancelNext(Now, Cancelled, Next)) { Emit(TEXT("[공지] 취소할 전투 예약이 없습니다.")); return true; }
   const FString Prefix = FString::Printf(TEXT("[공지] %02d시 %02d분에 예약된 전투가 취소되었습니다. "), Cancelled.GetHour(), Cancelled.GetMinute());
   Emit(Prefix + (Next == FDateTime::MaxValue() ? FString(TEXT("다음 전투 예약이 없습니다."))
    : FString::Printf(TEXT("다음 전투는 %02d시 %02d분입니다."), Next.GetHour(), Next.GetMinute())));
   return true;
  }
  if (Battle && Battle->IsBattleRunning()) { Emit(TEXT("[공지] 이미 전투 중입니다.")); return true; }
  FDateTime At = Now + FTimespan::FromSeconds(5);
  if (!Arg.IsEmpty())
  {
   if (Arg.Len()!=5 || Arg[2]!=TCHAR(':') || !Arg.Left(2).IsNumeric() || !Arg.Right(2).IsNumeric()
    || FCString::Atoi(*Arg.Left(2))>23 || FCString::Atoi(*Arg.Right(2))>59) { Error(); return true; }
   At = Now.GetDate() + FTimespan::FromHours(FCString::Atoi(*Arg.Left(2))) + FTimespan::FromMinutes(FCString::Atoi(*Arg.Right(2)));
   if (At <= Now) At += FTimespan::FromDays(1);
  }
  if (!Schedule->AddManualStart(At)) { Error(); return true; }
  Emit(Arg.IsEmpty() ? TEXT("[공지] 5초 후 전투를 시작합니다.") : FString::Printf(TEXT("[공지] %s에 전투를 예약했습니다."), *At.ToString(TEXT("%H:%M"))));
  return true;
 }
 case ECommandType::Maintenance:
 case ECommandType::EndMaintenance:
 {
  const bool Enabled = C.CommandType == ECommandType::Maintenance;
  if (!SetMaintenance(Arg, Enabled)) { Error(); return true; }
  // SetMaintenance updates the matching persistent notification and emits one history entry.
  return true;
 }
 case ECommandType::EventBuff:
 {
  FString TargetId,TargetName;
  if (!Resolve(Arg,TargetId,TargetName)) return true;
  if (HasEventBuff(TargetId)) return true;
  if (!GrantEvent(TargetId)) { Error(); return true; }
  if (Battle) Battle->RefreshEventBuff(TargetId);
  Emit(FString::Printf(TEXT("[%s] %s이 전시장에 방문하여 강력한 버프를 얻습니다! 최대 체력 +1, 공격력 +5, 치명타 확률·피해량 +10%%p! 사망 후에도 유지됩니다."),*Name,*TargetName),ESessionLogEvent::Event);
  return true;
 }
 default: return false;
 }
}
