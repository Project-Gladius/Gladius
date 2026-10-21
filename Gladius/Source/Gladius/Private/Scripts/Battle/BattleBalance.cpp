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

 bool GetFeverInt(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
  const FString& File, int32& Out, double Min, double Max)
 {
  double Value = 0;
  if (!Obj.IsValid() || !Obj->TryGetNumberField(Field, Value)
   || !FMath::IsFinite(Value) || Value < Min || Value > Max || Value != FMath::FloorToDouble(Value))
  {
   Fail(File, FString::Printf(TEXT("'%s' must be an integer in [%g, %g]"), *Field, Min, Max));
   return false;
  }
  Out = static_cast<int32>(Value);
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

	if (!GetFeverInt(Rules, TEXT("attackFeverGain"), AbilitiesPath, AttackFeverGain, 0, 100)
		|| !GetFeverInt(Rules, TEXT("defenseFeverGain"), AbilitiesPath, DefenseFeverGain, 0, 100)
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
		&& !GetFeverInt(Rules, TEXT("feverCap"), AbilitiesPath, FeverCap, 0, 10000))
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
	if (!GetFeverInt(*InitialPtr, TEXT("fever"), AbilitiesPath, InitialFever, 0, 10000)
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

		// ---- target (선택) ----
		//v1 규격에는 없지만 readme가 'Fury의 데이터 보존 확장'으로 남겨둔 항목이다.
		//randomOne이면 damageByZone에 적힌 구역들이 '후보'이고, 그중 한 곳만 맞는다 —
		//어느 구역인지는 StartTurn이 예고할 때 고른다. 없으면 적힌 구역을 그대로 다 때린다
		Action.TargetMode = EBossTargetMode::Fixed;

		const TSharedPtr<FJsonObject>* TargetPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("target"), TargetPtr))
		{
			FString ModeText;
			if (!GetString(*TargetPtr, TEXT("mode"), BossesPath, ModeText)) { return false; }

			if (ModeText == TEXT("fixed"))          { Action.TargetMode = EBossTargetMode::Fixed; }
			else if (ModeText == TEXT("randomOne")) { Action.TargetMode = EBossTargetMode::RandomOne; }
			else if (ModeText == TEXT("none"))      { Action.TargetMode = EBossTargetMode::None; }
			else
			{
				Fail(BossesPath, FString::Printf(
					TEXT("'%s' 의 target.mode 가 '%s' 입니다 — fixed / randomOne / none 중 하나여야 합니다"),
					*Action.Id, *ModeText));
				return false;
			}
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
		}

		//telegraph.motionId / attackMotionId 는 읽지 않는다. 파일에서 둘 다 비어 있고,
		//블루프린트는 행동 id(SingleStrikeLeft 같은 값)로 모션을 고른다

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

	// ---- preset ----
	//턴을 어떻게 채울지. 이걸 안 읽으면 파일이 '고정 순서'라고 적어둬도
	//게임은 혼자 추첨을 돌려, 시뮬레이터와 다른 전투가 된다
	const TSharedPtr<FJsonObject>* PresetPtr = nullptr;
	if (!Bosses->TryGetObjectField(TEXT("preset"), PresetPtr))
	{
		Fail(BossesPath, TEXT("'preset' 항목이 없습니다"));
		return false;
	}

	FString ModeText;
	if (!(*PresetPtr)->TryGetStringField(TEXT("mode"), ModeText))
	{
		Fail(BossesPath, TEXT("'preset.mode' 값이 없습니다"));
		return false;
	}

	if (ModeText == TEXT("direct"))      { PresetMode = EBossPresetMode::Direct; }
	else if (ModeText == TEXT("custom")) { PresetMode = EBossPresetMode::Custom; }
	else if (ModeText == TEXT("random")) { PresetMode = EBossPresetMode::Random; }
	else
	{
		Fail(BossesPath, FString::Printf(
			TEXT("'preset.mode' 가 '%s' 입니다 — direct / custom / random 중 하나여야 합니다"), *ModeText));
		return false;
	}

	if (PresetMode == EBossPresetMode::Direct)
	{
		const TArray<TSharedPtr<FJsonValue>>* DirectArray = nullptr;
		if (!(*PresetPtr)->TryGetArrayField(TEXT("directActions"), DirectArray) || DirectArray->Num() == 0)
		{
			Fail(BossesPath, TEXT("preset.mode가 direct인데 'directActions' 가 비어 있습니다"));
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *DirectArray)
		{
			const FString ActionId = Value->AsString();
			if (!FindAction(ActionId))
			{
				Fail(BossesPath, FString::Printf(TEXT("preset.directActions의 없는 공격 ID: %s"), *ActionId));
				return false;
			}
			PresetDirectActions.Add(ActionId);
		}

		//자르지도 늘이지도 않으므로 개수가 곧 턴 수다
		if (PresetDirectActions.Num() != Turns)
		{
			Fail(BossesPath, FString::Printf(
				TEXT("preset.directActions가 %d개인데 encounter.turns는 %d입니다 — 정확히 맞아야 합니다"),
				PresetDirectActions.Num(), Turns));
			return false;
		}
	}
	else if (PresetMode == EBossPresetMode::Custom)
	{
		const TArray<TSharedPtr<FJsonValue>>* CustomArray = nullptr;
		if (!(*PresetPtr)->TryGetArrayField(TEXT("customPatterns"), CustomArray) || CustomArray->Num() == 0)
		{
			Fail(BossesPath, TEXT("preset.mode가 custom인데 'customPatterns' 가 비어 있습니다"));
			return false;
		}

		int32 TotalSteps = 0;
		for (const TSharedPtr<FJsonValue>& Value : *CustomArray)
		{
			const FString PatternId = Value->AsString();
			const FBossPattern* Pattern = FindPattern(PatternId);
			if (!Pattern)
			{
				Fail(BossesPath, FString::Printf(TEXT("preset.customPatterns의 없는 패턴 ID: %s"), *PatternId));
				return false;
			}

			TotalSteps += Pattern->Steps.Num();
			PresetCustomPatterns.Add(PatternId);
		}

		//패턴을 중간에 자르지 않으므로 합이 정확히 맞아야 한다
		if (TotalSteps != Turns)
		{
			Fail(BossesPath, FString::Printf(
				TEXT("preset.customPatterns의 총 턴이 %d인데 encounter.turns는 %d입니다 — 패턴은 중간에 잘리지 않습니다"),
				TotalSteps, Turns));
			return false;
		}
	}

	//아래 두 검사는 추첨할 때만 의미가 있다.
	//고정 순서로 돌리는 판에서 1턴 패턴이 없다고 막으면 엉뚱하게 실패한다
	if (PresetMode != EBossPresetMode::Random)
	{
		return true;
	}

	// ---- 정확히 Turns턴을 채울 수 있는가 ----
	//패턴 길이 조합으로 딱 맞출 수 있는지를 동적 계획법으로 따진다.
	//1턴 패턴 유무만 보는 근사보다 정확하다 — 길이가 2·3뿐인 구성도 걸러낸다.
	//ChoosePattern이 매 추첨마다 CanFillExactly로 이 표를 참조한다
	FillableTurns.Init(false, Turns + 1);
	FillableTurns[0] = true;
	for (int32 Left = 1; Left <= Turns; ++Left)
	{
		for (const FPatternWeight& Entry : Pool)
		{
			const FBossPattern* Pattern = FindPattern(Entry.PatternId);
			if (Entry.Weight <= 0.f || !Pattern)
			{
				continue;
			}

			const int32 Length = Pattern->Steps.Num();
			if (Length <= Left && FillableTurns[Left - Length])
			{
				FillableTurns[Left] = true;
				break;
			}
		}
	}

	if (!FillableTurns[Turns])
	{
		Fail(BossesPath, FString::Printf(
			TEXT("가진 패턴 길이로는 정확히 %d턴을 채울 수 없습니다 — 길이 조합을 확인하세요"), Turns));
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

bool FBattleBalance::CanFillExactly(int32 TurnsLeft) const
{
	return FillableTurns.IsValidIndex(TurnsLeft) && FillableTurns[TurnsLeft];
}
