#include "Scripts/Battle/BattleManager.h"
#include "Scripts/Battle/BattleUnit.h"
#include "Scripts/Battle/BattlePlayerNameplate.h"
#include "Scripts/Battle/BattleSpot.h"
#include "Scripts/Battle/CutSceneApplier.h"
#include "Scripts/Backend/ChatCommandParser.h"
#include "Scripts/Backend/CommandDispatcher.h"
#include "Scripts/Backend/YouTubeChatManager.h"
#include "Scripts/Backend/GameManager.h"
#include "Scripts/Time/RealTimeSettings.h"
#include "Scripts/UI/ChatLogWidget.h"
#include "Scripts/UI/ChatLogSubsystem.h"
#include "Scripts/UI/Leaderboard/LeaderboardRotatorWidget.h"
#include "Scripts/UI/BossTelegraph/BossTelegraphWidget.h"
#include "Scripts/UI/BattleDataResetWidget.h"
#include "Scripts/UI/BattleHud/BattleTopHudWidget.h"
#include "Scripts/UI/UIWidgetBuild.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Animation/AnimMontage.h"

ABattleManager::ABattleManager()
{
	PrimaryActorTick.bCanEverTick = true;
	ChatLogWidgetClass = UChatLogWidget::StaticClass();
	LeaderboardWidgetClass = ULeaderboardRotatorWidget::StaticClass();
	BossTelegraphWidgetClass = UBossTelegraphWidget::StaticClass();
	TopHudWidgetClass = UBattleTopHudWidget::StaticClass();

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	//밸런스 파일을 읽지 못했을 때만 쓰이는 기본값. 수치는 abilities.json 이 기준이다.
	//배열 순서가 자리 배치의 앞뒤를 뜻한다 — 0번이 보스 쪽
	FCharacterDef Front;
	Front.Id = TEXT("gladius");
	Front.Name = TEXT("글라디우스");
	Front.Attacks[1].CritChance = 0.25f;
	Front.Attacks[1].CritDamage = 0.5f;
	Classes.Add(Front);

	FCharacterDef Back = Front;
	Back.Id = TEXT("sera");
	Back.Name = TEXT("세라");
	Classes.Add(Back);
}

int32 ABattleManager::ClassIndexFor(const FString& CharacterId) const
{
	const int32 Index = Classes.IndexOfByPredicate(
		[&CharacterId](const FCharacterDef& C) { return C.Id == CharacterId; });

	if (Index == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] 모르는 직업군 '%s' — 첫 번째 직업군으로 넣습니다"), *CharacterId);
		return 0;
	}
	return Index;
}

void ABattleManager::BeginPlay()
{
	Super::BeginPlay();

 //BATTLE ANALYSIS는 전투씬에 들어온 순간부터 산출
 SessionStats.Reset();
 InitializePlayerRecords();
 if (!bAutoStartBattle)
 {
  SetActorTickEnabled(false);
  return;
 }
 AYouTubeChatManager* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass()));
 if (!Chat)
 {
  if (UGameInstance* Instance = GetGameInstance())
  {
   FString ApiKey, VideoId;
   TSet<FString> AdminIds;
   if (Instance->GetSubsystem<UChatLogSubsystem>()->GetYouTubeSource(ApiKey, VideoId, AdminIds))
   {
    Chat = GetWorld()->SpawnActorDeferred<AYouTubeChatManager>(AYouTubeChatManager::StaticClass(), FTransform::Identity);
    if (Chat)
    {
     Chat->ApiKey = MoveTemp(ApiKey);
     Chat->VideoId = MoveTemp(VideoId);
     Chat->AdminUserIds = MoveTemp(AdminIds);
     Chat->FinishSpawning(FTransform::Identity);
    }
   }
  }
 }
 if (Chat) Chat->GetCommandDispatcher()->BindBattleManager(this);
 if (ChatLogWidgetClass)
 {
  ChatLogWidget = CreateWidget<UChatLogWidget>(GetWorld(), ChatLogWidgetClass);
  if (ChatLogWidget)
  {
   ChatLogWidget->SetExpansionAllowed(false);
   ChatLogWidget->AddToViewport(1);
  }
 }
 if (LeaderboardWidgetClass)
 {
  LeaderboardWidget = CreateWidget<ULeaderboardRotatorWidget>(GetWorld(), GladiusUI::ResolveWidgetClass<ULeaderboardRotatorWidget>(LeaderboardWidgetClass, GladiusUI::AssetPaths::LeaderboardRotator));
  if (LeaderboardWidget)
  {
   LeaderboardWidget->AddToViewport(1);
   LeaderboardWidget->BindToBattleManager(this);
  }
 }
 if (BossTelegraphWidgetClass)
 {
  BossTelegraphWidget = CreateWidget<UBossTelegraphWidget>(GetWorld(), GladiusUI::ResolveWidgetClass<UBossTelegraphWidget>(BossTelegraphWidgetClass, GladiusUI::AssetPaths::BossTelegraph));
  if (BossTelegraphWidget)
  {
   BossTelegraphWidget->AddToViewport(1);
   BossTelegraphWidget->BindToBattleManager(this);
  }
 }
 if (TopHudWidgetClass)
 {
  TopHudWidget = CreateWidget<UBattleTopHudWidget>(GetWorld(), GladiusUI::ResolveWidgetClass<UBattleTopHudWidget>(TopHudWidgetClass, GladiusUI::AssetPaths::TopHud));
  if (TopHudWidget)
  {
   TopHudWidget->AddToViewport(1);
   TopHudWidget->BindToBattleManager(this);
  }
 }
 if (bShowTemporaryDataResetButtons && UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("MainBattle"))
 {
  if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
  {
   DataResetWidget = CreateWidget<UBattleDataResetWidget>(PC, UBattleDataResetWidget::StaticClass());
   if (DataResetWidget)
   {
    DataResetWidget->SetBattleManager(this);
    DataResetWidget->AddToViewport(100);
    DataResetController = PC;
    bMouseCursorBeforeDataReset = PC->bShowMouseCursor;
    PC->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(InputMode);
   }
  }
 }
 GatherSpots();

	for (const FCharacterDef& Class : Classes)
	{
		const TSubclassOf<ABattleUnit>* Found = UnitClassByCharacter.Find(Class.Id);
		if (!Found || !*Found)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Battle] 직업군 '%s'에 블루프린트가 지정되지 않았습니다 — 메시가 없어 보이지 않습니다"),
				*Class.Id);
		}
	}

	if (!BossUnitClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] BossUnitClass가 비어 있습니다"));
	}

	StartBattle();
}

void ABattleManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 if (DataResetWidget) DataResetWidget->RemoveFromParent();
 if (DataResetController.IsValid()) DataResetController->bShowMouseCursor = bMouseCursorBeforeDataReset;
 if (ChatLogWidget) ChatLogWidget->RemoveFromParent();
 if (LeaderboardWidget) LeaderboardWidget->RemoveFromParent();
 if (BossTelegraphWidget) BossTelegraphWidget->RemoveFromParent();
 if (TopHudWidget) TopHudWidget->RemoveFromParent();
 if (AYouTubeChatManager* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass())))
  Chat->GetCommandDispatcher()->UnbindBattleManager(this);
 StopBattle();
 //방송이 정상 종료되는 경우의 마지막 저장
	SaveRecords();
	//전투씬에서 나가면 BATTLE ANALYSIS 초기화
	SessionStats.Reset();
	Super::EndPlay(EndPlayReason);
}

void ABattleManager::StartBattle()
{
 StopBattle();
 bBattleRunning = true;
 Phase = EBattlePhase::BattleStart;
 PreviousCutSceneClassId.Reset();
 SelectedCutSceneUsers.Reset();
 CutSceneUserId.Reset();
 CutSceneIds.Reset();
 ActiveCutSceneIds.Reset();
 CutSceneRandom.Initialize(FMath::Rand());
 for (auto& Pair : Players) { if (IsValid(Pair.Value.Unit)) Pair.Value.Unit->Destroy(); }
 Players.Reset();
 if (IsValid(BossUnit)) BossUnit->Destroy();
 if (AGameManager* Game = Cast<AGameManager>(UGameplayStatics::GetActorOfClass(this, AGameManager::StaticClass())))
  Game->SetGameState(EGameState::Combat);
 if (AYouTubeChatManager* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass())))
  Chat->SetCombatAutoPollingEnabled(false);
 BossDamageTaken = 0;
	BossDamageShown = 0;
	TurnIndex = 0;

	//패턴 진행도는 한 판에 속한 값이다. 안 지우면 다음 판이 중간부터 시작한다
	CurrentPatternId.Reset();
	PatternStep = 0;
	PresetPatternIndex = 0;

	// A new battle starts with a fresh participant list; persistent Records are retained.
	SpawnBoss();
	UpdateVisuals();
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::StartTurn, FMath::Max(0.001f, BattleStartSeconds), false);
}

void ABattleManager::FinishBattle()
{
 if (!bBattleRunning) return;
 //신전으로 가기 전에 이번 전투 참가자에게 축복 횟수 지급 (StopBattle이 저장)
 GrantBlessingsToParticipants();
 StopBattle();
 const TSoftObjectPtr<UWorld>& TempleMap = GetDefault<URealTimeSettings>()->TempleMap;
 if (TempleMap.IsNull())
 {
  UE_LOG(LogTemp, Error, TEXT("[Battle] Cannot return to temple: TempleMap is not configured."));
  return;
 }
 if (AGameManager* Game = Cast<AGameManager>(UGameplayStatics::GetActorOfClass(this, AGameManager::StaticClass())))
  Game->SetGameState(EGameState::Temple);
 UGameplayStatics::OpenLevelBySoftObjectPtr(this, TempleMap);
}

void ABattleManager::StopBattle()
{
 WalkRunOnlyCutSceneUnit.Reset();
 if (bBattleRunning) SaveRecords();
 bBattleRunning = false;
 Phase = EBattlePhase::BattleEnd;
 RefreshPlayerUI();
 StopTurnCutScenes();
 if (AYouTubeChatManager* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass())))
 {
  Chat->SetCombatAutoPollingEnabled(false);
  Chat->CancelPendingChatFetch();
 }
 GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	for (int32 Lane = 0; Lane < 3; ++Lane)
	{
		GetWorldTimerManager().ClearTimer(EvadeRollHandles[Lane]);
		GetWorldTimerManager().ClearTimer(EvadeApproachHandles[Lane]);
		GetWorldTimerManager().ClearTimer(StrikeLandHandles[Lane]);
	}

	//공격 연출 타이머는 반복이라 따로 끄지 않으면 전투가 끝나도 계속 돈다
	GetWorldTimerManager().ClearTimer(AttackCueHandle);
	AttackCues.Reset();
	AttackCueIndex = 0;
}

bool ABattleManager::IsAcceptingInput() const
{
	//예고 시간의 선택도 유효하다. 이동이 시작되면 이번 턴 행동은 확정된다
	return bBattleRunning && (Phase == EBattlePhase::Prepare
		|| Phase == EBattlePhase::Collect);
}

float ABattleManager::GetInputPhaseRemainingSeconds() const
{
	if (!IsAcceptingInput() || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Max(0.f, InputPhaseEndTime - GetWorld()->GetTimeSeconds());
}

bool ABattleManager::IsTurnCutScenePlaying() const
{
	return IsValid(CutSceneApplier) && CutSceneApplier->IsPlaying();
}

// ---------------- 좌표 ----------------

EMoveGait ABattleManager::ChooseGait(float Distance, float Seconds, float RunSpeed, float RollSpeed)
{
	//시간이 없으면 아무리 가까워도 가장 빠른 수단이 필요하다
	if (Seconds <= KINDA_SMALL_NUMBER)
	{
		return EMoveGait::Roll;
	}

	const float Speed = Distance / Seconds;
	if (Speed < RunSpeed)   { return EMoveGait::Walk; }
	if (Speed < RollSpeed)  { return EMoveGait::Run; }
	return EMoveGait::Roll;
}

EMoveGait ABattleManager::ChooseMovementGait(const ABattleUnit* Unit, float Distance, float Seconds) const
{
 const EMoveGait Gait = ChooseGait(Distance, Seconds, RunSpeed, RollSpeed);
 return Unit && Unit == WalkRunOnlyCutSceneUnit.Get() && Gait == EMoveGait::Roll
     ? EMoveGait::Run : Gait;
}

float ABattleManager::AttackMotionSeconds(float SkillSeconds, float FlurrySeconds)
{
	//스킬 한 번 + 평타 전부. 전원이 같은 순간에 끝나야 하므로
	//이 길이를 BattleTime에서 빼면 곧 출발 시각이 된다
	return FMath::Max(0.f, SkillSeconds) + FMath::Max(0.f, FlurrySeconds);
}

float ABattleManager::FeverFlurrySeconds(const TArray<float>& SectionSeconds, int32 Hits)
{
	if (SectionSeconds.Num() == 0)
	{
		return 0.f;
	}

	//섹션이 평타 수보다 적으면 앞에서부터 다시 쓴다 (3개면 1,2,3,1,2,3…)
	float Total = 0.f;
	for (int32 Hit = 0; Hit < FMath::Max(0, Hits); ++Hit)
	{
		Total += FMath::Max(0.f, SectionSeconds[Hit % SectionSeconds.Num()]);
	}
	return Total;
}

int32 ABattleManager::FeverStacksFor(int32 Fever) const
{
	const int32 Stacks = FMath::Max(0, Fever);
	//상한이 0이면 무제한이라는 뜻이다 (파일의 null)
	return FeverCap > 0.f ? FMath::Min(Stacks, FeverCap) : Stacks;
}

float ABattleManager::BossStrikeDelay(float FlurrySeconds, float RemainingToBattleTime)
{
	//전투 개시부터 BattleTime이 지난 순간에 치는 것이 기본이다.
	//다만 플레이어 공격 연출보다 먼저 치면 안 된다 —
	//기획서의 PlayerAttack → BossAttack 순서가 뒤집힌다
	return FMath::Max(FlurrySeconds, RemainingToBattleTime);
}

bool ABattleManager::IsRangedClass(int32 ClassIndex) const
{
	//자리 배치와 같은 규칙 — Classes 배열 순서가 앞뒤다. 직업군이 하나뿐이면 전부 근접
	return Classes.Num() > 1 && ClassIndex == Classes.Num() - 1;
}

FVector ABattleManager::ChargeLocation(const FVector& SlotLocation) const
{
	const FVector Boss = BossLocation();
	const FVector Outward = SlotLocation - Boss;
	const float Distance = Outward.Size();

	//보스에서 본 각도를 그대로 두고 거리만 줄인다. 구역은 보스를 꼭짓점으로 하는
	//부채꼴이므로, 각도가 그대로면 아무리 당겨도 자기 구역을 벗어나지 않는다
	if (Distance <= ChargeDistance || Distance <= KINDA_SMALL_NUMBER)
	{
		return SlotLocation;
	}

	return Boss + Outward / Distance * ChargeDistance;
}

FVector ABattleManager::BossLocation() const
{
	return GetActorLocation() + FVector(ArenaRadius * BossDistanceRatio, 0.f, 0.f);
}

void ABattleManager::GatherSpots()
{
	for (TArray<FVector>& Lane : Spots)
	{
		Lane.Reset();
	}

	bHasBoundary[0] = bHasBoundary[1] = false;

	//레벨에 놓인 것을 전부 자동으로 찾는다. 배열에 등록할 필요가 없다
	for (TActorIterator<ABattleSpot> It(GetWorld()); It; ++It)
	{
		if (It->bIsBoundary)
		{
			//경계 표식은 자리가 아니다. Mid는 좌/중인지 중/우인지 알 수 없어 거른다
			if (It->Lane == EBattleLane::Mid)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Battle] 경계 표식의 Lane이 Mid입니다 — 좌/중이면 Left, 중/우면 Right로 두세요"));
				continue;
			}

			const int32 Index = It->Lane == EBattleLane::Left ? 0 : 1;
			BoundaryPoints[Index] = It->GetActorLocation();
			bHasBoundary[Index] = true;
			continue;
		}

		Spots[(int32)It->Lane].Add(It->GetActorLocation());
	}

	//보스에서 가까운 순으로 채운다. 번호를 손으로 매기지 않아도 되게
	const FVector Boss = BossLocation();
	for (TArray<FVector>& Lane : Spots)
	{
		Lane.Sort([&Boss](const FVector& A, const FVector& B)
		{
			return FVector::DistSquared(A, Boss) < FVector::DistSquared(B, Boss);
		});
	}

	const int32 Total = Spots[0].Num() + Spots[1].Num() + Spots[2].Num();
	if (Total == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] BattleSpot이 없어 격자 계산으로 배치합니다"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] BattleSpot %d개 (좌 %d / 중 %d / 우 %d) / 경계 표식 좌중 %s 중우 %s"),
		Total, Spots[0].Num(), Spots[1].Num(), Spots[2].Num(),
		bHasBoundary[0] ? TEXT("있음") : TEXT("없음"),
		bHasBoundary[1] ? TEXT("있음") : TEXT("없음"));

	if (!bHasBoundary[0] || !bHasBoundary[1])
	{
		//없어도 돌아가지만, 회피가 구역을 넘는 것처럼 보이지 않는다
		UE_LOG(LogTemp, Warning,
			TEXT("[Battle] 구역 경계 표식이 모자랍니다 — 회피 구르기가 경계를 넘지 않고 자리에 내려앉습니다"));
	}

	for (int32 i = 0; i < 3; ++i)
	{
		if (Spots[i].Num() > 0 && Spots[i].Num() < MaxPlayers)
		{
			// ponytail: 스팟보다 인원이 많으면 자리가 겹친다.
			// 기획서에도 '회의/테스트 후 수정'으로 남아 있는 항목이라 지금은 경고만 남긴다
			UE_LOG(LogTemp, Warning, TEXT("[Battle] %s 구역 스팟이 %d개뿐입니다 (정원 %d) — 몰리면 겹칩니다"),
				*StaticEnum<EBattleLane>()->GetNameStringByValue(i), Spots[i].Num(), MaxPlayers);
		}
	}
}

