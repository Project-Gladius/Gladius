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

항상 메시지 한 개를 한 줄로 표시합니다. 줄바꿈과 탭은 공백으로 표시합니다. 짧은 메시지는 고정하고, 긴 메시지는 처음 1초 대기 후 초당 55 UI 단위의 일정한 속도로 오른쪽에서 왼쪽으로 이동합니다. 마지막 부분까지 드러난 뒤 1.5초 대기합니다.

새 메시지는 현재 내용을 중간에 끊지 않고 도착 순서대로 대기합니다. 짧은 메시지는 기본 4초 뒤 다음 메시지로 넘어갑니다. 대기 메시지가 없으면 짧은 메시지는 그대로 남고, 긴 메시지는 처음부터 반복합니다. 기존 내역은 SessionLogSubsystem에 유지되고, 맵에 진입하면 가장 최근 메시지를 표시합니다.

## 디자이너 설정

`SessionLogWidget`을 부모로 하는 빈 Widget Blueprint에서 Class Defaults의 `Session Log` 설정을 조절할 수 있습니다. MainBattle의 BattleManager 및 MainTemple의 TempleCycleController의 `Session Log Widget Class`에 지정합니다. 기본 C++ 클래스만으로도 동작합니다.

`Window Size`, `Window Offset`, `Text Padding`, `Log Font`, `Log Color`, `Admin Color`, `Admin Urgent Color`, `Event Color`, `Background Brush`, `Background Tint`를 조절할 수 있습니다. 기본 이미지를 유지하려면 Brush Tint와 Background Tint를 흰색으로 둡니다.

`Ticker` 항목의 `Scroll Speed`, `Start Hold Seconds`, `End Hold Seconds`, `Short Message Seconds`로 속도와 표시 시간을 조절합니다. 런타임에 스타일을 변경하면 `Refresh Style`을 호출합니다. 이 호출은 현재 메시지나 대기 순서를 초기화하지 않습니다.

기존 다중 행 설정 `Line Spacing`, `Max Visible Entries`는 호환성을 위해 남아 있으며 한 줄 전광판에는 적용하지 않습니다. 로그 저장소의 최근 내역 수 제한은 기존과 같습니다.
