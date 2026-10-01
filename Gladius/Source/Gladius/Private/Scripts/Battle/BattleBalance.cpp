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

	//숫자를 읽으면서 범위까지 확인한다. 시뮬레이터(engine.js)의 검사 범위와 맞춘다
	bool GetFloat(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
		const FString& File, float& Out, double Min, double Max)
	{
		double Num = 0.0;
		if (!Obj.IsValid() || !Obj->TryGetNumberField(Field, Num))
		{
			Fail(File, FString::Printf(TEXT("'%s' 값이 없거나 숫자가 아닙니다"), *Field));
			return false;
		}

		if (Num < Min || Num > Max)
		{
			Fail(File, FString::Printf(TEXT("'%s' 값 %g 은(는) %g ~ %g 범위를 벗어납니다"), *Field, Num, Min, Max));
			return false;
		}

		Out = Num;
		return true;
	}

	bool GetInt(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
		const FString& File, int32& Out, double Min, double Max)
	{
		float Value = 0.f;
		if (!GetFloat(Obj, Field, File, Value, Min, Max))
		{
			return false;
		}

		Out = FMath::RoundToInt(Value);
		return true;
	}

	//readme '읽는 방법과 공통 항목'의 schemaVersion / kind / revision 검사.
	//세 파일 모두 이 셋을 가진다
	bool CheckHeader(const TSharedPtr<FJsonObject>& Obj, const FString& File,
		const TCHAR* ExpectedKind, int32& OutRevision)
	{
		static constexpr int32 SupportedSchemaVersion = 1;

		double Version = 0.0;
		if (!Obj->TryGetNumberField(TEXT("schemaVersion"), Version))
		{
			Fail(File, TEXT("'schemaVersion' 이 없습니다"));
			return false;
		}

		if (FMath::RoundToInt(Version) != SupportedSchemaVersion)
		{
			Fail(File, FString::Printf(
				TEXT("형식 버전 %d 는 지원하지 않습니다 (지원: %d) — 게임 코드를 함께 갱신해야 합니다"),
				FMath::RoundToInt(Version), SupportedSchemaVersion));
			return false;
		}

		FString Kind;
		if (!Obj->TryGetStringField(TEXT("kind"), Kind))
		{
			Fail(File, TEXT("'kind' 가 없습니다"));
			return false;
		}

		if (Kind != ExpectedKind)
		{
			Fail(File, FString::Printf(TEXT("'%s' 파일이어야 하는데 '%s' 입니다 — 파일이 바뀐 것 같습니다"),
				ExpectedKind, *Kind));
			return false;
		}

		//재현 기록용. 없으면 0으로 두고 진행한다
		double Revision = 0.0;
		Obj->TryGetNumberField(TEXT("revision"), Revision);
		OutRevision = FMath::RoundToInt(Revision);

		return true;
	}

	//"이 값이어야 한다"가 정해진 문자열 항목. 다르면 실행하지 않는다
	bool CheckFixedString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field,
		const TCHAR* Expected, const FString& File)
	{
		FString Value;
		if (!Obj->TryGetStringField(Field, Value))
		{
			Fail(File, FString::Printf(TEXT("'%s' 가 없습니다"), Field));
			return false;
		}

		if (Value != Expected)
		{
			Fail(File, FString::Printf(
				TEXT("'%s' 는 '%s' 고정입니다 (현재 '%s') — 바꾸려면 게임 코드도 함께 고쳐야 합니다"),
				Field, Expected, *Value));
			return false;
		}

		return true;
	}

	//readme가 '고정 계약'이라고 못 박은 항목들. 게임 코드가 이 방식으로 짜여 있어서
	//파일만 바뀌면 시뮬레이터와 결과가 갈린다. 다르면 실행하지 않는다
	bool CheckFixedContract(const TSharedPtr<FJsonObject>& Rules, const FString& File)
	{
		if (!CheckFixedString(Rules, TEXT("attackDamageRounding"), TEXT("roundHalfUp"), File)
			|| !CheckFixedString(Rules, TEXT("defenseDamageRounding"), TEXT("ceil"), File))
		{
			return false;
		}

		static const TCHAR* ExpectedOrder[] = {
			TEXT("move"), TEXT("playerAttack"), TEXT("bossAttack"), TEXT("feverAndDeath") };

		const TArray<TSharedPtr<FJsonValue>>* Order = nullptr;
		if (!Rules->TryGetArrayField(TEXT("turnOrder"), Order)
			|| Order->Num() != UE_ARRAY_COUNT(ExpectedOrder))
		{
			Fail(File, TEXT("'turnOrder' 는 move / playerAttack / bossAttack / feverAndDeath 네 개여야 합니다"));
			return false;
		}

		for (int32 i = 0; i < Order->Num(); ++i)
		{
			if ((*Order)[i]->AsString() != ExpectedOrder[i])
			{
				Fail(File, FString::Printf(
					TEXT("'turnOrder' %d번째는 '%s' 고정입니다 (현재 '%s') — 바꾸려면 게임 코드도 함께 고쳐야 합니다"),
					i + 1, ExpectedOrder[i], *(*Order)[i]->AsString()));
				return false;
			}
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

int32 FBattleBalance::FindCharacterIndex(const FString& Id) const
{
	return Characters.IndexOfByPredicate([&Id](const FCharacterDef& C) { return C.Id == Id; });
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
		//여기까지 읽은 값이 남아 있으면 '파일 절반 + 에디터 절반'으로 돈다.
		//전부 통과했을 때만 쓴다는 약속을 지키려면 되돌려야 한다
		*this = FBattleBalance();
		return false;
	}

	bLoaded = true;

	FString CharacterList;
	for (const FCharacterDef& Def : Characters)
	{
		CharacterList += FString::Printf(TEXT(" %s(HP %d, 공격 %d·%d)"),
			*Def.Id, Def.BaseHP, Def.Attacks[0].BaseDamage, Def.Attacks[1].BaseDamage);
	}

	UE_LOG(LogTemp, Log, TEXT("[Balance] 불러옴 — 보스 '%s' HP %d / %d턴 / 행동 %d개 / 패턴 %d개 / 직업군%s"),
		*BossName, BossMaxHP, Turns, Actions.Num(), Patterns.Num(), *CharacterList);

	//어떤 개정판으로 돌았는지 남겨야 나중에 통계를 그때 수치와 맞춰볼 수 있다
	UE_LOG(LogTemp, Log, TEXT("[Balance] revision — abilities %d / enhancements %d / bosses %d"),
		AbilitiesRevision, EnhancementsRevision, BossRevision);

	return true;
}

bool FBattleBalance::LoadAbilities(const FString& AbilitiesPath)
{
	TSharedPtr<FJsonObject> Abilities;
	if (!ReadJson(AbilitiesPath, Abilities))
	{
		return false;
	}

	if (!CheckHeader(Abilities, AbilitiesPath, TEXT("gladius-abilities"), AbilitiesRevision))
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

	if (!CheckFixedContract(Rules, AbilitiesPath))
	{
		return false;
	}

	if (!GetFloat(Rules, TEXT("attackFeverGain"), AbilitiesPath, AttackFeverGain, 0, 100)
		|| !GetFloat(Rules, TEXT("defenseFeverGain"), AbilitiesPath, DefenseFeverGain, 0, 100)
		|| !GetFloat(Rules, TEXT("feverDamageCoefficient"), AbilitiesPath, FeverDamageCoefficient, 0, 20)
		|| !GetInt(Rules, TEXT("evadeThreshold"), AbilitiesPath, EvadeThreshold, 1, 100)
		|| !GetFloat(Rules, TEXT("evadeReduction"), AbilitiesPath, EvadeReduction, 0, 1))
	{
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
	if (Rules->HasField(TEXT("feverCap")) && !Rules->HasTypedField<EJson::Null>(TEXT("feverCap"))
		&& !GetFloat(Rules, TEXT("feverCap"), AbilitiesPath, FeverCap, 0, 10000))
	{
		return false;
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
			if (!GetFloat(*MultipliersPtr, Names[i], AbilitiesPath, ResistanceMultipliers[i], 0, 100))
			{
				return false;
			}
		}
	}

	const TSharedPtr<FJsonObject>* InitialPtr = nullptr;
	if (!Abilities->TryGetObjectField(TEXT("initial"), InitialPtr))
	{
		Fail(AbilitiesPath, TEXT("'initial' 항목이 없습니다"));
		return false;
	}
	if (!GetFloat(*InitialPtr, TEXT("fever"), AbilitiesPath, InitialFever, 0, 10000)
		|| !GetInt(*InitialPtr, TEXT("evadeStreak"), AbilitiesPath, InitialEvadeStreak, 0, 100))
	{
		return false;
	}
	if (!GetString(*InitialPtr, TEXT("zone"), AbilitiesPath, Text)) { return false; }
	if (Text == TEXT("left")) { InitialZone = EBattleLane::Left; }
	else if (Text == TEXT("center")) { InitialZone = EBattleLane::Mid; }
	else if (Text == TEXT("right")) { InitialZone = EBattleLane::Right; }
	else
	{
		Fail(AbilitiesPath, FString::Printf(TEXT("시작 구역 '%s' 을(를) 모릅니다"), *Text));
		return false;
	}

	//파일에 적힌 모든 직업군을 순서대로 읽는다.
	//이 순서가 자리 배치의 앞뒤가 된다 — 0번이 보스에 가장 가까운 쪽
	const TArray<TSharedPtr<FJsonValue>>* CharacterArray = nullptr;
	if (!Abilities->TryGetArrayField(TEXT("characters"), CharacterArray) || CharacterArray->Num() == 0)
	{
		Fail(AbilitiesPath, TEXT("'characters' 가 비어 있습니다"));
		return false;
	}

	Characters.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *CharacterArray)
	{
		const TSharedPtr<FJsonObject> Character = Value->AsObject();
		if (!Character.IsValid())
		{
			Fail(AbilitiesPath, TEXT("characters 항목을 읽을 수 없습니다"));
			return false;
		}

		FCharacterDef Def;
		if (!GetString(Character, TEXT("id"), AbilitiesPath, Def.Id)) { return false; }

		if (FindCharacterIndex(Def.Id) != INDEX_NONE)
		{
			Fail(AbilitiesPath, FString::Printf(TEXT("캐릭터 ID '%s' 가 중복입니다"), *Def.Id));
			return false;
		}

		//이름은 표시용이라 없어도 진행한다
		Character->TryGetStringField(TEXT("name"), Def.Name);
		if (!GetInt(Character, TEXT("baseHp"), AbilitiesPath, Def.BaseHP, 1, 1000)
			|| !GetFloat(Character, TEXT("defenseDamageRatio"), AbilitiesPath, Def.DefenseDamageRatio, 0, 1))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Character->TryGetArrayField(TEXT("skills"), Skills) || Skills->Num() != 2)
		{
			Fail(AbilitiesPath, FString::Printf(
				TEXT("'%s' 의 공격이 정확히 2개여야 합니다"), *Def.Id));
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

			Skill->TryGetStringField(TEXT("name"), Def.Attacks[i].Name);
			if (!GetInt(Skill, TEXT("baseDamage"), AbilitiesPath, Def.Attacks[i].BaseDamage, 0, 10000)
				|| !GetFloat(Skill, TEXT("critChance"), AbilitiesPath, Def.Attacks[i].CritChance, 0, 1)
				|| !GetFloat(Skill, TEXT("critBonus"), AbilitiesPath, Def.Attacks[i].CritDamage, 0, 10))
			{
				return false;
			}

			if (!GetString(Skill, TEXT("attribute"), AbilitiesPath, Text)
				|| !ParseAttribute(Text, AbilitiesPath, Def.Attacks[i].Type))
			{
				return false;
			}
		}

		Characters.Add(MoveTemp(Def));
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

	if (!CheckHeader(Enhancements, EnhancementsPath, TEXT("gladius-enhancements"), EnhancementsRevision))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PerLevelPtr = nullptr;
	if (!Enhancements->TryGetObjectField(TEXT("perLevel"), PerLevelPtr))
	{
		Fail(EnhancementsPath, TEXT("'perLevel' 항목이 없습니다"));
		return false;
	}
	if (!GetInt(*PerLevelPtr, TEXT("hp"), EnhancementsPath, HPPerLevel, 0, 1000)
		|| !GetInt(*PerLevelPtr, TEXT("damage"), EnhancementsPath, DamagePerLevel, 0, 1000)
		|| !GetFloat(*PerLevelPtr, TEXT("critChance"), EnhancementsPath, CritChancePerLevel, 0, 1)
		|| !GetFloat(*PerLevelPtr, TEXT("critBonus"), EnhancementsPath, CritDamagePerLevel, 0, 10)
		|| !GetFloat(Enhancements, TEXT("critChanceCap"), EnhancementsPath, CritChanceCap, 0, 1))
	{
		return false;
	}

	//새 유저가 들고 시작하는 강화 단계
	const TSharedPtr<FJsonObject>* InitialLevelsPtr = nullptr;
	if (!Enhancements->TryGetObjectField(TEXT("initialLevels"), InitialLevelsPtr))
	{
		Fail(EnhancementsPath, TEXT("'initialLevels' 항목이 없습니다"));
		return false;
	}

	if (!GetInt(*InitialLevelsPtr, TEXT("hp"), EnhancementsPath, InitialHPLevel, 0, 100)
		|| !GetInt(*InitialLevelsPtr, TEXT("damage"), EnhancementsPath, InitialDamageLevel, 0, 100)
		|| !GetInt(*InitialLevelsPtr, TEXT("critChance"), EnhancementsPath, InitialCritChanceLevel, 0, 100)
		|| !GetInt(*InitialLevelsPtr, TEXT("critBonus"), EnhancementsPath, InitialCritDamageLevel, 0, 100))
	{
		return false;
	}

	return true;
}

