# 운영진 공지·로그 전광판

MainBattle의 BattleManager와 MainTemple의 TempleCycleController가 공통 `SessionLogWidget`을 사용합니다. 기존 `ChatLogWidget`은 일반 채팅을 담당하고, 이 창은 운영진 공지·긴급 공지·이벤트·로그인/로그아웃 알림을 표시합니다.

## 기본 표시

- 사용자 제공 공지창 원본: `Gladius/SourceAssets/UI/Notice/T_NoticePanel.png`
- 게임 텍스처: `/Game/UI/UIDesign/T_NoticePanel`
- 우측 최상단 앵커 (1, 0), 정렬 (1, 0), 오프셋 (-24, 24).
- 크기 600×100. 화면 배율에 따라 함께 조절됩니다.
- 글자 영역 여백: 왼쪽 60, 위 20, 오른쪽 24, 아래 20. 느낌표와 장식 테두리를 피합니다.
- 마루부리 Regular 18. 일반 로그 흰색, 운영 공지 노랑, 긴급 공지 빨강, 이벤트 초록.

## 표시 순서와 이동

항상 메시지 한 개를 한 줄로 표시합니다. 줄바꿈과 탭은 공백으로 표시합니다. 긴 메시지는 처음 1초 대기 후 초당 55 UI 단위의 일정한 속도로 오른쪽에서 왼쪽으로 이동하고, 마지막 부분에서 1.5초 대기한 뒤 반복합니다.

일반 메시지는 짧고 긴 글 모두 표시를 시작한 시점부터 실제 시간 기준 20초간 표시합니다. 스크롤 길이나 전투 시간 배율에 따라 수명이 늘어나지 않습니다. 새 메시지는 도착 순서대로 대기하며, 각자 화면에 표시된 뒤 20초를 받습니다.

점검 알림은 점검 범위가 해제될 때까지 일반 메시지보다 우선 표시합니다. 전투·신전·전체·AI 점검은 각각 독립적으로 관리하고, 여러 점검이 켜져 있으면 진행 중인 범위를 함께 표시합니다. 점검 중에는 가려진 일반 메시지의 남은 시간을 일시 정지하며, 모든 점검 해제 후 이어서 표시합니다. 점검 완료 메시지는 일반 메시지처럼 20초 표시합니다. 이미 해제된 점검 시작 문구를 뒤늦게 대기열에서 재생하지 않습니다.

대기열이 비면 마지막으로 표시된 운영자의 `!공지` 메시지로 돌아가 계속 표시합니다. 로그인·이벤트·점검 완료 알림은 이 기본 공지를 덮어쓰지 않습니다. 공지는 최근 기록 100개에서 밀려나더라도 기본 표시용으로 유지합니다. 아직 운영 공지가 한 번도 없으면 대기열 소진 후 표시할 기본 공지도 없습니다.

표시 정책·대기열·남은 시간·진행 중 점검·최근 공지는 SessionLogSubsystem이 관리하여 맵 이동 후에도 유지합니다. 프로그램 재시작 시에는 기존 세션 기록과 함께 초기화됩니다.

## 디자이너 설정

`SessionLogWidget`을 부모로 하는 빈 Widget Blueprint에서 Class Defaults의 `Session Log` 설정을 조절할 수 있습니다. MainBattle의 BattleManager 및 MainTemple의 TempleCycleController의 `Session Log Widget Class`에 지정합니다. 기본 C++ 클래스만으로도 동작합니다.

`Window Size`, `Window Offset`, `Text Padding`, `Log Font`, `Log Color`, `Admin Color`, `Admin Urgent Color`, `Event Color`, `Background Brush`, `Background Tint`를 조절할 수 있습니다. 기본 이미지를 유지하려면 Brush Tint와 Background Tint를 흰색으로 둡니다.

`Ticker` 항목의 `Scroll Speed`, `Start Hold Seconds`, `End Hold Seconds`로 스크롤 속도와 양 끝 대기를 조절합니다. 일반 메시지 수명은 20초로 고정이며, 기존 `Short Message Seconds` 값은 호환용으로만 남아 표시 정책에는 사용하지 않습니다. 런타임에 스타일을 변경하면 `Refresh Style`을 호출합니다. 이 호출은 현재 메시지나 대기 순서를 초기화하지 않습니다.

기존 다중 행 설정 `Line Spacing`, `Max Visible Entries`는 호환성을 위해 남아 있으며 한 줄 전광판에는 적용하지 않습니다. 로그 저장소의 최근 내역 수 제한은 기존과 같습니다.
