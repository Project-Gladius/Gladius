#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/PackageName.h"

class UTextBlock;

// 전투 UI 위젯들이 공통으로 쓰는 도우미
// - 기본 레이아웃을 WidgetTree에 만들 때 쓰는 함수 (런타임 C++ 단독 사용 + WBP 자동 생성 커맨드릿 공용)
// - 디자이너용 WBP(/Game/Blueprints/UI)가 있으면 그 클래스를 쓰도록 찾아주는 함수
namespace GladiusUI
{
	// 디자이너가 편집하는 전투 UI WBP 경로 (패키지 경로)
	namespace AssetPaths
	{
		inline constexpr const TCHAR* Folder = TEXT("/Game/Blueprints/UI");
		inline constexpr const TCHAR* TopHud = TEXT("/Game/Blueprints/UI/WBP_TopHud");
		inline constexpr const TCHAR* BossTelegraph = TEXT("/Game/Blueprints/UI/WBP_BossTelegraph");
		inline constexpr const TCHAR* LeaderboardRotator = TEXT("/Game/Blueprints/UI/WBP_LeaderboardRotator");
		inline constexpr const TCHAR* LeaderboardBoard = TEXT("/Game/Blueprints/UI/WBP_LeaderboardBoard");
		inline constexpr const TCHAR* LeaderboardRow = TEXT("/Game/Blueprints/UI/WBP_LeaderboardRow");
	}

	//이름이 있는 위젯은 블루프린트 변수로 표시해 WBP에서 BindWidget으로 연결되게 함
	template<typename T>
	T* ConstructNamed(UWidgetTree* Tree, FName Name = NAME_None)
	{
		T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), Name);
		if (!Name.IsNone())
		{
			Widget->bIsVariable = true;
		}
		return Widget;
	}

	//FontFamily(UFont)가 있으면 그 폰트, 없으면 텍스트의 기본 폰트에 크기만 적용
	GLADIUS_API void ApplyFont(UTextBlock* Text, UObject* FontFamily, int32 Size, bool bBold = false);

	//Current가 비어 있거나 C++ 기본 클래스 그대로면, 디자이너용 WBP(AssetPath)가 있을 때 그 클래스를 반환
	//WBP가 없으면 C++ 클래스를 그대로 써서 지금처럼 기본 도형으로 동작
	template<typename T>
	TSubclassOf<T> ResolveWidgetClass(TSubclassOf<T> Current, const TCHAR* AssetPath)
	{
		if (Current && Current != T::StaticClass())
		{
			return Current; //레벨이나 블루프린트에서 직접 지정한 클래스가 우선
		}
		if (FPackageName::DoesPackageExist(AssetPath))
		{
			const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), AssetPath, *FPackageName::GetShortName(AssetPath));
			if (UClass* Loaded = LoadClass<T>(nullptr, *ClassPath))
			{
				return Loaded;
			}
		}
		return T::StaticClass();
	}
}
