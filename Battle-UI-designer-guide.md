# 전투 화면 UI 디자인 가이드 (디자이너용)

## 순위표 자유 배치 및 공통 Row 편집 (2026-10-26)

Row 내부 디자인은 공통 Row BP에서 한 번 수정하면 같은 화면의 10줄 모두에 반영됩니다. Board에서는 각 Row·제목·머리글·배경을 자유롭게 배치합니다.

| 화면 | 전체 배치 | 10줄 공통 내부 디자인 |
|---|---|---|
| LEADERBOARD | `WBP_LeaderboardBoard` | `WBP_LeaderboardRow` |
| BATTLE ANALYSIS | `WBP_LeaderboardBoard_Battle` | `WBP_LeaderboardRow_Battle` |

모든 BP는 `Content/Blueprints/UI`에 있습니다.

1. **Row 내부 10줄 일괄 수정**: 해당 Row BP를 열어 `RowCells` 아래의 순위·사용자 이름·캐릭터·체력·데미지 셀을 선택하고 움직입니다. Canvas Slot의 Position X/Y와 Size X/Y를 사용할 수 있습니다. 글꼴·색상·이미지도 이 BP에서 한 번 수정합니다. 컴파일 후 저장하면 보드의 10줄이 함께 갱신됩니다.
2. **개별 Row 위치 변경**: 해당 Board BP에서 `RowContainer` 아래의 `SharedRow01`~`SharedRow10`을 선택하고 Designer에서 드래그하거나 Canvas Slot → Position X/Y를 입력합니다. Row 안의 항목들은 함께 움직입니다.
3. **목록 전체 이동**: Board에서 `RowContainer`를 선택하고 Canvas Slot의 Position X/Y를 바꿉니다.
4. **제목·머리글 이동**: `BoardLayout` 아래의 `LB_TitleText`와 `HeaderPanel`은 Canvas에서 자유 배치할 수 있습니다. `HeaderCells` 아래의 각 머리글도 개별 배치할 수 있습니다.
5. **배경 이동·크기 조정**: `BackgroundCanvas` 아래의 `NativeBackgroundImage`를 움직이거나 Canvas Slot의 Position/Size를 수정합니다. 현재 배경은 원본 비율과 같은 1440 × 1600이며, 제목·머리글·행은 이 이미지 좌표에 맞춰 정렬되어 있습니다.
6. Class Defaults → Preview Page에서 Total / Battle Top / Battle Lower를 확인한 후 컴파일하고 저장합니다.

두 화면은 별도 Row BP를 사용하므로 서로 다른 디자인을 유지합니다. 기존 첫 번째 Row의 최신 내부 디자인을 공통 템플릿으로 가져왔고, 기존 개별 Row의 배치와 보드 장식은 유지했습니다. 변환 전 원본은 별도로 백업했습니다.

`SharedRow01`~`SharedRow10`, `RowContainer`, `LB_TitleText` 및 Row BP의 `RankText`, `NameText`, `CharacterText`, `HealthCell`, `HealthText`, `DamageText`, `Medal`, `RowBackground` 이름은 유지하세요. 위치·부모·크기·장식은 수정할 수 있습니다.

실행·페이지 전환은 데이터와 동적 연출만 갱신합니다. 저장된 Row를 재생성하거나 Canvas 위치를 덮어쓰지 않습니다. 순위·이름·데미지·체력, 메달·1위 강조·사망 시 흐림은 기존처럼 데이터에 따라 표시됩니다.


---


캐릭터 머리 위 NamePlate의 공격력·방어율과 하단 피벗 편집은 [NamePlate 상태 수치 기획 및 가이드](Nameplate-status-spec.md)를 참고하세요.

전투 화면의 UI 3가지를 언리얼 에디터에서 직접 꾸미는 방법입니다.

- **리더보드** (화면 왼쪽 위, 순위표)
- **보스 행동 예고 문구** (보스 바로 아래에 뜨는 상자)
- **상단 UI** (화면 맨 위 가운데, 턴·안내 문구·남은 시간)

> 메뉴 이름은 **한글 메뉴 (영어 메뉴)** 형식으로 적었습니다. 에디터가 영어로 되어 있으면 괄호 안의 이름을 찾으세요.

---

## 1. 핵심 규칙 3가지

1. **모양, 위치, 크기, 이미지, 글꼴, 색은 마음대로 바꿔도 됩니다.**
2. **아래 표에 있는 위젯은 이름을 바꾸거나 지우면 안 됩니다.**
   - 이 이름들은 프로그램이 찾아서 숫자와 글자를 넣는 곳입니다.
   - 이름은 **영어 그대로** 두세요. (예: `GuideText`)
   - 지우면 그 기능(예: 남은 시간 표시)만 사라지고, 이름을 바꾸면 화면에 아무것도 안 나옵니다.
