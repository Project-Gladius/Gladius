#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scripts/Battle/BattleManager.h"
#include "BattleSpot.generated.h"

class UStaticMeshComponent;

//플레이어가 설 수 있는 자리. 레벨에 놓아두면 BattleManager가 알아서 찾아 쓴다.
//배열에 등록할 필요 없이 놓기만 하면 되고, 뷰포트에서 그냥 끌어서 옮기면 된다.
//게임에서는 보이지 않는다
UCLASS()
class GLADIUS_API ABattleSpot : public AActor
{
	GENERATED_BODY()

public:
	ABattleSpot();

	//이 자리가 속한 구역
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle")
	EBattleLane Lane = EBattleLane::Mid;

private:
	//에디터에서 보고 집을 수 있게 하는 표식. 게임에서는 숨겨진다
	UPROPERTY(VisibleAnywhere, Category = "Battle")
	TObjectPtr<UStaticMeshComponent> Marker;
};
