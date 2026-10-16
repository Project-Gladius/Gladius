# CutSceneApplier 사용법

`ApplyCutScene(캐릭터 선택 전략, 컷신 종류)`를 호출하면 BattleManager에서 **한 명**을 고르고 지정한 Level Sequence를 한 번 재생합니다. `DefaultGladius`는 공격 1 또는 공격 2를 선택한 생존 글라디우스 중 최고 피버 한 명, `DefaultSera`는 같은 조건의 세라 중 최고 피버 한 명을 고르는 독립된 전략입니다. 다른 직업으로 자동 연결하거나 순차 재생하지 않습니다. HP가 0 이하이거나 부활 대기 중이거나 Actor가 사라진 참가자는 제외합니다. 전투 단계·공격 판정·캐릭터 모션을 자동으로 진행하지 않습니다.

## Dev_Battle에서 준비

1. C++ 빌드 후 에디터를 다시 엽니다.
2. `CutSceneApplier` Actor 또는 이를 상속한 `BP_CutSceneApplier`를 Dev_Battle에 하나 배치합니다.
3. Details의 `Battle Manager`에 이 전투의 관리자를 지정합니다. 비워 두면 현재 월드에 관리자가 정확히 하나 있을 때만 자동으로 찾습니다.
4. `Cut Scenes` 맵에 항목을 추가합니다. 예: `GladiusAttackCloseUp`에는 `LS_GladiusAttackCloseUp`, `SeraAttackCloseUp`에는 `LS_SeraAttackCloseUp`을 등록하고 World Time Scale은 각각 `0.1`로 설정합니다. 이름은 고정 enum이 아니라 FName이므로 원하는 연출을 계속 등록할 수 있습니다.
5. Level Blueprint나 필요한 전투 이벤트에서 배치한 Applier 참조로 `Apply Cut Scene`을 호출합니다. Strategy와 Cut Scene Type 두 값을 지정합니다. 반환값 false는 재생이 시작되지 않았다는 뜻입니다. 미등록 이름, 유효한 대상/보스 없음, 잘못된 설정, 다른 Applier가 재생 중인 경우 등이 원인입니다. 같은 Applier에 유효한 새 요청을 보내면 현재 컷신을 교체합니다.

```cpp
const bool bStarted = CutSceneApplier->ApplyCutScene(
    ECutSceneCharacterStrategy::DefaultGladius,
    FName(TEXT("GladiusAttackCloseUp"))
);
```

세라 연출이 필요한 시점에는 별도로 호출합니다. 각 이름은 Cut Scenes에 다른 시퀀스로 등록할 수 있습니다.

```cpp
CutSceneApplier->ApplyCutScene(
    ECutSceneCharacterStrategy::DefaultSera,
    FName(TEXT("SeraAttackCloseUp"))
);
```

같은 Applier에서 두 호출을 연달아 실행하면 두 번째 컷신이 첫 번째를 즉시 교체합니다. 각 연출이 필요한 별도의 전투 이벤트에서 호출하세요.

## 전략

| 전략 | 선택 기준 |
| --- | --- |
| DefaultGladius (핀 기본값) | 공격을 선택한 생존 글라디우스 중 최고 피버 한 명 |
| DefaultSera | 공격을 선택한 생존 세라 중 최고 피버 한 명 |
| RandomAlive | 생존 참가자 중 무작위 |
| HighestFever | Fever가 가장 큰 참가자 |
| LowestHealthRatio | 현재 HP / 최대 HP가 가장 작은 참가자 |
| ClosestToBoss | 실제 보스 Actor 위치와의 거리가 가장 짧은 참가자 |

무작위 외 전략의 동점은 UserId 사전순으로 결정합니다. 직업은 `BattleManager.Classes`에서 ID `gladius` / `sera`를 찾아 `FBattlePlayer.ClassIndex`와 비교합니다. 배열에서 직업 순서를 바꿔도 대상이 뒤섞이지 않습니다. 요청한 직업이 등록되지 않았거나 해당 직업의 생존 공격자가 없으면 false를 반환합니다. 다른 직업으로 대체하지 않습니다.

최저 체력 비율 전략은 최대 HP가 0 이하인 비정상 데이터도 제외합니다. 새로운 기준이 필요하면 전략 enum과 선정 함수를 확장하면 됩니다. `FindCharacter(전략)`으로 대상을 미리 확인할 수 있습니다. 블루프린트의 Strategy 핀 기본값은 DefaultGladius입니다. 이전 Default 통합 전략은 DefaultGladius / DefaultSera로 분리됐으며, 기존 노드를 사용했다면 Strategy 핀을 다시 선택하세요.

