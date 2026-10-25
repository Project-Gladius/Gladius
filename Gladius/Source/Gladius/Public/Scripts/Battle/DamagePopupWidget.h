#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scripts/Battle/BattleBalance.h"
#include "DamagePopupWidget.generated.h"

class UTextBlock;
class UImage;
class UFont;

//피해 숫자 하나. WBP_DamagePopup의 부모다.
//
//배치·폰트는 WBP 디자이너가 들고, **표시 규칙**(색·크기 배율·상성 글자·치명타)은
//여기에 있다. 규칙을 그래프에 흩어 두면 기획서와 대조할 수가 없다.
//
//WBP에 아래 세 이름의 위젯이 있어야 한다 — 없으면 컴파일 때 알려 준다
UCLASS(Abstract)
class GLADIUS_API UDamagePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//DamagePopupStack이 스폰 직후 한 번 부른다
	UFUNCTION(BlueprintCallable, Category = "Battle|DamagePopup")
	void SetDamage(int32 Damage, bool bCrit, bool bSkill, int32 ClassIndex, EResistance Resist);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DamageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResistText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CritIcon;

	//--- 표시 규칙. 전부 WBP의 Class Defaults에서 만진다 ---

	//ClassIndex로 찾는다. 배열 순서는 abilities.json의 characters[] 순서와 같다
	//(0 = 글라디우스, 1 = 세라). 직업군이 늘면 여기에 한 줄 더하면 된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	TArray<FLinearColor> SkillColors;

	//같은 직업군의 평타 색. 스킬보다 연하다
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	TArray<FLinearColor> FeverColors;

	//목록에 없는 ClassIndex(플레이어가 맞은 쪽은 음수로 온다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	FLinearColor FallbackColor = FLinearColor::White;

	//치명타일 때 숫자 폰트를 이걸로 바꾼다. 폰트 애셋마다 Typeface가 하나뿐이라
	//Bold 전환이 안 되므로 더 굵은 애셋(Cinzel_Black_Font)으로 갈아끼운다.
	//비워 두면 폰트를 바꾸지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	TObjectPtr<UFont> CritFont;

	//숫자 자릿수에 따라 **더해지는** 배율 (기획서: 2자리 10%, 3자리 40%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float TwoDigitScale = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float ThreeDigitScale = 0.4f;

	//스킬이면 추가로 더한다 (기획서: 10%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle|DamagePopup")
	float SkillScale = 0.1f;

private:
	//디자이너가 지정한 평상시 폰트. 치명타에서 되돌리려고 들고 있는다.
	//FSlateFontInfo::FontObject가 const라 여기도 const로 맞춘다
	UPROPERTY(Transient)
	TObjectPtr<const UObject> NormalFont;
};
