# Temple AI Phase 1

신전 프로젝트의 첫 번째 세로형 MVP입니다.

현재 목표는 하나의 질문을 받아 여신 답변을 구조화된 JSON으로 반환하는 것입니다. 아직 Unreal 연결, YouTube API, 다중 댓글 큐, 축복, 스트리밍은 포함하지 않습니다.

## 포함된 기능

- `POST /temple/respond`
- `GET /health`
- Gemini Interactions API 호출
- JSON Schema 기반 구조화 출력
- 고정 감정 enum
- 입력 길이와 공백 검증
- AI가 `uid`, `username`을 생성하지 못하도록 서버에서 원본 결합
- 기본적인 프롬프트 데이터 경계
- 실제 API 키 없이 실행되는 자동 테스트용 가짜 모델

## 요구 환경

- Python 3.10 이상
- Gemini API 키

## Windows 실행

CMD에서 간단히 실행하려면 `run_server.cmd`를 사용합니다. 같은 로컬 네트워크의 다른 컴퓨터에서 접속해야 하면 `run_server_lan.cmd`를 사용합니다. 자세한 절차는 `EXTERNAL_PC_SERVER_GUIDE_KO.md`를 참고합니다.

PowerShell에서 프로젝트 폴더로 이동한 뒤 실행합니다.

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt

$env:GEMINI_API_KEY="발급받은_API_키"
$env:GEMINI_MODEL="gemini-3.7-flash"

uvicorn app.main:app --reload --port 8000
```

API 키는 채팅, 코드, `.env.example`, Git 저장소에 입력하지 마세요. 현재 PowerShell 창에만 환경변수로 지정하는 방식이 가장 단순합니다.

## Windows 재실행용 스크립트

매번 명령어를 다시 입력하기 싫다면 프로젝트 폴더의 `.env.example`을 `.env`로 복사한 뒤, `.env`에 본인 API 키를 입력합니다.

```text
GEMINI_API_KEY=발급받은_API_키
GEMINI_MODEL=gemini-3.5-flash
```

`.env`는 `.gitignore`에 포함되어 있으므로 Git에는 올라가지 않습니다. 단, 화면 공유나 스크린샷에는 노출될 수 있으니 주의하세요.

그 다음부터는 프로젝트 폴더에서 아래 파일을 실행하면 됩니다.

```powershell
.\run_server.ps1
```

PowerShell 실행 정책 때문에 막히면 `run_server.bat`을 더블클릭하거나 아래처럼 실행하세요.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run_server.ps1
```

스크립트는 `.venv`가 없으면 자동으로 만들고, `requirements.txt` 설치 후 서버를 실행합니다.

## 확인 방법

서버 실행 후 아래 주소를 엽니다.

```text
http://127.0.0.1:8765/docs
```

`POST /temple/respond`에서 **Try it out**을 누르고 다음 요청을 실행합니다.

```json
{
  "uid": "youtube-001",
  "username": "별빛여행자",
  "message": "여신님, 오늘 전투에서 이길 수 있을까요?"
}
```

예상 응답 형태:

```json
{
  "priority": 72,
  "responseText": "승리는 정해진 운명이 아니니라. 그대의 의지로 길을 열어 보거라.",
  "emotion": "neutral",
  "uid": "youtube-001",
  "username": "별빛여행자"
}
```

답변 문장과 우선순위, 감정은 매번 달라질 수 있습니다.

## 자동 테스트

```powershell
pip install -r requirements-dev.txt
pytest -q
```

자동 테스트에서는 Gemini API를 호출하지 않으며 API 키도 필요하지 않습니다.

## API 계약

### 요청

| 필드 | 형식 | 제한 | 소유자 |
|---|---|---|---|
| `uid` | 문자열 | 1~128자 | 서버 입력 |
| `username` | 문자열 | 1~100자 | 서버 입력 |
| `message` | 문자열 | 1~200자 | 유저 입력 |

### 응답

| 필드 | 형식 | 설명 | 결정 주체 |
|---|---|---|---|
| `uid` | 문자열 | 원본 요청 식별자 | 서버 |
| `username` | 문자열 | 원본 표시 이름 | 서버 |
| `priority` | 정수 0~100 | 답변 우선순위 | AI |
| `responseText` | 문자열 1~70자, 최대 2문장 | 여신 답변 | AI |
| `emotion` | enum | 연출에 사용할 감정 | AI |

허용 감정:

```text
neutral, sorrowful, bestow, pleased, upset
```

## 현재 보안 수준

입력은 Gemini를 호출하기 전에 서버 코드에서 먼저 검사합니다. 성적 표현, 혐오, 폭력, 욕설, 현실 인물, 프롬프트 인젝션, URL, 광고 및 관련 없는 내용은 `422`로 차단됩니다. 점·공백·전각 문자를 이용한 대표적인 우회 표기도 정규화해서 검사합니다. 동일 사용자의 10초 내 8회 입력과 전체 채팅의 5초 내 동일 문장 3회 입력도 차단합니다.

한국어 욕설은 프로젝트 규칙과 `korcen` 사전을 함께 적용합니다. 원형 금칙어에 여러 구분 문자를 조합한 500개 이상의 변형 회귀 테스트를 유지하며, 반복 문자와 유니코드 표기도 정규화합니다.

모델 응답도 사용자에게 반환하기 전에 다시 검사합니다. 욕설, 성적·혐오 표현, 잔혹한 폭력 묘사, 현실 내용, 개발 및 AI 내부 사항이 발견되면 해당 응답을 노출하지 않습니다. 실제 YouTube 댓글을 연결하기 전에 다음 단계가 추가되어야 합니다.

- 문맥 기반 입력·출력 검열 강화
- 모델 응답 시간 제한과 재시도 정책
- 고정 안전 대사 fallback
- 요청 로그에서 개인정보 최소화

## 1단계 완료 조건

1. `/health`가 `status: ok`를 반환한다.
2. `/docs`에서 정상 질문을 전송할 수 있다.
3. 응답이 정의된 다섯 필드를 포함한다.
4. `uid`, `username`이 원본 요청과 일치한다.
5. `emotion`이 허용 목록 중 하나다.
6. 전체 응답 시간이 실제 목표에 들어오는지 기록한다.

이 조건을 통과하면 다음 단계에서 Unreal의 `FHttpModule`과 연결합니다.
