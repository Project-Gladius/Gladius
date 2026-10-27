#include "Scripts/Temple/Goddess.h"
#include "Scripts/Temple/TempleTalkScheduler.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AGoddess::AGoddess()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	BlessingIconActors.SetNum(GTempleEnhanceTierCount);
	BlessingIconMontage = FSoftObjectPath(TEXT("/Game/Asset/Shop/Aurelia/AnimationMontage/AM_Aurelia_Bestow.AM_Aurelia_Bestow"));
	BlessingDurations.Init(3.0f, GTempleEnhanceTierCount);
}

void AGoddess::BeginPlay()
{
	Super::BeginPlay();
	InitializeBlessingIcons();
}

void AGoddess::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBlessingIcons();
}

void AGoddess::InitializeBlessingIcons()
{
	for (int32 Tier = 0; Tier < FMath::Min(BlessingIconActors.Num(), GTempleEnhanceTierCount); ++Tier)
	{
		AActor* Icon = BlessingIconActors[Tier];
		if (!IsValid(Icon)) continue;
		TInlineComponentArray<UMeshComponent*> Meshes(Icon);
		for (UMeshComponent* Mesh : Meshes)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Original = Mesh->GetMaterial(Slot);
				float Glow = 0.f;
				if (!Original || !Original->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Glow")), Glow)) continue;
				UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(Slot);
				if (!Dynamic) continue;
				FBlessingIconMaterial& Entry = BlessingIconMaterials.AddDefaulted_GetRef();
				Entry.Mesh = Mesh;
				Entry.OriginalMaterial = Original;
				Entry.DynamicMaterial = Dynamic;
				Entry.Slot = Slot;
				Entry.TierIndex = Tier;
				Entry.OriginalGlow = Glow;
				Dynamic->SetScalarParameterValue(TEXT("Glow"), Glow * 0.04f);
			}
		}
	}
	if (!BlessingIconMaterials.IsEmpty()) LoadedBlessingIconMontage = BlessingIconMontage.LoadSynchronous();
	UpdateBlessingIcons();
}

void AGoddess::UpdateBlessingIcons()
{
	const bool bBlessing = State >= EGoddessState::Blessing1 && State <= EGoddessState::Blessing4;
	int32 ActiveTier = INDEX_NONE;
	if (bBlessing && LoadedBlessingIconMontage)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(this);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			UAnimInstance* Anim = Mesh->GetAnimInstance();
			if (Anim && Anim->Montage_IsPlaying(LoadedBlessingIconMontage))
			{
				ActiveTier = static_cast<int32>(State) - static_cast<int32>(EGoddessState::Blessing1);
				break;
			}
		}
	}
	ApplyBlessingIconBrightness(ActiveTier);
	SetActorTickEnabled(bBlessing && !BlessingIconMaterials.IsEmpty());
}

void AGoddess::ApplyBlessingIconBrightness(int32 ActiveTierIndex)
{
	if (LitBlessingIcon == ActiveTierIndex) return;
	LitBlessingIcon = ActiveTierIndex;
	for (const FBlessingIconMaterial& Entry : BlessingIconMaterials)
		if (UMaterialInstanceDynamic* Dynamic = Entry.DynamicMaterial.Get())
			Dynamic->SetScalarParameterValue(TEXT("Glow"), Entry.OriginalGlow * (Entry.TierIndex == ActiveTierIndex ? 0.4f : 0.04f));
}

void AGoddess::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StateTimerHandle);
	for (const FBlessingIconMaterial& Entry : BlessingIconMaterials)
		if (UMeshComponent* Mesh = Entry.Mesh.Get())
			if (Mesh->GetMaterial(Entry.Slot) == Entry.DynamicMaterial.Get())
				Mesh->SetMaterial(Entry.Slot, Entry.OriginalMaterial.Get());
	BlessingIconMaterials.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGoddess::StartCycle(const FTempleCycleCommands& Commands)
{
	GetWorldTimerManager().ClearTimer(StateTimerHandle);

	CycleStartedAt = GetWorld()->GetTimeSeconds();
	EnhanceTiers = Commands.EnhanceTiers;
	ReceivedTalkItems.Reset();
	TalkSequence.Reset();
	TalkIndex = 0;
	bBlessingFinished = false;
	TalkPhase = Commands.Talks.IsEmpty() ? ETalkPhase::None : ETalkPhase::Pending;

	//(c) 처리할 명령이 아예 없으면 Idle 유지
	if (Commands.IsEmpty())
	{
		bBlessingFinished = true;
		EnterIdle();
		return;
	}

	EnterBlessing(0);
}

void AGoddess::ReceiveTalkItems(TArray<FTempleTalkItem> Items)
{
	if (TalkPhase != ETalkPhase::Pending)
	{
		return; //이미 실패 처리되었거나 사이클이 바뀐 뒤 늦게 온 응답
	}

	if (Items.IsEmpty())
	{
		SetNoTalks();
		return;
	}

	ReceivedTalkItems = MoveTemp(Items);
	TalkPhase = ETalkPhase::Ready;

	//Blessing이 이미 끝나 Idle로 기다리던 중이면 바로 시작
	if (bBlessingFinished)
	{
		StartTalkSequence();
	}
}

void AGoddess::SetNoTalks()
{
	if (TalkPhase != ETalkPhase::Pending)
	{
		return;
	}

	TalkPhase = ETalkPhase::None;
	if (bBlessingFinished)
	{
		EnterIdle(); //응답을 기다리던 Idle에서 그냥 Idle로
	}
}

