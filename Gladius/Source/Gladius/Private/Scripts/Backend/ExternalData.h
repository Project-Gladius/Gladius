#pragma once
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GladiusExternalData
{
 inline FString Directory() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("ExternalData")); }
 inline FString File(const TCHAR* Name) { return Directory() / Name; }
 inline TSharedPtr<FJsonObject> Broadcast()
 {
  FString Json; TSharedPtr<FJsonObject> Object;
  if (FFileHelper::LoadFileToString(Json, *File(TEXT("BroadcastSettings.json"))))
   FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
  return Object;
 }
 inline TSharedPtr<FJsonObject> Connections()
 {
  FString Json; TSharedPtr<FJsonObject> Object;
  if (FFileHelper::LoadFileToString(Json, *File(TEXT("ConnectionSettings.json"))))
   FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
  bool Enabled = false;
  if (!Object || !Object->TryGetBoolField(TEXT("UseExternalConnectionSettings"), Enabled) || !Enabled) return nullptr;
  return Object;
 }
}