## GladiusRun

기본 Cut Scenes에 `GladiusRun`이 등록돼 있습니다. 시퀀스 에셋 없이 호출하면 선택한 캐릭터를 따라가는 동적 카메라가 만들어집니다.

```cpp
CutSceneApplier->ApplyCutScene(
    ECutSceneCharacterStrategy::DefaultGladius,
    FName(TEXT("GladiusRun"))
);
```

`BP_GladiusRunCamera`는 `/Game/Blueprints/CutScenes`에 있습니다. 이 블루프린트의 Class Defaults에서 아래 값을 편집한 뒤, 배치한 Applier의 `Cut Scenes > GladiusRun > Camera Class`에 이 블루프린트 클래스를 지정하세요. `Sequence`는 비워 둡니다. 기존 Applier에 GladiusRun 항목이 없다면 같은 이름으로 추가하면 됩니다. 네이티브 기본 클래스도 같은 기본값으로 즉시 재생할 수 있습니다.

| 속성 | 기본값 | 의미 |
| --- | --- | --- |
| Camera Distance | 500cm | 화면 중심 지점과 카메라 사이의 3D 거리 |
| Camera Pitch | -20도 | 재생 중 일정하게 유지할 상하 각도 |
| Focus Height | 100cm | 캐릭터 발밑에서 화면 중심 지점까지의 높이 |
| Duration Seconds | 0 (시간 미지정) | 0이면 다음 컷신 요청/취소까지 유지, 양수이면 실제 시간 기준 자동 종료 |
| Weights > Gladius | 0.5 | 다른 글라디우스 한 명당 가중치 |
| Weights > Sera | 0.5 | 다른 세라 한 명당 가중치 |
| Weights > Boss | 0 | 보스 가중치 |
| Max Angular Speed | 60도/초 | 좌우 최대 각속도 |
| Max Angular Acceleration | 120도/초² | 각속도가 갑자기 변하지 않게 하는 제한 |
| Yaw Response | 4 | 무게중심 방향을 따라가려는 반응 강도 |

무게중심은 `위치 × 직업 가중치`의 합을 총 가중치로 나눠 구합니다. 선택한 캐릭터 본인은 제외하며, BattleManager에 남아 있는 유효한 다른 캐릭터 Actor와 보스를 대상으로 합니다. 무게중심 계산에는 공격 선택 여부나 생존 여부로 추가 필터를 걸지 않습니다. 직업 ID로 가중치를 구분하므로 직업 배열 순서를 바꿔도 유지됩니다.

카메라는 주인공에서 무게중심으로 향하는 수평 방향의 반대편에 위치하고 주인공의 Focus Height 지점을 바라봅니다. 따라서 주인공은 화면 중앙에 있고, 카메라가 바라보는 좌우 방향은 무게중심 쪽입니다. 주인공이 이동해도 거리는 고정입니다. 모든 가중치가 0이거나 무게중심이 주인공과 수평으로 겹치면 마지막 방향에서 부드럽게 회전을 멈춥니다. 시작 시 유효한 방향이 없으면 보스 방향을 사용합니다.

좌우 회전은 최단 각도를 사용하며 각속도·각가속도 제한과 감속을 적용합니다. 가중치나 상대 위치가 갑자기 바뀌어도 회전 속도를 즉시 올리거나 반전하지 않습니다. 큰 프레임 지연은 최대 0.25초만 따라잡아 카메라가 한 번에 크게 돌지 않도록 했습니다. 카메라와 연출 시간은 월드 슬로모션을 보정하고, 종료·취소 시 게임 카메라와 시간 배율을 복원합니다.

다른 동적 연출도 `CutSceneCamera`를 상속해 `InitializeCamera` / `UpdateCamera`를 구현한 뒤 Camera Class에 등록할 수 있습니다. 각 컷신 항목은 Sequence와 Camera Class 중 하나만 지정해야 합니다.

## GladiusSkill1

기본 Cut Scenes에 `GladiusSkill1`이 등록돼 있습니다. `ApplyCutScene(DefaultGladius, "GladiusSkill1")`로 선택한 글라디우스 한 명을 촬영합니다. 기본 종료 시간은 0이므로 다음 컷신 지정이나 취소까지 유지합니다.

`/Game/Blueprints/CutScenes/BP_GladiusSkill1Camera`의 Class Defaults에서 아래 값을 편집하고, Applier의 `Cut Scenes > GladiusSkill1 > Camera Class`에 해당 클래스를 지정합니다. Sequence는 비워 둡니다.

