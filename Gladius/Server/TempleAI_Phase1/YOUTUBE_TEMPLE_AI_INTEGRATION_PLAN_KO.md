# YouTube Live Chat - Temple AI 연동 설계

## 1. 문서 목적

현재 프로젝트에는 다음 두 기능이 각각 구현되어 있다.

- Unreal의 `YouTubeChatManager`: YouTube 라이브 채팅 조회 및 게임 명령 처리
- `TempleAIClient`와 Temple AI 서버: 질문을 전송하고 구조화된 여신 응답 수신

그러나 두 기능을 연결하는 경로와 다중 사용자 응답 대기열은 아직 없다. 이 문서는 여러 사용자의 채팅이 동시에 들어오는 상황에서 댓글을 안전하게 수집하고, AI 응답을 Unreal 연출로 전달하기 위한 기준을 정의한다.

## 2. 현재 상태

### 구현되어 있는 부분

- YouTube Data API로 `activeLiveChatId` 조회
- 라이브 채팅의 `commentId`, `channelId`, `displayMessage` 수집
- 댓글 중복 페이지 처리를 위한 `nextPageToken` 사용
- 댓글 로그, 도배 검사 및 `!` 게임 명령 처리
- Unreal에서 Temple AI 서버의 `/temple/respond` 호출
- 다음 다섯 필드로 구성된 Temple AI 응답 수신

```json
{
  "priority": 72,
  "responseText": "승리는 정해진 운명이 아니니라. 그대의 의지로 길을 열어 보거라.",
  "emotion": "neutral",
  "uid": "youtube-comment-id",
  "username": "별빛여행자"
}
```

### 아직 구현되지 않은 부분

- YouTube 일반 댓글을 `TempleAIClient`로 전달하는 연결 코드
- `authorDetails.displayName`을 `username`으로 전달하는 코드
- 동시에 들어온 댓글과 AI 응답을 관리하는 제한된 대기열
- AI 응답 `priority`에 따른 재생 순서 결정
- 처리 중복 방지, 시간 초과, 재시도 및 오래된 댓글 폐기 정책
- AI 응답을 여신 UI, 음성 및 모션에 연결하는 최종 연출 코드
- YouTube 라이브 채팅에 AI 답변을 다시 작성하는 OAuth 2.0 코드

현재 `YouTubeChatManager`는 일반 댓글을 Temple AI로 보내지 않고, `!`로 시작하는 채팅만 게임 명령으로 처리한다.

## 3. 출력 형식과 데이터 소유권

### 요청

YouTube 댓글 한 건을 다음처럼 변환한다.

| Temple AI 필드 | YouTube 원본 | 설명 |
|---|---|---|
| `uid` | `message.id` | 댓글 단위 고유 식별자 |
| `username` | `authorDetails.displayName` | 화면에 표시할 작성자명 |
| `message` | `snippet.displayMessage` | 사용자가 작성한 질문 |

`channelId`는 사용자별 도배 제한과 내부 추적에 사용한다. 공개 응답의 `uid`로는 댓글 단위 중복 방지가 가능한 `message.id`를 사용하는 것을 권장한다.

### 응답

- AI가 결정: `priority`, `responseText`, `emotion`
- 서버가 원본 요청에서 복사: `uid`, `username`
- AI가 `uid`와 `username`을 생성하거나 변경해서는 안 된다.

JSON 필드 순서는 의미가 없다. 필드 이름, 형식, 값의 제한이 계약이다.

## 4. 콘솔과 화면 출력

현재 Unreal의 화면 메시지는 연출 확인을 위해 `emotion`과 `responseText`만 표시한다. 따라서 화면에 답변만 보이는 것은 JSON 필드가 사라진 것이 아니다.

개발용 Output Log에서는 다음 형식으로 전체 처리 결과를 확인한다.

```text
Temple AI response: [별빛여행자] priority=72 emotion=neutral text=승리는 ...
```

최종 게임 화면에는 보통 다음 정보만 사용한다.

- 작성자 표시: `username`
- 여신 대사: `responseText`
- 표정, 모션 및 후광: `emotion`
- 재생 순서: `priority`

`uid`는 중복 방지와 요청-응답 연결용 내부 값이므로 플레이어에게 표시할 필요가 없다.

## 5. 권장 처리 구조

```text
YouTube Live Chat
  -> 댓글 수집 및 commentId 중복 제거
  -> 명령어와 AI 질문 분리
  -> 길이, 도배, 금칙어 및 입력 검열
  -> 입력 대기열
  -> 제한된 동시 요청으로 Temple AI 호출
  -> 구조화된 응답 검증
  -> priority 응답 대기열
  -> Unreal 여신 대사, 감정 모션 및 음성 재생
```

