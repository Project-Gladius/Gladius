# YouTube Live Chat - Temple AI 개별 응답 배치 계약

## 1. 확정 동작

YouTube의 `!대화 <내용>`을 게임 세션 기준 20초 동안 모은다. 구간이 끝나면 모든 유효 요청을 `messages` 배열로 서버에 한 번 전송한다.

서버와 Gemini도 배치 호출 한 번으로 처리하지만, 결과는 질문마다 독립된 `responses` 배열로 반환한다. 여러 질문을 하나의 답변으로 종합하지 않는다.

```text
YouTube Live Chat
  -> !대화 명령 추출
  -> 20초 동안 messages[] 수집
  -> POST /temple/respond-batch 한 번 호출
  -> 질문별 responses[] 수신
  -> priority 순으로 자막, 음성 및 감정 모션 재생
```

기존 `POST /temple/respond`는 Unreal 수동 입력 UI와 단건 점검용으로 유지한다.

## 2. 입력 및 수집 규칙

유효한 입력은 `!대화 다음 전투에서는 무엇을 조심해야 하나요?` 형식이다.

- 수집 구간: 20초
- 구간당 최대 메시지: 30건
- 사용자당 구간당 첫 유효 요청 1건
- 메시지당 길이: 1~200자
- 배치 전체 메시지 길이: 최대 4,000자
- 유효 메시지가 없는 구간은 서버를 호출하지 않음
- 이전 배치 응답 대기 중에도 다음 구간을 계속 수집
- YouTube 조회는 `pollingIntervalMillis`를 준수
- 빈 입력, 중복 댓글 ID, 도배, 검열 위반 및 신전 외 상태의 입력은 제외

## 3. 요청 형식

```http
POST /temple/respond-batch
Content-Type: application/json
X-Temple-Client-Token: <설정된 경우>
```

```json
{
  "schemaVersion": 1,
  "batchId": "temple-20261010T120000Z-0001",
  "windowStartedAt": "2026-10-10T12:00:00Z",
  "windowEndedAt": "2026-10-10T12:00:20Z",
  "messages": [
    {
      "uid": "youtube-message-001",
      "userId": "youtube-channel-A",
      "username": "별빛여행자",
      "message": "다음 전투에서 무엇을 조심해야 하나요?"
    },
    {
      "uid": "youtube-message-002",
      "userId": "youtube-channel-B",
      "username": "푸른검투사",
      "message": "지금 강화를 해도 괜찮을까요?"
    }
  ]
}
```

| 필드 | 형식 | 의미 |
|---|---|---|
| `schemaVersion` | 정수 `1` | 요청·응답 계약 버전 |
| `batchId` | 문자열 | 20초 배치 고유 ID. 재시도할 때 같은 값을 사용 |
| `windowStartedAt` | UTC ISO 8601 | 수집 시작 시각 |
| `windowEndedAt` | UTC ISO 8601 | 수집 종료 시각 |
| `messages` | 배열 1~30건 | 수집된 유효 대화 요청 |
| `messages[].uid` | 문자열 | YouTube 댓글 고유 ID. 답변 연결과 중복 방지에 사용 |
| `messages[].userId` | 문자열 | YouTube 채널 ID. 사용자당 제한과 도배 검사에 사용 |
| `messages[].username` | 문자열 | YouTube 표시 이름 |
| `messages[].message` | 문자열 1~200자 | `!대화` 뒤 실제 질문 |

`uid`와 `userId`는 서로 다른 값이다. `uid`는 댓글 하나를 식별하고, `userId`는 댓글 작성자를 식별한다.

## 4. 응답 형식

```json
{
  "schemaVersion": 1,
  "batchId": "temple-20261010T120000Z-0001",
  "responses": [
    {
      "uid": "youtube-message-001",
      "username": "별빛여행자",
      "priority": 85,
      "responseText": "앞만 보지 말고 퇴로와 동료의 움직임을 먼저 살피거라.",
      "emotion": "neutral"
    },
    {
      "uid": "youtube-message-002",
      "username": "푸른검투사",
      "priority": 72,
      "responseText": "그대가 자주 무너지는 순간을 돌아본 뒤 힘을 선택하거라.",
      "emotion": "pleased"
    }
  ],
  "acceptedCount": 2,
  "rejectedCount": 0
}
```

