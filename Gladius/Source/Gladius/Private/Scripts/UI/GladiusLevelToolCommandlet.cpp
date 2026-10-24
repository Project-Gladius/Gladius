#include "Scripts/UI/GladiusLevelToolCommandlet.h"

#if WITH_EDITOR
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGladiusLevelTool, Log, All);

UGladiusLevelToolCommandlet::UGladiusLevelToolCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace GladiusLevelTool
{
	bool ParseVector(const FString& Text, FVector& Out)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3)
		{
			return false;
		}
		Out = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));
		return true;
	}

	bool ParseRotator(const FString& Text, FRotator& Out)
	{
		FVector V;
		if (!ParseVector(Text, V))
		{
			return false;
		}
		Out = FRotator(V.X, V.Y, V.Z); //Pitch, Yaw, Roll
		return true;
	}

	AActor* FindActorByLabel(UWorld* World, const FString& Label)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase) || It->GetName().Equals(Label, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	void DumpActors(UWorld* World)
	{
		UE_LOG(LogGladiusLevelTool, Display, TEXT("==== ACTORS BEGIN ===="));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			FVector Origin, Extent;
			Actor->GetActorBounds(false, Origin, Extent);
			const FVector Loc = Actor->GetActorLocation();
			const FRotator Rot = Actor->GetActorRotation();
			const FVector Scale = Actor->GetActorScale3D();

			FString Extra;
			TArray<UStaticMeshComponent*> Meshes;
			Actor->GetComponents(Meshes);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh && Mesh->GetStaticMesh())
				{
					Extra += FString::Printf(TEXT(" mesh=%s"), *Mesh->GetStaticMesh()->GetName());
					if (UMaterialInterface* Material = Mesh->GetMaterial(0))
					{
						Extra += FString::Printf(TEXT(" mat=%s"), *Material->GetName());
					}
					break;
				}
			}
			if (const ACameraActor* Camera = Cast<ACameraActor>(Actor))
			{
				Extra += FString::Printf(TEXT(" autoActivate=%d fov=%.1f"), (int32)Camera->GetAutoActivatePlayerIndex(),
					Camera->GetCameraComponent() ? Camera->GetCameraComponent()->FieldOfView : 0.f);
			}

			UE_LOG(LogGladiusLevelTool, Display, TEXT("ACTOR|%s|%s|loc=%.0f,%.0f,%.0f|rot=%.1f,%.1f,%.1f|scale=%.2f,%.2f,%.2f|boundsOrigin=%.0f,%.0f,%.0f|boundsExtent=%.0f,%.0f,%.0f|%s"),
				*Actor->GetActorLabel(), *Actor->GetClass()->GetName(),
				Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw, Rot.Roll, Scale.X, Scale.Y, Scale.Z,
				Origin.X, Origin.Y, Origin.Z, Extent.X, Extent.Y, Extent.Z, *Extra);
		}
		UE_LOG(LogGladiusLevelTool, Display, TEXT("==== ACTORS END ===="));
	}
}
#endif

