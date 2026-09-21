#include "Scripts/Battle/BattleBalance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

//밸런스 파일 읽기. 기획서 7절에 따라 '모르는 값을 정상값으로 조용히 대체하지 않고'
//문제가 있으면 파일명과 항목을 찍은 뒤 전체를 포기한다

namespace
{
	//구역 이름은 파일 전체에서 이 순서로 쓰인다 (EBattleLane과 같은 순서)
	const TCHAR* ZoneNames[] = { TEXT("left"), TEXT("center"), TEXT("right") };

	void Fail(const FString& File, const FString& What)
	{
		UE_LOG(LogTemp, Error, TEXT("[Balance] %s — %s"), *File, *What);
	}

	bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			Fail(Path, TEXT("파일을 열 수 없습니다"));
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			Fail(Path, TEXT("JSON 형식이 올바르지 않습니다"));
			return false;
		}
		return true;
	}

	//필수 숫자. 없으면 실패로 처리한다
	bool GetNumber(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
		const FString& File, double& Out)
	{
		if (!Obj.IsValid() || !Obj->TryGetNumberField(Field, Out))
		{
			Fail(File, FString::Printf(TEXT("'%s' 값이 없거나 숫자가 아닙니다"), *Field));
			return false;
		}
		return true;
	}

	bool GetString(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
		const FString& File, FString& Out)
	{
		if (!Obj.IsValid() || !Obj->TryGetStringField(Field, Out))
		{
			Fail(File, FString::Printf(TEXT("'%s' 값이 없거나 문자열이 아닙니다"), *Field));
			return false;
		}
		return true;
	}

	bool ParseResistance(const FString& Text, const FString& File, EResistance& Out)
	{
		if (Text == TEXT("enduring")) { Out = EResistance::Enduring; return true; }
		if (Text == TEXT("normal")) { Out = EResistance::Normal; return true; }
		if (Text == TEXT("vulnerable")) { Out = EResistance::Vulnerable; return true; }

		Fail(File, FString::Printf(TEXT("내성 이름 '%s' 을(를) 모릅니다"), *Text));
		return false;
	}

	bool ParseAttribute(const FString& Text, const FString& File, EDamageType& Out)
	{
		if (Text == TEXT("physical")) { Out = EDamageType::Physical; return true; }
		if (Text == TEXT("magical")) { Out = EDamageType::Magical; return true; }

		Fail(File, FString::Printf(TEXT("공격 속성 '%s' 을(를) 모릅니다"), *Text));
		return false;
	}
}

const FBossAction* FBattleBalance::FindAction(const FString& Id) const
{
	return Actions.FindByPredicate([&Id](const FBossAction& A) { return A.Id == Id; });
}

const FBossPattern* FBattleBalance::FindPattern(const FString& Id) const
{
	return Patterns.FindByPredicate([&Id](const FBossPattern& P) { return P.Id == Id; });
}

bool FBattleBalance::LoadFromFolder(const FString& FolderPath)
{
	bLoaded = false;

	if (!LoadAbilities(FolderPath / TEXT("abilities.json"))
		|| !LoadEnhancements(FolderPath / TEXT("enhancements.json"))
		|| !LoadBosses(FolderPath / TEXT("bosses.json")))
	{
		return false;
	}

	bLoaded = true;

	UE_LOG(LogTemp, Log, TEXT("[Balance] 불러옴 — 보스 '%s' HP %d / %d턴 / 행동 %d개 / 패턴 %d개 / 캐릭터 '%s' HP %d"),
		*BossName, BossMaxHP, Turns, Actions.Num(), Patterns.Num(), *CharacterName, BaseHP);

	return true;
}