| 필드 | 형식 | 의미 |
|---|---|---|
| `schemaVersion` | 정수 `1` | 응답 계약 버전 |
| `batchId` | 문자열 | 요청에서 받은 배치 ID |
| `responses` | 배열 | 검열을 통과한 질문별 개별 답변 |
| `responses[].uid` | 문자열 | 원본 질문과 답변을 연결하는 댓글 ID |
| `responses[].username` | 문자열 | 원본 요청에서 서버가 복사한 표시 이름 |
| `responses[].priority` | 정수 0~100 | 개별 답변 재생 우선순위 |
| `responses[].responseText` | 1~70자, 최대 2문장 | 개별 질문에 대한 여신 답변 |
| `responses[].emotion` | enum | 개별 답변의 표정·모션 값 |
| `acceptedCount` | 정수 | 정상 처리되어 `responses`에 포함된 수 |
| `rejectedCount` | 정수 | 검열, 중복 또는 제한으로 제외된 수 |

허용 감정은 `neutral`, `sorrowful`, `bestow`, `pleased`, `upset`이다.

AI는 `uid`, `username`, `batchId` 또는 건수를 생성하지 않는다. 서버가 검증된 원본 요청에서 복사하고, AI는 각 항목의 `priority`, `responseText`, `emotion`만 결정한다.

## 5. 순서와 실패 처리

- AI 출력은 입력 메시지와 같은 순서, 같은 개수여야 한다.
- 서버는 순서대로 원본 식별 정보와 AI 응답을 결합한다.
- 서버 검열에서 제외된 입력은 AI에 전달하지 않는다.
- 네트워크 실패 또는 `5xx`는 동일 `batchId`로 한 번 재시도한다.
- `4xx`는 자동 재시도하지 않는다.
- 같은 `batchId`와 같은 본문은 기존 결과를 반환해 중복 호출을 막는다.
- 같은 `batchId`에 다른 본문을 보내면 `409`를 반환한다.
- 유효 메시지가 하나도 없으면 `422 No acceptable messages`를 반환한다.
- Unreal 응답 큐는 `priority`가 높은 개별 답변부터 꺼낸다.
- 같은 우선순위는 먼저 받은 답변이 먼저이며 60초가 지난 답변은 폐기한다.

## 6. Unreal 설정값

| 속성 | 권장값 | 의미 |
|---|---|---|
| `ApiKey` | YouTube Data API v3 키 | YouTube 채팅 조회용 키. Gemini 키와 다름 |
| `VideoId` | 방송 영상 ID | 대상 YouTube 라이브 방송 |
| `TempleAIBatchEndpointUrl` | `http://127.0.0.1:8765/temple/respond-batch` | Temple AI 배치 주소 |
| `TempleAIClientToken` | 서버 값과 동일 | `.env`의 `TEMPLE_CLIENT_TOKEN`과 비교하는 공유 토큰 |
| `TalkBatchWindowSeconds` | `20.0` | 대화를 모으는 시간 |
| `MaxTalkMessagesPerBatch` | `30` | 구간당 최대 대화 수 |

블루프린트 연결 지점:

- `OnTempleTalkBatchResult`: 배치의 모든 개별 응답이 도착했을 때 호출
- `DequeueNextTempleResponse`: 우선순위가 가장 높은 개별 응답 하나를 반환
- `ResponseText`: 자막 및 TTS에 사용
- `Emotion`: 표정, 모션 및 후광에 사용
- `OnTempleTalkBatchError`: 서버 연결 또는 응답 형식 오류 처리

## 7. 서버 환경 변수

| 변수 | 의미 |
|---|---|
| `GEMINI_API_KEY` | 서버에서만 사용하는 Gemini API 키 |
| `GEMINI_MODEL` | 우선 호출할 Gemini 모델 |
| `GEMINI_FALLBACK_MODELS` | 실패 시 사용할 예비 모델 목록 |
| `TEMPLE_CLIENT_TOKEN` | Unreal 요청 인증용 공유 토큰. Gemini API 키가 아님 |

## 8. 구현 상태

- 20초 `messages[]` 수집 구현 완료
- 빈 `!대화`, 중복 ID, 사용자당 1회 및 길이 제한 구현 완료
- `authorDetails.displayName` 수집 구현 완료
- `POST /temple/respond-batch` 구현 완료
- 한 번의 Gemini 배치 호출에서 질문별 개별 응답 생성 구현 완료
- 서버의 `uid`·`username` 원본 복사 구현 완료
- `responses[]` Unreal 파싱 및 개별 우선순위 큐 구현 완료
- 동일 `batchId` 멱등 처리 및 1회 재시도 구현 완료
- `pollingIntervalMillis` 준수 구현 완료
- 자막, TTS 및 모션의 실제 블루프린트 연출 연결은 별도 작업
