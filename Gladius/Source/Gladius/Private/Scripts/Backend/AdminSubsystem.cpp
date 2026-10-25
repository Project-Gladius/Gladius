#include "Scripts/Backend/AdminSubsystem.h"
#include "Scripts/Temple/TempleCycleController.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpResponse.h"

void UAdminSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 StoragePath = FPaths::ProjectSavedDir() / TEXT("UserData.json");
 bTemporary = GIsAutomationTesting;
 if (bTemporary) StoragePath = FPaths::ProjectSavedDir() / TEXT("Automation") / (TEXT("UserData-") + FGuid::NewGuid().ToString() + TEXT(".json"));
 const FString Destination = StoragePath;
 LoadFromFile(IFileManager::Get().FileExists(*StoragePath) ? StoragePath : FPaths::ProjectContentDir() / TEXT("ExternalData/UserData.json"));
 StoragePath = Destination;
}
void UAdminSubsystem::Deinitialize()
{
 for (auto& Pair : HandleRequests) { Pair.Value->OnProcessRequestComplete().Unbind(); Pair.Value->CancelRequest(); }
 HandleRequests.Reset();
 if (bTemporary) IFileManager::Get().Delete(*StoragePath);
 Super::Deinitialize();
}
bool UAdminSubsystem::LoadFromFile(const FString& Path)
{
 StoragePath = Path;
 FString Json;
 FUserPermissionFile Loaded;
 bValid = false;
 TSharedPtr<FJsonObject> Root;
 if (!FFileHelper::LoadFileToString(Json, *Path)
  || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
  || !Root->HasTypedField<EJson::Array>(TEXT("users"))
  || !FJsonObjectConverter::JsonObjectToUStruct(Root.ToSharedRef(), &Loaded)) return false;
 TSet<FString> Ids;
 for (const auto& U : Loaded.Users)
 {
  if (U.UserId.IsEmpty() || Ids.Contains(U.UserId)
   || (U.Permission != TEXT("admin") && U.Permission != TEXT("user") && U.Permission != TEXT("blocked"))) return false;
  Ids.Add(U.UserId);
  if (!U.ChannelId.IsEmpty() && U.ChannelId != U.UserId)
  { if (Ids.Contains(U.ChannelId)) return false; Ids.Add(U.ChannelId); }
  FDateTime Time;
  if (!U.BlockTime.IsEmpty() && !FDateTime::ParseIso8601(*U.BlockTime, Time)) return false;
 }
 Data = MoveTemp(Loaded); bValid = true;
 return true;
}
int32 UAdminSubsystem::FindIndex(const FString& Id) const
{
 return Data.Users.IndexOfByPredicate([&](const FUserPermissionRecord& U)
 { return U.UserId == Id || (!U.ChannelId.IsEmpty() && U.ChannelId == Id); });
}
const FUserPermissionRecord* UAdminSubsystem::FindUser(const FString& Id) const
{ const int32 I = FindIndex(Id); return Data.Users.IsValidIndex(I) ? &Data.Users[I] : nullptr; }
bool UAdminSubsystem::IsReservedAdmin(const FString& Id) const
{ const auto* U = FindUser(Id); return bValid && U && U->Permission == TEXT("admin"); }
bool UAdminSubsystem::IsAdmin(const FString& Id) const
{
 const auto* U = FindUser(Id);
 return bValid && U && U->Permission == TEXT("admin") && !Id.StartsWith(TEXT("@")) && (U->UserId == Id || U->ChannelId == Id);
}
bool UAdminSubsystem::IsBlocked(const FString& Id, const FDateTime& Now) const
{
 if (!bValid) return true;
 if (IsAdmin(Id)) return false;
 const auto* U = FindUser(Id);
 if (!U || U->Permission != TEXT("blocked")) return false;
 FDateTime Until;
 return !U->BlockTime.IsEmpty() && (!FDateTime::ParseIso8601(*U->BlockTime, Until) || Until > Now);
}
bool UAdminSubsystem::CanInput(const FString& Id, EGameState State) const
{ return IsAdmin(Id) || (bValid && !IsBlocked(Id, FDateTime::UtcNow()) && !bAll && !(State == EGameState::Combat ? bBattle : bTemple)); }
bool UAdminSubsystem::HasEventBuff(const FString& Id) const
{ const auto* U = FindUser(Id); return bValid && U && U->EventBuff; }
FString UAdminSubsystem::GetAdminName(const FString& Id) const
{ const auto* U = FindUser(Id); return U && !U->AdminName.TrimStartAndEnd().IsEmpty() ? U->AdminName : TEXT("GM"); }
FString UAdminSubsystem::CanonicalId(const FString& Id) const
{ const auto* U = FindUser(Id); return U && !U->ChannelId.IsEmpty() ? U->ChannelId : Id; }
bool UAdminSubsystem::Commit(FUserPermissionFile Next)
{
 if (!bValid) return false;
 auto Root = MakeShared<FJsonObject>();
 TArray<TSharedPtr<FJsonValue>> Users;
 for (const auto& U : Next.Users)
 {
  auto Row = MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("userId"), U.UserId);
  Row->SetStringField(TEXT("channelId"), U.ChannelId);
  Row->SetStringField(TEXT("displayName"), U.DisplayName);
  Row->SetStringField(TEXT("permission"), U.Permission);
  Row->SetStringField(TEXT("adminName"), U.AdminName);
  if (!U.BlockTime.IsEmpty()) Row->SetStringField(TEXT("blockTime"), U.BlockTime);
  Row->SetBoolField(TEXT("eventBuff"), U.EventBuff);
  Users.Add(MakeShared<FJsonValueObject>(Row));
 }
 Root->SetArrayField(TEXT("users"), Users);
 FString Json; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(StoragePath), true);
 const FString Temp = StoragePath + TEXT(".tmp");
 if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
  || !IFileManager::Get().Move(*StoragePath, *Temp, true, true)) return false;
 Data = MoveTemp(Next); return true;
}
void UAdminSubsystem::RegisterUser(const FString& Id, const FString& Name)
{
 if (Id.IsEmpty() || !bValid) return;
 auto Next = Data; int32 I = FindIndex(Id);
 if (I == INDEX_NONE) { I = Next.Users.AddDefaulted(); Next.Users[I].UserId = Id; }
 else if (Next.Users[I].DisplayName == Name) return;
 Next.Users[I].DisplayName = Name; Commit(MoveTemp(Next));
}
bool UAdminSubsystem::Block(const FString& Id, const FDateTime& Until)
{
 if (Id.IsEmpty() || IsReservedAdmin(Id) || Until <= FDateTime::UtcNow()) return false;
 auto Next = Data; int32 I = FindIndex(Id);
 if (I == INDEX_NONE) { I = Next.Users.AddDefaulted(); Next.Users[I].UserId = Id; }
 Next.Users[I].Permission = TEXT("blocked"); Next.Users[I].BlockTime = Until.ToIso8601();
 return Commit(MoveTemp(Next));
}
bool UAdminSubsystem::Unblock(const FString& Id)
{
 auto Next = Data; const int32 I = FindIndex(Id);
 if (I == INDEX_NONE || IsReservedAdmin(Id)) return false;
 Next.Users[I].Permission = TEXT("user"); Next.Users[I].BlockTime.Reset();
 return Commit(MoveTemp(Next));
}
bool UAdminSubsystem::GrantEvent(const FString& Id)
{
 if (Id.IsEmpty() || HasEventBuff(Id)) return false;
 auto Next = Data; int32 I = FindIndex(Id);
 if (I == INDEX_NONE) { I = Next.Users.AddDefaulted(); Next.Users[I].UserId = Id; }
 Next.Users[I].EventBuff = true; return Commit(MoveTemp(Next));
}
bool UAdminSubsystem::BindVerifiedHandle(const FString& Handle, const FString& ChannelId)
{
 const int32 I = FindIndex(Handle);
 if (I == INDEX_NONE || !IsReservedAdmin(Handle) || !ChannelId.StartsWith(TEXT("UC")) || ChannelId.Len() != 24) return false;
 auto Next = Data; const int32 Existing = FindIndex(ChannelId);
 if (Existing != INDEX_NONE && Existing != I)
 {
  if (Next.Users[Existing].Permission == TEXT("admin")) return false;
  Next.Users[I].EventBuff |= Next.Users[Existing].EventBuff;
  Next.Users[I].DisplayName = Next.Users[Existing].DisplayName;
 }
 Next.Users[I].ChannelId = ChannelId;
 if (Existing != INDEX_NONE && Existing != I) Next.Users.RemoveAt(Existing);
 return Commit(MoveTemp(Next));
}
void UAdminSubsystem::ResolveAdminHandles(const FString& ApiKey)
{
 if (!bValid || ApiKey.IsEmpty() || GIsAutomationTesting) return;
 for (const auto& U : Data.Users)
 {
  if (U.Permission != TEXT("admin") || !U.UserId.StartsWith(TEXT("@")) || !U.ChannelId.IsEmpty() || HandleRequests.Contains(U.UserId)) continue;
  const FString Handle = U.UserId;
  auto Request = FHttpModule::Get().CreateRequest();
  Request->SetURL(TEXT("https://www.googleapis.com/youtube/v3/channels?part=id&forHandle=") + FGenericPlatformHttp::UrlEncode(Handle));
  Request->SetHeader(TEXT("X-Goog-Api-Key"), ApiKey);
  Request->SetVerb(TEXT("GET")); Request->SetTimeout(15);
  Request->OnProcessRequestComplete().BindWeakLambda(this, [this, Handle](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
  {
   HandleRequests.Remove(Handle);
   TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
   if (bSuccess && Res.IsValid() && Res->GetResponseCode() == 200
    && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Res->GetContentAsString()), Root) && Root.IsValid()
    && Root->TryGetArrayField(TEXT("items"), Items) && Items->Num() == 1)
   {
    const auto Row = (*Items)[0]->AsObject(); FString Id;
    if (Row.IsValid() && Row->TryGetStringField(TEXT("id"), Id) && BindVerifiedHandle(Handle, Id)) return;
   }
   UE_LOG(LogTemp, Warning, TEXT("Admin handle verification failed: %s. Privileged commands remain disabled for this handle."), *Handle);
  });
  HandleRequests.Add(Handle, Request);
  if (!Request->ProcessRequest()) HandleRequests.Remove(Handle);
 }
}
bool UAdminSubsystem::SetMaintenance(const FString& Scope, bool bEnabled)
{
 if (Scope == TEXT("전투")) bBattle = bEnabled;
 else if (Scope == TEXT("신전")) bTemple = bEnabled;
 else if (Scope == TEXT("전체")) bAll = bEnabled;
 else if (Scope == TEXT("AI"))
 {
  bAI = bEnabled;
  if (UWorld* World = GetGameInstance()->GetWorld())
   for (TActorIterator<ATempleCycleController> It(World); It; ++It) It->SetAIMaintenance(bEnabled);
 }
 else return false;
 return true;
}