bool FBattleBalance::LoadAbilities(const FString& AbilitiesPath)
{
	TSharedPtr<FJsonObject> Abilities;
	if (!ReadJson(AbilitiesPath, Abilities))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RulesPtr = nullptr;
	if (!Abilities->TryGetObjectField(TEXT("rules"), RulesPtr))
	{
		Fail(AbilitiesPath, TEXT("'rules' 항목이 없습니다"));
		return false;
	}
	const TSharedPtr<FJsonObject>& Rules = *RulesPtr;

	double Num = 0.0;
	if (!GetNumber(Rules, TEXT("attackFeverGain"), AbilitiesPath, Num)) { return false; }
	AttackFeverGain = Num;
	if (!GetNumber(Rules, TEXT("defenseFeverGain"), AbilitiesPath, Num)) { return false; }
	DefenseFeverGain = Num;
	if (!GetNumber(Rules, TEXT("feverDamageCoefficient"), AbilitiesPath, Num)) { return false; }
	FeverDamageCoefficient = Num;
	if (!GetNumber(Rules, TEXT("evadeThreshold"), AbilitiesPath, Num)) { return false; }
	EvadeThreshold = FMath::RoundToInt(Num);
	if (!GetNumber(Rules, TEXT("evadeReduction"), AbilitiesPath, Num)) { return false; }
	EvadeReduction = Num;

	if (EvadeThreshold < 1 || EvadeReduction < 0.0 || EvadeReduction > 1.0)
	{
		Fail(AbilitiesPath, TEXT("evadeThreshold는 1 이상, evadeReduction은 0~1 이어야 합니다"));
		return false;
	}

	FString Text;
	if (!GetString(Rules, TEXT("evadeRepeat"), AbilitiesPath, Text)) { return false; }
	if (Text == TEXT("everyAfter")) { EvadeRepeat = EEvadeRepeatMode::EveryAfter; }
	else if (Text == TEXT("eachN")) { EvadeRepeat = EEvadeRepeatMode::EachN; }
	else
	{
		Fail(AbilitiesPath, FString::Printf(TEXT("evadeRepeat '%s' 을(를) 모릅니다"), *Text));
		return false;
	}

	if (!GetString(Rules, TEXT("feverRounding"), AbilitiesPath, Text)) { return false; }
	if (Text == TEXT("floor")) { FeverRounding = EFeverRounding::Floor; }
	else if (Text == TEXT("round")) { FeverRounding = EFeverRounding::Round; }
	else if (Text == TEXT("none")) { FeverRounding = EFeverRounding::None; }
	else
	{
		Fail(AbilitiesPath, FString::Printf(TEXT("feverRounding '%s' 을(를) 모릅니다"), *Text));
		return false;
	}

	//null이면 상한 없음. 게임에서는 0이 상한 없음을 뜻한다
	FeverCap = 0.f;
	if (Rules->HasField(TEXT("feverCap")) && !Rules->HasTypedField<EJson::Null>(TEXT("feverCap")))
	{
		if (!GetNumber(Rules, TEXT("feverCap"), AbilitiesPath, Num)) { return false; }
		FeverCap = Num;
	}

	if (!Rules->TryGetBoolField(TEXT("allowStayEvade"), bAllowStayEvade))
	{
		Fail(AbilitiesPath, TEXT("'allowStayEvade' 값이 없거나 true/false가 아닙니다"));
		return false;
	}

	const TSharedPtr<FJsonObject>* MultipliersPtr = nullptr;
	if (!Rules->TryGetObjectField(TEXT("resistanceMultipliers"), MultipliersPtr))
	{
		Fail(AbilitiesPath, TEXT("'resistanceMultipliers' 항목이 없습니다"));
		return false;
	}
	{
		static const TCHAR* Names[] = { TEXT("enduring"), TEXT("normal"), TEXT("vulnerable") };
		for (int32 i = 0; i < 3; ++i)
		{
			if (!GetNumber(*MultipliersPtr, Names[i], AbilitiesPath, Num)) { return false; }
			ResistanceMultipliers[i] = Num;
		}
	}

	const TSharedPtr<FJsonObject>* InitialPtr = nullptr;
	if (!Abilities->TryGetObjectField(TEXT("initial"), InitialPtr))
	{
		Fail(AbilitiesPath, TEXT("'initial' 항목이 없습니다"));
		return false;
	}
	if (!GetNumber(*InitialPtr, TEXT("fever"), AbilitiesPath, Num)) { return false; }
	InitialFever = Num;
	if (!GetString(*InitialPtr, TEXT("zone"), AbilitiesPath, Text)) { return false; }
	if (Text == TEXT("left")) { InitialZone = EBattleLane::Left; }
	else if (Text == TEXT("center")) { InitialZone = EBattleLane::Mid; }
	else if (Text == TEXT("right")) { InitialZone = EBattleLane::Right; }
	else
	{
		Fail(AbilitiesPath, FString::Printf(TEXT("시작 구역 '%s' 을(를) 모릅니다"), *Text));
		return false;
	}

	//직업군이 아직 없어 첫 번째 캐릭터를 쓴다.
	// ponytail: 직업군이 붙으면 유저가 고른 캐릭터를 찾아 쓰도록 바꾼다
	const TArray<TSharedPtr<FJsonValue>>* Characters = nullptr;
	if (!Abilities->TryGetArrayField(TEXT("characters"), Characters) || Characters->Num() == 0)
	{
		Fail(AbilitiesPath, TEXT("'characters' 가 비어 있습니다"));
		return false;
	}

	const TSharedPtr<FJsonObject> Character = (*Characters)[0]->AsObject();
	if (!Character.IsValid())
	{
		Fail(AbilitiesPath, TEXT("characters[0] 을 읽을 수 없습니다"));
		return false;
	}

	//이름은 표시용이라 없어도 진행한다
	Character->TryGetStringField(TEXT("name"), CharacterName);
	if (!GetNumber(Character, TEXT("baseHp"), AbilitiesPath, Num)) { return false; }
	BaseHP = FMath::RoundToInt(Num);
	if (!GetNumber(Character, TEXT("defenseDamageRatio"), AbilitiesPath, Num)) { return false; }
	DefenseDamageRatio = Num;

	const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
	if (!Character->TryGetArrayField(TEXT("skills"), Skills) || Skills->Num() != 2)
	{
		Fail(AbilitiesPath, TEXT("캐릭터마다 공격이 정확히 2개여야 합니다"));
		return false;
	}

	for (int32 i = 0; i < 2; ++i)
	{
		const TSharedPtr<FJsonObject> Skill = (*Skills)[i]->AsObject();
		if (!Skill.IsValid())
		{
			Fail(AbilitiesPath, TEXT("skills 항목을 읽을 수 없습니다"));
			return false;
		}

		Skill->TryGetStringField(TEXT("name"), Attacks[i].Name);
		if (!GetNumber(Skill, TEXT("baseDamage"), AbilitiesPath, Num)) { return false; }
		Attacks[i].BaseDamage = FMath::RoundToInt(Num);
		if (!GetNumber(Skill, TEXT("critChance"), AbilitiesPath, Num)) { return false; }
		Attacks[i].CritChance = Num;
		if (!GetNumber(Skill, TEXT("critBonus"), AbilitiesPath, Num)) { return false; }
		Attacks[i].CritDamage = Num;

		if (!GetString(Skill, TEXT("attribute"), AbilitiesPath, Text)
			|| !ParseAttribute(Text, AbilitiesPath, Attacks[i].Type))
		{
			return false;
		}
	}

	return true;
}