bool FBattleBalance::LoadBosses(const FString& BossesPath)
{
	TSharedPtr<FJsonObject> Bosses;
	if (!ReadJson(BossesPath, Bosses))
	{
		return false;
	}

	if (!CheckHeader(Bosses, BossesPath, TEXT("gladius-boss-patterns"), BossRevision))
	{
		return false;
	}

	FString Text;

	Bosses->TryGetStringField(TEXT("name"), BossName);
	if (!GetInt(Bosses, TEXT("maxHp"), BossesPath, BossMaxHP, 1, 1e9)) { return false; }

	const TSharedPtr<FJsonObject>* EncounterPtr = nullptr;
	if (!Bosses->TryGetObjectField(TEXT("encounter"), EncounterPtr))
	{
		Fail(BossesPath, TEXT("'encounter' 항목이 없습니다"));
		return false;
	}
	if (!GetInt(*EncounterPtr, TEXT("turns"), BossesPath, Turns, 1, 10)) { return false; }

	if (!CheckFixedString(*EncounterPtr, TEXT("patternFit"), TEXT("remainingTurns"), BossesPath))
	{
		return false;
	}

	// ---- actions ----
	const TArray<TSharedPtr<FJsonValue>>* ActionArray = nullptr;
	if (!Bosses->TryGetArrayField(TEXT("actions"), ActionArray) || ActionArray->Num() == 0)
	{
		Fail(BossesPath, TEXT("'actions' 가 비어 있습니다"));
		return false;
	}

	if (ActionArray->Num() > 100)
	{
		Fail(BossesPath, FString::Printf(TEXT("공격은 최대 100개입니다 (현재 %d개)"), ActionArray->Num()));
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

		if (!GetInt(Obj, TEXT("hitCount"), BossesPath, Action.HitCount, 1, 10)) { return false; }

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

			if (!GetInt(*DamagePtr, ZoneName, BossesPath, Action.DamageByZone[Zone], 0, 100))
			{
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

	if (PatternArray->Num() > 100)
	{
		Fail(BossesPath, FString::Printf(TEXT("패턴은 최대 100개입니다 (현재 %d개)"), PatternArray->Num()));
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

	if (!CheckFixedString(*SelectionPtr, TEXT("mode"), TEXT("weightedRandom"), BossesPath))
	{
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

		if (!GetFloat(Obj, TEXT("weight"), BossesPath, Weight.Weight, 0, 10000)) { return false; }

		Pool.Add(MoveTemp(Weight));
	}

	//정확히 Turns턴에 맞춰 끝내려면 1턴 패턴이 반드시 있어야 한다.
	//남은 턴이 1일 때 쓸 수 있는 패턴은 그것뿐이고, 1턴 패턴이 있으면 남은 턴이
	//얼마든 후보가 생기므로 이 검사 하나로 모든 남은 턴을 덮는다.
	// ponytail: 기획서는 '도달 가능한 모든 상태'를 보라고 하지만 연속 재선택을 허용하는
	// 한 결과가 같다. 금지 모드가 실제로 쓰이면 그때 상태 탐색으로 확장
	const bool bHasSingleTurn = Pool.ContainsByPredicate([this](const FPatternWeight& W)
	{
		const FBossPattern* Pattern = FindPattern(W.PatternId);
		return W.Weight > 0.f && Pattern && Pattern->Steps.Num() == 1;
	});

	if (!bHasSingleTurn)
	{
		Fail(BossesPath, FString::Printf(
			TEXT("가중치가 있는 1턴 패턴이 없습니다 — 마지막 1턴을 채울 수 없어 정확히 %d턴에 끝낼 수 없습니다"), Turns));
		return false;
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
