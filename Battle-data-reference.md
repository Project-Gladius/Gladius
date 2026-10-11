<!--
    전투 씬 인게임 데이터 레퍼런스
    26.09.14 파일 생성 — 밸런스 파일 설계를 위한 현재 구현 정리
-->

# 전투 씬 인게임 데이터 레퍼런스

밸런스 수치 파일을 설계하기 위해, **현재 게임에 구현된 능력치와 계산 함수**를 정리한 문서입니다.

기준 문서는 `Balancing/보스 패턴 데이터 기획서.md`와 `Balancing/게임 수치 분석 시뮬레이션 기획서.md`입니다. **두 기획서가 기준이고 게임이 그쪽에 맞춥니다.** 이 문서는 "지금 게임은 이렇게 되어 있다"를 알려주는 용도이지 규격 제안이 아닙니다.

> **26.09.21 — 밸런스 파일 연동 완료.** `Balancing/Data`의 `abilities.json` · `enhancements.json` · `bosses.json` 세 개를 게임이 실행할 때 읽습니다. 아래 수치표는 **파일이 없을 때 쓰는 기본값**이고, 실제 값은 그 파일들이 기준입니다.

## 1. 데이터 분리 현황

보스 패턴 기획서 6절의 분리 기준에 현재 상태를 대응시키면 다음과 같습니다.

| 데이터 | 파일 | 게임 구현 |
|---|---|---|
| 보스 행동·패턴·선택 가중치·체력 | `bosses.json` | ✅ 읽어서 사용 |
| 공통 전투 규칙 | `abilities.json` 의 `rules` | ✅ 읽어서 사용 |
| 플레이어·공격 데이터 | `abilities.json` 의 `characters` | ✅ 읽어서 사용 (첫 캐릭터) |
| 강화 단계당 증가량 | `enhancements.json` | ✅ 읽어서 사용 |
| 시뮬레이션 설정 (`simulationDefaults`, `preset`) | — | 읽지 않음, 시뮬레이터 전용 |
| 실행 상태 | — | 메모리 |

**읽는 위치**는 `BattleManager`의 `Battle | Balance` 항목에서 정합니다.

| 값 | 기본값 | 역할 |
|---|---|---|
| `bUseBalanceFiles` | 켜짐 | 끄면 파일을 무시하고 에디터 값으로 돕니다 |
| `BalanceDataDir` | `../Balancing/Data` | `.uproject` 폴더 기준 상대 경로 |

파일에 문제가 있으면 **무엇이 잘못됐는지 Output Log에 찍고 전부 포기한 뒤** 에디터 값으로 진행합니다. 모르는 값을 조용히 기본값으로 대체하지 않습니다(보스 패턴 기획서 7절).

> 패키징한 게임은 저장소 폴더가 없으므로 `BalanceDataDir`을 패키지 안의 경로로 바꾸거나 폴더를 함께 배포해야 합니다. 지금은 에디터에서 실행하는 것을 전제로 합니다.

## 2. 용어 대응

기획서 기호와 게임 변수명의 대응입니다.

| 기획서 / 파일 | 게임 |
|---|---|
| H (체력) `baseHp` | `Classes[].BaseHP` + `MaxHPBonus × HPPerBonus` |
| A (공격력) `baseDamage` | `Classes[].Attacks[0·1].BaseDamage` |
| F (피버) | `FBattlePlayer::Fever` (실수) |
| `attackFeverGain` | `FeverIncrement` |
| G_D `defenseFeverGain` | `DefenseFeverGain` |
| `feverDamageCoefficient` | `FeverDamageCoefficient` |
| N `evadeThreshold` | `EvadeStreakN` |
| `evadeReduction` (0~1) | `EvadeFeverReduction` |
| `defenseDamageRatio` | `Classes[].DefenseDamageRatio` |
| R (위치·속성 배율) `resistanceByZone` | `FBossAction::ResistanceByZone[3]` |
| K (치명타 배율) `critChance`/`critBonus` | `Classes[].Attacks[].CritChance`·`CritDamage` + 신전 강화분 |
| `resistanceMultipliers` | `ResistanceMultiplier()` |
| Pa / Pd / Pe, `simulationDefaults`, `preset` | 시뮬레이터 전용, 읽지 않음 |

