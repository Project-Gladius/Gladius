#include "Scripts/Battle/DamagePopupWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Font.h"

void UDamagePopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//디자이너가 고른 폰트를 기억해 둔다. 치명타에서 바꿨다가 되돌릴 때 쓴다
	if (DamageText)
	{
		NormalFont = DamageText->GetFont().FontObject;
	}

	//기획서의 네 색. WBP의 Class Defaults에서 덮어쓸 수 있다
	if (SkillColors.Num() == 0)
	{
		SkillColors = {
			FLinearColor(1.f, 0.48f, 0.08f),   //글라디우스 — 주황
			FLinearColor(0.16f, 0.45f, 0.95f)  //세라 — 파랑
		};
	}
	if (FeverColors.Num() == 0)
	{
		FeverColors = {
			FLinearColor(1.f, 0.76f, 0.47f),   //글라디우스 평타 — 연한 주황
			FLinearColor(0.48f, 0.82f, 0.97f)  //세라 평타 — 하늘색
		};
	}
}

void UDamagePopupWidget::SetDamage(int32 Damage, bool bCrit, bool bSkill, int32 ClassIndex,
	EResistance Resist)
{
	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(Damage));

		const TArray<FLinearColor>& Palette = bSkill ? SkillColors : FeverColors;
		DamageText->SetColorAndOpacity(FSlateColor(
			Palette.IsValidIndex(ClassIndex) ? Palette[ClassIndex] : FallbackColor));

		//폰트 애셋마다 Typeface가 하나라 Bold는 더 굵은 애셋으로 갈아끼워 낸다.
		//FontObject가 TObjectPtr<const UObject>라 대입이 안 걸려 생성자로 만든다
		if (CritFont)
		{
			const FSlateFontInfo Current = DamageText->GetFont();
			const UObject* Face = bCrit
				? static_cast<const UObject*>(CritFont.Get()) : NormalFont.Get();

			if (Face)
			{
				DamageText->SetFont(FSlateFontInfo(Face, Current.Size,
					Current.TypefaceFontName, Current.OutlineSettings));
			}
		}
	}

	if (CritIcon)
	{
		CritIcon->SetVisibility(bCrit ? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (ResistText)
	{
		//'보통'은 띄우지 않는다. 대부분의 타격이 보통이라 띄우면 화면이 글자로 덮이고,
		//기획서 첨부 이미지도 취약/견딤에만 글자가 붙어 있다
		const bool bShow = Resist != EResistance::Normal;
		ResistText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);

		if (bShow)
		{
			ResistText->SetText(Resist == EResistance::Vulnerable
				? NSLOCTEXT("Gladius", "ResistVulnerable", "취약")
				: NSLOCTEXT("Gladius", "ResistEnduring", "견딤"));

			//상성 글자도 숫자와 같은 색으로 묶어야 누구의 타격인지 읽힌다
			if (DamageText)
			{
				ResistText->SetColorAndOpacity(DamageText->GetColorAndOpacity());
			}
		}
	}

	//자릿수와 스킬 여부로 커진다. 기획서는 곱이 아니라 **합**이다 —
	//3자리 스킬이면 1 + 0.4 + 0.1 = 1.5배
	const int32 Magnitude = FMath::Abs(Damage);
	const float DigitScale = Magnitude >= 100 ? ThreeDigitScale
		: Magnitude >= 10 ? TwoDigitScale : 0.f;

	const float Scale = 1.f + DigitScale + (bSkill ? SkillScale : 0.f);
	SetRenderScale(FVector2D(Scale, Scale));
}
