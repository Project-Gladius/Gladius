# Temple AI External Network Test

같은 와이파이가 아닌 PC, 노트북, 모바일 기기에서 로컬 Python 서버에 붙이고 싶다면 터널을 사용합니다.

터널은 아래처럼 동작합니다.

```text
Unity on another network
  -> public https tunnel URL
  -> your local PC
  -> http://localhost:8000
  -> Temple AI Python server
```

이 방식은 빠른 외부 테스트용입니다. 서버를 켠 PC와 터널 프로그램이 계속 켜져 있어야 하며, 임시 URL은 실행할 때마다 바뀔 수 있습니다.

## 1. Python 서버 실행

먼저 기존 서버를 켭니다.

```powershell
.\run_server.bat
```

브라우저에서 아래 주소가 열리면 정상입니다.

```text
http://127.0.0.1:8000/docs
```

## 2A. Cloudflare Quick Tunnel

`cloudflared`를 설치한 뒤, 새 PowerShell 창에서 Phase 1 폴더로 이동하고 실행합니다.

```powershell
.\run_tunnel_cloudflare.ps1
```

또는 직접 실행:

```powershell
cloudflared tunnel --url http://localhost:8000
```

터미널에 아래와 비슷한 주소가 출력됩니다.

```text
https://example-random-name.trycloudflare.com
```

Unity의 `Endpoint Url`은 뒤에 `/temple/respond`를 붙여서 사용합니다.

```text
https://example-random-name.trycloudflare.com/temple/respond
```

## 2B. ngrok

`ngrok`를 설치하고 로그인/토큰 설정을 마친 뒤, 새 PowerShell 창에서 Phase 1 폴더로 이동하고 실행합니다.

```powershell
.\run_tunnel_ngrok.ps1
```

또는 직접 실행:

```powershell
ngrok http 8000
```

ngrok가 출력하는 `https://...` 주소 뒤에 `/temple/respond`를 붙여 Unity의 `Endpoint Url`로 사용합니다.

## 3. Client Token 사용

외부 네트워크에 열 때는 `.env`에 토큰을 넣는 것을 권장합니다.

```text
TEMPLE_CLIENT_TOKEN=임의의_긴_문자열
```

Unity의 `TempleAIClient` 컴포넌트에도 같은 값을 `Client Token`에 넣습니다.

## 4. 어떤 방식을 써야 하나

| 방식 | 용도 | 장점 | 단점 |
|---|---|---|---|
| Cloudflare Quick Tunnel | 빠른 외부 테스트 | 계정 없이도 임시 테스트 가능 | URL이 임시이며 PC가 켜져 있어야 함 |
| ngrok | 빠른 외부 테스트 | 설정과 대시보드가 편함 | 계정/토큰 설정이 필요할 수 있음 |
| Docker cloud deploy | 장기 테스트/실서비스 전 단계 | PC를 꺼도 서버 유지 가능 | 배포 서비스 설정이 필요 |

처음은 터널로 확인하고, 장기 테스트가 필요해지면 `README_DEPLOY.md`의 Docker 배포로 넘어갑니다.