## 3. 수치 목록

파일이 있으면 그 값이 쓰이고, 아래 "기본값"은 파일이 없을 때만 쓰입니다.

### 3.1 전투 규칙 — `abilities.json` 의 `rules`

| 게임 변수 | 파일 필드 | 현재 파일값 |
|---|---|---:|
| `FeverIncrement` | `attackFeverGain` | 5 |
| `DefenseFeverGain` | `defenseFeverGain` | 5 |
| `FeverDamageCoefficient` | `feverDamageCoefficient` | 1 |
| `EvadeStreakN` | `evadeThreshold` | 2 |
| `EvadeFeverReduction` | `evadeReduction` | 1 |
| `EvadeRepeatMode` | `evadeRepeat` | `everyAfter` |
| `FeverRounding` | `feverRounding` | `floor` |
| `FeverCap` | `feverCap` | `null` (무제한) |
| `bAllowStayEvade` | `allowStayEvade` | `true` |
| `DefenseDamageRatio` | `defenseDamageRatio` | 0.5 |
| 내성 배율 | `resistanceMultipliers` | 0.5 / 1.0 / 1.5 |

> `turnOrder`(`move → playerAttack → bossAttack → feverAndDeath`)와 `attackDamageRounding: roundHalfUp`, `defenseDamageRounding: ceil` 은 코드가 이미 그 순서·방식이라 읽지 않고 맞춰만 두었습니다. 파일에서 바뀌면 코드도 고쳐야 합니다.

### 3.2 플레이어·공격 — `abilities.json` 의 `characters`

**`characters[]` 를 전부 읽습니다.** 배열 순서가 자리 배치의 앞뒤를 뜻합니다 — 0번이 보스에 가장 가까운 쪽입니다.

| 직업군 | id | 유닛 블루프린트 | 자리 |
|---|---|---|---|
| 글라디우스 (전사) | `gladius` | `BP_BattleUnitG` | 앞 |
| 세라 (마법사) | `sera` | `BP_BattleUnitS` | 뒤 |

현재 두 직업군의 수치가 완전히 같습니다. `abilities.json`에서 한쪽만 고치면 바로 갈라집니다.

유저의 직업군은 `PlayerData.json`의 `characterId`에 저장되어 다시 참가해도 유지됩니다.

| 게임 변수 | 파일 필드 | 현재 파일값 |
|---|---|---:|
| `Classes[].BaseHP` | `baseHp` | 7 |
| 시작 구역 | `initial.zone` | `left` |
| 시작 피버 | `initial.fever` | 0 |

| 필드 | 공격 1 (기본 베기) | 공격 2 (집중 타격) |
|---|---:|---:|
| `BaseDamage` | 10 | 7 |
| `Type` | 물리 | 물리 |
| `CritChance` | 0 | 0.25 |
| `CritDamage` | 0 | 0.5 |

> **실제 치명타 = 공격의 기본값 + 신전 강화분**이며 `CritChanceCap`(파일의 `critChanceCap`, 현재 1)으로 잘립니다. 강화와 합치는 방식은 기획서 범위 밖이라 게임 쪽 가정입니다.

### 3.3 게임 전용 수치 — 기획서에 대응 항목이 없음

방송 진행에만 필요한 값들이라 에디터에서 정합니다. (`Turns`와 `EnemyMaxHP`는 `bosses.json`의 `encounter.turns` / `maxHp`에서 옵니다.)

