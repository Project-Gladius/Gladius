#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Battle/BattleManager.h"
#include "Scripts/UI/SessionLogSubsystem.h"
#include "Scripts/Time/BattleScheduleSubsystem.h"
#include "Engine/GameInstance.h"

bool UAdminSubsystem::IsAdminCommand(ECommandType Type)
{
 return Type == ECommandType::Notice || Type == ECommandType::Block || Type == ECommandType::Unblock
  || Type == ECommandType::TravelToCombat || Type == ECommandType::Maintenance
  || Type == ECommandType::EndMaintenance || Type == ECommandType::EventBuff;
}
bool UAdminSubsystem::HandleAdminCommand(const FParsedCommand& C, ABattleManager* Battle)
{
 if (!IsAdminCommand(C.CommandType)) return false;
 if (!IsAdmin(C.UserId)) return true;
 auto* Log = GetGameInstance()->GetSubsystem<USessionLogSubsystem>();
 const FString Name = GetAdminName(C.UserId);
 auto Emit = [Log](const FString& Text, ESessionLogEvent Style = ESessionLogEvent::Admin) { Log->AddMessage(Text, Style); };
 auto Error = [&]() { Emit(TEXT("[운영자] 명령을 처리하지 못했습니다. 대상 ID, 인자 및 저장 상태를 확인하세요."), ESessionLogEvent::AdminUrgent); };
 const FString Arg = C.Argument.TrimStartAndEnd();
 TArray<FString> Words; Arg.ParseIntoArrayWS(Words);
 switch (C.CommandType)
 {
 case ECommandType::Notice:
  if (!Arg.IsEmpty()) Emit(FString::Printf(TEXT("[%s] %s"), *Name, *Arg));
  return true;
 case ECommandType::Block:
 {
  if (Words.Num() < 1 || Words.Num() > 2) { Error(); return true; }
  if (IsReservedAdmin(Words[0])) return true;
  const FDateTime Now = FDateTime::UtcNow();
  FDateTime Until;
  if (Words.Num() == 1)
   Until = FDateTime(Now.GetYear()+1, Now.GetMonth(), FMath::Min(Now.GetDay(), FDateTime::DaysInMonth(Now.GetYear()+1, Now.GetMonth())), Now.GetHour(), Now.GetMinute(), Now.GetSecond());
  else
  {
   double Hours = 0;
   if (!LexTryParseString(Hours, *Words[1]) || !FMath::IsFinite(Hours) || Hours <= 0
    || Hours >= (FDateTime::MaxValue() - Now).GetTotalHours()) { Error(); return true; }
   Until = Now + FTimespan::FromHours(Hours);
  }
  if (!Block(Words[0], Until)) { Error(); return true; }
  const FDateTime LocalUntil = Until + (FDateTime::Now() - FDateTime::UtcNow());
  Emit(Words.Num() == 1 ? FString::Printf(TEXT("[%s] %s이 차단되었습니다. 차단 기한 : 1년."), *Name, *Words[0])
   : FString::Printf(TEXT("[%s] %s이 차단되었습니다. 차단 시한 : %s."), *Name, *Words[0], *LocalUntil.ToString(TEXT("%Y-%m-%d %H:%M:%S"))), ESessionLogEvent::AdminUrgent);
  return true;
 }
 case ECommandType::Unblock:
  if (Words.Num() != 1) { Error(); return true; }
  if (IsReservedAdmin(Words[0])) return true;
  if (!Unblock(Words[0])) { Error(); return true; }
  Emit(FString::Printf(TEXT("[%s] %s의 차단이 해제되었습니다."), *Name, *Words[0]), ESessionLogEvent::AdminUrgent);
  return true;
 case ECommandType::TravelToCombat:
 {
  auto* Schedule = GetGameInstance()->GetSubsystem<UBattleScheduleSubsystem>();
  const FDateTime Now = FDateTime::Now();
  if (Arg == TEXT("취소"))
  {
   FDateTime Cancelled, Next;
   if (!Schedule->CancelNext(Now, Cancelled, Next)) { Emit(TEXT("[공지] 취소할 전투 예약이 없습니다.")); return true; }
   const FString Prefix = FString::Printf(TEXT("[공지] %02d시 %02d분에 예약된 전투가 취소되었습니다. "), Cancelled.GetHour(), Cancelled.GetMinute());
   Emit(Prefix + (Next == FDateTime::MaxValue() ? FString(TEXT("다음 전투 예약이 없습니다."))
    : FString::Printf(TEXT("다음 전투는 %02d시 %02d분입니다."), Next.GetHour(), Next.GetMinute())));
   return true;
  }
  Emit(TEXT("[공지] 전투는 외부 시간표(BattleSchedule.csv)에 따라서만 시작됩니다."));
  return true;
 }
 case ECommandType::Maintenance:
 case ECommandType::EndMaintenance:
 {
  const bool Enabled = C.CommandType == ECommandType::Maintenance;
  if (!SetMaintenance(Arg, Enabled)) { Error(); return true; }
  if (Arg == TEXT("전체")) Emit(Enabled ? TEXT("[긴급점검] 게임 점검 중! 입력이 비활성화됩니다.") : TEXT("[점검완료] 게임 입력이 재활성화됩니다."), Enabled ? ESessionLogEvent::AdminUrgent : ESessionLogEvent::Admin);
  else if (Arg == TEXT("AI")) Emit(Enabled ? TEXT("[긴급점검] 여신 AI 기능이 비활성화됩니다.") : TEXT("[점검완료] 여신 AI 기능이 재활성화됩니다."));
  else Emit((Enabled ? FString::Printf(TEXT("[긴급점검] %s 조작 기능이 비활성화됩니다."), *Arg) : FString::Printf(TEXT("[점검완료] %s 조작 기능이 재활성화됩니다."), *Arg)), Enabled ? ESessionLogEvent::AdminUrgent : ESessionLogEvent::Admin);
  return true;
 }
 case ECommandType::EventBuff:
 {
  if (Words.Num() != 1) { Error(); return true; }
  if (HasEventBuff(Words[0])) return true;
  if (!GrantEvent(Words[0])) { Error(); return true; }
  if (Battle) Battle->RefreshEventBuff(CanonicalId(Words[0]));
  Emit(FString::Printf(TEXT("[%s] %s이 전시장에 방문하여 강력한 버프를 얻습니다! R777로 오셔서 체력 버프를 받아가세요!"), *Name, *Words[0]), ESessionLogEvent::Event);
  return true;
 }
 default: return false;
 }
}
