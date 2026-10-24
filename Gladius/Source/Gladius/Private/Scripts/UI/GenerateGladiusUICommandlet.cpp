#include "Scripts/UI/GenerateGladiusUICommandlet.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardRowWidget.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGladiusUIGen, Log, All);

UGenerateGladiusUICommandlet::UGenerateGladiusUICommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace GladiusUIGeneration
{
	enum class EResult : uint8 { Created, Skipped, Failed };

	//PackagePath에 ParentClass를 부모로 하는 WBP를 만들고 Build로 Designer 트리를 채운 뒤 컴파일·저장
	EResult CreateWidgetAsset(const TCHAR* PackagePath, UClass* ParentClass, TFunctionRef<void(UWidgetTree*)> Build)
	{
		if (FPackageName::DoesPackageExist(PackagePath))
		{
			UE_LOG(LogGladiusUIGen, Display, TEXT("이미 있음, 건너뜀: %s"), PackagePath);
			return EResult::Skipped;
		}

		UPackage* Package = CreatePackage(PackagePath);
		Package->FullyLoad();

		const FName AssetName(*FPackageName::GetShortName(PackagePath));
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, AssetName, BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			UE_LOG(LogGladiusUIGen, Error, TEXT("WBP 생성 실패: %s"), PackagePath);
			return EResult::Failed;
		}

		Build(Blueprint->WidgetTree);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		FAssetRegistryModule::AssetCreated(Blueprint);
		Package->MarkPackageDirty();

		//다음 WBP가 이 WBP를 클래스로 찾을 수 있도록 바로 저장
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Package }, false))
		{
			UE_LOG(LogGladiusUIGen, Error, TEXT("WBP 저장 실패: %s"), PackagePath);
			return EResult::Failed;
		}

		UE_LOG(LogGladiusUIGen, Display, TEXT("생성: %s"), PackagePath);
		return EResult::Created;
	}
}
#endif

int32 UGenerateGladiusUICommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace GladiusUIGeneration;
	namespace Paths = GladiusUI::AssetPaths;

	int32 Created = 0;
	int32 Failed = 0;
	auto Count = [&](EResult Result)
	{
		Created += Result == EResult::Created ? 1 : 0;
		Failed += Result == EResult::Failed ? 1 : 0;
	};

	//줄 -> 보드 -> 로테이터 순서: 보드는 줄 WBP를, 로테이터는 보드 WBP를 기본 클래스로 쓴다
	Count(CreateWidgetAsset(Paths::LeaderboardRow, ULeaderboardRowWidget::StaticClass(),
		[](UWidgetTree* Tree) { GetDefault<ULeaderboardRowWidget>()->BuildDefaultLayout(Tree); }));
	Count(CreateWidgetAsset(Paths::LeaderboardBoard, ULeaderboardBoardWidget::StaticClass(),
		[](UWidgetTree* Tree) { GetDefault<ULeaderboardBoardWidget>()->BuildDefaultLayout(Tree); }));
	Count(CreateWidgetAsset(Paths::LeaderboardRotator, ULeaderboardRotatorWidget::StaticClass(),
		[](UWidgetTree* Tree) { GetDefault<ULeaderboardRotatorWidget>()->BuildDefaultLayout(Tree); }));
	Count(CreateWidgetAsset(Paths::TopHud, UBattleTopHudWidget::StaticClass(),
		[](UWidgetTree* Tree) { GetDefault<UBattleTopHudWidget>()->BuildDefaultLayout(Tree); }));
	Count(CreateWidgetAsset(Paths::BossTelegraph, UBossTelegraphWidget::StaticClass(),
		[](UWidgetTree* Tree) { GetDefault<UBossTelegraphWidget>()->BuildDefaultLayout(Tree); }));

	UE_LOG(LogGladiusUIGen, Display, TEXT("전투 UI WBP 생성 완료: 새로 만든 것 %d개, 실패 %d개"), Created, Failed);
	return Failed > 0 ? 1 : 0;
#else
	return 1;
#endif
}