| 게임 변수 | 현재값 | 의미 |
|---|---:|---|
| `MaxPlayers` | 30 | 동시 참여 정원 |
| `IdleTurnsBeforeKick` | 2 | 무입력 시 전장에서 빠지는 턴 수 |
| `ReviveTurns` | 3 | 게임 오버 후 부활까지 대기 턴 |
| `GameOverLoss` | 0.5 | 부활 시 잃는 강화 능력치 비율 |
| `TelegraphSeconds` | 5 | 예고 시간 |
| `MoveSeconds` | 1.5 | 회피 이동 연출 시간 |
| `DefaultCharacterId` | `gladius` | 새 유저에게 줄 직업군 |
| `BossActionSeconds` | 1.5 | 공격 모션 시간 |
| `PlayerSelectCountdown` | 90 | 행동 선택 시간 |
| `CollectSeconds` | 6 | 늦게 도착하는 채팅 수집 시간 |
| `ResolveSeconds` | 4 | 결과 표시 시간 |

> 뒤의 네 개는 애니메이션 길이에 좌우되므로 임시값입니다.

### 3.4 강화 능력치 — 신전(상점) 영역

유저별로 저장되며 `Saved/PlayerData.json`에 기록됩니다.

단계당 증가량은 `enhancements.json` 의 `perLevel` 에서 옵니다.

| 저장 필드 | 게임 변수 | 파일 필드 | 현재 파일값 |
|---|---|---|---:|
| `MaxHPBonus` | `HPPerBonus` | `hp` | 1 |
| `DamageBonus` | `DamagePerBonus` | `damage` | 1 |
| `CritChanceBonus` | `CritChancePerBonus` | `critChance` | 0.05 |
| `CritDamageBonus` | `CritDamagePerBonus` | `critBonus` | 0.1 |
| `TotalDamageDealt` | — | — | 방송 전체 누적 피해량 |

> 파일의 `initialLevels` 는 아직 쓰지 않습니다. 새 유저는 전부 0단계로 시작합니다.

## 4. 계산 함수

양쪽 실행기가 동일해야 하는 계산입니다. 전부 상태를 참조하지 않는 순수 함수이고, 에디터의 `Run Self Test` 버튼으로 검증됩니다.

### 보스에게 주는 피해

```
DamageToBoss(기본피해, 강화피해, 피버, 피버계수, 내성배율, 치명타여부, 치명타피해)
  = max(0, round( (기본피해 + 강화피해 + 피버 × 피버계수) × 내성배율 × (1 + 치명타 ? 치명타피해 : 0) ))

시뮬레이터는 강화 피해를 기본 피해에 미리 더해 넣으므로 결과가 같습니다.
```

### 플레이어가 받는 피해

```
DamageToPlayer(행동, 구역피해, 타격횟수, 방어비율)
  구역피해 0  → 0 (그 구역은 맞지 않음)
  방어가 아님 → 구역피해 × 타격횟수
  방어        → ceil(구역피해 × 방어비율) × 타격횟수

올림은 타격마다 따로 적용됩니다. 시뮬레이터와 같은 순서입니다.
```

### 그 외

| 함수 | 내용 |
|---|---|
| `DecayFever(피버, 연속회피횟수, N, 감소율, 반복방식, 소수처리)` | 기준에 닿으면 `F × (1 - 감소율)`, 아니면 그대로 |
| `ResistanceMultiplier(내성)` | 파일의 `resistanceMultipliers` 값 (0.5 / 1.0 / 1.5) |
| `ZoneResistMultiplier(보스행동, 이동 후 구역, 속성)` | 그 구역·속성의 내성 배율 |
| `ApplyMove(행동, 현재구역)` | 회피 행동이면 해당 구역으로, 아니면 제자리 |
| `MaxHPFor(기록)` | `PlayerBaseHP + MaxHPBonus × HPPerBonus` |

## 5. 턴 진행 순서

```
Telegraph → Input → Collect → Resolve → (다음 턴)
```

`Resolve` 단계의 처리 순서입니다.

1. 회피 행동을 반영해 구역 이동 (이동 연출 `MoveSeconds`)
2. 플레이어 공격 피해 계산 및 누적
3. 보스 공격 피격 판정 및 체력 차감
4. 연속 회피 횟수 갱신 → 피버 갱신 → 상한 적용
5. 체력 0이면 게임 오버 처리
6. 무입력 이탈 처리
7. 종료 조건 검사 (보스 처치 또는 `Turns` 도달)

