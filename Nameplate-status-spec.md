# NamePlate 상태별 수치 표시 기획 및 Designer 가이드

## 적용 위치

- Widget Blueprint: `/Game/Blueprints/UI/MyBattlePlayerNameplate`
- 유닛: `ABattleUnit`의 `Nameplate` 컴포넌트
- 기준점: 아랫변 중앙 `(0.5, 1.0)`

기존 `ActionValue`는 공격/방어 설명과 숫자를 한 텍스트로 표시했으나, 공격력은 예전 피버 계수 식을 사용했다. 상태별 숫자를 명확하게 선택할 수 있도록 아래 전용 TextBlock으로 구분한다.

## 상태별 표시 규칙

| 상태 | Designer 계층의 TextBlock | 표시 |
|---|---|---|
| Attack1 | `State_Attack1` → `Attack1_AttackDamageValue` | 치명타 제외 공격력을 `A + B`의 두 숫자로 표시 |
| Attack2 | `State_Attack2` → `Attack2_AttackDamageValue` | 치명타 제외 공격력을 `A + B`의 두 숫자로 표시 |
| Defense | `State_Defense` → `Defense_DefenseReductionValue` | 받는 피해 감소율. 기본 `50%` |

Attack1과 Attack2는 각각 선택한 스킬의 기본 공격력을 사용한다.

`표시 형식 = A + B`

- `A = 선택 스킬 기본 공격력 + 신전 강화로 증가한 공격력`
- `B = 평타 공격력 × 피버 대수`

A와 B를 합쳐 하나의 숫자로 표시하지 않는다. 기존 TextBlock에 예를 들어 `16 + 15`로 표시한다.

- 평타 공격력은 전투 관리자의 `FeverAttackDamage`를 사용한다. 현재 기본값은 5다.
- 피버 대수는 실제 공격과 동일한 상한을 적용한다.
- 신전 강화분은 스킬에 한 번만 더한다. 피버 평타마다 강화분을 다시 더하지 않는다.
- 치명타 확률·피해 배율과 보스의 내성 배율은 이 숫자에 적용하지 않는다. 실제 타격 피해와 구분되는 선택 단계의 기준 공격력이다.
- 예: 강화 +6, 피버 3대일 때 Attack1 기본 공격력 10은 `16 + 15`, Attack2 기본 공격력 7은 `13 + 15`다.
- 피버가 0이어도 두 번째 숫자를 유지한다. 예: `16 + 0`.

방어 감소율은 `1 - 캐릭터의 DefenseDamageRatio`다. `DefenseDamageRatio=0.5`이면 `50%`를 표시한다. 실제 방어 밸런스는 `Balancing/Data/abilities.json`의 캐릭터별 `defenseDamageRatio`가 기준이다.

## Designer에서 조작하는 방법

1. `MyBattlePlayerNameplate`를 열고 Designer에서 `ActionPages`의 편집할 상태를 선택한다.
2. 위 표의 TextBlock을 선택한다. 기존 상태별 `ActionValue`가 있었다면 같은 위젯의 이름을 변경했으므로 기존 위치·크기·폰트·색이 유지된다.
3. Canvas Slot의 Position/Size, Font, Color and Opacity, Justification을 자유롭게 변경한다. 위젯 이름은 유지한다.
4. 표시되는 수치는 실행 중 전투 데이터로 갱신된다. 미리보기 숫자는 Class Defaults → Battle UI → Preview의 `PreviewData`에서 설정한다.
5. 공격 미리보기는 `BaseDamage`, `BonusDamage`, `Fever`, `FeverAttackDamage`를 사용한다. 방어 미리보기의 `DefenseReduction=0.5`는 `50%`, `0.25`는 `25%`다. 이는 미리보기 값이며 실제 전투 밸런스를 변경하지 않는다.
6. Compile → Save 후 플레이에서 확인한다.

선택한 상태 페이지와 배치·폰트·색은 수치 갱신으로 초기화하지 않는다. Designer에서 직접 선택한 아이콘 이미지도 보존한다.

## 피벗 동작

- 컴포넌트 배치 피벗과 위젯 확대·축소 피벗 모두 아랫변 중앙을 사용한다.
- 크기·거리 배율이 달라져도 하단 중앙이 유닛의 Nameplate 기준 위치에 고정된다.
- 에디터 구성, BeginPlay, UI 갱신, 거리 배율 갱신 시 적용하여 저장된 Blueprint 피벗 값이나 같은 위젯 클래스 재사용에 영향을 받지 않게 한다.

## 기존 애셋 보완

`UBattlePlayerNameplate::EnsureStatusValueWidgets`는 세 숫자 필드만 보완한다. 기존 TextBlock은 엔진의 Blueprint 이름 변경 기능으로 전환해 그래프 참조와 배치를 보존하고, 필드가 없을 때만 생성한다. 전체 위젯 트리를 다시 만들지 않는다.