### 동시 입력 처리

YouTube의 한 번의 조회 결과에는 여러 사용자 댓글이 함께 들어올 수 있다. 각 댓글을 즉시 무제한으로 Gemini에 보내면 API 사용량과 응답 순서를 제어하기 어렵다.

권장 기본값:

- 입력 대기열 최대 100건
- Temple AI 동시 요청 2건
- 댓글 단위 제한 시간 10초
- 실패 시 최대 1회 재시도
- 60초 이상 지난 미처리 댓글 폐기
- `commentId` 기준 중복 처리 방지

`priority`는 AI 응답을 받은 후에 알 수 있으므로 처리 구조는 두 단계가 된다.

1. 입력 단계에서는 도배, 길이, 질문 여부와 오래된 댓글을 기준으로 호출 대상을 줄인다.
2. 응답 단계에서는 AI가 반환한 `priority`를 기준으로 여신이 말할 순서를 정한다.

같은 우선순위에서는 먼저 들어온 댓글을 먼저 처리한다. 낮은 우선순위가 영원히 밀리지 않도록 대기 시간이 길어질수록 점수를 조금 올리는 방식도 적용할 수 있다.

## 6. YouTube API 주의사항

### 채팅 읽기

공개 라이브 채팅 조회는 제한된 YouTube Data API 키로 가능하다. Gemini API 키와 YouTube API 키는 서로 다른 키이며 용도도 다르다.

현재 코드는 YouTube 응답의 `pollingIntervalMillis`를 무시하고 신전 40초, 전투 5초의 고정 주기를 사용한다. 안정적인 연동을 위해 YouTube가 반환한 최소 폴링 간격 이상으로 요청하고, `403` 및 할당량 오류에 지수 백오프를 적용해야 한다.

### 채팅 쓰기

AI 답변을 YouTube 라이브 채팅에 직접 올리려면 API 키만으로는 부족하다. 채널 소유자의 OAuth 2.0 인증과 쓰기 권한, 토큰 갱신 및 `liveChatMessages.insert` 구현이 필요하다.

첫 연동 단계에서는 YouTube 채팅을 읽고 여신이 게임 안에서 말하게 하는 단방향 구성을 권장한다. YouTube 채팅 쓰기는 별도 단계로 분리한다.

## 7. 구현 순서

### 1단계: 단일 댓글 연결

- `Gladius.Build.cs`에 `TempleAIClient` 모듈 의존성 추가
- `YouTubeChatManager`에 `UTempleAIClientComponent` 연결
- `displayName` 추출
- 일반 질문 한 건을 `SendTempleMessage`로 전송
- `OnResponse`와 `OnError` 처리

### 2단계: 다중 댓글 안정화

- 입력 및 응답 대기열 추가
- 동시 요청 수 제한
- `commentId` 중복 방지
- 시간 초과, 재시도 및 폐기 정책 추가
- `priority` 순서 재생

### 3단계: 게임 연출 연결

- `emotion`을 표정, 모션 및 후광에 매핑
- `responseText`를 자막과 음성 출력에 연결
- 말하는 동안 다음 응답을 대기열에 유지
- 응답 완료 후 다음 항목 재생

### 4단계: 운영 안정화

- YouTube 폴링 간격 준수
- API 오류와 할당량 모니터링
- API 키 제한 및 비밀값 분리
- 개인정보를 최소화한 로그 정책 적용
- 필요할 때만 OAuth 기반 YouTube 답변 전송 추가

## 8. 완료 조건

1. 라이브 댓글 한 건이 동일한 `uid`, `username`으로 Temple AI에 전달된다.
2. 동시에 여러 댓글이 들어와도 누락, 중복 및 무제한 요청이 발생하지 않는다.
3. 응답은 다섯 필드 계약을 항상 만족한다.
4. 높은 `priority` 응답이 먼저 재생되고 같은 값에서는 먼저 도착한 응답이 먼저 재생된다.
5. `emotion`에 맞는 Unreal 연출이 실행된다.
6. 서버 또는 Gemini 장애 시 게임이 멈추지 않고 오류나 안전 대사로 복구한다.
7. YouTube API 할당량 또는 네트워크 오류 시 폴링이 과도하게 반복되지 않는다.

## 9. 현재 판단

현재 구조는 단일 테스트 메시지와 구조화된 AI 응답 검증에는 적합하다. 실제 YouTube 연동을 위해서는 기존 코드를 교체할 필요는 없고, `YouTubeChatManager`와 `TempleAIClient` 사이의 연결 계층과 제한된 대기열을 추가하면 된다.
