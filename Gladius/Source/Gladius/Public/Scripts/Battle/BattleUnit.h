#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleUnit.generated.h"

class USkeletalMeshComponent;
class UTextRenderComponent;

//전투에 등장하는 유닛. 보스와 플레이어가 같은 클래스를 쓰고 스케일로만 구분한다
UCLASS()
class GLADIUS_API ABattleUnit : public AActor
{
	GENERATED_BODY()

public:
	ABattleUnit();

	//머리 위 체력 숫자 갱신
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetHP(int32 Current, int32 Max);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UTextRenderComponent> HPText;
};
