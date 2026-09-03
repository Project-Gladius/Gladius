#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitNameplate.generated.h"

class UTextBlock;

//유닛 머리 위에 뜨는 이름표. 텍스트 한 줄뿐이라 위젯 블루프린트 없이 C++에서 직접 만든다.
//TextRenderComponent와 달리 Slate 폰트를 쓰므로 한글·이모지 핸들이 그대로 나온다
UCLASS()
class GLADIUS_API UUnitNameplate : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetText(const FString& InText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	//위젯 블루프린트에 'Label'이라는 이름의 TextBlock이 있으면 자동으로 연결된다.
	//없으면 RebuildWidget이 C++로 하나 만들어 쓴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	//위젯이 만들어지기 전에 SetText가 불릴 수 있어 들고 있다가 적용한다
	FString PendingText;
};