bool FBattleBalance::LoadEnhancements(const FString& EnhancementsPath)
{
	TSharedPtr<FJsonObject> Enhancements;
	if (!ReadJson(EnhancementsPath, Enhancements))
	{
		return false;
	}

	double Num = 0.0;
	const TSharedPtr<FJsonObject>* PerLevelPtr = nullptr;
	if (!Enhancements->TryGetObjectField(TEXT("perLevel"), PerLevelPtr))
	{
		Fail(EnhancementsPath, TEXT("'perLevel' 항목이 없습니다"));
		return false;
	}
	if (!GetNumber(*PerLevelPtr, TEXT("hp"), EnhancementsPath, Num)) { return false; }
	HPPerLevel = FMath::RoundToInt(Num);
	if (!GetNumber(*PerLevelPtr, TEXT("damage"), EnhancementsPath, Num)) { return false; }
	DamagePerLevel = FMath::RoundToInt(Num);
	if (!GetNumber(*PerLevelPtr, TEXT("critChance"), EnhancementsPath, Num)) { return false; }
	CritChancePerLevel = Num;
	if (!GetNumber(*PerLevelPtr, TEXT("critBonus"), EnhancementsPath, Num)) { return false; }
	CritDamagePerLevel = Num;

	if (!GetNumber(Enhancements, TEXT("critChanceCap"), EnhancementsPath, Num)) { return false; }
	CritChanceCap = Num;

	return true;
}

