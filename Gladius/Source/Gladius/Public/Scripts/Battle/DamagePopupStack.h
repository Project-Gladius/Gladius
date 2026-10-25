#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Scripts/Battle/BattleBalance.h"
#include "DamagePopupStack.generated.h"

class UWidgetComponent;
class UDamagePopupWidget;
class UMaterialInterface;

//머리 위에 뜨는 피해 숫자를 3열로 쌓는다.
//
//유닛의 Tick은 이동·회전이 끝나면 스스로 꺼지므로 거기에 얹으면 이동 로직을
//건드려야 한다. 그래서 자기 Tick을 가진 컴포넌트로 떼어 놓았다.
//
//이 컴포넌트가 정하는 것은 **배치와 수명**뿐이다. 숫자의 생김새(폰트·색·아이콘·
//크기 배율)는 PopupWidgetClass가 가리키는 WBP와 그 부모 UDamagePopupWidget이 맡는다
UCLASS(ClassGroup = (Battle), meta = (BlueprintSpawnableComponent))
class GLADIUS_API UDamagePopupStack : public USceneComponent
{
	GENERATED_BODY()

public:
	UDamagePopupStack();

	//피해 한 대를 띄운다. 값들은 숫자의 생김새만 가른다 — 피해는 이미 확정돼 있다
	void Push(int32 Damage, bool bCrit, bool bSkill, int32 ClassIndex, EResistance Resist);

	//이 유닛에 떠 있는 팝업 수. 디버그·검사용
	int32 LivePopupCount() const { return Popups.Num(); }

	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	//--- 조절값. 전부 에디터에서 만진다 ---

	//띄울 위젯. WBP_DamagePopup을 꽂는다. 비워 두면 아무것도 안 뜬다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	TSubclassOf<UDamagePopupWidget> PopupWidgetClass;

	//숫자를 무기·뿔·UI 뒤로 숨기지 않으려면 깊이 판정을 꺼야 한다.
	//기획서: "무기 등 다른 물체에 가리지 않고 UI처럼 가장 위에 표시"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	TObjectPtr<UMaterialInterface> PopupMaterial;

	//뜬 뒤 이만큼 그대로 있다가
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float DamageDisplayTime = 1.f;

	//이만큼에 걸쳐 사라진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float DamageFadeoutTime = 1.f;

	//숫자 크기. 월드 위젯이라 컴포넌트 스케일이 곧 글씨 크기다.
	//기획서는 '보스의 머리 정도'를 말한다 — 유닛마다 다르므로 BP에서 맞춘다.
	//Tick에서 매번 적용하므로 PIE 중에 값을 바꾸면 바로 보인다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float PopupScale = 3.f;

	//첫 줄의 높이. 이름표보다 위여야 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float BaseHeight = 220.f;

	//열 사이 간격. 카메라가 +X를 보므로 열은 Y축으로 벌어진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float ColumnSpacing = 120.f;

	//밀려 올라갈 때 한 칸의 높이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float RowSpacing = 70.f;

	//좌우 열이 바깥쪽을 보는 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float ColumnYaw = 10.f;

	//밀려 올라가는 속도. 클수록 빨리 따라붙는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float RiseSpeed = 8.f;

	//한 열에 이보다 많아지면 가장 오래된 것부터 바로 지운다.
	//30명이 한 턴에 때리면 화면이 숫자로 덮인다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|DamagePopup")
	int32 MaxPerColumn = 8;

private:
	//열은 좌·중·우 셋으로 고정이다 (기획서 "대미지의 표시")
	static constexpr int32 ColumnCount = 3;

	struct FPopup
	{
		TObjectPtr<UWidgetComponent> Widget;
		int32 Column = 0;

		//아래에서 몇 번째인가. 새 숫자가 들어오면 전부 하나씩 올라간다
		int32 Row = 0;

		//보간 중인 실제 높이. 목표는 BaseHeight + Row * RowSpacing 이다
		float Height = 0.f;
		float Age = 0.f;
	};

	TArray<FPopup> Popups;

	//열을 돌아가며 쓴다. 한쪽에 몰리면 3열로 나눈 뜻이 없다
	int32 NextColumn = 0;

	void Reflow();
};
