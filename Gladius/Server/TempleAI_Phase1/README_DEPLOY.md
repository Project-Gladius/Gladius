# Temple AI Remote Deployment

로컬에서 검증한 Python 서버를 다른 PC, 모바일 기기, 또는 클라우드에서 접근할 수 있게 만드는 단계입니다.

## 현재 구조

```text
Unity Client
  -> POST /temple/respond
Python FastAPI Server
  -> Gemini API
```

API 키는 Unity에 넣지 않습니다. Unity는 Python 서버 주소만 알고, Python 서버만 Gemini API 키를 가집니다.

## 같은 와이파이에서 테스트

서버 PC에서 Phase 1 폴더를 열고 다음 명령으로 실행합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run_server.ps1 -HostAddress 0.0.0.0 -Port 8000
```

서버 PC의 내부 IP를 확인합니다.

```powershell
ipconfig
```

예를 들어 서버 PC의 IPv4 주소가 `192.168.0.12`라면 Unity의 `Endpoint Url`을 아래처럼 바꿉니다.

```text
http://192.168.0.12:8000/temple/respond
```

Windows 방화벽에서 Python 또는 포트 `8000` 접근 허용이 필요할 수 있습니다.

## 다른 네트워크에서 빠르게 테스트

같은 와이파이가 아닌 기기에서 테스트하려면 터널을 사용합니다.

```powershell
.\run_tunnel_cloudflare.ps1
```

또는:

```powershell
.\run_tunnel_ngrok.ps1
```

터널이 출력하는 `https://...` 주소 뒤에 `/temple/respond`를 붙여 Unity의 `Endpoint Url`로 넣습니다.

자세한 절차는 `README_TUNNEL.md`를 참고합니다.

## 간단한 접속 토큰

서버를 다른 기기에서 접근 가능하게 열면 아무나 요청을 보낼 수 있습니다. 빠른 테스트용으로 `.env`에 아래 값을 추가할 수 있습니다.

```text
TEMPLE_CLIENT_TOKEN=임의의_긴_문자열
```

이 값이 설정되면 `/temple/respond`는 `X-Temple-Client-Token` 헤더가 맞는 요청만 허용합니다.

Unity의 `TempleAIClient` 컴포넌트에서도 `Client Token`에 같은 값을 넣습니다.

이 토큰은 배포된 Unity 클라이언트에서 완전히 숨길 수 있는 비밀키는 아닙니다. 공개 테스트를 막는 최소 문지기 정도로만 사용하고, 실제 서비스 단계에서는 계정/세션/서명/요청 제한이 추가되어야 합니다.

## Docker 배포

Docker 기반 호스트에서는 이 폴더를 서버 프로젝트로 올린 뒤 아래 환경변수를 설정합니다.

| 변수 | 필수 | 설명 |
|---|---:|---|
| `GEMINI_API_KEY` | 예 | Gemini API 키 |
| `GEMINI_MODEL` | 아니오 | 예: `gemini-3.5-flash` |
| `TEMPLE_CLIENT_TOKEN` | 권장 | Unity에서 보낼 간단한 접속 토큰 |
| `ALLOWED_ORIGINS` | 선택 | WebGL/browser CORS 허용 출처 |
| `PORT` | 호스트 제공 | 서버 포트. 없으면 `8000` |

이미 포함된 `Dockerfile`은 아래 명령을 실행합니다.

```text
uvicorn app.main:app --host 0.0.0.0 --port ${PORT:-8000}
```

클라우드 배포 후 Unity의 `Endpoint Url`은 배포 URL로 바꿉니다.

```text
https://your-deployed-server.example.com/temple/respond
```

## WebGL 주의

Unity Editor, Windows 빌드, Android 빌드는 일반적으로 CORS의 영향을 받지 않습니다. Unity WebGL처럼 브라우저에서 실행되는 클라이언트는 CORS가 필요할 수 있습니다.

테스트 중에는 아래처럼 열 수 있습니다.

```text
ALLOWED_ORIGINS=*
```

실제 배포에서는 `*` 대신 웹 페이지 도메인을 명시하는 편이 안전합니다.
