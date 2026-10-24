#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GladiusLevelToolCommandlet.generated.h"

// 에디터를 열지 않고 레벨의 액터를 조회·이동·추가하는 에디터 전용 커맨드릿 (레벨 배치 보조 도구)
//
// 액터 목록 출력:
//   UnrealEditor-Cmd.exe Gladius.uproject -run=GladiusLevelTool -map=/Game/Levels/Main/MainTemple -dump
// 액터 이동 (라벨|위치|회전[|크기], 여러 개는 ; 로 구분):
//   ... -set="BP_Goddess|0,0,0|0,180,0;CameraActor|-500,0,200|-10,0,0"
// 액터 추가 (클래스 경로|라벨|위치|회전):
//   ... -spawn="/Script/Gladius.TempleCycleController|TempleCycleController|0,0,0|0,0,0"
// 플레이어 카메라로 쓸 카메라 지정 (라벨):
//   ... -viewcamera=CameraActor
// -set / -spawn / -viewcamera가 있으면 레벨을 저장한다
UCLASS()
class GLADIUS_API UGladiusLevelToolCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGladiusLevelToolCommandlet();

	virtual int32 Main(const FString& Params) override;
};