void ABattleManager::GenerateSpotsFromGrid()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	//이미 놓인 게 있으면 손댄 배치를 덮어쓰지 않는다
	if (TActorIterator<ABattleSpot>(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] 이미 BattleSpot이 있습니다. 다시 만들려면 먼저 지우세요"));
		return;
	}

	int32 Created = 0;
	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		const FString LaneName = StaticEnum<EBattleLane>()->GetNameStringByValue(LaneIndex);

		for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
		{
			//스팟을 만드는 중이므로 스팟을 참조하지 않는 격자 계산을 직접 쓴다
			ABattleSpot* Spot = World->SpawnActor<ABattleSpot>(
				GridSlotLocation((EBattleLane)LaneIndex, Slot), FRotator::ZeroRotator);
			if (!Spot)
			{
				continue;
			}

			Spot->Lane = (EBattleLane)LaneIndex;
			++Created;

#if WITH_EDITOR
			//아웃라이너에서 구역별로 접어 볼 수 있게 정리해 둔다
			Spot->SetActorLabel(FString::Printf(TEXT("Spot_%s_%02d"), *LaneName, Slot));
			Spot->SetFolderPath(FName(*FString::Printf(TEXT("BattleSpots/%s"), *LaneName)));
#endif
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] BattleSpot %d개를 격자 위치에 만들었습니다"), Created);
}

FVector ABattleManager::GridSlotLocation(EBattleLane Lane, int32 SlotIndex) const
{
	//레인 중심각. 보스에서 중심을 바라본 방향(-X)이 가운데 레인이다
	const float LaneCenter = (Lane == EBattleLane::Left) ? LaneAngle
		: (Lane == EBattleLane::Right) ? -LaneAngle : 0.f;

	//부채꼴 안을 격자로 채운다. 열은 각도로, 행은 보스로부터의 거리로 벌린다
	const int32 Cols = FMath::Max(1, SlotColumns);
	const int32 Col = SlotIndex % Cols;
	const int32 Row = SlotIndex / Cols;

	const float ColStep = LaneAngle * ColumnSpreadRatio / Cols;
	const float ColOffset = (Col - (Cols - 1) * 0.5f) * ColStep;
	const float Distance = ArenaRadius * (LaneDistanceRatio + Row * RowSpacingRatio);

	const FVector Dir = FRotator(0.f, 180.f + LaneCenter + ColOffset, 0.f).RotateVector(FVector::ForwardVector);
	return BossLocation() + Dir * Distance;
}

#if WITH_EDITOR
void ABattleManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	FlushPersistentDebugLines(World);
	if (!bPreviewSlots)
	{
		return;
	}

	DrawArenaGuides();

	//BattleSpot을 놓았으면 그 액터들이 곧 자리다. 격자 점을 같이 그리면
	//옮겨둔 스팟과 옛 격자가 겹쳐 보여 헷갈린다
	if (TActorIterator<ABattleSpot>(World))
	{
		return;
	}

	//자리 하나당 점 하나. 레인별로 색을 달리해 구분한다
	const FColor LaneColors[] = { FColor::Green, FColor::Yellow, FColor::Blue };
	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
		{
			DrawDebugSphere(World, GridSlotLocation((EBattleLane)LaneIndex, Slot),
				25.f, 8, LaneColors[LaneIndex], true, -1.f, 0, 2.f);
		}
	}
}
#endif

void ABattleManager::DrawArenaGuides() const
{
	const FVector Center = GetActorLocation();

	//YAxis/ZAxis를 넘겨 수평 원으로 그린다
	DrawDebugCircle(GetWorld(), Center, ArenaRadius, 48, FColor::Black, true, -1.f, 0, 5.f,
		FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);

	//레인 중심각의 절반 지점이 경계. 보스에서 투기장 테두리까지만 뻗는다
	const float HalfAngle = LaneAngle * 0.5f;
	const FVector Start = BossLocation();
	const FVector Offset = Start - Center;

	for (const float Side : { -HalfAngle, HalfAngle })
	{
		const FVector Dir = FRotator(0.f, 180.f + Side, 0.f).RotateVector(FVector::ForwardVector);

		//원과 만나는 지점까지의 거리 (레이-원 교차)
		const float B = FVector::DotProduct(Offset, Dir);
		const float Length = -B + FMath::Sqrt(FMath::Max(0.f, B * B - Offset.SizeSquared() + ArenaRadius * ArenaRadius));

		DrawDebugLine(GetWorld(), Start, Start + Dir * Length, FColor(160, 160, 160), true, -1.f, 0, 6.f);
	}
}

// ---------------- 참가 / 입력 ----------------

bool ABattleManager::ParseCommand(const FString& Text, EBattleAction& OutAction)
{
 FParsedCommand Command;
 return FChatCommandParser::TryParse(TEXT(""), Text, Command)
  && FChatCommandParser::TryGetBattleAction(Command.CommandType, OutAction);
}

// ---------------- 저장 ----------------

FString ABattleManager::SaveFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("PlayerData.json");
}

void ABattleManager::SaveRecords()
{
 if (bRecordLoadFailed) return;
 for (const auto& Pair : Players)
 {
  if (FPlayerRecord* Record = Records.Find(Pair.Key)) Record->CurrentHealth = Pair.Value.HP;
 }
	FPlayerRecordFile File;
	Records.GenerateValueArray(File.Records);

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(File, Json))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 기록 직렬화 실패"));
		return;
	}

	//인코딩을 지정하지 않으면 한글 핸들이 섞였을 때 UTF-16으로 저장돼
	//외부 도구에서 열기 번거로워진다. 읽을 때는 자동 판별되므로 저장만 고정하면 된다
	if (!FFileHelper::SaveStringToFile(Json, *RecordFilePath(),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 기록 저장 실패: %s"), *RecordFilePath());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] 유저 기록 %d명 저장"), File.Records.Num());
}

bool ABattleManager::ResetSavedUserData(bool bTestUsersOnly)
{
 InitializePlayerRecords();
 // A selective reset cannot preserve real users if their source file could not be read.
 if (bTestUsersOnly && bRecordLoadFailed) return false;
 auto Matches = [bTestUsersOnly](const FString& Id)
 {
  if (!bTestUsersOnly) return true;
  const FString Prefix = TEXT("@testuser");
  if (!Id.StartsWith(Prefix, ESearchCase::CaseSensitive)) return false;
  const FString Suffix = Id.Mid(Prefix.Len());
  if (Suffix.IsEmpty()) return false;
  for (TCHAR Digit : Suffix) if (Digit < TEXT('0') || Digit > TEXT('9')) return false;
  return true;
 };
 auto Remaining = Records;
 for (const auto& Pair : Players)
  if (FPlayerRecord* Record = Remaining.Find(Pair.Key)) Record->CurrentHealth = Pair.Value.HP;
 for (auto It = Remaining.CreateIterator(); It; ++It)
  if (Matches(It.Key())) It.RemoveCurrent();
 FPlayerRecordFile File;
 Remaining.GenerateValueArray(File.Records);
 FString Json;
 // Persist first: failed writes must leave live participants and progression intact.
 if (!FJsonObjectConverter::UStructToJsonObjectString(File, Json)
     || !FFileHelper::SaveStringToFile(Json, *RecordFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
 {
  UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 데이터 초기화 저장 실패: %s"), *RecordFilePath());
  return false;
 }
 if (Matches(CutSceneUserId))
 {
  StopTurnCutScenes();
  CutSceneUserId.Reset();
  CutSceneIds.Reset();
 }
 TArray<FString> RemoveIds;
 for (const auto& Pair : Players) if (Matches(Pair.Key)) RemoveIds.Add(Pair.Key);
 for (const FString& Id : RemoveIds) RemovePlayer(Id);
 Records = MoveTemp(Remaining);
 bRecordLoadFailed = false;
 for (auto It = SelectedCutSceneUsers.CreateIterator(); It; ++It)
  if (Matches(*It)) It.RemoveCurrent();
 if (!bTestUsersOnly && bBattleRunning) StartBattle();
 UpdateVisuals();
 RefreshPlayerUI();
 UE_LOG(LogTemp, Log, TEXT("[Battle] %s 유저 데이터 초기화 완료"), bTestUsersOnly ? TEXT("테스트") : TEXT("전체"));
 return true;
}

void ABattleManager::LoadRecords()
{
 bRecordLoadFailed = false;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *RecordFilePath()))
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] 저장된 유저 기록 없음 — 새로 시작합니다"));
		return;
	}

	FPlayerRecordFile File;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &File))
	{
		bRecordLoadFailed = true;
		//파일이 깨졌다면 덮어쓰지 않고 남겨둔다.
		UE_LOG(LogTemp, Error, TEXT("[Battle] 유저 기록을 읽지 못했습니다: %s"), *RecordFilePath());
		return;
	}

	Records.Empty();
 bool bMigrated = false;
 for (FPlayerRecord Record : File.Records)
 {
  // Old records lack currentHealth. Start them at the class's base maximum.
  if (Record.CurrentHealth < 0) { Record.CurrentHealth = MaxHPFor(Record); bMigrated = true; }
  Record.CurrentHealth = FMath::Clamp(Record.CurrentHealth, 0, MaxHPFor(Record));
  Records.Add(Record.UserId, Record);
 }
 if (bMigrated) SaveRecords();

	UE_LOG(LogTemp, Log, TEXT("[Battle] 유저 기록 %d명 불러옴"), Records.Num());
}

int32 ABattleManager::MaxHPFor(const FPlayerRecord& Record) const
{
	return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Classes[ClassIndexFor(Record.CharacterId)].BaseHP)
		+ static_cast<int64>(Record.MaxHPBonus) * HPPerBonus, 5, 6));
}

void ABattleManager::ApplyRecordToPlayer(const FPlayerRecord& Record, FBattlePlayer& Player) const
{
	Player.ClassIndex = ClassIndexFor(Record.CharacterId);
	Player.MaxHP = MaxHPFor(Record);
	Player.DamageBonus = Record.DamageBonus * DamagePerBonus;
	Player.CritChanceBonus = Record.CritChanceBonus * CritChancePerBonus;
	Player.CritDamageBonus = Record.CritDamageBonus * CritDamagePerBonus;
}

void ABattleManager::ChooseClassSpots(const TArray<FVector>& Candidates, const TArray<int32>& CountsByClass,
	const FVector& Anchor, TArray<TArray<int32>>& OutIndicesByClass)
{
	OutIndicesByClass.Reset();
	OutIndicesByClass.SetNum(CountsByClass.Num());

	int32 Total = 0;
	for (const int32 Count : CountsByClass)
	{
		Total += Count;
	}

	if (Candidates.Num() == 0 || Total <= 0)
	{
		return;
	}

	//후보를 보스에서 가까운 순으로 늘어놓는다. 이 순서가 곧 앞뒤다
	TArray<int32> Order;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Order.Add(i);
	}
	Order.Sort([&Candidates, &Anchor](const int32 A, const int32 B)
	{
		return FVector::DistSquared(Candidates[A], Anchor) < FVector::DistSquared(Candidates[B], Anchor);
	});

	//인원 비율로 구간을 쪼갠다. 자리가 인원보다 적으면 구간이 겹칠 수 있는데,
	//그건 ChooseSpreadSpots가 '앞에서부터 다시 쓰기'로 처리한다
	int32 Cursor = 0;
	int32 Remaining = Total;

	for (int32 ClassIdx = 0; ClassIdx < CountsByClass.Num(); ++ClassIdx)
	{
		const int32 Count = CountsByClass[ClassIdx];
		if (Count <= 0)
		{
			continue;
		}

		//뒤에 아직 사람이 남은 직업군이 몇 개인지. 그만큼은 자리를 남겨 둬야
		//뒤 직업군이 앞 직업군의 자리를 다시 쓰는 일이 없다
		int32 ClassesAfter = 0;
		for (int32 j = ClassIdx + 1; j < CountsByClass.Num(); ++j)
		{
			ClassesAfter += CountsByClass[j] > 0 ? 1 : 0;
		}

		//남은 자리를 남은 인원으로 나눠 이 직업군의 몫을 정한다
		const int32 Left = Candidates.Num() - Cursor;
		const int32 Share = ClassesAfter == 0
			? FMath::Max(Left, 1)
			: FMath::Clamp(FMath::RoundToInt((float)Left * Count / Remaining),
				1, FMath::Max(Left - ClassesAfter, 1));

		TArray<FVector> Slice;
		TArray<int32> SliceToCandidate;
		for (int32 i = 0; i < Share && Cursor + i < Order.Num(); ++i)
		{
			SliceToCandidate.Add(Order[Cursor + i]);
			Slice.Add(Candidates[Order[Cursor + i]]);
		}

		if (Slice.Num() == 0)
		{
			//자리가 바닥났으면 맨 뒤 자리를 같이 쓴다
			SliceToCandidate.Add(Order.Last());
			Slice.Add(Candidates[Order.Last()]);
		}

		//맨 뒤 직업군은 자기 구간의 '뒤에서부터' 채운다.
		//그래서 기준점을 보스에서 가장 먼 자리로 준다
		const FVector SliceAnchor = ClassesAfter == 0 ? Slice.Last() : Anchor;

		TArray<int32> Picked;
		ChooseSpreadSpots(Slice, Count, SliceAnchor, Picked);

		for (const int32 Index : Picked)
		{
			OutIndicesByClass[ClassIdx].Add(SliceToCandidate[Index]);
		}

		Cursor += Share;
		Remaining -= Count;
	}
}

void ABattleManager::ChooseSpreadSpots(const TArray<FVector>& Candidates, int32 Count,
	const FVector& Anchor, TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	if (Candidates.Num() == 0 || Count <= 0)
	{
		return;
	}

	//첫 자리는 보스에 가장 가까운 곳. 인원이 적어도 앞줄이 비지 않는다
	int32 First = 0;
	for (int32 i = 1; i < Candidates.Num(); ++i)
	{
		if (FVector::DistSquared(Candidates[i], Anchor) < FVector::DistSquared(Candidates[First], Anchor))
		{
			First = i;
		}
	}
	OutIndices.Add(First);

	while (OutIndices.Num() < Count)
	{
		//자리가 인원보다 적으면 앞에서부터 다시 쓴다. 겹치지만 아무도 빠지지는 않는다.
		//Add에 자기 배열의 원소를 직접 넘기면 재할당 때 끊어지므로 값으로 복사해 둔다
		if (OutIndices.Num() >= Candidates.Num())
		{
			const int32 Reused = OutIndices[OutIndices.Num() - Candidates.Num()];
			OutIndices.Add(Reused);
			continue;
		}

		//이미 고른 자리들에서 가장 멀리 떨어진 후보를 하나씩 더한다
		int32 Best = INDEX_NONE;
		float BestDistance = -1.f;

		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			if (OutIndices.Contains(i))
			{
				continue;
			}

			float Nearest = TNumericLimits<float>::Max();
			for (const int32 Taken : OutIndices)
			{
				Nearest = FMath::Min(Nearest, FVector::DistSquared(Candidates[i], Candidates[Taken]));
			}

			if (Nearest > BestDistance)
			{
				BestDistance = Nearest;
				Best = i;
			}
		}

		OutIndices.Add(Best);
	}
}

bool ABattleManager::SubmitChat(const FString& UserId, const FString& DisplayName,
	const FString& Text, const FDateTime& SentAt)
{
	EBattleAction Action;
	if (!ParseCommand(Text, Action))
	{
		return false;
	}

	//실측 지연. CollectSeconds를 추측이 아니라 이 값을 보고 맞춘다.
	// ponytail: 지금은 로그만 남긴다. 지연 편차가 CollectSeconds를 넘기 시작하면
	// 그때 SentAt으로 어느 턴에 속하는지 직접 계산하도록 바꾼다
	if (SentAt != FDateTime())
	{
		const double DelaySeconds = (FDateTime::UtcNow() - SentAt).GetTotalSeconds();
		UE_LOG(LogTemp, Verbose, TEXT("[Battle] 채팅 지연 %.1f초 (%s)"), DelaySeconds, *UserId);

		if (DelaySeconds > PlayerSelectCountdown + CollectSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Battle] 지연 %.1f초 — 입력+수집 시간을 넘었습니다. CollectSeconds를 늘리세요"), DelaySeconds);
		}
	}

	return SubmitAction(UserId, DisplayName, Action);
}