보스 행동은 턴이 시작될 때 **진행 중인 패턴의 다음 step**에서 나옵니다. 패턴이 끝난 턴에만 새 패턴을 뽑습니다.

> 보스 패턴 기획서 5절의 순서와 같습니다. 공격한 턴에 사망해도 그 피해는 기록됩니다.
>
> **피버는 피해 계산 뒤에 갱신됩니다.** 그래서 피버 0에서 10회 연속 공격하면 총피해가 `10A + 45`가 됩니다(기획서 10절 검증 기준).

## 6. 기획서 대비 정합 이력

어긋나 있던 지점과 각각을 어떻게 맞췄는지입니다.

### 6.1 구조를 바꿔야 했던 것

**① 회피 피버 감소 모델** — ✅ **해결됨 (26.09.16)**

`DecayFever()`로 기획서 모델(`F × (1 - M/100)`, 연속 회피 `e`, 반복 방식, 소수 처리)을 구현했습니다. 피버가 실수가 되어 `소수 유지` 모드를 쓸 수 있습니다. 기획서 10절의 검증 기준이 `RunSelfTest`에 들어가 있습니다.

**② 내성이 구역별이어야 합니다** — ✅ **해결됨 (26.09.21)**

`FBossAction`이 규격의 행동 정의와 1:1로 대응하도록 바뀌었습니다.

| 규격 | 게임 |
|---|---|
| `id`, `name` | `Id`, `Name` |
| `damage` | `Damage` |
| `target.zones` | `Zones` (`TArray<EBattleLane>`, 빈 배열 = 공격 없음) |
| `resistanceByZone` | `ResistanceByZone[3]` (좌·중·우 순) |
| `telegraph.text` / `.motionId` | `TelegraphText` / `TelegraphMotionId` |
| `attackMotionId` | `AttackMotionId` |

내성은 `ZoneResistMultiplier()`가 **이동 후 플레이어 구역**으로 적용합니다. `target.mode`는 첫 버전이 `fixed`뿐이라 필드를 두지 않았고, 구역 조합 9종을 나열하던 `EHitArea`는 삭제했습니다.

**③ 치명타가 스킬별입니다** — ✅ **해결됨 (26.09.17)**

`FAttackDef`에 공격별 치명타를 두었습니다. 신전 강화분은 여기에 **더합니다**(게임 쪽 가정, 3.2 참고).

기획서 피해식 `D = (A + F) × R × K`에는 강화 피해 항이 없습니다. 게임의 `+ DamageBonus`는 신전 확장분입니다.

**④ 보스 패턴 체계** — ✅ **해결됨 (26.09.21)**

`actions` / `patterns` / `selection` / `encounter` 를 읽고, 가중 랜덤으로 뽑은 패턴을 끝까지 실행합니다. 실행 중에는 다시 뽑지 않습니다.

**남은 턴에 들어가는 패턴만 후보가 되어** 정확히 마지막 턴에 패턴도 함께 끝납니다. 중간 절단·초과 실행·다음 구간 승계는 없습니다.

로딩 검증: ID 중복, `steps` 참조 유효성, 패턴 길이 1~4, 구역·내성·속성 이름, 피해·가중치 범위, `pool`의 패턴 존재 여부와 중복, 연속 재선택 금지 시 후보 수, **남은 턴별로 들어갈 패턴이 있는지**.

> 기획서는 "도달 가능한 모든 상태"를 검사하라고 하지만, 지금은 남은 턴별 후보 유무만 봅니다. 연속 재선택을 허용하는 한 결과가 같고, 금지 설정이 실제로 쓰이면 그때 확장하면 됩니다.

### 6.2 값 차이

**⑤ 피버 감소의 소수 처리** — ✅ **해결됨.** `feverRounding: "floor"`.

> `feverRounding`은 **피버 감소 결과**의 소수 처리이며, 보스에게 주는 피해의 반올림(`attackDamageRounding`)과는 별개입니다.

