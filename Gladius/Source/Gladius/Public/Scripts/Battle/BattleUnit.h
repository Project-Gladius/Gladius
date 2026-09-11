#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleUnit.generated.h"

class USkeletalMeshComponent;
class UWidgetComponent;

//전투에 등장하는 유닛. 메시·크기·이름표 위치는 블루프린트에서 지정한다.
//BattleManager가 PlayerUnitClass / BossUnitClass로 어떤 블루프린트를 쓸지 고른다
UCLASS()
class GLADIUS_API ABattleUnit : public AActor
{
	GENERATED_BODY()

public:
	ABattleUnit();

	//머리 위 이름과 체력 갱신. UMG라 한글·이모지 핸들도 그대로 나온다
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetLabel(const FString& Name, int32 Current, int32 Max);

	//스폰 시 스케일이 1로 덮이므로, 크기는 Mesh 컴포넌트 쪽에서 준다.
	//그래서 액터 루트는 빈 SceneComponent로 두고 Mesh를 그 아래 붙인다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	//Mesh가 아니라 Root에 붙는다. 메시 크기와 무관하게 높이를 직접 정할 수 있다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UWidgetComponent> Nameplate;

protected:
	virtual void BeginPlay() override;

private:
	//스폰 직후엔 위젯이 아직 없을 수 있어 들고 있다가 BeginPlay에서 한 번 더 적용한다
	FString PendingLabel;

	void ApplyLabel();
};