bool ABattleManager::SubmitAction(const FString& UserId, const FString& DisplayName, EBattleAction Action,
	const FString& CharacterId)
{
	if (UserId.TrimStartAndEnd().IsEmpty() || !IsAcceptingInput())
	{
		return false;
	}

	FBattlePlayer* Player = Players.Find(UserId);

	if (!Player)
	{
		if (Players.Num() >= MaxPlayers)
		{
			// ponytail: 대기열은 YouTube 댓글 응답이 필요해 아직 없다. 지금은 그냥 거절
			return false;
		}

		FPlayerRecord& Record = GetOrCreateRecord(UserId, DisplayName, CharacterId);

		FBattlePlayer NewPlayer;
		NewPlayer.DisplayName = DisplayName;
		NewPlayer.Lane = Balance.InitialZone;
		NewPlayer.Fever = Balance.InitialFever;
		NewPlayer.EvadeStreak = Balance.InitialEvadeStreak;
		ApplyRecordToPlayer(Record, NewPlayer);
  if (Record.CurrentHealth < 0) Record.CurrentHealth = NewPlayer.MaxHP;
  NewPlayer.HP = FMath::Clamp(Record.CurrentHealth, 0, NewPlayer.MaxHP);
  Record.CurrentHealth = NewPlayer.HP;
  if (NewPlayer.HP == 0) NewPlayer.ReviveCountdown = FMath::Max(1, ReviveTurns);
		//직업군에 지정된 블루프린트로 스폰한다
		const TSubclassOf<ABattleUnit>* UnitClass = UnitClassByCharacter.Find(Record.CharacterId);
		NewPlayer.Unit = GetWorld()->SpawnActor<ABattleUnit>(
			(UnitClass && *UnitClass) ? UnitClass->Get() : ABattleUnit::StaticClass(),
			BossLocation(), FRotator::ZeroRotator);
		//바로 안 불러주면 다음 판정 때까지 컴포넌트 기본 문자열이 보인다
		NewPlayer.Unit->SetLabel(DisplayName, NewPlayer.HP, NewPlayer.MaxHP);
		NewPlayer.Unit->OnHitLanded.AddUObject(this, &ABattleManager::HandleHitLanded, UserId);

		//플레이어는 평소에 보스를 본다. 이동 중에만 진행 방향을 보고 도착하면 되돌아온다.
		//보스에게는 지정하지 않는다 — 스폰할 때 준 각도를 그대로 유지해야 한다
		NewPlayer.Unit->SetFacingTarget(BossLocation());
#if WITH_EDITOR
		NewPlayer.Unit->SetActorLabel(UserId);
#endif

		Player = &Players.Add(UserId, NewPlayer);
  SessionStats.RegisterPlayer(UserId, DisplayName, Record.CharacterId);
  SaveRecords();

		//새 참가자가 낄 자리를 포함해 그 구역을 다시 배분한다.
		//본인은 바로 세우고, 자리가 밀린 사람들은 그 즉시 비켜 준다 —
		//다음 이동 단계까지 미루면 그 사이 두 명이 같은 자리에 겹쳐 보인다
		AssignSlots();
		for (const TPair<FString, FBattlePlayer>& Pair : Players)
		{
			const FBattlePlayer& Other = Pair.Value;
			if (!Other.Unit)
			{
				continue;
			}

			if (Pair.Key == UserId)
			{
				Other.Unit->SetActorLocation(Other.TargetLocation);
			}
			else
			{
				Other.Unit->MoveTo(Other.TargetLocation, MoveSeconds);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[Battle] 참가: %s [%s] (HP %d, 피해+%d, 치명+%.0f%%/+%.0f%%) — 현재 %d/%d명"),
			*UserId, *Record.CharacterId, NewPlayer.MaxHP, NewPlayer.DamageBonus,
			NewPlayer.CritChanceBonus * 100.f, NewPlayer.CritDamageBonus * 100.f, Players.Num(), MaxPlayers);
	}

	//핸들은 바뀔 수 있으니 매번 갱신
	Player->DisplayName = DisplayName;
	if (FPlayerRecord* Record = Records.Find(UserId))
	{
		Record->DisplayName = DisplayName;
	}

	//사망 중이면 입력을 받지 않는다
	if (Player->ReviveCountdown > 0)
	{
		return false;
	}

	//서 있는 구역으로 다시 회피하는 것을 막는 설정이면 거절한다
	if (!bAllowStayEvade && IsEvade(Action) && ApplyMove(Action, Player->Lane) == Player->Lane)
	{
		return false;
	}

	//마지막에 들어온 입력이 이김
	Player->PendingAction = Action;
	Player->bActedThisTurn = true;

	//고른 즉시 준비 자세로 바꾼다. 실제 행동은 행동 턴에 일괄 재생되므로
	//여기서 공격 모션을 틀면 중복으로 때리는 것처럼 보인다
	if (Player->Unit && Player->ReviveCountdown == 0)
	{
		Player->Unit->NotifyActionSelected(Action);
	}
	RefreshPlayerUI();

	return true;
}

// ---------------- 턴 ----------------

void ABattleManager::StartTurn()
{
 if (!bBattleRunning) return;
 StopTurnCutScenes();
 ++TurnIndex;
 Phase = EBattlePhase::Prepare;
 //상단 UI: Prepare는 예고 + 선택 시간이 끝날 때(Collect 시작)까지
 InputPhaseDuration = FMath::Max(0.f, TelegraphSeconds) + FMath::Max(0.f, PlayerSelectCountdown);
 InputPhaseEndTime = GetWorld()->GetTimeSeconds() + InputPhaseDuration;
 if (AYouTubeChatManager* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass())))
  Chat->SetCombatAutoPollingEnabled(true);

	//보스 행동을 먼저 정해 예고한다.
	//판정 시점에 정하면 회피가 순수 확률 찍기가 되어 선택지의 의미가 없어진다
	TelegraphedAction = NextBossAction();

	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		Pair.Value.PendingAction = EBattleAction::Idle;
		Pair.Value.EarlyHitReports = 0;

		//준비 자세를 풀라는 신호. 유닛은 턴이 바뀐 걸 알 방법이 없어서
		//여기서 Idle을 한 번 쏴 줘야 지난 턴 자세로 계속 서 있지 않는다
		if (Pair.Value.Unit && Pair.Value.ReviveCountdown == 0)
		{
			Pair.Value.Unit->NotifyActionSelected(EBattleAction::Idle);

			//보스가 그 구역을 치지 않아 OnHit이 오지 않은 방어자가 있다.
			//여기서 풀지 않으면 다음 턴까지 막을 들고 서 있는다
			Pair.Value.Unit->SetDefending(false);
		}

		Pair.Value.bActedThisTurn = false;
	}

	//예고 자세. 플레이어가 자세만 보고도 피격 범위를 짐작할 수 있어야 한다
	if (BossUnit)
	{
		BossUnit->OnBossTelegraph(TelegraphedAction.TelegraphMotion(), TelegraphedAction.TelegraphText);
	}

	//암시 문구는 보스 유닛(모션)과 별개로 UI 등 구독자에게 알린다
	OnBossTelegraph.Broadcast(TelegraphedAction.TelegraphMotion(), TelegraphedAction.TelegraphText);

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d/%d 예고 — %.1f초 / %d명 / '%s' → %s"),
		TurnIndex, Turns, TelegraphSeconds, Players.Num(),
		*TelegraphedAction.Name, *ZonesLabel(TelegraphedAction));

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BeginInput, FMath::Max(0.001f, TelegraphSeconds), false);
	RefreshPlayerUI();
}

void ABattleManager::BeginInput()
{
	if (!bBattleRunning || Phase != EBattlePhase::Prepare) return;

	//예고 모션이 끝나고 플레이어가 고르는 동안의 대기 자세
	if (BossUnit)
	{
		BossUnit->OnBossWait();
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 선택 — %.1f초"), TurnIndex, PlayerSelectCountdown);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BeginCollect, FMath::Max(0.001f, PlayerSelectCountdown), false);
}

void ABattleManager::BeginCollect()
{
	//시청자 화면에서는 아직 입력 시간이다. 늦게 도착하는 채팅을 계속 받는다
	Phase = EBattlePhase::Collect;
	//상단 UI: Collect는 수집 시간이 끝날 때(Move 시작)까지 다시 가득 찬 상태에서 시작
	InputPhaseDuration = FMath::Max(0.f, CollectSeconds);
	InputPhaseEndTime = GetWorld()->GetTimeSeconds() + InputPhaseDuration;

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 수집 — %.1f초"), TurnIndex, CollectSeconds);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BeginMove, FMath::Max(0.001f, CollectSeconds), false);
	RefreshPlayerUI();
}

TArray<FName> ABattleManager::CutSceneIdsFor(const FString& ClassId, EBattleAction Action)
{
 if (Action != EBattleAction::Attack1 && Action != EBattleAction::Attack2) return {};
 if (ClassId == TEXT("gladius"))
  return {TEXT("GladiusRun"), Action == EBattleAction::Attack1 ? FName(TEXT("GladiusSkill1")) : FName(TEXT("GladiusSkill2"))};
 if (ClassId == TEXT("sera"))
  return {Action == EBattleAction::Attack1 ? FName(TEXT("SeraSkill1")) : FName(TEXT("SeraSkill2"))};
 return {};
}

FString ABattleManager::SelectTurnCutSceneUser(const TMap<FString, FBattlePlayer>& Candidates,
 const TArray<FCharacterDef>& CharacterClasses, const FString& PreviousClassId,
 const TSet<FString>& PreviouslySelectedUsers, FRandomStream& Random)
{
 TArray<FString> Ids;
 int32 HighestFever = MIN_int32;
 for (const auto& Pair : Candidates)
 {
  const FBattlePlayer& P = Pair.Value;
  if (P.HP <= 0 || P.ReviveCountdown > 0 || !IsValid(P.Unit) || !CharacterClasses.IsValidIndex(P.ClassIndex)
   || CutSceneIdsFor(CharacterClasses[P.ClassIndex].Id, P.PendingAction).IsEmpty()) continue;
  if (P.Fever > HighestFever) { HighestFever = P.Fever; Ids.Reset(); }
  if (P.Fever == HighestFever) Ids.Add(Pair.Key);
 }
 if (Ids.IsEmpty()) return FString();
 // Fever is the first priority; class preference breaks ties in that top group.
 TArray<FString> Preferred;
 for (const FString& Id : Ids)
 {
  const FString& ClassId = CharacterClasses[Candidates.FindChecked(Id).ClassIndex].Id;
  if (PreviousClassId.IsEmpty() ? ClassId == TEXT("gladius") : ClassId != PreviousClassId) Preferred.Add(Id);
 }
 if (!Preferred.IsEmpty()) Ids = MoveTemp(Preferred);
 TArray<FString> Unseen;
 for (const FString& Id : Ids) if (!PreviouslySelectedUsers.Contains(Id)) Unseen.Add(Id);
 if (!Unseen.IsEmpty()) Ids = MoveTemp(Unseen);
 Ids.Sort();
 return Ids[Random.RandRange(0, Ids.Num()-1)];
}

void ABattleManager::StartMoveCutScenes()
{
 if (!bBattleRunning || Phase != EBattlePhase::Move) return;
 if (AYouTubeChatManager* Chat = Cast<AYouTubeChatManager>(UGameplayStatics::GetActorOfClass(this, AYouTubeChatManager::StaticClass())))
 {
  Chat->SetCombatAutoPollingEnabled(false);
  Chat->CancelPendingChatFetch();
 }
 CutSceneUserId.Reset();
 CutSceneIds.Reset();
 ActiveCutSceneIds.Reset();
 CutSceneIndex = 0;
 CutSceneAttackUserId.Reset();
 if (!bEnableTurnCutScenes) return;
 CutSceneUserId = SelectTurnCutSceneUser(Players, Classes, PreviousCutSceneClassId, SelectedCutSceneUsers, CutSceneRandom);
 const FBattlePlayer* Selected = Players.Find(CutSceneUserId);
 if (!Selected) return;
 const FString ClassId = Classes[Selected->ClassIndex].Id;
 CutSceneIds = CutSceneIdsFor(ClassId, Selected->PendingAction);
 ActiveCutSceneIds = CutSceneIds;

 PreviousCutSceneClassId = ClassId;
 SelectedCutSceneUsers.Add(CutSceneUserId);
 if (!IsValid(CutSceneApplier))
  CutSceneApplier = Cast<ACutSceneApplier>(UGameplayStatics::GetActorOfClass(this, ACutSceneApplier::StaticClass()));
 if (!IsValid(CutSceneApplier)) CutSceneApplier = GetWorld()->SpawnActor<ACutSceneApplier>();
 if (IsValid(CutSceneApplier))
 {
  CutSceneApplier->BattleManager = this;
  CutSceneApplier->EnsureDefaultCutScenes();
  CutSceneApplier->OnCutSceneEnded.RemoveDynamic(this, &ABattleManager::HandleCutSceneEnded);
  CutSceneApplier->CancelCutScene();
  if (!bBattleRunning || Phase != EBattlePhase::Move) return;
  CutSceneApplier->OnCutSceneEnded.AddUniqueDynamic(this, &ABattleManager::HandleCutSceneEnded);
 }

 PlayNextCutScene();
 if (ClassId == TEXT("gladius") && IsValid(Selected->Unit))
 {
  if (IsValid(CutSceneApplier) && CutSceneApplier->IsPlaying() && CutSceneApplier->GetCurrentSubject() == Selected->Unit)
   WalkRunOnlyCutSceneUnit = Selected->Unit;
  Selected->Unit->OnAttackStarted.RemoveAll(this);
  Selected->Unit->OnAttackStarted.AddUObject(this, &ABattleManager::HandleSelectedGladiusAttackStarted);
  Selected->Unit->OnMovementFinished.RemoveAll(this);
  Selected->Unit->OnMovementFinished.AddUObject(this, &ABattleManager::StartSelectedGladiusAttack);
  if (!Selected->Unit->IsMoving()) StartSelectedGladiusAttack();
 }
}

void ABattleManager::HandleSelectedGladiusAttackStarted(int32 AttackIndex)
{
 // Also support designer-provided run sequences; the camera changes only after the montage start.
 if ((AttackIndex == 1 || AttackIndex == 2) && bBattleRunning && CutSceneIndex == 1
     && CutSceneIndex < ActiveCutSceneIds.Num() && IsValid(CutSceneApplier) && CutSceneApplier->IsPlaying())
  CutSceneApplier->CancelCutScene();
}

void ABattleManager::StartSelectedGladiusAttack()
{
 if (!bBattleRunning || (Phase != EBattlePhase::Move && Phase != EBattlePhase::BossAction)
     || CutSceneAttackUserId == CutSceneUserId) return;
 const FBattlePlayer* P = Players.Find(CutSceneUserId);
 if (!P || !IsValid(P->Unit) || P->HP <= 0 || P->ReviveCountdown > 0
     || !Classes.IsValidIndex(P->ClassIndex) || Classes[P->ClassIndex].Id != TEXT("gladius")
     || (P->PendingAction != EBattleAction::Attack1 && P->PendingAction != EBattleAction::Attack2)) return;
 P->Unit->OnMovementFinished.RemoveAll(this);
 const int32 AttackIndex = P->PendingAction == EBattleAction::Attack1 ? 1 : 2;
 const FAttackDef& Attack = Classes[P->ClassIndex].Attacks[AttackIndex - 1];
 CutSceneAttackUserId = CutSceneUserId;
 CutSceneAttackStartTime = GetWorld()->GetTimeSeconds();
 bCutSceneAttackCrit = FMath::FRand() < FMath::Min(Attack.CritChance + P->CritChanceBonus, CritChanceCap);
 P->Unit->PerformSkill(AttackIndex, bCutSceneAttackCrit, true);
}

void ABattleManager::PlayNextCutScene()
{
 if (!bBattleRunning || (Phase != EBattlePhase::Move && Phase != EBattlePhase::BossAction)) return;
 while (IsValid(CutSceneApplier) && CutSceneIndex < ActiveCutSceneIds.Num())
 {
  const FName Id = ActiveCutSceneIds[CutSceneIndex++];
  if (CutSceneApplier->ApplyCutSceneForUser(CutSceneUserId, Id))
  {
   if (Id == TEXT("SeraSkill1") || Id == TEXT("SeraSkill2"))
   {
    if (const FBattlePlayer* P = Players.Find(CutSceneUserId); P && IsValid(P->Unit) && Classes.IsValidIndex(P->ClassIndex))
    {
     // The camera starts the skill immediately, independently of movement and attack groups.
     CutSceneAttackUserId = CutSceneUserId;
     CutSceneAttackStartTime = GetWorld()->GetTimeSeconds();
     const int32 AttackIndex = P->PendingAction == EBattleAction::Attack1 ? 1 : 2;
     const FAttackDef& Attack = Classes[P->ClassIndex].Attacks[AttackIndex - 1];
     bCutSceneAttackCrit = FMath::FRand() < FMath::Min(Attack.CritChance + P->CritChanceBonus, CritChanceCap);
     P->Unit->PerformSkill(AttackIndex, bCutSceneAttackCrit, true);
    }
   }
   RefreshPlayerUI();
   return;
  }
 }
 RefreshPlayerUI();
}

void ABattleManager::StopTurnCutScenes()
{
 for (const auto& Pair : Players)
  if (IsValid(Pair.Value.Unit))
  {
   Pair.Value.Unit->OnMovementFinished.RemoveAll(this);
   Pair.Value.Unit->OnAttackStarted.RemoveAll(this);
  }
 CutSceneAttackUserId.Reset();
 if (IsValid(CutSceneApplier))
 {
  CutSceneApplier->OnCutSceneEnded.RemoveDynamic(this, &ABattleManager::HandleCutSceneEnded);
  CutSceneApplier->CancelCutScene();
 }
 ActiveCutSceneIds.Reset();
 CutSceneIndex = 0;
}

void ABattleManager::HandleCutSceneEnded(FName Type, bool bInterrupted)
{
 PlayNextCutScene();
}

void ABattleManager::InitializePlayerRecords()
{
 if (bRecordsInitialized) return;
 ApplyBalanceFiles();
 LoadRecords();
 bRecordsInitialized = true;
}

FPlayerRecord& ABattleManager::GetOrCreateRecord(const FString& UserId, const FString& DisplayName, const FString& CharacterId)
{
 const bool bNew = !Records.Contains(UserId);
 FPlayerRecord& Record = Records.FindOrAdd(UserId);
 Record.UserId = UserId;
 if (!DisplayName.IsEmpty()) Record.DisplayName = DisplayName;
 if (Record.CharacterId.IsEmpty()) Record.CharacterId = CharacterId.IsEmpty() ? DefaultCharacterId : CharacterId;
 if (bNew)
 {
  Record.MaxHPBonus = Balance.InitialHPLevel;
  Record.DamageBonus = Balance.InitialDamageLevel;
  Record.CritChanceBonus = Balance.InitialCritChanceLevel;
  Record.CritDamageBonus = Balance.InitialCritDamageLevel;
  Record.CurrentHealth = MaxHPFor(Record);
 }
 if (Record.CurrentHealth < 0) Record.CurrentHealth = MaxHPFor(Record);
 return Record;
}

bool ABattleManager::ApplyTempleEnhancement(const FString& UserId, const FString& DisplayName, int32 Tier)
{
 if (UserId.TrimStartAndEnd().IsEmpty() || Tier < 1 || Tier > 4 || IsBattleRunning()) return false;
 InitializePlayerRecords();
 if (bRecordLoadFailed) return false;
 //축복 횟수는 전투가 끝날 때 참가자에게만 지급된다. 남은 횟수가 없으면 적용하지 않음
 FPlayerRecord* Existing = Records.Find(UserId);
 if (!Existing || Existing->BlessingsAvailable <= 0)
 {
  UE_LOG(LogTemp, Log, TEXT("[Temple] 남은 축복이 없어 강화 무시: %s (강화 %d)"), *UserId, Tier);
  return false;
 }
 FPlayerRecord& Record = GetOrCreateRecord(UserId, DisplayName);
 --Record.BlessingsAvailable;
 switch (Tier)
 {
 case 1: Record.CurrentHealth = FMath::Clamp(Record.CurrentHealth + BlessingHealAmount, 0, MaxHPFor(Record)); break;
 case 2: ++Record.DamageBonus; break;
 case 3: ++Record.CritChanceBonus; break;
 case 4: ++Record.CritDamageBonus; break;
 }
 if (FBattlePlayer* Player = Players.Find(UserId))
 {
  ApplyRecordToPlayer(Record, *Player);
  SetPlayerHealth(UserId, Record.CurrentHealth);
 }
 SaveRecords();
 UE_LOG(LogTemp, Log, TEXT("[Temple] 축복 적용: %s 강화 %d (남은 %d회)"), *UserId, Tier, Record.BlessingsAvailable);
 OnBlessingApplied.Broadcast(UserId, Record.DisplayName, Tier, Record.BlessingsAvailable);
 return true;
}

int32 ABattleManager::GetRemainingBlessings(const FString& UserId) const
{
 const FPlayerRecord* Record = Records.Find(UserId);
 return Record ? FMath::Max(0, Record->BlessingsAvailable) : 0;
}

void ABattleManager::GrantBlessingsToParticipants()
{
 for (const TPair<FString, FBattlePlayer>& Pair : Players)
 {
  FPlayerRecord& Record = GetOrCreateRecord(Pair.Key, Pair.Value.DisplayName);
  Record.BlessingsAvailable = FMath::Max(0, BlessingsPerBattle);
 }
 UE_LOG(LogTemp, Log, TEXT("[Battle] 전투 종료: 참가자 %d명에게 축복 %d회 지급"), Players.Num(), BlessingsPerBattle);
}

bool ABattleManager::SetPlayerHealth(const FString& UserId, int32 CurrentHealth)
{
 FBattlePlayer* Player = Players.Find(UserId);
 FPlayerRecord* Record = Records.Find(UserId);
 if (!Player || !Record) return false;
 const int32 Health = FMath::Clamp(CurrentHealth, 0, Player->MaxHP);
 if (Player->HP == Health && Record->CurrentHealth == Health) return true;
 Player->HP = Health;
 Record->CurrentHealth = Health;
 if (Health == 0)
 {
  Player->ReviveCountdown = FMath::Max(1, ReviveTurns);
  Player->bDiedThisTurn = true;
 }
 else if (Player->ReviveCountdown > 0)
 {
  Player->ReviveCountdown = 0;
  Player->bDiedThisTurn = false;
  if (IsValid(Player->Unit)) Player->Unit->NotifyRevived();
 }
 SaveRecords();
 return true;
}

void ABattleManager::LogoutPlayer(const FString& UserId)
{
 const FBattlePlayer* Player = Players.Find(UserId);
 if (!Player)
 {
  return;
 }
 const FString DisplayName = Player->DisplayName;
 //유닛이 사라지기 전에 퇴장 연출 기회를 줌
 if (IsValid(Player->Unit)) Player->Unit->OnLoggedOut();
 RemovePlayer(UserId);
 SaveRecords();
 UE_LOG(LogTemp, Log, TEXT("[Battle] 로그아웃: %s"), *UserId);
 OnPlayerLoggedOut.Broadcast(UserId, DisplayName);
}

FString ABattleManager::ResolveCharacterId(const FString& NameOrId) const
{
 const FString Key = NameOrId.TrimStartAndEnd();
 for (const FCharacterDef& Class : Classes)
 {
  if (Class.Id.Equals(Key, ESearchCase::IgnoreCase) || Class.Name.Equals(Key, ESearchCase::IgnoreCase)) return Class.Id;
 }
 //밸런스 파일 이름이 바뀌어도 채팅의 기본 이름은 받는다
 if (Key == TEXT("글라디우스")) return TEXT("gladius");
 if (Key == TEXT("세라")) return TEXT("sera");
 return FString();
}

bool ABattleManager::LoginPlayer(const FString& UserId, const FString& DisplayName, const FString& CharacterName)
{
 if (UserId.TrimStartAndEnd().IsEmpty() || !bBattleRunning)
 {
  return false;
 }
 if (!IsAcceptingInput())
 {
  UE_LOG(LogTemp, Log, TEXT("[Battle] 입력 시간이 아니라 로그인 무시: %s"), *UserId);
  return false;
 }
 if (Players.Contains(UserId))
 {
  UE_LOG(LogTemp, Log, TEXT("[Battle] 이미 로그인됨: %s (캐릭터 변경은 로그아웃 후 다시 로그인)"), *UserId);
  return false;
 }
 const FString CharacterId = ResolveCharacterId(CharacterName);
 if (CharacterId.IsEmpty())
 {
  UE_LOG(LogTemp, Log, TEXT("[Battle] 알 수 없는 캐릭터라 로그인 무시: %s '%s'"), *UserId, *CharacterName);
  return false;
 }

 //기록이 있는 유저도 이번에 고른 캐릭터로 바꾼다 (강화 능력치는 유지)
 InitializePlayerRecords();
 FPlayerRecord& Record = GetOrCreateRecord(UserId, DisplayName, CharacterId);
 Record.CharacterId = CharacterId;

 //참가 처리(기록 반영·스폰·자리 배치)는 기존 SubmitAction이 담당. 로그인 자체는 '대기' 행동으로 들어간다
 if (!SubmitAction(UserId, DisplayName, EBattleAction::Idle, CharacterId) || !Players.Contains(UserId))
 {
  UE_LOG(LogTemp, Log, TEXT("[Battle] 로그인 실패(정원 초과 등): %s"), *UserId);
  return false;
 }

 UE_LOG(LogTemp, Log, TEXT("[Battle] 로그인: %s [%s]"), *UserId, *CharacterId);
 OnPlayerLoggedIn.Broadcast(UserId, DisplayName, CharacterId);
 return true;
}

const FCharacterMotionTimes& ABattleManager::MotionTimesFor(int32 ClassIndex) const
{
	static const FCharacterMotionTimes Default;

	if (Classes.IsValidIndex(ClassIndex))
	{
		if (const FCharacterMotionTimes* Found = MotionTimesByCharacter.Find(Classes[ClassIndex].Id))
		{
			return *Found;
		}
	}
	return Default;
}

void ABattleManager::BeginMove()
{
 if (!bBattleRunning) return;
 StopTurnCutScenes();
 WalkRunOnlyCutSceneUnit.Reset();
	Phase = EBattlePhase::Move;
	ResolveBattleStartTarget();

	//**회피자의 구역은 아직 바꾸지 않는다.** 실제로 넘어가는 것은 보스 공격 직전이고,
	//여기서 미리 옮기면 아직 서 있는 자리를 같은 구역의 다른 사람이 배정받아 겹친다.
	//구역 변경과 재배치는 ResolveTurn에서 한다 (turnOrder의 '회피 이동'도 거기다)
	AssignSlots();

	//기획서: 모든 공격 모션이 '보스 공격' 순간에 끝나도록 맞춘다.
	//때릴 게 많은(피버가 높은) 사람일수록 일찍 출발하고 그만큼 빨리 움직인다
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		if (!P.Unit || P.ReviveCountdown > 0)
		{
			continue;
		}

		if (P.PendingAction == EBattleAction::Defense)
		{
			//자리로 가기만 한다. 막는 자세는 도착한 뒤(ResolveTurn)에 잡는다 —
			//여기서 같이 부르면 모션을 시작하자마자 이동이 덮어 캔슬된 것처럼 보인다
			P.Unit->MoveTo(P.TargetLocation, MoveSeconds);
			continue;
		}

		if (IsEvade(P.PendingAction))
		{
			//**여기서는 움직이지 않는다.** 그 전에 목적 구역에 서 있으면 회피로 보이지 않는다.
			//접근은 ResolveTurn에서 보스 공격 직전에 시작하고, 구역을 넘는 것은
			//BossAttackBeat의 구르기가 맡는다
			continue;
		}

		const bool bAttacking = P.PendingAction == EBattleAction::Attack1
			|| P.PendingAction == EBattleAction::Attack2;

		if (!bAttacking)
		{
			//무입력. 자리만 맞춘다
			P.Unit->MoveTo(P.TargetLocation, MoveSeconds);
			continue;
		}

		//때릴 시간을 먼저 확보하고, 남는 만큼만 이동에 쓴다.
		//모션 길이는 직업군마다 다르다 — 애셋이 다르므로 같을 이유가 없다
		const FCharacterMotionTimes& Motion = MotionTimesFor(P.ClassIndex);
		const float SkillSeconds = (P.PendingAction == EBattleAction::Attack1)
			? Motion.Skill1 : Motion.Skill2;
		const float MotionSeconds = AttackMotionSeconds(SkillSeconds,
			FeverFlurrySeconds(Motion.FeverAttacks, FeverStacksFor(P.Fever)));
		const float MoveSecondsForMe = FMath::Max(MinMoveSeconds, BattleTime - MotionSeconds);

		//근접만 보스에게 붙는다. 원거리는 제자리에서 쏘므로 자기 자리로만 간다
		const FVector Target = IsRangedClass(P.ClassIndex)
			? P.TargetLocation : ChargeLocation(P.TargetLocation);
		const float Distance = FVector::Dist(P.Unit->GetActorLocation(), Target);

		EMoveGait ApproachGait = ChooseMovementGait(P.Unit, Distance, MoveSecondsForMe);
		// An attacking Gladius charges straight into the skill; rolls belong to evade actions.
		if (Classes.IsValidIndex(P.ClassIndex) && Classes[P.ClassIndex].Id == TEXT("gladius")
			&& ApproachGait == EMoveGait::Roll) ApproachGait = EMoveGait::Run;
		P.Unit->MoveTo(Target, MoveSecondsForMe, ApproachGait);
	}

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::ResolveTurn, FMath::Max(0.001f, MoveSeconds), false);
 StartMoveCutScenes();
 RefreshPlayerUI();
}