3. **같은 파일을 두 사람이 동시에 고치면 안 됩니다.** 작업 전에 담당자에게 알려주세요.

장식용 이미지나 글자는 **얼마든지 새로 추가**해도 됩니다. 이름은 아무거나 괜찮습니다.

---

## 2. 파일 위치

콘텐츠 브라우저 (Content Browser)에서 `Content` → `Blueprints` → `UI` 폴더를 엽니다.

| 파일 이름 | 화면에서 보이는 것 |
|---|---|
| `WBP_TopHud` | 화면 맨 위 가운데: 턴, 안내 문구, 남은 시간 막대, 남은 시간 숫자 |
| `WBP_BossTelegraph` | 보스 아래에 뜨는 예고 문구 상자 |
| `WBP_LeaderboardRotator` | 순위표가 놓이는 자리 (위치와 크기만 정함) |
| `WBP_LeaderboardBoard` | 순위표의 틀: 배경, 제목, 열 이름 |
| `WBP_LeaderboardRow` | 순위표 **한 줄**의 디자인. 이것 하나를 꾸미면 10줄 모두 똑같이 바뀜 |

---

## 3. 작업 순서

### 3-1. 이미지 가져오기
1. 콘텐츠 브라우저에서 `Content/Blueprints/UI` 안에 `Textures` 폴더를 만듭니다. (없으면)
2. PNG 파일을 그 폴더로 **끌어다 놓습니다**.
3. 가져온 이미지를 더블클릭해서 설정을 바꿉니다. (권장)
   - 텍스처 그룹 (Texture Group) → `UI`
   - 압축 설정 (Compression Settings) → `사용자 인터페이스 2D (UserInterface2D)`

### 3-2. 꾸미기
1. 꾸밀 파일(예: `WBP_TopHud`)을 **더블클릭**해서 엽니다.
2. 오른쪽 위에서 **디자이너 (Designer)** 탭이 선택되어 있는지 확인합니다.
3. 왼쪽 **계층구조 (Hierarchy)** 창이나 가운데 화면에서 꾸밀 부분을 클릭합니다.
4. 오른쪽 **디테일 (Details)** 창에서 바꿉니다.
   - **배경 이미지 넣기** (Border 또는 Image 위젯)
     - 브러시 (Brush) → 이미지 (Image)에 가져온 이미지를 지정합니다.
     - 그리기 방식 (Draw As): 그냥 이미지면 `이미지 (Image)`, 늘려도 모서리가 깨지지 않게 하려면 `박스 (Box)`
   - **글자 바꾸기** (Text 위젯)
     - 글꼴 (Font), 크기 (Size), 색 (Color and Opacity), 정렬 (Justification)
   - **위치·크기 조정**: 가운데 화면에서 직접 끌거나, 디테일 창의 슬롯 (Slot) 항목에서 숫자로 입력합니다.
5. 왼쪽 위 **컴파일 (Compile)** → **저장 (Save)** 버튼을 누릅니다.
6. 전투 레벨(`MainBattle`)을 열고 **플레이 (Play)** 를 눌러 확인합니다.

> 오른쪽 아래 **위젯 바인딩 (Bind Widgets)** 창에 "이름을 지키면 안 되는 위젯" 목록이 표시됩니다. 초록색이면 정상, 빨간색/노란색이면 이름이 틀렸거나 없는 것입니다.

---

## 4. 이름을 바꾸면 안 되는 위젯 목록

### WBP_TopHud (화면 맨 위 가운데)

각 요소는 이제 Canvas Slot으로 자유 배치합니다. Designer에서 `TurnLabelText`, `TH_TurnValueText`, `TH_TurnTotalText`, `TH_GuideText`, `TH_Gauge`, `TH_TimerText`를 직접 선택해 드래그하거나 Details → Slot (Canvas Panel Slot) → Position X/Y, Size X/Y를 수정합니다. 기존 `HorizontalBox_*`, `VerticalBox_*`, `ScaleBox_*`, `SizeBox_*` 이름의 컨테이너도 Canvas로 변환되어 자동 정렬을 강제하지 않습니다.

턴 영역 전체는 `TurnPanel`, 안내·게이지 영역은 `CenterPanel`, 타이머 영역은 `TimerPanel`을 이동하면 됩니다. HUD 전체 이동은 최상위 `TopHudFrame`의 Canvas Slot에서 조절합니다. `Image_35`는 프레임 전체를 채우는 배경 이미지이며, Anchors (0,0)~(1,1), Offsets 0을 유지하면 전체 크기를 따라갑니다.