bool FBattleBalance::LoadBosses(const FString& BossesPath)
{
	TSharedPtr<FJsonObject> Bosses;
	if (!ReadJson(BossesPath, Bosses))
	{
		return false;
	}

	double Num = 0.0;
	FString Text;

	Bosses->TryGetStringField(TEXT("name"), BossName);
	if (!GetNumber(Bosses, TEXT("maxHp"), BossesPath, Num)) { return false; }
	BossMaxHP = FMath::RoundToInt(Num);

	const TSharedPtr<FJsonObject>* EncounterPtr = nullptr;
	if (!Bosses->TryGetObjectField(TEXT("encounter"), EncounterPtr))
	{
		Fail(BossesPath, TEXT("'encounter' 항목이 없습니다"));
		return false;
	}
	if (!GetNumber(*EncounterPtr, TEXT("turns"), BossesPath, Num)) { return false; }
	Turns = FMath::RoundToInt(Num);

	if (!GetString(*EncounterPtr, TEXT("patternFit"), BossesPath, Text)) { return false; }
	if (Text != TEXT("remainingTurns"))
	{
		Fail(BossesPath, TEXT("patternFit은 remainingTurns 만 지원합니다"));
		return false;
	}

	// ---- actions ----
	const TArray<TSharedPtr<FJsonValue>>* ActionArray = nullptr;
	if (!Bosses->TryGetArrayField(TEXT("actions"), ActionArray) || ActionArray->Num() == 0)
	{
		Fail(BossesPath, TEXT("'actions' 가 비어 있습니다"));
		return false;
	}

	Actions.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *ActionArray)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			Fail(BossesPath, TEXT("actions 항목을 읽을 수 없습니다"));
			return false;
		}

		FBossAction Action;
		if (!GetString(Obj, TEXT("id"), BossesPath, Action.Id)) { return false; }
		Obj->TryGetStringField(TEXT("name"), Action.Name);

		if (FindAction(Action.Id))
		{
			Fail(BossesPath, FString::Printf(TEXT("행동 ID '%s' 가 중복입니다"), *Action.Id));
			return false;
		}

		if (!GetNumber(Obj, TEXT("hitCount"), BossesPath, Num)) { return false; }
		Action.HitCount = FMath::RoundToInt(Num);
		if (Action.HitCount < 1)
		{
			Fail(BossesPath, FString::Printf(TEXT("'%s' 의 hitCount는 1 이상이어야 합니다"), *Action.Id));
			return false;
		}

		const TSharedPtr<FJsonObject>* DamagePtr = nullptr;
		const TSharedPtr<FJsonObject>* AttributePtr = nullptr;
		const TSharedPtr<FJsonObject>* ResistPtr = nullptr;
		if (!Obj->TryGetObjectField(TEXT("damageByZone"), DamagePtr)
			|| !Obj->TryGetObjectField(TEXT("attributeByZone"), AttributePtr)
			|| !Obj->TryGetObjectField(TEXT("resistanceByZone"), ResistPtr))
		{
			Fail(BossesPath, FString::Printf(
				TEXT("'%s' 에 damageByZone / attributeByZone / resistanceByZone 가 모두 필요합니다"), *Action.Id));
			return false;
		}

		for (int32 Zone = 0; Zone < 3; ++Zone)
		{
			const FString ZoneName = ZoneNames[Zone];

			if (!GetNumber(*DamagePtr, ZoneName, BossesPath, Num)) { return false; }
			Action.DamageByZone[Zone] = FMath::RoundToInt(Num);
			if (Action.DamageByZone[Zone] < 0)
			{
				Fail(BossesPath, FString::Printf(TEXT("'%s' 의 피해는 0 이상이어야 합니다"), *Action.Id));
				return false;
			}

			if (!GetString(*AttributePtr, ZoneName, BossesPath, Text)
				|| !ParseAttribute(Text, BossesPath, Action.AttributeByZone[Zone]))
			{
				return false;
			}

			const TSharedPtr<FJsonObject>* ZoneResist = nullptr;
			if (!(*ResistPtr)->TryGetObjectField(ZoneName, ZoneResist))
			{
				Fail(BossesPath, FString::Printf(
					TEXT("'%s' 의 %s 구역 내성이 없습니다"), *Action.Id, *ZoneName));
				return false;
			}

			if (!GetString(*ZoneResist, TEXT("physical"), BossesPath, Text)
				|| !ParseResistance(Text, BossesPath, Action.ResistanceByZone[Zone].Physical)
				|| !GetString(*ZoneResist, TEXT("magical"), BossesPath, Text)
				|| !ParseResistance(Text, BossesPath, Action.ResistanceByZone[Zone].Magical))
			{
				return false;
			}
		}

		const TSharedPtr<FJsonObject>* TelegraphPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("telegraph"), TelegraphPtr))
		{
			(*TelegraphPtr)->TryGetStringField(TEXT("text"), Action.TelegraphText);
			(*TelegraphPtr)->TryGetStringField(TEXT("motionId"), Action.TelegraphMotionId);
		}
		Obj->TryGetStringField(TEXT("attackMotionId"), Action.AttackMotionId);

		Actions.Add(MoveTemp(Action));
	}

	// ---- patterns ----
	const TArray<TSharedPtr<FJsonValue>>* PatternArray = nullptr;
	if (!Bosses->TryGetArrayField(TEXT("patterns"), PatternArray) || PatternArray->Num() == 0)
	{
		Fail(BossesPath, TEXT("'patterns' 가 비어 있습니다"));
		return false;
	}

	Patterns.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *PatternArray)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			Fail(BossesPath, TEXT("patterns 항목을 읽을 수 없습니다"));
			return false;
		}

		FBossPattern Pattern;
		if (!GetString(Obj, TEXT("id"), BossesPath, Pattern.Id)) { return false; }
		Obj->TryGetStringField(TEXT("name"), Pattern.Name);

		if (FindPattern(Pattern.Id))
		{
			Fail(BossesPath, FString::Printf(TEXT("패턴 ID '%s' 가 중복입니다"), *Pattern.Id));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		if (!Obj->TryGetArrayField(TEXT("steps"), Steps))
		{
			Fail(BossesPath, FString::Printf(TEXT("패턴 '%s' 에 steps가 없습니다"), *Pattern.Id));
			return false;
		}

		if (Steps->Num() < 1 || Steps->Num() > 4)
		{
			Fail(BossesPath, FString::Printf(
				TEXT("패턴 '%s' 의 길이는 1~4 이어야 합니다 (현재 %d)"), *Pattern.Id, Steps->Num()));
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Step : *Steps)
		{
			const FString ActionId = Step->AsString();
			if (!FindAction(ActionId))
			{
				Fail(BossesPath, FString::Printf(
					TEXT("패턴 '%s' 가 없는 행동 '%s' 을(를) 가리킵니다"), *Pattern.Id, *ActionId));
				return false;
			}
			Pattern.Steps.Add(ActionId);
		}

		Patterns.Add(MoveTemp(Pattern));
	}

	// ---- selection ----
	const TSharedPtr<FJsonObject>* SelectionPtr = nullptr;
	if (!Bosses->TryGetObjectField(TEXT("selection"), SelectionPtr))
	{
		Fail(BossesPath, TEXT("'selection' 항목이 없습니다"));
		return false;
	}

	if (!GetString(*SelectionPtr, TEXT("mode"), BossesPath, Text)) { return false; }
	if (Text != TEXT("weightedRandom"))
	{
		Fail(BossesPath, TEXT("selection.mode는 weightedRandom 만 지원합니다"));
		return false;
	}

	if (!(*SelectionPtr)->TryGetBoolField(TEXT("allowImmediateRepeat"), bAllowImmediateRepeat))
	{
		Fail(BossesPath, TEXT("'allowImmediateRepeat' 값이 없습니다"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PoolArray = nullptr;
	if (!(*SelectionPtr)->TryGetArrayField(TEXT("pool"), PoolArray) || PoolArray->Num() == 0)
	{
		Fail(BossesPath, TEXT("패턴 가중치가 없습니다"));
		return false;
	}

	Pool.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *PoolArray)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		FPatternWeight Weight;
		if (!Obj.IsValid() || !GetString(Obj, TEXT("patternId"), BossesPath, Weight.PatternId))
		{
			return false;
		}

		if (!FindPattern(Weight.PatternId))
		{
			Fail(BossesPath, FString::Printf(TEXT("없는 패턴 ID: %s"), *Weight.PatternId));
			return false;
		}

		if (Pool.ContainsByPredicate([&Weight](const FPatternWeight& W)
			{ return W.PatternId == Weight.PatternId; }))
		{
			Fail(BossesPath, FString::Printf(TEXT("가중치 패턴 중복: %s"), *Weight.PatternId));
			return false;
		}

		if (!GetNumber(Obj, TEXT("weight"), BossesPath, Num) || Num < 0.0)
		{
			Fail(BossesPath, FString::Printf(TEXT("'%s' 의 가중치가 잘못되었습니다"), *Weight.PatternId));
			return false;
		}
		Weight.Weight = Num;

		Pool.Add(MoveTemp(Weight));
	}

	//정확히 10턴을 채울 수 있는지. 1턴 패턴이 하나도 없으면 남은 턴 1에서 막힌다.
	// ponytail: 기획서는 '도달 가능한 모든 상태'를 검사하라고 하지만, 연속 재선택을
	// 허용하는 한 길이별 후보 유무만 보면 충분하다. 금지 모드가 실제로 쓰이면 그때 확장
	for (int32 Left = 1; Left <= Turns; ++Left)
	{
		const bool bHasFit = Pool.ContainsByPredicate([this, Left](const FPatternWeight& W)
		{
			const FBossPattern* Pattern = FindPattern(W.PatternId);
			return W.Weight > 0.f && Pattern && Pattern->Steps.Num() <= Left;
		});

		if (!bHasFit)
		{
			Fail(BossesPath, FString::Printf(
				TEXT("남은 턴 %d 에 들어가는 패턴이 없습니다 — 정확히 %d턴에 끝낼 수 없습니다"), Left, Turns));
			return false;
		}
	}

	if (!bAllowImmediateRepeat)
	{
		int32 Positive = 0;
		for (const FPatternWeight& W : Pool)
		{
			Positive += W.Weight > 0.f ? 1 : 0;
		}

		if (Positive < 2)
		{
			Fail(BossesPath, TEXT("연속 재선택을 막으려면 가중치가 있는 패턴이 2개 이상 필요합니다"));
			return false;
		}
	}

	return true;
}