void ABattleManager::AssignSlots()
{
	const FVector Boss = BossLocation();

	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		//이 구역에 서 있는 참가자를 직업군별로 모은다. 배치가 매번 같도록 순서를 고정한다
		TArray<TArray<FString>> IdsByClass;
		IdsByClass.SetNum(Classes.Num());

		int32 Total = 0;
		for (const TPair<FString, FBattlePlayer>& Pair : Players)
		{
			if ((int32)Pair.Value.Lane != LaneIndex)
			{
				continue;
			}

			IdsByClass[FMath::Clamp(Pair.Value.ClassIndex, 0, Classes.Num() - 1)].Add(Pair.Key);
			++Total;
		}

		if (Total == 0)
		{
			continue;
		}

		TArray<int32> Counts;
		for (TArray<FString>& Ids : IdsByClass)
		{
			Ids.Sort();
			Counts.Add(Ids.Num());
		}

		//레벨에 스팟이 있으면 그것이 후보고, 없으면 격자 좌표를 후보로 쓴다
		TArray<FVector> Candidates = Spots[LaneIndex];
		if (Candidates.Num() == 0)
		{
			for (int32 i = 0; i < MaxPlayers; ++i)
			{
				Candidates.Add(GridSlotLocation((EBattleLane)LaneIndex, i));
			}
		}

		//직업군 순서대로 앞에서 뒤로 자리를 나눠 받는다
		TArray<TArray<int32>> ChosenByClass;
		ChooseClassSpots(Candidates, Counts, Boss, ChosenByClass);

		for (int32 ClassIdx = 0; ClassIdx < IdsByClass.Num(); ++ClassIdx)
		{
			TArray<FVector> Targets;
			for (const int32 Index : ChosenByClass[ClassIdx])
			{
				Targets.Add(Candidates[Index]);
			}

			//덜 움직이도록, 지금 서 있는 곳에서 가장 가까운 자리부터 가져간다
			for (const FString& Id : IdsByClass[ClassIdx])
			{
				FBattlePlayer& P = Players[Id];
				const FVector From = P.Unit ? P.Unit->GetActorLocation() : Targets[0];

				int32 Best = 0;
				float BestDist = TNumericLimits<float>::Max();
				for (int32 i = 0; i < Targets.Num(); ++i)
				{
					const float Dist = FVector::DistSquared(From, Targets[i]);
					if (Dist < BestDist)
					{
						BestDist = Dist;
						Best = i;
					}
				}

				P.TargetLocation = Targets[Best];
				Targets.RemoveAt(Best);
			}
		}
	}
}