데이터 위젯 이름은 유지하세요. 컴파일·저장 후에도 위치와 크기는 유지되며, 전투 중에는 내용·시간·진행률·표시 여부만 갱신됩니다.

| 이름 (그대로 유지) | 종류 | 하는 일 |
|---|---|---|
| `TopHudBar` | 아무 위젯 | 상단 UI 전체. 컷신이 나올 때와 전투 시작 전·후에 통째로 숨겨집니다 |
| `TurnValueText` | 글자 | 현재 턴 숫자 "5" |
| `TurnTotalText` | 글자 | 전체 턴 "/10" |
| `GuideText` | 글자 | "행동을 선택하세요..." / "대기 중..." / "전투 중!" |
| `Gauge` | 진행 막대 (Progress Bar) | 남은 시간 막대. 막대의 배경·채움 이미지는 디테일 창의 **스타일 (Style)** 에서 지정 |
| `TimerContent` | 아무 위젯 | 시계 아이콘 + 남은 시간 숫자를 묶은 영역. 전투 중에는 숨겨집니다 |
| `TimerText` | 글자 | 남은 시간 "30.00" |

### WBP_BossTelegraph (보스 예고 문구)
| 이름 (그대로 유지) | 종류 | 하는 일 |
|---|---|---|
| `TelegraphBox` | 아무 위젯 | 문구 상자 전체. 보스를 따라 움직입니다. **반드시 맨 바깥 캔버스 (Canvas Panel) 바로 안에 두세요** (다른 위젯 안에 넣으면 보스를 따라가지 않음) |
| `MessageText` | 글자 | 예고 문구 |

### WBP_LeaderboardRotator (순위표 자리)
| 이름 (그대로 유지) | 종류 | 하는 일 |
|---|---|---|
| `Board` | 순위표 (WBP_LeaderboardBoard) | 10초마다 내용이 바뀌는 순위표. 위치·크기·배율만 조정하세요 |

### WBP_LeaderboardBoard (순위표 틀)
| 이름 (그대로 유지) | 종류 | 하는 일 |
|---|---|---|
| `TitleText` | 글자 | 제목 "LEADERBOARD" / "BATTLE ANALYSIS" |
| `HealthHeaderCell` | 아무 위젯 | "체력" 열 제목. LEADERBOARD에서는 숨겨집니다 |
| `DamageHeaderText` | 글자 | "누적 데미지" / "데미지" |
| `RowContainer` | 세로 상자 (Vertical Box) | 순위 10줄이 자동으로 채워지는 자리. 행 자체의 디자인은 해당 상태의 Row BP에서 편집합니다 |

> 편집 화면에는 예시 순위 10줄이 미리 보입니다. 실제 게임에서는 진짜 순위로 바뀝니다.

### WBP_LeaderboardRow (순위표 한 줄)
| 이름 (그대로 유지) | 종류 | 하는 일 |
|---|---|---|
| `RowBackground` | 테두리 (Border) | 줄 배경 |
| `Medal` | 테두리 (Border) | 순위 숫자 뒤의 배경 (1~3위 메달) |
| `RankText` | 글자 | 순위 숫자 |
| `NameText` | 글자 | 사용자 이름 |
| `CharacterText` | 글자 | 글라디우스 / 세라 |
| `HealthCell` | 아무 위젯 | 체력 칸 전체. LEADERBOARD에서는 숨겨집니다 |
| `HealthText` | 글자 | 체력 숫자 |
| `DamageText` | 글자 | 데미지 "98 540" |

---

## 5. 게임 중에 바뀌는 이미지 (메달, 1위 강조 등)

순위에 따라 바뀌는 이미지는 디자이너 화면이 아니라 **다른 곳**에서 지정해야 합니다.
디자이너 화면에서 바꾸면 게임 중에 덮어써집니다.

**지정하는 곳**: `WBP_LeaderboardRow` 열기 → 위쪽 툴바의 **클래스 디폴트 (Class Defaults)** 버튼 → 오른쪽 디테일 창 → **Style** 항목

| 항목 이름 | 뜻 |
|---|---|
| `Medal Brushes` | 메달 이미지. **0번 = 1위(금), 1번 = 2위(은), 2번 = 3위(동)** |
| `First Row Brush` | 1위 줄의 강조 배경 |
| `First Damage Color` | 1위 데미지 글자 색 |
| `Dimmed Opacity` | 체력이 0인 줄을 흐리게 하는 정도 (0 = 안 보임, 1 = 그대로) |

