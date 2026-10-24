#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateGladiusUICommandlet.generated.h"

// 전투 UI의 디자이너용 WBP를 /Game/Blueprints/UI에 만드는 에디터 전용 커맨드릿
// 각 위젯 C++의 BuildDefaultLayout으로 지금 화면과 같은 기본 레이아웃을 Designer 트리에 심고 컴파일·저장한다
// 이미 있는 에셋은 건드리지 않는다 (디자이너 작업 보호). 다시 만들려면 에셋을 지운 뒤 실행
//
// 실행 (에디터를 끈 상태):
//   UnrealEditor-Cmd.exe Gladius.uproject -run=GenerateGladiusUI
UCLASS()
class GLADIUS_API UGenerateGladiusUICommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGenerateGladiusUICommandlet();

	virtual int32 Main(const FString& Params) override;
};