void ABattleManager::ResolveTurn()
{
	//이동이 끝났고 이제 공격 모션이 도는 구간이다
	Phase = EBattlePhase::BossAction;
	RefreshPlayerUI();

	//turnOrder의 첫 항목 — 회피 이동. 여기서야 구역이 바뀐다.
	//BeginMove에서 미리 바꾸면 아직 떠나지도 않은 자리를 남이 배정받는다
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		if (P.ReviveCountdown == 0)
		{
			//어느 경계를 넘어야 하는지는 '어디서 어디로'가 정한다. 지금 적어 둔다
			P.LaneBeforeMove = P.Lane;
			P.Lane = ApplyMove(P.PendingAction, P.Lane);
		}
	}

	//구역 인원이 바뀌었으니 자리를 다시 배분한다. 떠난 자리는 이제 비었으므로
	//남아 있는 사람이 받아도 겹치지 않는다 — 실제 이동은 EndTurn의 원위치 이동이 맡는다
	AssignSlots();

	//도착한 뒤에 막을 펼친다. 실제로 막는 순간은 BossAttackBeat의 OnHit이고,
	//그 사이를 버티는 자세까지가 이 모션의 몫이다
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		if (P.Unit && P.ReviveCountdown == 0 && P.PendingAction == EBattleAction::Defense)
		{
			//!방어 -> 캐릭터의 Defend (막는 모션 + 버티는 자세 상태)
			P.Unit->Defend();
		}
	}

	//턴 시작 때 예고한 그대로 친다
	const FBossAction& Boss = TelegraphedAction;

	int32 TotalDealt = 0;
	int32 Crits = 0;
	int32 Died = 0;
	int32 Revived = 0;


	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		FPlayerRecord* Record = Records.Find(Pair.Key);

		//게임 오버 중에는 입력할 수 없으므로 무입력으로 세지 않는다
		if (P.ReviveCountdown == 0)
		{
			P.IdleTurns = P.bActedThisTurn ? 0 : P.IdleTurns + 1;
		}

		if (P.ReviveCountdown > 0)
		{
			// ponytail: 기획상 부활 전에 '아직 시청 중인지'를 확인해야 하지만
			// 그건 YouTube API라 여기서는 항상 시청 중이라고 본다.
			// 슈퍼챗 즉시부활도 아직 없다
			if (--P.ReviveCountdown == 0)
			{
				//부활하면서 강화 능력치의 GameOverLoss 비율을 잃는다
				if (Record)
				{
					const float Keep = 1.f - GameOverLoss;
					Record->MaxHPBonus = FMath::FloorToInt(Record->MaxHPBonus * Keep);
					Record->DamageBonus = FMath::FloorToInt(Record->DamageBonus * Keep);
					Record->CritChanceBonus = FMath::FloorToInt(Record->CritChanceBonus * Keep);
					Record->CritDamageBonus = FMath::FloorToInt(Record->CritDamageBonus * Keep);
					ApplyRecordToPlayer(*Record, P);
				}

				//부활은 처음 참가한 것과 같은 상태로 되돌린다
				SetPlayerHealth(Pair.Key, P.MaxHP);
				P.Fever = Balance.InitialFever;
				P.EvadeStreak = Balance.InitialEvadeStreak;
				++Revived;

				if (P.Unit)
				{
					P.Unit->NotifyRevived();
				}
			}
			continue;
		}

		const bool bAttack1 = P.PendingAction == EBattleAction::Attack1;
		const bool bAttack2 = P.PendingAction == EBattleAction::Attack2;
		const bool bEvade = IsEvade(P.PendingAction);

		//기획서 5절 순서 — 플레이어 공격을 먼저 계산한다.
		//피버는 이번 턴 공격 피해에 반영된 뒤에 갱신된다
		if (bAttack1 || bAttack2)
		{
			const FAttackDef& Attack = Classes[P.ClassIndex].Attacks[bAttack1 ? 0 : 1];
			//내성은 이동 후 구역 기준 (기획서 2절)
			const float Resist = ZoneResistMultiplier(Boss, P.Lane, Attack.Type);

			//치명타는 플레이어마다 따로 굴린다. 공격의 기본 치명타 + 신전 강화분 (상한 적용)
			const float Chance = FMath::Min(Attack.CritChance + P.CritChanceBonus, CritChanceCap);
			const float CritDamage = Attack.CritDamage + P.CritDamageBonus;

			//**스킬 피해에 피버는 들어가지 않는다.** 피버는 뒤이어 나가는 평타 횟수다
			const bool bCrit = (Pair.Key == CutSceneAttackUserId)
				? bCutSceneAttackCrit : FMath::FRand() < Chance;
			int32 Dealt = DamageToBoss(Attack.BaseDamage, P.DamageBonus, Resist, bCrit, CritDamage);

			//화면에는 블루프린트가 타격을 알릴 때 한 대씩 나간다. 스킬이 먼저, 평타가 그 뒤다
			P.HitsToShow.Reset();
			P.HitsToShow.Add({ Dealt, bCrit });

			Crits += bCrit ? 1 : 0;

			//피버 한 칸이 평타 한 대다. 한 대는 고정 피해에 구역 내성이 걸리고,
			//치명타는 **평타마다 따로** 굴린다 (스킬의 치명타 확률을 그대로 쓴다)
			TArray<bool> FeverCrits;
			const int32 FeverHits = FeverStacksFor(P.Fever);
			FeverCrits.Reserve(FeverHits);

			for (int32 Hit = 0; Hit < FeverHits; ++Hit)
			{
				const bool bFeverCrit = FMath::FRand() < Chance;
				FeverCrits.Add(bFeverCrit);

				//강화 공격력은 스킬에만 붙는다 — 평타는 고정값이다
				const int32 FeverDealt = DamageToBoss(FeverAttackDamage, 0, Resist, bFeverCrit, CritDamage);
				Dealt += FeverDealt;
				P.HitsToShow.Add({ FeverDealt, bFeverCrit });
				Crits += bFeverCrit ? 1 : 0;
			}

			TotalDealt += Dealt;

			//모션은 지금 재생하지 않는다. 전원이 한 프레임에 때리면 누가 쳤는지 보이지 않아,
			//아래에서 랜덤 순서로 조금씩 끊어 재생한다. 피해는 이미 여기서 확정됐다.
			//피버는 '이번 공격에 실린 값'이어야 하므로 아래에서 오르기 전에 담는다
			FAttackCue Cue;
			Cue.UserId = Pair.Key;
			Cue.AttackIndex = bAttack1 ? 1 : 2;
			Cue.bCrit = bCrit;
			Cue.Fever = P.Fever;
			Cue.ClassIndex = P.ClassIndex;
			Cue.Damage = Dealt;
			Cue.FeverCrits = MoveTemp(FeverCrits);
			AttackCues.Add(MoveTemp(Cue));

			//Records는 방송 전체 누적이라 초기화되지 않는다. 판 단위 순위는 이쪽이다
			//순위판(SessionStats)은 화면 표시라 ShowBossHit에서 한 대씩 쌓는다
			P.DamageDealt += Dealt;

			if (Record)
			{
				Record->TotalDamageDealt += Dealt;
			}

			P.Fever += FeverIncrement;

			//세라 컷신처럼 피해가 정해지기 전에 이미 맞은 타격이 있으면 지금 띄운다
			for (; P.EarlyHitReports > 0 && P.HitsToShow.Num() > 0; --P.EarlyHitReports)
			{
				ShowBossHit(Pair.Key, P.HitsToShow[0]);
				P.HitsToShow.RemoveAt(0);
			}
		}

		const int32 Taken = DamageToPlayer(P.PendingAction,
			Boss.DamageByZone[(int32)P.Lane], Boss.HitCount, Classes[P.ClassIndex].DefenseDamageRatio);
		const int32 HPBefore = P.HP;
		SetPlayerHealth(Pair.Key, P.HP - Taken);
		P.HPLossToShow = HPBefore - P.HP;

		//피격 모션은 여기서 재생하지 않는다. 보스가 아직 휘두르지도 않았는데
		//맞는 모션이 먼저 나가면 순서가 뒤집힌다. BossAttackBeat에서 꺼내 쓴다
		P.DamageTakenThisTurn = Taken;

		//연속 회피 횟수는 회피가 아닌 행동(무입력 포함)에서 끊긴다
		P.EvadeStreak = bEvade ? P.EvadeStreak + 1 : 0;

		if (P.PendingAction == EBattleAction::Defense)
		{
			P.Fever += DefenseFeverGain;
		}
		else if (bEvade)
		{
			P.Fever = DecayFever(P.Fever, P.EvadeStreak, EvadeStreakN,
				EvadeFeverReduction, EvadeRepeatMode, FeverRounding);
		}

		//상한은 어느 경로로 바뀌었든 똑같이 적용한다
		if (FeverCap > 0.f)
		{
			P.Fever = FMath::Min(P.Fever, FeverCap);
		}

		if (P.HP <= 0)
		{
			P.ReviveCountdown = FMath::Max(1, ReviveTurns);
			P.bDiedThisTurn = true;
			++Died;
		}
	}

	//깎을 체력이 없으므로 쌓기만 한다
	BossDamageTaken += TotalDealt;

	//무입력이 이어진 유저를 전장에서 뺀다. 순회 중에는 지울 수 없어 모아뒀다가 처리한다
	TArray<FString> Leaving;
	for (const TPair<FString, FBattlePlayer>& Pair : Players)
	{
		if (Pair.Value.IdleTurns >= IdleTurnsBeforeKick)
		{
			Leaving.Add(Pair.Key);
		}
	}
	for (const FString& UserId : Leaving)
	{
		RemovePlayer(UserId);
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Turn %d 결과 — 보스 %s(%d타) / 입힌 피해 %d (치명타 %d) / 게임오버 %d / 부활 %d → 누적 %d"),
		TurnIndex, *ZonesLabel(Boss), Boss.HitCount, TotalDealt, Crits, Died, Revived, BossDamageTaken);

	LastTurnSummary = FString::Printf(TEXT("Boss hit %s   Damage dealt %d (%d crit)   Down %d   Revived %d"),
		*ZonesLabel(Boss), TotalDealt, Crits, Died, Revived);

	//기록이 바뀌는 건 참가와 강화뿐이고 둘 다 턴 안에서 끝난다.
	//주기 저장 대신 턴 경계에서만 쓰면 조절할 값도 없고 저장 시점도 분명해진다
	SaveRecords();

	//직업군마다 피버가 가장 높았던 한 명을 특수 연출 대상으로 뽑는다.
	//피버가 같으면 이번 턴에 더 많이 때린 쪽. 섞기 전에 골라야 순서와 무관하게 같은 결과가 나온다
	MarkFeverHighlights(AttackCues);

	//기획서는 '랜덤 순서대로' 치라고 한다. 자리 순서대로 치면 줄 세운 것처럼 보인다
	for (int32 i = AttackCues.Num() - 1; i > 0; --i)
	{
		AttackCues.Swap(i, FMath::RandRange(0, i));
	}
	AttackCueIndex = 0;

	//한 묶음에 AttackCount명씩, AttackTerm 간격으로 나간다
	const int32 Groups = FMath::DivideAndRoundUp(AttackCues.Num(), FMath::Max(1, AttackCount));
	float FlurrySeconds = Groups * FMath::Max(0.f, AttackTerm);

	//피버 평타까지 끝나야 보스가 친다. 묶음 간격만 보면 피버가 높은 사람의 연타가
	//보스 공격 뒤까지 이어져, 맞고 쓰러지면서 때리는 그림이 나온다.
	//공격 순서를 어떻게 끊든 가장 늦게 끝나는 사람을 기준으로 잡는다
	for (int32 Index = 0; Index < AttackCues.Num(); ++Index)
	{
		const FAttackCue& Cue = AttackCues[Index];
		const FCharacterMotionTimes& Motion = MotionTimesFor(Cue.ClassIndex);
		const float SkillSeconds = Cue.AttackIndex == 1 ? Motion.Skill1 : Motion.Skill2;

		//이 사람의 연출이 시작되는 시각 + 스킬 + 평타 전부
		const float StartsAt = (Index / FMath::Max(1, AttackCount)) * FMath::Max(0.f, AttackTerm);
		const float EndsAt = StartsAt + SkillSeconds
			+ FeverFlurrySeconds(Motion.FeverAttacks, Cue.FeverCrits.Num());

		FlurrySeconds = FMath::Max(FlurrySeconds, EndsAt);
	}

	if (Groups > 0)
	{
		//때리는 사람이 있을 때만 피격 상태로 둔다.
		//아무도 안 때리면 기획서대로 예고 자세를 그대로 유지한다
		if (BossUnit)
		{
			BossUnit->OnBossHit(TotalDealt, AttackCues.Num());
		}

		GetWorldTimerManager().SetTimer(AttackCueHandle, this, &ABattleManager::FlushAttackCues,
			FMath::Max(KINDA_SMALL_NUMBER, AttackTerm), true, 0.f);
	}

	//전투 개시부터 재서 BattleTime이 되는 순간에 보스가 친다.
	//여기는 이미 MoveSeconds가 지난 뒤라 그만큼 뺀다
	const float BossDelay = BossStrikeDelay(FlurrySeconds, BattleTime - MoveSeconds);

	//BossDelay는 **휘두르기 시작**이다. 칼은 그보다 뒤에, 구역마다 다른 시각에 닿는다
	//(LeftRight는 왼쪽을 먼저 친다). 피격과 구르기는 닿는 순간에 맞춘다 — 시작에 맞추면
	//칼이 오기도 전에 맞는 모션이 나가고, 회피자는 다 구른 뒤 멈춰 서서 칼을 맞는다
	const UAnimMontage* Strike = BossUnit ? BossUnit->GetStrikeMontage(TelegraphedAction.AttackMotion()) : nullptr;
	StrikeLandsWithin = 0.f;

	for (int32 Index = 0; Index < 3; ++Index)
	{
		const EBattleLane Lane = static_cast<EBattleLane>(Index);
		const float Impact = StrikeImpactSeconds(Strike, Lane);
		const float LandAt = BossDelay + Impact;
		StrikeLandsWithin = FMath::Max(StrikeLandsWithin, Impact);

		GetWorldTimerManager().SetTimer(StrikeLandHandles[Index],
			FTimerDelegate::CreateUObject(this, &ABattleManager::LandBossStrike, Lane),
			FMath::Max(KINDA_SMALL_NUMBER, LandAt), false);

		//구르기는 칼이 닿기 RollLead초 전에 시작한다. 동시에 걸면 같은 순간에 오는
		//피격 모션이 구르기를 곧바로 덮어, 구른 것은 안 보이고 미끄러지는 것만 보인다
		const float RollAt = FMath::Max(0.f, LandAt - RollLead);

		//회피자는 **여기서 바로 움직이지 않는다.** 구르기 직전에 달려가 구르기 시작점에
		//도착해야 하므로, 달리기에 쓸 시간만큼 앞에서 출발한다.
		//**RollAt을 그대로 쓰면 안 된다** — 피버 연타가 길어지면 BossDelay가 20초까지
		//늘어나는데, 그만큼을 이동에 주면 걸어가는 것처럼 기어간다
		EvadeApproachSeconds[Index] = FMath::Min(MoveSeconds, RollAt);
		const float ApproachAt = RollAt - EvadeApproachSeconds[Index];

		if (ApproachAt > KINDA_SMALL_NUMBER)
		{
			GetWorldTimerManager().SetTimer(EvadeApproachHandles[Index],
				FTimerDelegate::CreateUObject(this, &ABattleManager::BeginEvadeApproach, Lane),
				ApproachAt, false);
		}
		else
		{
			//기다릴 틈이 없으면 지금 출발한다
			BeginEvadeApproach(Lane);
		}

		if (RollAt > KINDA_SMALL_NUMBER)
		{
			GetWorldTimerManager().SetTimer(EvadeRollHandles[Index],
				FTimerDelegate::CreateUObject(this, &ABattleManager::BeginEvadeRolls, Lane),
				RollAt, false);
		}
		else
		{
			//보스가 바로 친다. 달려갈 틈이 없으니 그 자리에서 구른다
			BeginEvadeRolls(Lane);
		}
	}

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::BossAttackBeat,
		FMath::Max(KINDA_SMALL_NUMBER, BossDelay), false);
}

void ABattleManager::MarkFeverHighlights(TArray<FAttackCue>& Cues)
{
	//직업군별 최고 피버 한 명. 같은 피버면 더 많이 때린 쪽을 고른다.
	//UserId까지 비교해 완전히 결정적으로 만든다 — 안 그러면 맵 순회 순서에 따라 대상이 흔들린다
	TMap<int32, int32> BestByClass;

	for (int32 i = 0; i < Cues.Num(); ++i)
	{
		const FAttackCue& Cue = Cues[i];

		//피버를 쌓지 않은 사람은 후보가 아니다. 하한이 없으면 전원이 0일 때도
		//첫 공격자가 뽑혀 평범한 공격에 특수 모션이 나간다
		if (Cue.Fever <= 0.f)
		{
			continue;
		}

		const int32* Found = BestByClass.Find(Cue.ClassIndex);
		if (!Found)
		{
			BestByClass.Add(Cue.ClassIndex, i);
			continue;
		}

		const FAttackCue& Best = Cues[*Found];
		const bool bWins =
			Cue.Fever > Best.Fever ||
			(Cue.Fever == Best.Fever && Cue.Damage > Best.Damage) ||
			(Cue.Fever == Best.Fever && Cue.Damage == Best.Damage && Cue.UserId < Best.UserId);

		if (bWins)
		{
			BestByClass[Cue.ClassIndex] = i;
		}
	}

	for (const TPair<int32, int32>& Pair : BestByClass)
	{
		Cues[Pair.Value].bFeverHighlight = true;
	}
}

void ABattleManager::FlushAttackCues()
{
	for (int32 n = 0; n < FMath::Max(1, AttackCount) && AttackCueIndex < AttackCues.Num(); ++n, ++AttackCueIndex)
	{
		const FAttackCue& Cue = AttackCues[AttackCueIndex];

		//연출이 도는 사이에 이탈한 유저가 있을 수 있다
		if (const FBattlePlayer* P = Players.Find(Cue.UserId))
		{
			if (P->Unit)
			{
				const bool bSkillAlreadyStarted = Cue.UserId == CutSceneAttackUserId;
				if (!bSkillAlreadyStarted)
				{
					// Fallback for an attack dispatched before the selected unit's arrival callback.
					if (Cue.UserId == CutSceneUserId)
					{
						P->Unit->OnMovementFinished.RemoveAll(this);
						CutSceneAttackUserId = Cue.UserId;
						CutSceneAttackStartTime = GetWorld()->GetTimeSeconds();
						bCutSceneAttackCrit = Cue.bCrit;
					}
					P->Unit->PerformSkill(Cue.AttackIndex, Cue.bCrit, Cue.bFeverHighlight);
				}

				//평타는 스킬 모션이 끝난 뒤부터 이어진다. 그 길이를 AttackMotionSeconds가
				//이미 BattleTime에서 예약해 뒀으므로, 마지막 대가 보스 공격과 맞물린다
				if (Cue.FeverCrits.Num() > 0)
				{
					const FCharacterMotionTimes& Motion = MotionTimesFor(Cue.ClassIndex);
					const float SkillSeconds = Cue.AttackIndex == 1 ? Motion.Skill1 : Motion.Skill2;
					const float RemainingSkillSeconds = bSkillAlreadyStarted
						? FMath::Max(0.f, SkillSeconds - (GetWorld()->GetTimeSeconds() - CutSceneAttackStartTime)) : SkillSeconds;
					P->Unit->PlayFeverFlurry(Cue.FeverCrits, RemainingSkillSeconds, Motion.FeverAttacks);
				}
			}
		}
	}

	if (AttackCueIndex >= AttackCues.Num())
	{
		GetWorldTimerManager().ClearTimer(AttackCueHandle);
		AttackCues.Reset();
	}
}

bool ABattleManager::TryGetLaneBoundary(EBattleLane From, EBattleLane To, FVector& OutBoundary) const
{
	if (From == To)
	{
		return false;
	}

	//목적 구역이 가장자리면 그 구역의 경계, 가운데면 들어온 쪽의 경계다.
	//좌→우는 중/우 경계를 마지막으로 넘는다
	const int32 Index =
		To == EBattleLane::Left  ? 0 :
		To == EBattleLane::Right ? 1 :
		(From == EBattleLane::Left ? 0 : 1);

	if (!bHasBoundary[Index])
	{
		return false;
	}

	OutBoundary = BoundaryPoints[Index];
	return true;
}

float ABattleManager::BossStrikeSecondsFor(const FString& StateId) const
{
	if (const float* Found = BossStrikeSecondsByState.Find(StateId))
	{
		return FMath::Max(0.f, *Found);
	}
	//공격 모션이 없는 상태(기 모으기 등)와 아직 애셋이 안 들어온 상태가 여기로 온다
	return FMath::Max(0.f, BossActionSeconds);
}

void ABattleManager::BeginEvadeApproach(EBattleLane Leaving)
{
	//구르기 시작점까지 달려간다. 도착하는 순간이 곧 BeginEvadeRolls다.
	//회피 한 번은 세 토막으로 나뉜다 — 경계 앞까지 달리기 / 구르며 경계 넘기 /
	//남은 거리를 걸어 자리로 (마지막은 EndTurn의 원위치 이동이 맡는다)
	const float ApproachSeconds = EvadeApproachSeconds[static_cast<int32>(Leaving)];

	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		if (!P.Unit || P.ReviveCountdown > 0 || !IsEvade(P.PendingAction) || P.LaneBeforeMove != Leaving)
		{
			continue;
		}

		const FVector From = P.Unit->GetActorLocation();
		const FVector ToTarget = P.TargetLocation - From;
		const float Total = ToTarget.Size();
		if (Total <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Dir = ToTarget / Total;

		//넘어야 할 경계가 있으면 그 앞까지만 간다. 없으면(제자리 회피이거나
		//레벨에 표식이 없으면) 예전처럼 구르기 한 번 거리를 남기고 간다.
		//
		//**경계는 점이 아니라 선이다.** 구역이 보스 중심 부채꼴이므로 경계선은
		//보스에서 표식을 지나 뻗어나가는 선이고, 표식은 그 선 위 아무 곳에나 두면 된다.
		//점으로만 보고 경로에 투영하면, 보스에서 먼 데서 건너는 사람의 경계가 어긋난다
		FVector Boundary = FVector::ZeroVector;
		float Lead = FMath::Max(0.f, Total - RollDistance);

		if (TryGetLaneBoundary(P.LaneBeforeMove, P.Lane, Boundary))
		{
			const FVector Boss = BossLocation();
			const FVector Along = (Boundary - Boss).GetSafeNormal2D();

			//경계선의 법선. 경로와 이 선의 교점까지가 달려갈 거리다
			const FVector Normal(-Along.Y, Along.X, 0.f);
			const float Denom = FVector::DotProduct(Dir, Normal);

			//경로가 경계선과 나란하면 교점이 없다. 그때는 기본값을 쓴다
			if (!Along.IsNearlyZero() && FMath::Abs(Denom) > SMALL_NUMBER)
			{
				Lead = FMath::Clamp(FVector::DotProduct(Boss - From, Normal) / Denom, 0.f, Total);
			}
		}

		//경계가 코앞이면 달려갈 것 없이 그 자리에서 구른다
		if (Lead <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Waypoint = From + Dir * Lead;
		P.Unit->MoveTo(Waypoint, ApproachSeconds,
			ChooseMovementGait(P.Unit, Lead, ApproachSeconds));
	}
}

void ABattleManager::BeginEvadeRolls(EBattleLane Leaving)
{
	//떠나는 구역에 칼이 닿기 RollLead초 전. 구르기를 먼저 보여 주고, 그 끝자락에
	//LandBossStrike의 피격 모션이 덮는다 — 구르다 걸린 것처럼 보인다.
	//구르기 전까지는 원래 구역에 서 있었으므로, 구역을 넘는 것이 곧 이 구르기다
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		if (!P.Unit || P.ReviveCountdown > 0 || !IsEvade(P.PendingAction) || P.LaneBeforeMove != Leaving)
		{
			continue;
		}

		//구르기는 '남은 거리만큼'이 아니라 **정해진 거리**를 덮는다. 모션에 이동이
		//실려 있어서 늘이고 줄일 수 없기 때문이다. 자리까지 모자라는 만큼은
		//구른 뒤에 걷거나 뛰어서 마저 간다 (EndTurn의 원위치 이동이 그 역할을 한다)
		//출발점이 '경계에서 구르기 절반 앞'이므로, 여기서 RollDistance를 덮으면
		//경계를 확실히 넘는다. 자리까지 모자라는 만큼만 걸어서 마저 간다
		const FVector From = P.Unit->GetActorLocation();
		const float Remaining = FVector::Dist(From, P.TargetLocation);
		const float Covered = FMath::Min(RollDistance, Remaining);

		const FVector RollTo = Remaining > KINDA_SMALL_NUMBER
			? FMath::Lerp(From, P.TargetLocation, Covered / Remaining)
			: From;

		//!회피 좌/중/우 -> 캐릭터의 DodgeLeft/DodgeMid/DodgeRight
		P.Unit->PerformDodge(P.PendingAction, RollTo, RollSeconds, Covered);
	}
}

void ABattleManager::BossAttackBeat()
{
	//맞은 연출이 끝났으니 이제 보스가 친다. 피해는 ResolveTurn에서 이미 반영됐다.
	//예고한 것과 같은 행동이므로 MotionId도 그때 보여준 것과 짝이 맞는다
	if (BossUnit)
	{
		BossUnit->OnBossAttack(TelegraphedAction.AttackMotion(), TelegraphedAction.HitCount);
	}

	//맞는 쪽 반응은 여기가 아니라 칼이 닿는 LandBossStrike에서 나간다.
	//공격 모션이 끝나야 다음 턴으로 간다. 상태마다 길이가 2.7초~8.4초로 갈린다.
	//마지막 구역에 칼이 닿기 전에 끝나면 맞는 모션이 다음 턴으로 넘어가므로 그보다는 길게 잡는다
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::EndTurn,
		FMath::Max(BossStrikeSecondsFor(TelegraphedAction.AttackMotion()), StrikeLandsWithin + KINDA_SMALL_NUMBER), false);
}

