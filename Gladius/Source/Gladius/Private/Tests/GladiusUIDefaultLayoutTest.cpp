#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardBoardWidget.h"
#include "Scripts/UI/Leaderboard/LeaderboardRowWidget.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_METADATA

namespace
{
	//BuildDefaultLayout이 만든 트리에, 클래스의 BindWidgetOptional 위젯이 같은 이름·맞는 타입으로 모두 있는지 검사
	//(WBP 자동 생성은 이 레이아웃을 그대로 Designer에 심으므로, 이름이 어긋나면 WBP에서 해당 기능이 빠진다)
	template<typename TWidget>
	void CheckDefaultLayoutBindings(FAutomationTestBase& Test)
	{
		UWidgetTree* Tree = NewObject<UWidgetTree>(GetTransientPackage());
		GetDefault<TWidget>()->BuildDefaultLayout(Tree);

		const FString ClassName = TWidget::StaticClass()->GetName();
		Test.TestNotNull(*FString::Printf(TEXT("%s: 루트 위젯"), *ClassName), Tree->RootWidget.Get());

		int32 Checked = 0;
		for (TFieldIterator<FObjectProperty> It(TWidget::StaticClass()); It; ++It)
		{
			if (!It->HasMetaData(TEXT("BindWidgetOptional")) && !It->HasMetaData(TEXT("BindWidget")))
			{
				continue;
			}
			++Checked;
			UWidget* Widget = Tree->FindWidget(It->GetFName());
			Test.TestNotNull(*FString::Printf(TEXT("%s: '%s' 위젯이 기본 레이아웃에 있음"), *ClassName, *It->GetName()), Widget);
			if (Widget)
			{
				Test.TestTrue(*FString::Printf(TEXT("%s: '%s' 타입 일치"), *ClassName, *It->GetName()), Widget->IsA(It->PropertyClass));
				Test.TestTrue(*FString::Printf(TEXT("%s: '%s' 블루프린트 변수"), *ClassName, *It->GetName()), Widget->bIsVariable != 0);
			}
		}
		Test.TestTrue(*FString::Printf(TEXT("%s: 바인딩 위젯이 하나 이상"), *ClassName), Checked > 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGladiusUIDefaultLayoutTest, "Gladius.UI.DefaultLayoutBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGladiusUIDefaultLayoutTest::RunTest(const FString& Parameters)
{
	CheckDefaultLayoutBindings<UBattleTopHudWidget>(*this);
	CheckDefaultLayoutBindings<UBossTelegraphWidget>(*this);
	CheckDefaultLayoutBindings<ULeaderboardBoardWidget>(*this);
	CheckDefaultLayoutBindings<ULeaderboardRowWidget>(*this);
	return true;
}

#endif