int32 UGladiusLevelToolCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace GladiusLevelTool;

	FString MapPath;
	if (!FParse::Value(*Params, TEXT("map="), MapPath))
	{
		UE_LOG(LogGladiusLevelTool, Error, TEXT("-map=/Game/... 가 필요합니다"));
		return 1;
	}

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!World)
	{
		UE_LOG(LogGladiusLevelTool, Error, TEXT("레벨을 열 수 없습니다: %s"), *MapPath);
		return 1;
	}

	bool bModified = false;
	int32 Errors = 0;

	//액터 추가
	FString SpawnList;
	if (FParse::Value(*Params, TEXT("spawn="), SpawnList, false))
	{
		TArray<FString> Entries;
		SpawnList.ParseIntoArray(Entries, TEXT(";"));
		for (const FString& Entry : Entries)
		{
			TArray<FString> Parts;
			Entry.ParseIntoArray(Parts, TEXT("|"));
			FVector Loc; FRotator Rot;
			UClass* Class = Parts.Num() == 4 ? LoadClass<AActor>(nullptr, *Parts[0]) : nullptr;
			if (!Class || !ParseVector(Parts[2], Loc) || !ParseRotator(Parts[3], Rot))
			{
				UE_LOG(LogGladiusLevelTool, Error, TEXT("spawn 형식 오류: %s"), *Entry);
				++Errors;
				continue;
			}
			if (FindActorByLabel(World, Parts[1]))
			{
				UE_LOG(LogGladiusLevelTool, Display, TEXT("이미 있음, 추가 안 함: %s"), *Parts[1]);
				continue;
			}
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AActor* Spawned = World->SpawnActor<AActor>(Class, Loc, Rot, SpawnParams))
			{
				Spawned->SetActorLabel(Parts[1]);
				bModified = true;
				UE_LOG(LogGladiusLevelTool, Display, TEXT("추가: %s (%s)"), *Parts[1], *Class->GetName());
			}
		}
	}

	//액터 이동
	FString SetList;
	if (FParse::Value(*Params, TEXT("set="), SetList, false))
	{
		TArray<FString> Entries;
		SetList.ParseIntoArray(Entries, TEXT(";"));
		for (const FString& Entry : Entries)
		{
			TArray<FString> Parts;
			Entry.ParseIntoArray(Parts, TEXT("|"));
			FVector Loc; FRotator Rot; FVector Scale;
			AActor* Actor = (Parts.Num() == 3 || Parts.Num() == 4) ? FindActorByLabel(World, Parts[0]) : nullptr;
			if (!Actor || !ParseVector(Parts[1], Loc) || !ParseRotator(Parts[2], Rot)
				|| (Parts.Num() == 4 && !ParseVector(Parts[3], Scale)))
			{
				UE_LOG(LogGladiusLevelTool, Error, TEXT("set 대상/형식 오류: %s"), *Entry);
				++Errors;
				continue;
			}
			Actor->Modify();
			Actor->SetActorLocationAndRotation(Loc, Rot);
			if (Parts.Num() == 4)
			{
				Actor->SetActorScale3D(Scale); //4번째 값(선택): 크기
			}
			bModified = true;
			UE_LOG(LogGladiusLevelTool, Display, TEXT("이동: %s -> %s / %s"), *Parts[0], *Loc.ToString(), *Rot.ToString());
		}
	}

	//플레이어 카메라 지정: 해당 카메라만 Player 0 자동 활성화
	FString ViewCamera;
	if (FParse::Value(*Params, TEXT("viewcamera="), ViewCamera))
	{
		ACameraActor* Target = Cast<ACameraActor>(FindActorByLabel(World, ViewCamera));
		if (!Target)
		{
			UE_LOG(LogGladiusLevelTool, Error, TEXT("카메라를 찾을 수 없습니다: %s"), *ViewCamera);
			++Errors;
		}
		else
		{
			//AutoActivateForPlayer는 private이라 리플렉션으로 설정 (에디터 디테일 창에서 바꾸는 것과 같음)
			FProperty* AutoActivate = ACameraActor::StaticClass()->FindPropertyByName(TEXT("AutoActivateForPlayer"));
			for (TActorIterator<ACameraActor> It(World); It; ++It)
			{
				if (FByteProperty* ByteProperty = CastField<FByteProperty>(AutoActivate))
				{
					It->Modify();
					ByteProperty->SetPropertyValue_InContainer(*It,
						static_cast<uint8>(*It == Target ? EAutoReceiveInput::Player0 : EAutoReceiveInput::Disabled));
				}
			}
			bModified = true;
			UE_LOG(LogGladiusLevelTool, Display, TEXT("플레이어 카메라: %s"), *ViewCamera);
		}
	}

	if (FParse::Param(*Params, TEXT("dump")))
	{
		DumpActors(World);
	}

	if (bModified && Errors == 0)
	{
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPath))
		{
			UE_LOG(LogGladiusLevelTool, Error, TEXT("레벨 저장 실패: %s"), *MapPath);
			return 1;
		}
		UE_LOG(LogGladiusLevelTool, Display, TEXT("레벨 저장: %s"), *MapPath);
	}
	else if (bModified)
	{
		UE_LOG(LogGladiusLevelTool, Error, TEXT("오류가 있어 저장하지 않았습니다"));
	}

	return Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