**⑥ 방어 시 피버 증가** — ✅ **해결됨.** `defenseFeverGain: 5`.

**⑪ 공격 기본 피해** — ✅ **해결됨.** 파일에서 읽습니다 (현재 `기본 베기` 10 / `집중 타격` 7, 둘 다 물리).

### 6.3 결정이 필요했던 것

**⑦ 피버 상한** — ✅ **해결됨.** `abilities.json`의 `feverCap: null`(상한 없음). 값이 들어오면 그대로 적용됩니다.

**⑧ 보스 체력의 존재 여부** — ✅ **해결됨.** `bosses.json`에 `maxHp: 100`이 있습니다. 게임은 이 값을 쓰고, 0이 되면 승리로 끝냅니다.

> 다만 **시청자가 30명이면 1턴에 보스가 죽습니다.** 공격 기본 피해가 10이라 10명만 공격해도 100입니다. 시뮬레이터는 1인 기준이라 인원수가 반영되지 않은 값으로 보입니다. 기획팀 확인이 필요합니다.

**⑨ 회피 감소 기본값** — ✅ **해결됨.** `evadeThreshold: 2`, `evadeReduction: 1`(비율 0~1이므로 기준에 닿으면 피버가 0), `evadeRepeat: "everyAfter"`.

**⑩ 보스에게 주는 피해의 소수 처리** — ✅ **해결됨.** `abilities.json`의 `attackDamageRounding: "roundHalfUp"`. 게임의 반올림과 같습니다.

## 7. 외부 진입점

채팅 연동이 붙는 지점입니다. 밸런스와 무관하지만 참고용으로 적습니다.

```cpp
bool SubmitChat(UserId, DisplayName, Text, SentAt);
```

명령어 파싱은 전투 쪽이 담당합니다. 현재 어휘는 임시로 지정한 것이며 확정되지 않았습니다.

| 행동 | 현재 인식하는 단어 |
|---|---|
| `Attack1` | 공격 / attack / a |
| `Attack2` | 강공격 / attack2 / aa |
| `AvoidL` | 왼쪽 / left / l |
| `AvoidM` | 가운데 / mid / m |
| `AvoidR` | 오른쪽 / right / r |
| `Defense` | 방어 / defend / d |

앞의 `!`는 있어도 없어도 되며 대소문자를 구분하지 않습니다.

## 8. 남은 확인 사항

**2026-10-08 `Balancing/` 문서를 전수 확인한 결과입니다.** 몇 가지는 이미 답이 문서에 있었고, 아래는 그걸 반영해 정리한 것입니다.

### 8.1 문서에 답이 있어 해소된 것

| 항목 | 결론 | 근거 |
|---|---|---|
| 보스 체력의 용도 | **게임용 수치가 맞습니다.** 시뮬레이터가 안 쓰는 건 분석 목적이 '누적 피해 측정'이기 때문입니다 | `Balancing/Simulator/README.md` — "`maxHp`는 실제 게임용 데이터이며 분석의 종료 조건이 아닙니다" |
| 치명타가 플레이어 속성인지 공격 속성인지 | **공격별 치명타 + 강화분 합산.** 현재 구현이 이대로입니다 | `Balancing/Simulator/README.md` — "각 공격의 치명타에 강화분이 더해지므로 공격 1도 강화 후 치명타가 발생할 수 있습니다" |

M1 기획서(`전투 기본 시스템.md` 7.2)의 "플레이어 치명타 확률 25% / 치명타 피해량 40%"는 **구버전**입니다. `Balancing/Data`가 기준입니다.

### 8.2 기획팀 결정이 필요한 것

1. **보스를 격파하면 10턴을 안 채우고 끝낼지** — 분석 기획서 11절에 "공격·피격 처리 순서와 **보스 처치 시 조기 종료 여부**"가 미정으로 남아 있고, 본문에도 "확정 전까지 결과에 표시한다"고 적혀 있습니다. **현재 코드는 격파 시 즉시 종료합니다.** 아니라면 `EndTurn`의 승리 분기를 빼면 됩니다.