void AGoddess::StopCycle()
{
	GetWorldTimerManager().ClearTimer(StateTimerHandle);
	TalkPhase = ETalkPhase::None;
	bBlessingFinished = true;
	EnterIdle();
}

void AGoddess::EnterBlessing(int32 TierIndex)
{
	// 요청이 없는 축복은 상태 전환과 타이머를 모두 건너뜁니다.
	while (TierIndex < GTempleEnhanceTierCount
		&& (!EnhanceTiers.IsValidIndex(TierIndex) || EnhanceTiers[TierIndex].Commands.IsEmpty()))
	{
		++TierIndex;
	}

	if (TierIndex >= GTempleEnhanceTierCount)
	{
		FinishBlessing();
		return;
	}

	FGoddessStateContext Context;
	if (EnhanceTiers.IsValidIndex(TierIndex))
	{
		const int32 Tier = TierIndex + 1;
		for (const FParsedCommand& Command : EnhanceTiers[TierIndex].Commands)
		{
			Context.BlessedUsernames.Add(Command.Username.IsEmpty() ? Command.UserId : Command.Username);
			OnEnhanceExecuted.Broadcast(Command.UserId, Command.Username, Tier);
		}
	}

	const EGoddessState BlessingState = static_cast<EGoddessState>(static_cast<uint8>(EGoddessState::Blessing1) + TierIndex);
	SetState(BlessingState, Context);

	GetWorldTimerManager().SetTimer(
		StateTimerHandle,
		FTimerDelegate::CreateUObject(this, &AGoddess::EnterBlessing, TierIndex + 1),
		GetBlessingDuration(TierIndex),
		false
	);
}

void AGoddess::FinishBlessing()
{
	bBlessingFinished = true;

	switch (TalkPhase)
	{
	case ETalkPhase::Ready:
		StartTalkSequence();
		break;
	case ETalkPhase::Pending:
		EnterIdle(true); //AI 응답이 올 때까지 Idle로 대기
		break;
	default:
		EnterIdle();
		break;
	}
}

void AGoddess::StartTalkSequence()
{
	TalkPhase = ETalkPhase::None;

	TArray<FTempleTalkItem> Items = MoveTemp(ReceivedTalkItems);
	FTempleTalkScheduler::ComputeDurations(Items, ReadingBaseSeconds, SpeakingBaseSeconds, ByteTimeSeconds);

	//응답이 늦게 와 Idle로 기다린 시간까지 반영해, 남은 사이클 시간 안에 끝나는 대화만 처리
	const float AvailableSeconds = CycleBudgetSeconds - GetCycleElapsedSeconds();
	const int32 RequestedCount = Items.Num();
	TalkSequence = FTempleTalkScheduler::SelectWithinBudget(MoveTemp(Items), AvailableSeconds);
	TalkIndex = 0;

	if (TalkSequence.Num() < RequestedCount)
	{
		UE_LOG(LogTemp, Log, TEXT("[여신] 시간 부족으로 대화 %d건 중 %d건만 처리 (남은 시간 %.1f초)"),
			RequestedCount, TalkSequence.Num(), AvailableSeconds);
	}

	if (TalkSequence.IsEmpty())
	{
		EnterIdle();
		return;
	}

	EnterReading();
}

void AGoddess::EnterReading()
{
	const FTempleTalkItem& Item = TalkSequence[TalkIndex];

	FGoddessStateContext Context;
	Context.Username = Item.Username;
	Context.Text = Item.QueryMessage;
	SetState(EGoddessState::Reading, Context);

	GetWorldTimerManager().SetTimer(StateTimerHandle, this, &AGoddess::EnterSpeaking, FMath::Max(Item.ReadingSeconds, KINDA_SMALL_NUMBER), false);
}

void AGoddess::EnterSpeaking()
{
	const FTempleTalkItem& Item = TalkSequence[TalkIndex];

	FGoddessStateContext Context;
	Context.Username = Item.Username;
	Context.Text = Item.ResponseText;
	Context.Emotion = Item.Emotion;
	SetState(EGoddessState::Speaking, Context);

	GetWorldTimerManager().SetTimer(StateTimerHandle, this, &AGoddess::AdvanceTalk, FMath::Max(Item.SpeakingSeconds, KINDA_SMALL_NUMBER), false);
}

void AGoddess::AdvanceTalk()
{
	++TalkIndex;
	if (TalkSequence.IsValidIndex(TalkIndex))
	{
		EnterReading();
		return;
	}

	TalkSequence.Reset();
	EnterIdle();
}

void AGoddess::EnterIdle(bool bWaitingForResponse)
{
	FGoddessStateContext Context;
	Context.bWaitingForResponse = bWaitingForResponse;
	SetState(EGoddessState::Idle, Context);
}

void AGoddess::SetState(EGoddessState NewState, const FGoddessStateContext& Context)
{
	State = NewState;
	OnStateChanged.Broadcast(NewState, Context);
	// Blueprint subscribers start the montage synchronously during the broadcast.
	UpdateBlessingIcons();
}

float AGoddess::GetBlessingDuration(int32 TierIndex) const
{
	const float Duration = BlessingDurations.IsValidIndex(TierIndex) ? BlessingDurations[TierIndex] : 3.0f;
	return FMath::Max(Duration, KINDA_SMALL_NUMBER); //0초면 타이머가 동작하지 않으므로 최소값 보장
}

float AGoddess::GetCycleElapsedSeconds() const
{
	return static_cast<float>(GetWorld()->GetTimeSeconds() - CycleStartedAt);
}