`bHighlight First Row` 체크를 끄면 1위 줄도 다른 줄과 같은 배경을 씁니다.

### 다른 파일의 클래스 디폴트 (Class Defaults) 설정
| 파일 | 항목 | 뜻 |
|---|---|---|
| WBP_BossTelegraph | `Display Seconds` | 예고 문구가 보이는 시간 (초, 기본 5) |
| WBP_BossTelegraph | `Below Boss Screen Offset` | 보스와 상자 사이의 거리 (Y가 클수록 아래로) |
| WBP_BossTelegraph | `bUse Built In Fade` / `Fade Seconds` | 기본으로 서서히 나타나고 사라지는 효과 / 그 시간 |
| WBP_LeaderboardRotator | `Switch Seconds` | 순위표가 바뀌는 간격 (초, 기본 10) |

---

## 6. 애니메이션 넣기 (선택)

나타날 때 튀어나오는 효과 등을 넣고 싶을 때 사용합니다.

1. 디자이너 (Designer) 화면 아래 **애니메이션 (Animations)** 창에서 애니메이션을 만듭니다.
2. 오른쪽 위 **그래프 (Graph)** 탭으로 갑니다.
3. 왼쪽 **내 블루프린트 (My Blueprint)** → 함수 (Functions) 옆 **오버라이드 (Override)** 에서 아래 이벤트를 고릅니다.
4. 그 이벤트에 **애니메이션 재생 (Play Animation)** 을 연결합니다.

| 파일 | 이벤트 | 언제 실행되나 |
|---|---|---|
| WBP_TopHud | `On Hud State Changed` | 안내 문구가 바뀌거나 상단 UI가 보이고/숨겨질 때 |
| WBP_BossTelegraph | `On Telegraph Shown` | 예고 문구가 나타날 때 |
| WBP_BossTelegraph | `On Telegraph Hidden` | 예고 문구가 사라질 때 |
| WBP_LeaderboardBoard | `On Page Changed` | 순위표 내용이 바뀔 때 (`Title Changed`가 체크되어 있으면 다른 순위표로 넘어간 것) |
| WBP_LeaderboardRow | `On Row Changed` | 한 줄의 내용이 바뀔 때 |

> 예고 문구에 직접 만든 애니메이션을 쓸 때는 **클래스 디폴트 (Class Defaults)** 에서 `bUse Built In Fade` 체크를 끄세요. 켜 두면 기본 효과와 겹칩니다.

---

## 7. 문제가 생겼을 때

| 증상 | 확인할 것 |
|---|---|
| 글자나 숫자가 안 나옴 | 4번 표의 이름이 영어 그대로인지, 지우지 않았는지 |
| 예고 문구 상자가 보스를 안 따라감 | `TelegraphBox`가 맨 바깥 캔버스 (Canvas Panel) 바로 안에 있는지 |
| 메달 이미지가 안 바뀜 | 디자이너 화면이 아니라 클래스 디폴트 (Class Defaults) → Style → `Medal Brushes`에 넣었는지 |
| 처음 상태로 되돌리고 싶음 | 개발 담당자에게 요청 (파일을 지우고 다시 만들어 줌) |

---

## (개발 담당자용) 파일을 처음 상태로 다시 만들기

1. 해당 WBP를 콘텐츠 브라우저에서 삭제합니다.
2. 에디터를 끈 상태에서 아래를 실행합니다.
   ```
   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Gladius\Gladius\Gladius.uproject" -run=GenerateGladiusUI
   ```
   이미 있는 파일은 건드리지 않고, 없는 파일만 기본 모양으로 새로 만듭니다.


## 배경 이미지 비율과 크기

두 Board의 Designer에서 BackgroundFrame을 선택합니다. Hierarchy 검색창에 BackgroundFrame을 입력하면 바로 찾을 수 있습니다.

- Slot (Canvas Panel Slot)의 Size X / Size Y: 배경이 들어갈 영역 크기.
- Position X / Position Y: 배경 위치.
- Stretch = Scale To Fit: 원본 가로세로 비율을 유지합니다. 영역 비율이 다르면 여백이 생깁니다.
- NativeBackgroundImage: 원본 이미지를 교체하는 곳. 현재 원본은 3751 × 4167입니다.

배경 크기는 BackgroundFrame의 Size로 조절하세요. Render Transform의 Scale에는 기존 BoardPanel 비균등 배율을 상쇄하는 값이 있으므로 그대로 둡니다. 제목·헤더·행은 별도 BoardLayout에 남아 있어 배경 크기 변경으로 함께 늘어나지 않습니다. 컴파일이나 데이터 갱신 시 이 배치를 다시 만들지 않습니다.