float ABattleManager::StrikeImpactSeconds(const UAnimMontage* Montage, EBattleLane Lane)
{
	if (!Montage)
	{
		return 0.f;
	}

	const FName LaneHit = Lane == EBattleLane::Left ? FName(TEXT("LeftHit"))
		: Lane == EBattleLane::Right ? FName(TEXT("RightHit"))
		: NAME_None;

	//구역 전용 → 공용 → 아무 구역 순으로 찾는다. LeftRight처럼 가운데를 안 치는 공격에서
	//가운데를 떠나는 회피자는 먼저 닿는 쪽(LeftHit)에 맞춰 구른다.
	//NotifyTimeIn은 못 찾으면 경고를 남기므로, 있는지는 여기서 먼저 본다
	const FName Candidates[] = { LaneHit, TEXT("Hit"), TEXT("LeftHit"), TEXT("RightHit") };
	for (const FName& Name : Candidates)
	{
		const bool bHas = !Name.IsNone() && Montage->Notifies.ContainsByPredicate(
			[&Name](const FAnimNotifyEvent& Event) { return Event.NotifyName == Name; });
		if (bHas)
		{
			return ABattleUnit::NotifyTimeIn(Montage, Name, TEXT("Strike"));
		}
	}

	//공격 몬타주인데 타격 표시가 없다. 조용히 넘어가면 휘두르기 시작에 맞는 그림이 나와
	//원인을 짚기 어렵다
	UE_LOG(LogTemp, Warning, TEXT("StrikeImpactSeconds: %s 에 Hit / LeftHit / RightHit 노티파이가 없다"),
		*Montage->GetName());
	return 0.f;
}

void ABattleManager::LandBossStrike(EBattleLane Lane)
{
	//칼이 이 구역에 닿는 바로 이 순간에 맞는 쪽 반응이 나간다.
	//방어한 사람도 여기로 들어오므로 막는 모션과 그냥 맞는 모션은 블루프린트가 가른다.
	//이번 턴에 쓰러진 사람은 맞는 모션까지 보여준 뒤 쓰러진다.
	//구역은 **옮긴 뒤** 자리다 — 피해도 그 자리 기준으로 계산됐다
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		FBattlePlayer& P = Pair.Value;
		if (P.Lane != Lane)
		{
			continue;
		}

		if (P.Unit)
		{
			if (P.DamageTakenThisTurn > 0)
			{
				//쓰러질 타격인지 지금 알려준다. OnDied는 뒤에 오므로
				//블루프린트가 그걸 기다리면 이미 일어나는 모션이 섞인 뒤다
				P.Unit->OnHit(P.DamageTakenThisTurn, P.bDiedThisTurn);
				P.Unit->OnDamageShown(P.DamageTakenThisTurn, false);

				//막는 모션이 나갔으니 버티는 자세는 여기서 끝난다.
				//막기가 끝나면 그 밑은 평소 자세여야지 다시 막고 서 있으면 안 된다
				P.Unit->SetDefending(false);
			}
			if (P.bDiedThisTurn)
			{
				P.Unit->NotifyDied();
			}
		}

		//이름표 체력은 지금 깎인다. 쓰러진 사람의 이름표도 여기서 사라진다
		P.DamageTakenThisTurn = 0;
		P.HPLossToShow = 0;
		P.bDiedThisTurn = false;
	}
	RefreshPlayerUI();
}

void ABattleManager::HandleHitLanded(FString UserId)
{
	FBattlePlayer* P = Players.Find(UserId);
	if (!P)
	{
		return;
	}

	if (P->HitsToShow.Num() == 0)
	{
		//아직 피해가 안 정해졌다. ResolveTurn이 정하자마자 띄운다
		++P->EarlyHitReports;
		return;
	}

	ShowBossHit(UserId, P->HitsToShow[0]);
	P->HitsToShow.RemoveAt(0);
}

void ABattleManager::ShowBossHit(const FString& UserId, const FPendingHit& Hit)
{
	BossDamageShown += Hit.Damage;
	SessionStats.AddDamage(UserId, Hit.Damage);

	if (BossUnit)
	{
		BossUnit->SetLabelText(FString::Printf(TEXT("BOSS\n%d"), BossDamageShown));
		BossUnit->OnDamageShown(Hit.Damage, Hit.bCrit);
	}
}

void ABattleManager::EndTurn()
{
 StopTurnCutScenes();
	Phase = EBattlePhase::Resolve;

	//기획서 M2의 마지막 단계 — 돌진했던 캐릭터가 자기 자리로 돌아간다.
	//구르고 나서 자리까지 남은 거리도 여기서 마저 간다. 거리에 따라 걷거나 뛴다.
	//이미 제자리면 MoveTo가 알아서 넘어가므로 전원에게 그냥 걸어도 된다
	for (const TPair<FString, FBattlePlayer>& Pair : Players)
	{
		const FBattlePlayer& P = Pair.Value;
		if (P.Unit && P.ReviveCountdown == 0)
		{
			const float Distance = FVector::Dist(P.Unit->GetActorLocation(), P.TargetLocation);
			P.Unit->MoveTo(P.TargetLocation, ReturnSeconds,
				ChooseMovementGait(P.Unit, Distance, ReturnSeconds));
		}
	}

	//블루프린트가 알리지 않은 타격(배선이 없는 모션, 턴 도중 이탈 등)은 숫자 없이 합계에만 넣는다.
	//안 그러면 표시가 실제 누적 피해보다 계속 모자란다
	for (TPair<FString, FBattlePlayer>& Pair : Players)
	{
		for (const FPendingHit& Hit : Pair.Value.HitsToShow)
		{
			SessionStats.AddDamage(Pair.Key, Hit.Damage);
		}
		Pair.Value.HitsToShow.Reset();
		Pair.Value.HPLossToShow = 0;
	}
	BossDamageShown = BossDamageTaken;

	//체력 변화와 사망 숨김은 모션이 끝난 지금 반영한다
	UpdateVisuals();

	//보스는 체력이 없으므로 격파로 끝나는 길이 없다. 끝나는 조건은 이것 하나다 —
	//정해진 턴을 다 치르면, 얼마나 버텼고 얼마나 넣었는지로 결과가 갈린다
	if (TurnIndex >= Turns)
	{
		FString TopName;
		int32 TopDamage = 0;
		int32 Alive = 0;
		for (const TPair<FString, FBattlePlayer>& Pair : Players)
		{
			const FBattlePlayer& P = Pair.Value;
			if (P.ReviveCountdown == 0)
			{
				++Alive;
			}
			if (P.DamageDealt > TopDamage)
			{
				TopDamage = P.DamageDealt;
				TopName = P.DisplayName;
			}
		}

		LastTurnSummary = FString::Printf(TEXT("END   %d turns   damage %d   alive %d/%d"),
			Turns, BossDamageTaken, Alive, Players.Num());

		UE_LOG(LogTemp, Log, TEXT("[Battle] 종료 — %d턴 완주 / 누적 피해 %d / 생존 %d/%d / 1위 %s (%d)"),
			Turns, BossDamageTaken, Alive, Players.Num(), *TopName, TopDamage);

		GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::FinishBattle, FMath::Max(0.001f, FMath::Max(ReturnSeconds, ResolveSeconds)), false);
		return;
	}

	//결과를 보여주는 시간을 준 뒤 다음 턴으로
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ABattleManager::StartTurn, FMath::Max(0.001f, FMath::Max(ReturnSeconds, ResolveSeconds)), false);
}

void ABattleManager::RemovePlayer(const FString& UserId)
{
	FBattlePlayer* Player = Players.Find(UserId);
	if (!Player)
	{
		return;
	}

	//능력치는 Records에 그대로 남는다. 다시 명령어를 치면 이어서 참가한다
	if (Player->Unit)
	{
		Player->Unit->Destroy();
	}

	Players.Remove(UserId);
	//전투 중 이탈하면 BATTLE ANALYSIS 목록과 이번 씬 기록에서도 제거
	SessionStats.RemovePlayer(UserId);

	UE_LOG(LogTemp, Log, TEXT("[Battle] 이탈: %s (무입력 %d턴) — 현재 %d/%d명"),
		*UserId, IdleTurnsBeforeKick, Players.Num(), MaxPlayers);
}

FString ABattleManager::BalanceFolderPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BalanceDataDir);
}

void ABattleManager::ApplyBalanceFiles()
{
	if (!bUseBalanceFiles)
	{
		UE_LOG(LogTemp, Log, TEXT("[Balance] 파일을 쓰지 않습니다 — 에디터 값으로 진행합니다"));
		return;
	}

	const FString Folder = BalanceFolderPath();
	if (!Balance.LoadFromFolder(Folder))
	{
		//기획서 7절: 모르는 값을 조용히 기본값으로 대체하지 않는다.
		//전부 포기하고 에디터에 들어 있던 값으로 진행한다
		UE_LOG(LogTemp, Warning, TEXT("[Balance] %s 를 읽지 못해 에디터 값으로 진행합니다"), *Folder);
		return;
	}

	FeverIncrement = Balance.AttackFeverGain;
	DefenseFeverGain = Balance.DefenseFeverGain;
	FeverDamageCoefficient = Balance.FeverDamageCoefficient;
	FeverCap = Balance.FeverCap;
	EvadeStreakN = Balance.EvadeThreshold;
	EvadeFeverReduction = Balance.EvadeReduction;
	EvadeRepeatMode = Balance.EvadeRepeat;
	FeverRounding = Balance.FeverRounding;
	bAllowStayEvade = Balance.bAllowStayEvade;

	Classes = Balance.Characters;

	HPPerBonus = Balance.HPPerLevel;
	DamagePerBonus = Balance.DamagePerLevel;
	CritChancePerBonus = Balance.CritChancePerLevel;
	CritDamagePerBonus = Balance.CritDamagePerLevel;
	CritChanceCap = Balance.CritChanceCap;
	if (Balance.BlessingHealAmount >= 0) BlessingHealAmount = Balance.BlessingHealAmount;

	Turns = Balance.Turns;

	//보스가 추첨으로 도는지 고정 순서로 도는지는 로그만 보고도 알아야 한다.
	//둘을 헷갈리면 시뮬레이터 결과와 왜 다른지 추적하기 어렵다
	const TCHAR* ModeText =
		Balance.PresetMode == EBossPresetMode::Direct ? TEXT("direct (고정 공격 순서)") :
		Balance.PresetMode == EBossPresetMode::Custom ? TEXT("custom (고정 패턴 순서)") :
		TEXT("random (가중치 추첨)");

	//에디터에서 고친 값이 조용히 사라지는 일이 없도록, 덮어쓴 결과를 남긴다
	UE_LOG(LogTemp, Log, TEXT("[Balance] 에디터 값을 파일 값으로 덮어썼습니다 — ")
		TEXT("직업군 %d종 / 피버 +%d·%d×%.0f / %d턴 / 보스 진행 %s"),
		Classes.Num(), FeverIncrement, DefenseFeverGain, FeverDamageCoefficient, Turns, ModeText);
}

const FBossPattern* ABattleManager::ChoosePattern(int32 TurnsLeft, const FString& PreviousId) const
{
	//가중치가 있고, 연속 재선택 제한에 걸리지 않고, 남은 턴에 들어가는 것만 후보
	TArray<const FBossPattern*> Candidates;
	TArray<float> Weights;
	float Total = 0.f;

	for (const FPatternWeight& Entry : Balance.Pool)
	{
		if (Entry.Weight <= 0.f)
		{
			continue;
		}
		if (!Balance.bAllowImmediateRepeat && Entry.PatternId == PreviousId)
		{
			continue;
		}

		const FBossPattern* Pattern = Balance.FindPattern(Entry.PatternId);
		if (!Pattern || Pattern->Steps.Num() > TurnsLeft)
		{
			continue;
		}

		//**고르고 난 뒤에도 남은 턴을 딱 맞출 수 있어야 한다.** 패턴이 2·3턴뿐인데
		//10턴에서 3+3+3을 고르면 1턴이 남아 아무것도 못 넣는 막다른 길이 된다
		if (!Balance.CanFillExactly(TurnsLeft - Pattern->Steps.Num()))
		{
			continue;
		}

		Candidates.Add(Pattern);
		Weights.Add(Entry.Weight);
		Total += Entry.Weight;
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	//누적 가중치 구간. pool의 배열 순서를 그대로 쓴다 (기획서 4절)
	float Pick = FMath::FRand() * Total;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Pick -= Weights[i];
		if (Pick < 0.f)
		{
			return Candidates[i];
		}
	}
	return Candidates.Last();
}

namespace
{
	//target.mode가 randomOne인 상태는 후보 구역 중 **한 곳만** 친다.
	//예고하기 전에 여기서 뽑아 두어야 예고와 판정이 같은 구역을 가리킨다 —
	//판정 시점에 뽑으면 회피가 확률 찍기가 된다
	void ResolveRandomTarget(FBossAction& Action)
	{
		if (Action.TargetMode != EBossTargetMode::RandomOne)
		{
			return;
		}

		TArray<int32, TInlineAllocator<3>> Candidates;
		for (int32 Zone = 0; Zone < 3; ++Zone)
		{
			if (Action.DamageByZone[Zone] > 0)
			{
				Candidates.Add(Zone);
			}
		}

		if (Candidates.Num() == 0)
		{
			return;
		}

		const int32 Picked = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		for (int32 Zone = 0; Zone < 3; ++Zone)
		{
			if (Zone != Picked)
			{
				Action.DamageByZone[Zone] = 0;
			}
		}
	}
}

void ABattleManager::ResolveBattleStartTarget()
{
	FBossAction& Action = TelegraphedAction;
	if (!Action.NeedsBattleStartTarget()) return;

	int64 Counts[3] = { 0, 0, 0 };
	int64 FeverTotals[3] = { 0, 0, 0 };
	for (const auto& Pair : Players)
	{
		const FBattlePlayer& Player = Pair.Value;
		if (Player.HP <= 0 || Player.ReviveCountdown > 0) continue;
		const int32 Lane = static_cast<int32>(Player.Lane);
		if (Lane < 0 || Lane >= 3) continue;
		++Counts[Lane];
		FeverTotals[Lane] += FMath::Max(0, Player.Fever);
	}

	//빈 구역도 후보다. 동률이면 한 곳을 균등 추첨한다.
	TArray<int32, TInlineAllocator<3>> Candidates;
	int64 Best = 0;
	for (int32 Lane = 0; Lane < 3; ++Lane)
	{
		if (Action.DamageByZone[Lane] <= 0) continue;
		const int64 Score = Action.TargetMode == EBossTargetMode::MostDamage ? FeverTotals[Lane] : Counts[Lane];
		const bool bBetter = Candidates.IsEmpty() || (Action.TargetMode == EBossTargetMode::LeastPlayer ? Score < Best : Score > Best);
		if (bBetter) { Best = Score; Candidates.Reset(); }
		if (Score == Best) Candidates.Add(Lane);
	}
	if (Candidates.IsEmpty()) return;
	const int32 Picked = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	for (int32 Lane = 0; Lane < 3; ++Lane)
		if (Lane != Picked) Action.DamageByZone[Lane] = 0;
	//이후 이동·피버 증감으로 다시 고르지 않는다.
	Action.TargetMode = EBossTargetMode::Fixed;
	UE_LOG(LogTemp, Log, TEXT("[Battle] 입력 종료 대상 확정 '%s' -> %s (인원 %lld/%lld/%lld, 피버 %lld/%lld/%lld)"),
		*Action.Id, *ZonesLabel(Action), Counts[0], Counts[1], Counts[2], FeverTotals[0], FeverTotals[1], FeverTotals[2]);
}

FBossAction ABattleManager::NextBossAction()
{
	if (!Balance.bLoaded)
	{
		// ponytail: 파일이 없을 때만 도는 임시 행동. 가운데를 2로 친다
		FBossAction Action;
		Action.Id = TEXT("fallback");
		Action.DamageByZone[(int32)EBattleLane::Mid] = 2;
		Action.TelegraphText = TEXT("NO BALANCE FILE");
		return Action;
	}

	//고정 순서 모드는 패턴을 거치지 않는다. 적힌 공격을 턴 번호대로 꺼내면 끝이다.
	//로딩 때 개수가 Turns와 같은지 확인했으므로 범위를 벗어나지 않는다
	if (Balance.PresetMode == EBossPresetMode::Direct)
	{
		const int32 Index = TurnIndex - 1;
		if (!Balance.PresetDirectActions.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Error, TEXT("[Battle] preset.directActions에 %d번째 턴이 없습니다"), TurnIndex);
			return FBossAction();
		}

		const FBossAction* Direct = Balance.FindAction(Balance.PresetDirectActions[Index]);
		FBossAction DirectAction = Direct ? *Direct : FBossAction();
		ResolveRandomTarget(DirectAction);
		return DirectAction;
	}

	const FBossPattern* Pattern = Balance.FindPattern(CurrentPatternId);

	//패턴이 없거나 끝났으면 다음 패턴으로 넘어간다. 실행 중에는 바꾸지 않는다
	if (!Pattern || PatternStep >= Pattern->Steps.Num())
	{
		//이번 턴을 포함해 남은 턴. 여기에 딱 맞게 끝나야 한다 (기획서 5절)
		const int32 TurnsLeft = Turns - TurnIndex + 1;

		if (Balance.PresetMode == EBossPresetMode::Custom)
		{
			//적힌 순서 그대로. 추첨하지 않는다
			if (!Balance.PresetCustomPatterns.IsValidIndex(PresetPatternIndex))
			{
				UE_LOG(LogTemp, Error, TEXT("[Battle] preset.customPatterns를 다 썼습니다 (남은 %d턴)"), TurnsLeft);
				return FBossAction();
			}

			Pattern = Balance.FindPattern(Balance.PresetCustomPatterns[PresetPatternIndex]);
			++PresetPatternIndex;
		}
		else
		{
			//직전에 끝난 패턴은 지금 CurrentPatternId에 들어 있다
			Pattern = ChoosePattern(TurnsLeft, CurrentPatternId);
		}

		if (!Pattern)
		{
			UE_LOG(LogTemp, Error, TEXT("[Battle] 남은 %d턴에 들어가는 패턴이 없습니다"), TurnsLeft);
			return FBossAction();
		}

		CurrentPatternId = Pattern->Id;
		PatternStep = 0;

		UE_LOG(LogTemp, Log, TEXT("[Battle] 패턴 '%s' (%d턴, 남은 %d턴)"),
			*Pattern->Name, Pattern->Steps.Num(), TurnsLeft);
	}

	const FBossAction* Action = Balance.FindAction(Pattern->Steps[PatternStep]);
	++PatternStep;

	FBossAction Chosen = Action ? *Action : FBossAction();
	ResolveRandomTarget(Chosen);
	return Chosen;
}

