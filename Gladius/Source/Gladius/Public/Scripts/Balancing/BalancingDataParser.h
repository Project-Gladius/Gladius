#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/NoExportTypes.h"
#include "BalancingDataParser.generated.h"

// 레포 루트의 Balancing/Data 폴더에 있는 밸런싱 데이터(abilities.json, bosses.json, enhancements.json)를
// 불러와 FJsonObject 형태로 보관해두는 클래스. 실제 게임 데이터 구조체(USTRUCT)로의 변환은
// 이 클래스가 들고 있는 Json 트리를 참조해 추후 구현 예정
UCLASS()
class GLADIUS_API UBalancingDataParser : public UObject
{
	GENERATED_BODY()

public:
	// abilities.json / bosses.json / enhancements.json을 모두 불러옴. 하나라도 실패하면 false를 반환하지만
	// 성공한 나머지 데이터는 계속 보관하므로, 실패 원인은 출력 로그(UE_LOG)에서 확인할 것
	bool LoadAll();

	bool LoadAbilities();
	bool LoadBosses();
	bool LoadEnhancements();

	const TSharedPtr<FJsonObject>& GetAbilitiesJson() const { return AbilitiesJson; }
	const TSharedPtr<FJsonObject>& GetBossesJson() const { return BossesJson; }
	const TSharedPtr<FJsonObject>& GetEnhancementsJson() const { return EnhancementsJson; }

private:
	// Gladius.uproject가 위치한 프로젝트 폴더 기준 한 단계 위 -> Balancing/Data 절대경로를 반환
	static FString GetDataDirectory();

	// DataDirectory 안의 FileName 파일을 읽어 OutJsonObject로 역직렬화. 파일 없음/파싱 실패 시 false
	static bool LoadJsonFile(const FString& FileName, TSharedPtr<FJsonObject>& OutJsonObject);

	TSharedPtr<FJsonObject> AbilitiesJson;
	TSharedPtr<FJsonObject> BossesJson;
	TSharedPtr<FJsonObject> EnhancementsJson;
};
