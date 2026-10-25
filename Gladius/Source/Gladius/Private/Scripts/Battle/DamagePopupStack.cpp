#include "Scripts/Battle/DamagePopupStack.h"
#include "Scripts/Battle/DamagePopupWidget.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"

UDamagePopupStack::UDamagePopupStack()
{
	//팝업이 하나도 없으면 꺼 둔다. 서 있는 유닛 30개가 빈 Tick을 돌 이유가 없다
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	//엔진의 Widget3DPassThrough를 복제해 Disable Depth Test만 켠 것이다.
	//BP에서 다른 것으로 바꿀 수 있다
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NoDepth(
		TEXT("/Game/UI/M_DamagePopup_NoDepth.M_DamagePopup_NoDepth"));
	if (NoDepth.Succeeded())
	{
		PopupMaterial = NoDepth.Object;
	}
}

void UDamagePopupStack::Push(int32 Damage, bool bCrit, bool bSkill, int32 ClassIndex,
	EResistance Resist)
{
	const int32 Column = NextColumn;
	NextColumn = (NextColumn + 1) % ColumnCount;

	//넘치면 그 열에서 가장 오래된 것을 바로 치운다. 밀려 올라가다 사라지길
	//기다리면 숫자가 화면을 덮는다
	int32 Count = 0;
	for (const FPopup& Popup : Popups)
	{
		Count += Popup.Column == Column ? 1 : 0;
	}

	while (Count >= FMath::Max(1, MaxPerColumn))
	{
		//Popups는 들어온 순서대로라 앞에서 찾으면 그 열의 가장 오래된 것이다
		const int32 Oldest = Popups.IndexOfByPredicate(
			[Column](const FPopup& Popup) { return Popup.Column == Column; });
		if (Oldest == INDEX_NONE)
		{
			break;
		}
		if (Popups[Oldest].Widget)
		{
			Popups[Oldest].Widget->DestroyComponent();
		}
		Popups.RemoveAt(Oldest);
		--Count;
	}

	if (!PopupWidgetClass)
	{
		//생김새를 아직 안 꽂았다. 조용히 넘어가면 '숫자가 안 뜬다'로 오진하게 된다
		UE_LOG(LogTemp, Warning,
			TEXT("DamagePopupStack: PopupWidgetClass가 비어 있다 — %s"), *GetNameSafe(GetOwner()));
		return;
	}

	UWidgetComponent* Widget = NewObject<UWidgetComponent>(this);
	Widget->SetupAttachment(this);
	Widget->SetWidgetSpace(EWidgetSpace::World);
	Widget->SetDrawAtDesiredSize(true);
	Widget->SetWidgetClass(PopupWidgetClass);
	Widget->RegisterComponent();
	Widget->InitWidget();

	//GetMaterial이 OverrideMaterials를 먼저 보므로 SetMaterial로 걸린다
	if (PopupMaterial)
	{
		Widget->SetMaterial(0, PopupMaterial);
	}

	//색·크기 배율·상성 글자·치명타는 전부 위젯이 정한다
	if (UDamagePopupWidget* Popup3D = Cast<UDamagePopupWidget>(Widget->GetUserWidgetObject()))
	{
		Popup3D->SetDamage(Damage, bCrit, bSkill, ClassIndex, Resist);
	}

	FPopup Popup;
	Popup.Widget = Widget;
	Popup.Column = Column;

	//새 숫자는 맨 아래에서 시작하고, 기존 것들이 한 칸씩 올라간다
	Popup.Height = BaseHeight;
	Popups.Add(Popup);

	Reflow();
	SetComponentTickEnabled(true);
}

void UDamagePopupStack::Reflow()
{
	//열마다 아래에서부터 0, 1, 2… 를 다시 매긴다. 뒤에 들어온 것이 아래다
	int32 RowByColumn[ColumnCount] = { 0, 0, 0 };

	for (int32 Index = Popups.Num() - 1; Index >= 0; --Index)
	{
		FPopup& Popup = Popups[Index];
		Popup.Row = RowByColumn[Popup.Column]++;
	}
}

void UDamagePopupStack::TickComponent(float DeltaSeconds, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	const float LifeSpan = FMath::Max(0.f, DamageDisplayTime) + FMath::Max(0.f, DamageFadeoutTime);
	bool bRemoved = false;

	for (int32 Index = Popups.Num() - 1; Index >= 0; --Index)
	{
		FPopup& Popup = Popups[Index];
		Popup.Age += DeltaSeconds;

		if (Popup.Age >= LifeSpan || !Popup.Widget)
		{
			if (Popup.Widget)
			{
				Popup.Widget->DestroyComponent();
			}
			Popups.RemoveAt(Index);
			bRemoved = true;
			continue;
		}

		//밀려 올라가는 것은 순간이동이 아니라 보간이다
		const float TargetHeight = BaseHeight + Popup.Row * RowSpacing;
		Popup.Height = FMath::FInterpTo(Popup.Height, TargetHeight, DeltaSeconds, RiseSpeed);

		//카메라가 +X를 보므로 열은 Y축으로 벌어지고, 좌우 열만 바깥을 향해 돌린다
		const float ColumnOffset = (Popup.Column - 1) * ColumnSpacing;
		Popup.Widget->SetRelativeLocation(FVector(0.f, ColumnOffset, Popup.Height));
		Popup.Widget->SetRelativeRotation(FRotator(0.f, -(Popup.Column - 1) * ColumnYaw, 0.f));
		Popup.Widget->SetRelativeScale3D(FVector(FMath::Max(0.01f, PopupScale)));

		//DamageDisplayTime 동안 그대로 있다가 남은 시간에 걸쳐 사라진다
		const float FadeElapsed = Popup.Age - FMath::Max(0.f, DamageDisplayTime);
		const float Opacity = FadeElapsed <= 0.f || DamageFadeoutTime <= 0.f
			? 1.f : FMath::Clamp(1.f - FadeElapsed / DamageFadeoutTime, 0.f, 1.f);

		if (UUserWidget* Widget = Popup.Widget->GetUserWidgetObject())
		{
			Widget->SetRenderOpacity(Opacity);
		}
	}

	if (bRemoved)
	{
		Reflow();
	}

	if (Popups.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}
