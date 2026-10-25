# 로그인·로그아웃 공용 로그 창

`SessionLogWidget`은 기존 `ChatLogWidget`과 다른 창입니다. 일반 채팅과 신전 대화는 기존 채팅 창에 표시하며, 로그인 결과·로그아웃·전투 무입력 자동 퇴장 알림은 이 창에만 표시합니다. MainBattle의 BattleManager와 MainTemple의 TempleCycleController가 각각 창을 생성합니다. 최근 내역은 맵 이동 후에도 유지됩니다.

## 디자이너 설정

1. Widget Blueprint를 만들고 부모 클래스를 `SessionLogWidget`으로 지정합니다(예: WBP_SessionLog).
2. 기본 자동 레이아웃을 사용할 때는 빈 위젯 트리를 유지합니다. Class Defaults의 `Session Log` 항목에서 설정합니다.
3. `Background Brush`의 Image에 텍스처·UI 머티리얼·아틀라스 스프라이트를 지정합니다. Draw As를 Image 또는 Box로 바꾸고, 원본 색을 그대로 쓰려면 Brush Tint와 Background Tint를 흰색으로 설정합니다. Box의 Margin으로 9-slice를 조정할 수 있습니다. 미지정 시 단색 기본 패널이 표시됩니다.
4. `Log Font`: 기본 마루부리 Regular. 폰트 에셋, 크기, 윤곽선을 변경할 수 있습니다. `Log Color`: 기본 흰색(네 종류 알림 모두 동일).
5. `Window Size`, `Window Offset`, `Text Padding`, `Line Spacing`, `Max Visible Entries`로 크기·위치·여백·줄 간격·표시 행 수를 변경합니다. 기본 위치는 왼쪽 아래입니다. 런타임 변경 후에는 `Refresh Style`을 호출합니다.
6. MainBattle의 BattleManager와 MainTemple의 TempleCycleController에 있는 `Session Log Widget Class`에 같은 WBP를 지정합니다. 각 맵에서 다른 WBP를 지정해도 됩니다. C++ 기본 클래스로도 창이 표시됩니다.

직접 Designer 레이아웃을 만들 경우 Canvas > SizeBox `LogWindow` > Overlay 아래에 Image `LogBackground`, ScrollBox `LogScrollBox` > VerticalBox `LogMessages`를 배치하고 Is Variable을 켭니다. Image가 먼저, ScrollBox가 나중이어야 텍스트가 이미지 위에 보입니다. 기본 앵커는 좌하단, 정렬은 (0, 1), SizeBox 슬롯은 Auto Size입니다. 배경과 스크롤 슬롯은 Fill로 지정합니다. `LogMessages`는 필수 표시 대상이며 나머지는 선택 바인딩입니다. 스타일은 Class Defaults 값이 적용됩니다.

## 동작

- 기존 `!로그인 글라디우스` / `!로그인 세라` 규칙을 유지합니다. 캐릭터 이름 없는 `!로그인`은 기존처럼 무시됩니다.
- 실제 캐릭터 생성 및 참가 성공: `닉네임 님이 로그인하셨습니다`
- 로그인 실패(생성 실패, 정원 초과, 잘못된 캐릭터, 참가 불가능 상태 등): `닉네임님의 로그인이 정상 처리되지 못했습니다`
- 참가 중인 사용자의 `!로그아웃`: `닉네임 님이 로그아웃하셨습니다`
- 전투에서 무입력 제한 도달: `닉네임 님의 입력이 없어 자동 로그아웃되었습니다`
- 참가하지 않은 사용자의 로그아웃은 성공 알림을 만들지 않습니다. 신전에서 새 캐릭터 로그인을 허용하지 않습니다.
- 로그 저장소는 최근 100개, 창은 최근 30개가 기본이며 자동으로 마지막 줄로 스크롤합니다.
