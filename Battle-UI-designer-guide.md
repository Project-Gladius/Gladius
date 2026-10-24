# 전투 화면 UI 디자인 가이드 (디자이너용)

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
| `RowContainer` | 세로 상자 (Vertical Box) | 순위 10줄이 자동으로 채워지는 자리. 안에 넣어 둔 것은 게임 실행 시 지워집니다 |

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