| 속성 | 기본값 | 의미 |
| --- | --- | --- |
| Distance | 500cm | 캐릭터 원점에서 카메라까지의 수평 거리 |
| Height | 170cm | 초기 카메라와 조준점의 발밑 기준 높이 |
| Right Angle | 70도 | 캐릭터→보스 정면에서 오른쪽으로 잰 카메라 위치 각도 |
| Duck Ratio | 1.5 | 높이 감소에 따른 카메라 하강 배율 |
| Tilt Ratio | 1.1 | 높이 감소에 따른 조준점 하강 배율 |
| Pelvis Bone Name | pelvis | 우선 추적할 골반 관절 이름 |
| Height Source Component Name | None | 높이를 측정할 메시 컴포넌트. None이면 보이는 메시 전체 |

높이는 다음 순서로 감지합니다.

1. 보이는 스킨 메시에 지정한 pelvis 관절이 하나 있으면 그 월드 Z에서 캐릭터 원점 Z를 뺀 상대 높이를 추적합니다. `jnt_pelvis01`처럼 이름이 다르면 Pelvis Bone Name을 실제 이름으로 지정합니다.
2. 관절이 없으면 보이는 정적/스킨 메시들의 경계 상자 중 가장 높은 Z에서 캐릭터 원점 Z를 뺍니다. 이름표·콜라이더·이펙트는 높이 계산에 포함하지 않습니다. 붙어 있는 자식 Actor의 메시도 포함합니다.
3. 관절 소스가 여러 개이거나 측정할 메시가 없으면 임의의 값으로 재생하지 않고 요청을 거절하며 로그에 원인을 기록합니다. Height Source Component Name으로 특정 메시를 선택할 수 있습니다.

시작할 때 측정한 상대 높이를 Initial Character Height에 저장하고, 매 프레임 `delta = max(0, 초기 높이 - 현재 높이)`로 계산합니다. 전체 캐릭터가 이동하거나 바닥 높이가 변해도 같은 상대 자세이면 delta는 바뀌지 않습니다.

```text
카메라 Z = 캐릭터 원점 Z + Height - delta × DuckRatio
조준점 Z = 캐릭터 원점 Z + Height - delta × TiltRatio
조준점 XY = 캐릭터 원점 XY
```

초기에는 카메라와 조준점이 모두 170cm여서 수평으로 촬영합니다. delta가 20cm이면 카메라는 140cm, 조준점은 148cm입니다. 이때 조준점의 월드 높이는 낮아지지만 카메라가 더 많이 내려가므로 시선은 약간 위쪽으로 기웁니다. 이는 지정한 두 비율을 그대로 적용한 결과입니다.

pelvis 추적 중에는 해당 스켈레탈 메시가 화면 밖에서도 관절을 갱신하도록 설정하고, 종료 시 이전 애니메이션 갱신 옵션을 복원합니다. 관절이 없는 정적 메시의 형태나 상대 위치가 실제로 변하지 않으면 숙임을 감지할 수 없습니다. 표시용 무기 메시까지 높이에 영향을 주는 경우에는 Height Source Component Name으로 몸통 메시를 지정하세요. 높이 감지 소스와 기준은 재생 시작 시점에 결정됩니다.

## 시퀀스 제작 기준

- 기준점은 재생을 시작할 때의 주인공 Actor 위치입니다. 로컬 +X는 보스 방향, +Y는 오른쪽, +Z는 위입니다. 높이 차이로 기준이 기울어지지 않고 스케일은 항상 1입니다.
- 제작용 원점 (0,0,0), 회전 0, 스케일 1을 기준으로 카메라 Transform 키를 만듭니다. 예를 들어 (-300,100,150)은 주인공 뒤 300cm, 오른쪽 100cm, 위 150cm입니다. 카메라를 시퀀스의 Spawnable로 만들고 Camera Cuts 트랙에 연결하세요. 별도의 부모 Actor에 붙인 카메라에는 원점 변환이 중복되지 않는지 확인해야 합니다.
- C++이 실행 시 Transform Origin Actor를 Applier로 지정합니다. 원점은 매 프레임 추적하지 않고 재생 시작 위치에 고정됩니다. Applier를 다른 Actor에 부착하거나 재생 중 움직이지 마세요. 캐릭터 자체를 보스 방향으로 돌리거나 이동시키지는 않습니다.
- 카메라만 연출한다면 캐릭터/보스 트랙은 필요 없습니다. 캐릭터의 Transform 트랙을 추가하면 게임의 이동 위치를 덮어쓸 수 있습니다.
- 캐릭터나 보스 트랙이 필요하면 Sequencer의 **바인딩 태그**를 `Subject`, `Boss`로 지정하세요. Actor Tags와는 다른 설정입니다. 설정의 Subject Binding Tag / Boss Binding Tag를 바꾸거나 None으로 비울 수도 있습니다. 동일한 태그를 양쪽에 지정하면 재생을 거절합니다. 서로 다른 스켈레톤에 같은 애니메이션을 재생할 수 있는지는 별도로 확인해야 합니다.
- 이번 구현은 시퀀스 인스턴스의 시계를 Tick으로 고정해 월드 시간 배율을 보정합니다. 에셋 자체는 변경하지 않습니다. 시퀀스에 Time Dilation 트랙을 추가하거나 다른 시스템이 재생 중 Global Time Dilation을 변경하지 않도록 하세요.

