#include "Scripts/Balancing/BalancingDataParser.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

FString UBalancingDataParser::GetDataDirectory()
{
	// Gladius.uproject가 위치한 폴더(ProjectDir) 기준 한 단계 위 폴더의 Balancing/Data
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Balancing/Data/"));
}

bool UBalancingDataParser::LoadJsonFile(const FString& FileName, TSharedPtr<FJsonObject>& OutJsonObject)
{
	const FString FilePath = GetDataDirectory() / FileName;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[BalancingDataParser] 파일을 찾을 수 없습니다: %s"), *FilePath);
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, OutJsonObject) || !OutJsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BalancingDataParser] JSON 파싱에 실패했습니다: %s"), *FilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[BalancingDataParser] 데이터 로드 완료: %s"), *FilePath);
	return true;
}

bool UBalancingDataParser::LoadAbilities()
{
	return LoadJsonFile(TEXT("abilities.json"), AbilitiesJson);
}

bool UBalancingDataParser::LoadBosses()
{
	return LoadJsonFile(TEXT("bosses.json"), BossesJson);
}

bool UBalancingDataParser::LoadEnhancements()
{
	return LoadJsonFile(TEXT("enhancements.json"), EnhancementsJson);
}

bool UBalancingDataParser::LoadAll()
{
	const bool bAbilitiesLoaded = LoadAbilities();
	const bool bBossesLoaded = LoadBosses();
	const bool bEnhancementsLoaded = LoadEnhancements();

	return bAbilitiesLoaded && bBossesLoaded && bEnhancementsLoaded;
}
