#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleUnit.generated.h"

class USkeletalMeshComponent;
class UWidgetComponent;

//전투에 등장하는 유닛. 보스와 플레이어가 같은 클래스를 쓰고 스케일로만 구분한다
UCLASS()
class GLADIUS_API ABattleUnit : public AActor
{
	GENERATED_BODY()

public:
	ABattleUnit();

	//머리 위 이름과 체력 갱신. UMG라 한글·이모지 핸들도 그대로 나온다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetLabel(const FString& Name, int32 Current, int32 Max);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UWidgetComponent> Nameplate;

protected:
	virtual void BeginPlay() override;

private:
	//스폰 직후엔 위젯이 아직 없을 수 있어 들고 있다가 BeginPlay에서 한 번 더 적용한다
	FString PendingLabel;

	void ApplyLabel();
};