## 시간과 종료

World Time Scale은 컷신 시작 전 월드 배율에 곱해집니다. 주인공의 Actor Tick 속도는 컷신 시작 전 속도를 유지하도록 보정하며 카메라 타임라인은 실제 초 단위로 재생합니다. 이전 월드 배율이 1이면 주인공도 평소 속도가 됩니다. 이미 다른 슬로모션이 있었다면 그 이전 속도를 유지합니다.

동적 카메라의 Duration Seconds 기본값은 0입니다. 0이면 자동 종료 없이 계속 추적하며, 다음 유효한 컷신 요청이나 `CancelCutScene()`까지 유지합니다. 양수이면 지정한 실제 초 뒤에 자동으로 끝납니다. Level Sequence 연출은 시퀀스 에셋에 지정한 재생 구간을 따릅니다.

자연 종료, 새 컷신으로 교체, `CancelCutScene()`, 주인공/보스/재생 Actor 파괴, Applier EndPlay 시 기존 시간 배율과 게임 카메라를 복원합니다. 취소를 여러 번 해도 안전합니다. 새 컷신은 기존 연출의 상태를 복원한 뒤 시작하므로 슬로모션이 중첩되지 않습니다. 사전 검사에서 이름/대상/설정 오류로 거절된 요청은 현재 연출을 중단하지 않습니다. 다른 Applier가 재생 중일 때의 요청은 거절합니다.

`OnCutSceneApplied(종류, 주인공)`은 선택한 대상의 컷신이 시작할 때 발생합니다. `OnCutSceneEnded(종류, 중단 여부)`는 그 컷신이 끝났을 때 한 번 발생합니다. 교체된 이전 컷신은 중단 여부 true로 종료 알림을 보냅니다. 교체 처리 중 콜백에서 또 다른 재생 요청을 보내는 재진입은 거절하며, 자연 종료 뒤의 콜백에서는 다음 연출을 요청할 수 있습니다. 설정 오류처럼 시작 자체가 실패하면 완료 이벤트는 발생하지 않습니다. 레벨 종료/Applier 제거에서는 외부 전투 진행 콜백을 발생시키지 않습니다. 입력 차단이나 다음 전투 단계 전환은 호출자가 이 이벤트와 반환값으로 처리합니다.

물리 시뮬레이션, 별도로 스폰한 투사체/무기, Niagara 독립 시간 설정, FMOD 사운드는 Actor Custom Time Dilation과 별개일 수 있습니다. 해당 효과를 주인공 속도에 맞추는 코드는 이번 클래스에 포함하지 않습니다. 로컬 싱글플레이/방송 화면 기준이며 멀티플레이 동기화는 구현하지 않습니다.

## 코드 읽는 순서

1. `FindCharacter`: 전략에 맞는 직업 ID 확인 → `SelectCharacter`로 생존·공격 조건과 피버 비교.
2. `MakeOrigin`: 주인공 위치와 보스 방향으로 좌표계 생성.
3. `ApplyCutScene`: 설정 검증 → 대상 한 명 확정 → `StartShot`으로 재생.
4. `StartShot`: 재생기 생성 → 기준점/바인딩 → 시간 보정 → 재생.
5. `FinishCutScene`: 재진입 차단 → 기존 상태 복원 → 완료 이벤트.

공개 API는 `Public/Scripts/Battle/CutSceneApplier.h`, 실제 동작은 `Private/Scripts/Battle/CutSceneApplier.cpp`에 있습니다. BattleManager에는 읽기 전용 조회 함수 두 개만 추가했습니다.