// ---------------- 표시 ----------------

void ABattleManager::SpawnBoss()
{
	if (BossUnit)
	{
		return;
	}

	//보스는 중심 쪽(-X)을 본다
	BossUnit = GetWorld()->SpawnActor<ABattleUnit>(
		BossUnitClass ? BossUnitClass.Get() : ABattleUnit::StaticClass(),
		BossLocation(), FRotator(0.f, 180.f, 0.f));
#if WITH_EDITOR
	BossUnit->SetActorLabel(TEXT("Boss"));
#endif
}

void ABattleManager::UpdateVisuals()
{
	if (BossUnit)
	{
		//체력이 없으니 남은 양이 아니라 쌓인 피해를 보여준다.
		//때려도 아무 반응이 없으면 공격이 들어가는지 알 수 없다
		BossUnit->SetLabelText(FString::Printf(TEXT("BOSS\n%d"), BossDamageShown));
	}

	for (const TPair<FString, FBattlePlayer>& Pair : Players)
	{
		const FBattlePlayer& P = Pair.Value;
		if (!P.Unit)
		{
			continue;
		}

		//사망 중에는 자리만 잡아두고 숨긴다
		P.Unit->SetActorHiddenInGame(P.ReviveCountdown > 0);

	}
	RefreshPlayerUI();
}

void ABattleManager::RefreshPlayerUI()
{
	const bool bCutScenePlaying = Phase == EBattlePhase::Move
		&& IsValid(CutSceneApplier) && CutSceneApplier->IsPlaying();
	const bool bShowInPhase = Phase != EBattlePhase::BattleStart && Phase != EBattlePhase::BattleEnd;

	for (const TPair<FString, FBattlePlayer>& Pair : Players)
	{
		const FBattlePlayer& Player = Pair.Value;
		if (!IsValid(Player.Unit)) continue;

		FBattlePlayerUIData Data;
		Data.DisplayName = Player.DisplayName;
		//칼이 닿기 전에는 깎이기 전 체력을 그린다
		Data.HP = Player.HP + Player.HPLossToShow;
		Data.MaxHP = Player.MaxHP;
		Data.Fever = Player.Fever;
		Data.Action = (Phase == EBattlePhase::Prepare || Phase == EBattlePhase::Collect)
			? Player.PendingAction : EBattleAction::Idle;

		if (Classes.IsValidIndex(Player.ClassIndex))
		{
			const FCharacterDef& Character = Classes[Player.ClassIndex];
			Data.CharacterId = Character.Id;
			Data.DefenseReduction = 1.f - Character.DefenseDamageRatio;
			if (Data.Action == EBattleAction::Attack1 || Data.Action == EBattleAction::Attack2)
			{
				const FAttackDef& Attack = Character.Attacks[Data.Action == EBattleAction::Attack1 ? 0 : 1];
				Data.BaseDamage = Attack.BaseDamage;
				Data.BonusDamage = FMath::RoundToInt(Player.DamageBonus + Player.Fever * FeverDamageCoefficient);
				Data.CritChance = FMath::Min(Attack.CritChance + Player.CritChanceBonus, CritChanceCap);
				Data.CritDamage = Attack.CritDamage + Player.CritDamageBonus;
			}
		}

		if (Data.Action == EBattleAction::AvoidL || Data.Action == EBattleAction::AvoidM
			|| Data.Action == EBattleAction::AvoidR)
		{
			Data.bFeverWillDrop = DecayFever(Player.Fever, Player.EvadeStreak + 1,
				EvadeStreakN, EvadeFeverReduction, EvadeRepeatMode, FeverRounding) < Player.Fever;
		}

		Player.Unit->SetPlayerUI(Data);
		//이번 턴에 쓰러질 사람도 칼이 닿을 때까지는 이름표를 남긴다
		Player.Unit->SetPlayerUIVisible(bShowInPhase && (Player.ReviveCountdown == 0 || Player.HPLossToShow > 0)
			&& (!bCutScenePlaying || Pair.Key == CutSceneUserId));
	}
}

void ABattleManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	// Screen-space widgets keep a constant pixel size unless we scale them explicitly.
	PlayerUIScaleElapsed += DeltaTime;
	if (PlayerUIScaleElapsed >= 0.1f)
	{
		PlayerUIScaleElapsed = 0.f;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			for (const TPair<FString, FBattlePlayer>& Pair : Players)
			{
				if (IsValid(Pair.Value.Unit)) Pair.Value.Unit->UpdatePlayerUIScale(CameraLocation);
			}
		}
	}
}

// ---------------- 순수 함수 ----------------

EBattleLane ABattleManager::ApplyMove(EBattleAction Action, EBattleLane Current)
{
	switch (Action)
	{
	case EBattleAction::AvoidL: return EBattleLane::Left;
	case EBattleAction::AvoidM: return EBattleLane::Mid;
	case EBattleAction::AvoidR: return EBattleLane::Right;
	default: return Current;
	}
}

bool ABattleManager::IsEvade(EBattleAction Action)
{
	return Action == EBattleAction::AvoidL
		|| Action == EBattleAction::AvoidM
		|| Action == EBattleAction::AvoidR;
}

float ABattleManager::ResistanceMultiplier(EResistance Resistance) const
{
	//배율도 밸런스 파일이 들고 있다 (resistanceMultipliers)
	return Balance.ResistanceMultipliers[(int32)Resistance];
}

float ABattleManager::ZoneResistMultiplier(const FBossAction& Action, EBattleLane Lane, EDamageType Type) const
{
	const FZoneResistance& Zone = Action.ResistanceByZone[(int32)Lane];
	return ResistanceMultiplier(Type == EDamageType::Physical ? Zone.Physical : Zone.Magical);
}

FString ABattleManager::ZonesLabel(const FBossAction& Action)
{
	if (Action.NeedsBattleStartTarget()) return TEXT("Pending (input closes)");
	static const TCHAR* Names[] = { TEXT("Left"), TEXT("Mid"), TEXT("Right") };

	FString Label;
	for (int32 Zone = 0; Zone < 3; ++Zone)
	{
		//피해가 0인 구역은 맞지 않는다
		if (Action.DamageByZone[Zone] <= 0)
		{
			continue;
		}

		if (!Label.IsEmpty())
		{
			Label += TEXT("+");
		}
		Label += Names[Zone];
	}

	return Label.IsEmpty() ? TEXT("Nothing") : Label;
}

int32 ABattleManager::DamageToBoss(int32 BaseDamage, int32 DamageBonus,
	float Resist, bool bCrit, float CritDamage)
{
	//피버는 여기 들어오지 않는다. 피버 한 칸은 평타 한 대이고,
	//평타는 BaseDamage = FeverAttackDamage / DamageBonus = 0 으로 다시 들어온다
	const float Raw = (BaseDamage + DamageBonus) * Resist * (1.f + (bCrit ? CritDamage : 0.f));

	//파일의 attackDamageRounding: roundHalfUp
	return FMath::Max(0, FMath::RoundToInt(Raw));
}

int32 ABattleManager::DecayFever(int32 Fever, int32 EvadeStreak, int32 StreakN,
	float Reduction, EEvadeRepeatMode Repeat, EFeverRounding Rounding)
{
	const int32 N = FMath::Max(1, StreakN);

	//기준 도달 후 매회는 N회째부터 계속, N회마다는 N의 배수일 때만
	const bool bTrigger = (Repeat == EEvadeRepeatMode::EveryAfter)
		? EvadeStreak >= N
		: (EvadeStreak > 0 && EvadeStreak % N == 0);


	if (!bTrigger)
	{
		return Fever;
	}

	//감소율이 1을 넘어도 음수가 되지 않게 배율에서 막는다
	const float Reduced = Fever * FMath::Max(0.f, 1.f - Reduction);

	switch (Rounding)
	{
	case EFeverRounding::Floor: return FMath::FloorToInt(Reduced);
	case EFeverRounding::Round: return FMath::RoundToInt(Reduced);
	default:                    return FMath::FloorToInt(Reduced);
	}
}

int32 ABattleManager::DamageToPlayer(EBattleAction Action, int32 ZoneDamage, int32 HitCount, float DefenseRatio)
{
	if (ZoneDamage <= 0)
	{
		return 0;
	}

	//방어는 비율을 곱한 뒤 올림 (파일의 defenseDamageRounding: ceil)
	const int32 PerHit = Action == EBattleAction::Defense
		? FMath::CeilToInt(ZoneDamage * DefenseRatio)
		: ZoneDamage;

	return PerHit * FMath::Max(1, HitCount);
}