2. **보스 체력 100 대 시청자 30명** — 6절 ⑧ 참고. 값 자체는 게임용이 맞지만 인원수가 반영된 수치인지는 별개입니다.

3. **2턴 연속 무입력 시 전장에서 제외** — `Balancing/`과 M1·M2 기획서 **어디에도 없습니다.** 기획서는 "선택 없음 = `Idle`"을 정식 행동으로 두고, 이탈은 명시적 로그아웃이나 시청 중단으로만 일어납니다. 구현 요청을 받은 사항이라면 기획서에 반영이 필요합니다.

4. **세라의 수치** — 방향은 정해져 있습니다. 분석 기획서 4.2가 "세라 / 마법 — 특정 상황에서만 속성 영향, 초보자의 피해 저점 확보", "글라디우스 / 물리 — 기본 피해는 낮고 위치·패턴 활용 시 고점"으로 적고 있습니다. 다만 "공격 번호와 물리·마법을 일대일로 묶는 규칙은 명시되지 않았으므로 각각 독립 입력으로 둔다"며 보류했고, `abilities.json`은 **두 직업군이 모두 `physical`에 수치도 동일**합니다. 그래서 **보스의 마법 내성 분기가 한 번도 실행되지 않습니다.**

### 8.3 구현이 임의로 정한 것 — 원문이 미완성인 부분

`전투 기본 시스템.md`가 `[!question]`으로 표시해 둔, 원문만으로는 확정할 수 없는 항목들입니다. 나중에 기획이 확정되면 대조해야 합니다.

| 항목 | 현재 구현 |
|---|---|
| 부활 시 체력·피버 | 처음 참가한 것과 같은 상태로 되돌림 |
| 게임 오버 3턴의 산정 시작점 | 사망한 턴의 판정 직후부터 셈 |
| 강화 능력치 50% 손실의 정수 처리 | 내림 (`FloorToInt`) |
| 세라 투사체 딜레이 이후 무엇을 처리하는지 | **미구현.** 대미지 숫자 표시와 같이 넣을 예정 |
| 부활 전 시청 여부 검사 / 슈퍼챗 즉시 부활 | **미구현.** YouTube API 필요 |

### 8.4 데이터는 있으나 아직 안 쓰는 것

1. **`hitCount`** — 현재 전부 1입니다. 게임은 "구역 피해를 그 횟수만큼 반복해서 준다"로 해석했습니다(시뮬레이터와 동일). 방어 올림도 타격마다 따로 적용됩니다.
2. **`attributeByZone`** — 보스 공격의 속성입니다. 플레이어 쪽에 속성 저항이 없어 지금은 읽어만 두고 쓰지 않습니다.
3. **`initialLevels`** — 새 유저의 시작 강화 단계입니다. 아직 반영하지 않았습니다.
4. **직업군 선택 수단** — 둘 다 읽어서 쓰고 있습니다(전사 앞 / 마법사 뒤). 유저가 고를 방법이 없어 디버그 키 `Q`·`W`로만 넣습니다. 채팅 명령어는 연동 때 붙입니다.
5. **`preset.seed`** — 시뮬레이터는 재현성을 위해 고정 시드를 쓰지만, 게임에 그대로 넣으면 매 방송의 보스 순서가 같아집니다. 읽지 않습니다.

## 9. 요약

**6절의 불일치는 전부 해소되었습니다.** 회피 피버 모델, 구역별 내성, 공격별 치명타, 보스 패턴 체계까지 기획서와 밸런스 파일에 맞췄습니다.

**게임은 이제 `Balancing/Data`의 세 파일을 읽습니다.** 수치를 바꾸려면 게임을 고칠 필요 없이 그 파일만 고치고 다시 실행하면 됩니다.

**남은 것은 8.2절의 기획 결정 4건**과, 채팅 연동 시 정해질 명령어 어휘입니다. 그 중 **보스 격파 시 조기 종료**와 **세라의 수치**가 코드 동작을 바꾸는 항목입니다.