void ABattleManager::RunSelfTest()
{
	check(ApplyMove(EBattleAction::AvoidL, EBattleLane::Right) == EBattleLane::Left);
	check(ApplyMove(EBattleAction::AvoidR, EBattleLane::Left) == EBattleLane::Right);
	check(ApplyMove(EBattleAction::Attack1, EBattleLane::Right) == EBattleLane::Right);

	check(IsEvade(EBattleAction::AvoidL) && IsEvade(EBattleAction::AvoidM) && IsEvade(EBattleAction::AvoidR));
	check(!IsEvade(EBattleAction::Attack1) && !IsEvade(EBattleAction::Defense) && !IsEvade(EBattleAction::Idle));
	check(ApplyMove(EBattleAction::Idle, EBattleLane::Mid) == EBattleLane::Mid);

	//구역 피해 0 = 안 맞음, 방어는 비율 곱한 뒤 올림, 마지막에 타격 횟수
	check(DamageToPlayer(EBattleAction::Idle, 0, 1, 0.5f) == 0);
	check(DamageToPlayer(EBattleAction::Idle, 20, 1, 0.5f) == 20);
	check(DamageToPlayer(EBattleAction::Defense, 20, 1, 0.5f) == 10);
	check(DamageToPlayer(EBattleAction::Defense, 0, 1, 0.5f) == 0);
	//홀수는 올림이어야 한다 (5 × 0.5 = 2.5 → 3)
	check(DamageToPlayer(EBattleAction::Defense, 5, 1, 0.5f) == 3);
	check(DamageToPlayer(EBattleAction::Defense, 1, 1, 0.5f) == 1);
	//올림은 타격마다 따로 적용된다 (3 → 2씩 두 번 = 4)
	check(DamageToPlayer(EBattleAction::Defense, 3, 2, 0.5f) == 4);
	check(DamageToPlayer(EBattleAction::Idle, 3, 2, 0.5f) == 6);

	check(ResistanceMultiplier(EResistance::Enduring) == 0.5f);
	check(ResistanceMultiplier(EResistance::Normal) == 1.0f);
	check(ResistanceMultiplier(EResistance::Vulnerable) == 1.5f);

	//구역별 내성 — 오른쪽을 치는 동안 왼쪽에서 물리로 때리면 취약
	{
		FBossAction Action;
		Action.DamageByZone[(int32)EBattleLane::Right] = 2;
		Action.ResistanceByZone[(int32)EBattleLane::Left].Physical = EResistance::Vulnerable;
		Action.ResistanceByZone[(int32)EBattleLane::Mid].Physical = EResistance::Enduring;

		check(ZoneResistMultiplier(Action, EBattleLane::Left, EDamageType::Physical) == 1.5f);
		check(ZoneResistMultiplier(Action, EBattleLane::Left, EDamageType::Magical) == 1.0f);
		check(ZoneResistMultiplier(Action, EBattleLane::Mid, EDamageType::Physical) == 0.5f);
		check(ZoneResistMultiplier(Action, EBattleLane::Right, EDamageType::Physical) == 1.0f);

		check(ZonesLabel(Action) == TEXT("Right"));
		Action.DamageByZone[(int32)EBattleLane::Left] = 1;
		check(ZonesLabel(Action) == TEXT("Left+Right"));

		//피해가 하나도 없으면 아무도 맞지 않는다
		check(ZonesLabel(FBossAction()) == TEXT("Nothing"));
	}

	//(기본 + 강화) * 내성 * (1 + 치명타 피해). **피버는 들어가지 않는다**
	check(DamageToBoss(1, 0, 1.0f, false, 0.4f) == 1);
	check(DamageToBoss(1, 2, 1.0f, false, 0.4f) == 3);
	check(DamageToBoss(2, 0, 0.5f, false, 0.4f) == 1);
	check(DamageToBoss(2, 0, 1.5f, false, 0.4f) == 3);
	check(DamageToBoss(10, 0, 1.0f, true, 0.4f) == 14);

	//피버 평타 — 고정 5에 구역 내성이 걸리고, 강화 공격력은 붙지 않는다
	check(DamageToBoss(5, 0, 1.0f, false, 0.f) == 5);
	check(DamageToBoss(5, 0, 1.5f, false, 0.f) == 8);   // 7.5 → 반올림
	check(DamageToBoss(5, 0, 0.5f, false, 0.f) == 3);   // 2.5 → 반올림
	check(DamageToBoss(5, 0, 1.0f, true, 0.4f) == 7);   // 5 * 1.4
	//피버 상한 — 파일의 feverCap이 스택 수를 그대로 자른다
	{
		const float SavedCap = FeverCap;
		FeverCap = 9.f;
		check(FeverStacksFor(0.f) == 0);
		check(FeverStacksFor(5.f) == 5);
		check(FeverStacksFor(9.f) == 9);
		check(FeverStacksFor(30.f) == 9);
		//0이면 무제한이라는 뜻이다
		FeverCap = 0.f;
		check(FeverStacksFor(30.f) == 30);
		FeverCap = SavedCap;
	}

	// ---- 연속 회피 피버 감소 (기획서 10절 검증 기준) ----

	//F=8, N=2, 감소율 0.5, 소수 유지, 4회 연속 회피
	//  기준 도달 후 매회 → 8, 4, 2, 1
	{
		float F = 8.f;
		const float Expected[] = { 8.f, 4.f, 2.f, 1.f };
		for (int32 e = 1; e <= 4; ++e)
		{
			F = DecayFever(F, e, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::None);
			check(F == Expected[e - 1]);
		}
	}

	//  N회마다 → 8, 4, 4, 2
	{
		float F = 8.f;
		const float Expected[] = { 8.f, 4.f, 4.f, 2.f };
		for (int32 e = 1; e <= 4; ++e)
		{
			F = DecayFever(F, e, 2, 0.5f, EEvadeRepeatMode::EachN, EFeverRounding::None);
			check(F == Expected[e - 1]);
		}
	}

	//감소율 1이면 기준에 닿는 순간 0이 된다 (밸런스 파일 기본값)
	check(DecayFever(7.f, 2, 2, 1.f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Floor) == 0.f);
	//기준에 닿기 전에는 손대지 않는다
	check(DecayFever(7.f, 1, 2, 1.f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Floor) == 7.f);
	//소수 처리 — 5 × 0.5 = 2.5
	check(DecayFever(5.f, 2, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::None) == 2);
	check(DecayFever(5.f, 2, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Floor) == 2.f);
	check(DecayFever(5.f, 2, 2, 0.5f, EEvadeRepeatMode::EveryAfter, EFeverRounding::Round) == 3.f);

	//자리 배분 — 일직선으로 늘어선 후보 10개에서 고른다 (0번이 기준점에 가장 가까움)
	{
		TArray<FVector> Line;
		for (int32 i = 0; i < 10; ++i)
		{
			Line.Add(FVector(i * 100.f, 0.f, 0.f));
		}

		TArray<int32> Picked;

		//하나면 기준점에 가장 가까운 자리
		ChooseSpreadSpots(Line, 1, FVector::ZeroVector, Picked);
		check(Picked.Num() == 1 && Picked[0] == 0);

		//둘이면 양 끝
		ChooseSpreadSpots(Line, 2, FVector::ZeroVector, Picked);
		check(Picked.Num() == 2 && Picked.Contains(0) && Picked.Contains(9));

		//셋이면 양 끝과 가운데
		ChooseSpreadSpots(Line, 3, FVector::ZeroVector, Picked);
		check(Picked.Num() == 3 && Picked.Contains(0) && Picked.Contains(9));
		check(Picked.Contains(4) || Picked.Contains(5));

		//자리를 다 쓰면 중복 없이 전부 고른다
		ChooseSpreadSpots(Line, 10, FVector::ZeroVector, Picked);
		check(Picked.Num() == 10 && TSet<int32>(Picked).Num() == 10);

		//인원이 자리보다 많으면 겹치되 아무도 빠지지 않는다
		ChooseSpreadSpots(Line, 12, FVector::ZeroVector, Picked);
		check(Picked.Num() == 12);

		//후보가 없으면 빈 결과
		ChooseSpreadSpots({}, 3, FVector::ZeroVector, Picked);
		check(Picked.Num() == 0);
	}

	//직업군별 앞뒤 분할 — 앞 직업군은 전원이 뒤 직업군 전원보다 보스에 가까워야 한다
	{
		const FVector Boss = FVector::ZeroVector;
		TArray<FVector> Line;
		for (int32 i = 0; i < 10; ++i)
		{
			Line.Add(FVector(i * 100.f, 0.f, 0.f));
		}

		TArray<TArray<int32>> ByClass;
		ChooseClassSpots(Line, { 2, 3 }, Boss, ByClass);

		check(ByClass.Num() == 2);
		check(ByClass[0].Num() == 2 && ByClass[1].Num() == 3);

		//자리가 겹치지 않는다
		TSet<int32> Used;
		for (const TArray<int32>& Picks : ByClass)
		{
			for (const int32 Index : Picks)
			{
				check(!Used.Contains(Index));
				Used.Add(Index);
			}
		}

		//앞 직업군의 가장 먼 자리 < 뒤 직업군의 가장 가까운 자리
		float FrontFar = 0.f;
		for (const int32 Index : ByClass[0])
		{
			FrontFar = FMath::Max(FrontFar, FVector::Dist(Line[Index], Boss));
		}

		float BackNear = TNumericLimits<float>::Max();
		for (const int32 Index : ByClass[1])
		{
			BackNear = FMath::Min(BackNear, FVector::Dist(Line[Index], Boss));
		}
		check(FrontFar < BackNear);

		//맨 뒤 직업군은 가장 먼 자리를 쓴다
		check(ByClass[1].Contains(9));

		//한 직업군만 있으면 그 구역을 전부 쓴다
		ChooseClassSpots(Line, { 0, 2 }, Boss, ByClass);
		check(ByClass[0].Num() == 0 && ByClass[1].Num() == 2);
		check(ByClass[1].Contains(9));

		//자리가 인원보다 적어도 직업군끼리는 자리를 섞어 쓰지 않는다.
		//같은 직업군 안에서는 겹치되, 앞 직업군이 뒤 직업군 자리를 침범하면 안 된다
		{
			TArray<FVector> Few;
			for (int32 i = 0; i < 3; ++i)
			{
				Few.Add(FVector(i * 100.f, 0.f, 0.f));
			}

			ChooseClassSpots(Few, { 4, 4 }, Boss, ByClass);
			check(ByClass[0].Num() == 4 && ByClass[1].Num() == 4);

			for (const int32 FrontIndex : ByClass[0])
			{
				check(!ByClass[1].Contains(FrontIndex));
			}
		}
	}

	EBattleAction A;
	check(ParseCommand(TEXT("  !공격 1 "), A) && A == EBattleAction::Attack1);
	check(ParseCommand(TEXT("!회피 좌"), A) && A == EBattleAction::AvoidL);
	check(ParseCommand(TEXT("!회피 우"), A) && A == EBattleAction::AvoidR);
	check(ParseCommand(TEXT("!방어"), A) && A == EBattleAction::Defense);
	check(!ParseCommand(TEXT("안녕하세요"), A));
	check(!ParseCommand(TEXT(""), A));

	//강화 단계가 실제 수치로 반영되는지
	{
		FPlayerRecord R;
		R.CharacterId = Classes[0].Id;
		check(MaxHPFor(R) == FMath::Clamp(Classes[0].BaseHP, 5, 6));
		R.MaxHPBonus = 3;
		R.CurrentHealth = 1;
		check(MaxHPFor(R) == FMath::Clamp(Classes[0].BaseHP + 3 * HPPerBonus, 5, 6));

		//강화 수치가 전투용 스냅샷으로 옮겨지는지
		R.DamageBonus = 2;
		R.CritChanceBonus = 1;
		FBattlePlayer P;
		ApplyRecordToPlayer(R, P);
		check(P.MaxHP == FMath::Clamp(Classes[0].BaseHP + 3 * HPPerBonus, 5, 6));
		check(P.DamageBonus == 2 * DamagePerBonus);
		check(FMath::IsNearlyEqual(P.CritChanceBonus, CritChancePerBonus));
	}

	//같은 자리 번호는 레인이 달라도 서로 다른 좌표여야 한다.
	//스팟 유무에 좌우되지 않도록 격자 계산을 직접 검사한다
	check(!GridSlotLocation(EBattleLane::Left, 0).Equals(GridSlotLocation(EBattleLane::Right, 0)));
	check(!GridSlotLocation(EBattleLane::Mid, 0).Equals(GridSlotLocation(EBattleLane::Mid, 1)));

	// --- 피버 특수 연출 대상 고르기 ---
	{
		auto Make = [](const TCHAR* Id, int32 Class, float Fever, int32 Damage)
		{
			FAttackCue C;
			C.UserId = Id;
			C.ClassIndex = Class;
			C.Fever = Fever;
			C.Damage = Damage;
			return C;
		};

		//직업군마다 정확히 하나씩, 피버가 가장 높은 것
		TArray<FAttackCue> Cues = {
			Make(TEXT("@a"), 0, 5.f, 10), Make(TEXT("@b"), 0, 15.f, 10),
			Make(TEXT("@c"), 1, 20.f, 10), Make(TEXT("@d"), 1, 10.f, 10) };
		MarkFeverHighlights(Cues);
		check(!Cues[0].bFeverHighlight && Cues[1].bFeverHighlight);
		check(Cues[2].bFeverHighlight && !Cues[3].bFeverHighlight);

		//피버가 같으면 더 많이 때린 쪽
		TArray<FAttackCue> Tie = { Make(TEXT("@a"), 0, 9.f, 3), Make(TEXT("@b"), 0, 9.f, 7) };
		MarkFeverHighlights(Tie);
		check(!Tie[0].bFeverHighlight && Tie[1].bFeverHighlight);

		//피버도 피해도 같으면 UserId로 가른다 — 순서가 바뀌어도 같은 사람이 뽑혀야 한다
		TArray<FAttackCue> Same = { Make(TEXT("@b"), 0, 9.f, 5), Make(TEXT("@a"), 0, 9.f, 5) };
		MarkFeverHighlights(Same);
		check(Same[1].bFeverHighlight && !Same[0].bFeverHighlight);

		//한 명뿐이어도 피버가 있으면 뽑힌다
		TArray<FAttackCue> One = { Make(TEXT("@a"), 0, 3.f, 0) };
		MarkFeverHighlights(One);
		check(One[0].bFeverHighlight);

		//**피버가 0이면 아무도 뽑히지 않는다.** 전원이 0인데 한 명이 뽑히면
		//평범한 공격에 특수 모션이 나간다
		TArray<FAttackCue> Flat = { Make(TEXT("@a"), 0, 0.f, 9), Make(TEXT("@b"), 0, 0.f, 4) };
		MarkFeverHighlights(Flat);
		check(!Flat[0].bFeverHighlight && !Flat[1].bFeverHighlight);

		TArray<FAttackCue> None;
		MarkFeverHighlights(None);
		check(None.Num() == 0);
	}

	// --- 보스 공격 시점 ---
	{
		//BattleTime까지 남은 시간이 길면 그만큼 기다린다
		check(FMath::IsNearlyEqual(BossStrikeDelay(0.5f, 2.f), 2.f));

		//어떤 값을 넣어도 플레이어 공격 연출보다 빨라지지 않는다 (기획서 PlayerAttack → BossAttack)
		for (const float Flurry : { 0.f, 0.5f, 1.2f, 3.f })
		{
			for (const float Remaining : { -1.f, 0.f, 0.3f, 2.f, 10.f })
			{
				check(BossStrikeDelay(Flurry, Remaining) >= Flurry);
			}
		}
	}

	// --- 걸음걸이 ---
	{
		//같은 거리라도 시간이 짧을수록 빨라져야 한다
		check(ChooseGait(300.f, 3.f, 350.f, 700.f) == EMoveGait::Walk);
		check(ChooseGait(1500.f, 3.f, 350.f, 700.f) == EMoveGait::Run);
		check(ChooseGait(3000.f, 3.f, 350.f, 700.f) == EMoveGait::Roll);

		//경계는 '미만'이 아래쪽이다
		check(ChooseGait(350.f, 1.f, 350.f, 700.f) == EMoveGait::Run);
		check(ChooseGait(700.f, 1.f, 350.f, 700.f) == EMoveGait::Roll);

		//시간이 없으면 거리가 0이어도 가장 빠른 것으로 본다 (0으로 나누지 않는다)
		check(ChooseGait(0.f, 0.f, 350.f, 700.f) == EMoveGait::Roll);
	}

	// --- 공격 모션 길이 ---
	{
		//스킬 한 번 + 평타 N회
		check(FMath::IsNearlyEqual(AttackMotionSeconds(1.f, 0.f), 1.f));
		check(FMath::IsNearlyEqual(AttackMotionSeconds(1.f, 2.f), 3.f));

		//음수가 들어와도 줄어들지 않는다
		check(FMath::IsNearlyEqual(AttackMotionSeconds(1.f, -3.f), 1.f));
		check(FMath::IsNearlyEqual(AttackMotionSeconds(-1.f, 2.f), 2.f));

		//섹션 길이를 돌려 쓴다 — 세라의 실제 값 1.12 / 2.05 / 1.50
		const TArray<float> Sections = { 1.12f, 2.05f, 1.5f };
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(Sections, 0), 0.f));
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(Sections, 1), 1.12f));
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(Sections, 3), 4.67f));
		//네 번째 대는 다시 1번 섹션
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(Sections, 4), 5.79f));
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(Sections, 6), 9.34f));

		//비어 있으면 0 — 연출이 안 나가는 것이지 시간을 먹지는 않는다
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(TArray<float>(), 5), 0.f));
		check(FMath::IsNearlyEqual(FeverFlurrySeconds(Sections, -2), 0.f));

		//피버가 높을수록 때릴 게 많아 이동에 쓸 시간이 줄어든다
		check(FeverFlurrySeconds(Sections, 9) > FeverFlurrySeconds(Sections, 2));
	}

	// --- 노티파이 시각 읽기 ---
	//
	//타격·발사 시점을 손으로 적지 않고 애셋에서 가져온다. 기댓값은 전부
	//**에디터에서 직접 읽은 절대 시각**이다 — 환산한 값을 넣으면 검사가
	//자기 자신을 증명하는 꼴이 되므로 넣지 않는다
	{
		const auto Load = [](const TCHAR* Path) -> UAnimMontage*
		{
			return LoadObject<UAnimMontage>(nullptr, Path);
		};

		UAnimMontage* const Attack1 = Load(TEXT("/Game/Asset/Battle/Sera/Animation/AnimationMontage/AM_Sera_Attack1.AM_Sera_Attack1"));
		UAnimMontage* const Attack2 = Load(TEXT("/Game/Asset/Battle/Sera/Animation/AnimationMontage/AM_Sera_Attack2.AM_Sera_Attack2"));
		UAnimMontage* const Fever = Load(TEXT("/Game/Asset/Battle/Sera/Animation/AnimationMontage/AM_Sera_FeverHit.AM_Sera_FeverHit"));

		//애셋이 옮겨가면 여기서 터진다. 조용히 넘어가는 것보다 낫다
		check(Attack1 && Attack2 && Fever);

		//섹션이 하나뿐인 몬타주는 절대 시각이 그대로 나온다
		check(FMath::IsNearlyEqual(ABattleUnit::NotifyTimeIn(Attack1, TEXT("Fire"), NAME_None), 2.33f, 0.01f));
		check(FMath::IsNearlyEqual(ABattleUnit::NotifyTimeIn(Attack2, TEXT("Fire"), NAME_None), 1.772f, 0.01f));

		//같은 이름이 셋이면 Occurrence로 고른다
		const float FeverAbs[3] = { 0.794f, 2.802f, 3.914f };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			check(FMath::IsNearlyEqual(
				ABattleUnit::NotifyTimeIn(Fever, TEXT("Fire"), NAME_None, Index),
				FeverAbs[Index], 0.01f));
		}

		//섹션을 주면 그 섹션 시작 기준으로 환산해 돌려준다.
		//기댓값을 적는 대신 '환산값 + 섹션 시작 == 절대 시각'을 본다 —
		//섹션 경계가 바뀌어도 검사가 따라온다
		const FName FeverSections[3] = { TEXT("FeverAttack1"), TEXT("FeverAttack2"), TEXT("FeverAttack3") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const int32 SectionIndex = Fever->GetSectionIndex(FeverSections[Index]);
			check(SectionIndex != INDEX_NONE);

			float Start = 0.f;
			float End = 0.f;
			Fever->GetSectionStartAndEndTime(SectionIndex, Start, End);

			const float Relative = ABattleUnit::NotifyTimeIn(Fever, TEXT("Fire"), FeverSections[Index]);

			//구간 안에 있어야 한다. 0이면 못 찾은 것이다
			check(Relative > 0.f && Relative <= End - Start);
			check(FMath::IsNearlyEqual(Start + Relative, FeverAbs[Index], 0.01f));
		}

		//없는 것을 물으면 0을 준다 (경고는 남는다)
		check(FMath::IsNearlyEqual(ABattleUnit::NotifyTimeIn(nullptr, TEXT("Fire"), NAME_None), 0.f));
		check(FMath::IsNearlyEqual(ABattleUnit::NotifyTimeIn(Attack1, TEXT("Nope"), NAME_None), 0.f));
		check(FMath::IsNearlyEqual(ABattleUnit::NotifyTimeIn(Attack1, TEXT("Fire"), TEXT("Nope")), 0.f));
		check(FMath::IsNearlyEqual(ABattleUnit::NotifyTimeIn(Attack1, TEXT("Fire"), NAME_None, 9), 0.f));
	}

	// --- 보스 칼이 닿는 시각 ---
	//
	//절대 시각은 에디터에서 읽어 둔 값이 없으므로 관계만 본다 —
	//Strike 섹션 안에서 닿고, 휘두르기 시작보다는 뒤이며, LeftRight는 왼쪽을 먼저 친다
	{
		const auto Load = [](const TCHAR* Name) -> UAnimMontage*
		{
			const FString Path = FString::Printf(
				TEXT("/Game/Asset/Battle/Trudon/Animation/AnimationMontage/%s.%s"), Name, Name);
			return LoadObject<UAnimMontage>(nullptr, *Path);
		};

		const auto StrikeLength = [](const UAnimMontage* Montage)
		{
			float Start = 0.f;
			float End = 0.f;
			Montage->GetSectionStartAndEndTime(Montage->GetSectionIndex(TEXT("Strike")), Start, End);
			return End - Start;
		};

		UAnimMontage* const Center = Load(TEXT("AM_Trudon_SingleStrikeCenter"));
		UAnimMontage* const LeftRight = Load(TEXT("AM_Trudon_LeftRight"));
		check(Center && LeftRight);

		//구역 전용 노티파이가 없으면 어느 구역이든 같은 Hit을 쓴다
		const float CenterHit = StrikeImpactSeconds(Center, EBattleLane::Mid);
		check(CenterHit > 0.f && CenterHit <= StrikeLength(Center));
		check(FMath::IsNearlyEqual(StrikeImpactSeconds(Center, EBattleLane::Left), CenterHit));
		check(FMath::IsNearlyEqual(StrikeImpactSeconds(Center, EBattleLane::Right), CenterHit));

		//LeftRight는 카메라 기준 왼쪽을 먼저 친다. 가운데는 안 맞지만,
		//가운데를 떠나는 회피자는 먼저 닿는 쪽에 맞춰 구른다
		const float LeftHit = StrikeImpactSeconds(LeftRight, EBattleLane::Left);
		const float RightHit = StrikeImpactSeconds(LeftRight, EBattleLane::Right);
		check(LeftHit > 0.f && LeftHit < RightHit && RightHit <= StrikeLength(LeftRight));
		check(FMath::IsNearlyEqual(StrikeImpactSeconds(LeftRight, EBattleLane::Mid), LeftHit));

		//공격이 없는 상태는 몬타주가 없다. 휘두르기 시작과 동시에 처리된다
		check(FMath::IsNearlyEqual(StrikeImpactSeconds(nullptr, EBattleLane::Mid), 0.f));
	}

	// --- 돌진 좌표 ---
	{
		const FVector Boss = BossLocation();

		//구역을 벗어나면 안 되므로, 보스에서 본 방향이 그대로여야 한다.
		//거리는 ChargeDistance로 줄어들어야 한다
		for (const EBattleLane Lane : { EBattleLane::Left, EBattleLane::Mid, EBattleLane::Right })
		{
			const FVector Slot = GridSlotLocation(Lane, 0);
			const FVector Charged = ChargeLocation(Slot);

			const FVector SlotDir = (Slot - Boss).GetSafeNormal();
			const FVector ChargedDir = (Charged - Boss).GetSafeNormal();
			check(FVector::DotProduct(SlotDir, ChargedDir) > 0.999f);

			//원래 자리가 더 가까웠다면 그대로 둬야 한다
			if ((Slot - Boss).Size() > ChargeDistance)
			{
				check(FMath::IsNearlyEqual((Charged - Boss).Size(), ChargeDistance, 1.f));
				check((Charged - Boss).Size() < (Slot - Boss).Size());
			}
			else
			{
				check(Charged.Equals(Slot));
			}
		}

		//보스 자리에 겹쳐 선 경우에도 0으로 나누지 않아야 한다
		check(ChargeLocation(Boss).Equals(Boss));
	}

	//패턴이 정확히 Turns에 맞춰 끝나는지. 난수가 끼어 있어 한 번으로는 못 잡는다.
	//밸런스 파일을 읽지 못했으면 검사할 패턴이 없으므로 건너뛴다
	//에디터에서 버튼으로 부르면 BeginPlay를 거치지 않아 아직 비어 있다.
	//수치를 매니저에 옮기지는 않고 검사용으로만 읽는다
	if (!Balance.bLoaded && bUseBalanceFiles)
	{
		Balance.LoadFromFolder(BalanceFolderPath());
	}

	//추첨 모드일 때만 반복 시행이 의미가 있다
	//randomOne은 후보 중 **정확히 한 구역만** 남겨야 한다. 둘 이상 남으면
	//예고한 것보다 넓게 맞고, 하나도 안 남으면 공격이 사라진다
	{
		FBossAction Fury;
		Fury.TargetMode = EBossTargetMode::RandomOne;
		Fury.DamageByZone[0] = Fury.DamageByZone[1] = Fury.DamageByZone[2] = 1;

		bool bSeen[3] = { false, false, false };
		for (int32 Trial = 0; Trial < 200; ++Trial)
		{
			FBossAction Rolled = Fury;
			ResolveRandomTarget(Rolled);

			int32 HitZones = 0;
			int32 Which = INDEX_NONE;
			for (int32 Zone = 0; Zone < 3; ++Zone)
			{
				if (Rolled.DamageByZone[Zone] > 0) { ++HitZones; Which = Zone; }
			}

			check(HitZones == 1);
			bSeen[Which] = true;
		}

		//200회면 세 구역이 전부 한 번씩은 나와야 한다 (안 나올 확률은 사실상 0)
		check(bSeen[0] && bSeen[1] && bSeen[2]);
	}

	if (Balance.bLoaded && Balance.PresetMode == EBossPresetMode::Random)
	{
		//에디터의 Turns는 아직 파일 값으로 덮이기 전일 수 있으므로 파일 쪽을 본다
		const int32 TotalTurns = Balance.Turns;

		for (int32 Trial = 0; Trial < 200; ++Trial)
		{
			int32 Left = TotalTurns;
			FString Previous;

			while (Left > 0)
			{
				const FBossPattern* Pattern = ChoosePattern(Left, Previous);
				//후보가 없으면 로딩 검증이 놓친 것이다
				check(Pattern);
				//남은 턴보다 긴 패턴은 후보가 될 수 없다
				check(Pattern->Steps.Num() <= Left);

				Left -= Pattern->Steps.Num();
				Previous = Pattern->Id;
			}

			//남기지도 넘기지도 않고 딱 맞아야 한다
			check(Left == 0);
		}

		UE_LOG(LogTemp, Log, TEXT("[Battle] 패턴 검사 통과 — %d턴을 200회 모두 정확히 채움"), TotalTurns);
	}
	else if (Balance.bLoaded)
	{
		//고정 순서 모드는 난수가 없어 반복 시행이 의미가 없다.
		//턴 수가 맞는지는 로딩 때 이미 막았으므로 여기서는 어떤 모드로 도는지만 남긴다
		UE_LOG(LogTemp, Log, TEXT("[Battle] 보스는 고정 순서(%s)로 돕니다 — 추첨 검사 생략"),
			Balance.PresetMode == EBossPresetMode::Direct ? TEXT("direct") : TEXT("custom"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] 밸런스 파일이 없어 패턴 검사를 건너뜁니다"));
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] SelfTest 통과"));
}
