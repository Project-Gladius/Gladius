# TempleAI Windows 서버 실행 가이드

## 1. 이 컴퓨터에서 CMD 창으로 실행

`Server\TempleAI_Phase1` 폴더의 `run_server.cmd`를 더블클릭한다.

CMD에서 직접 실행하려면 다음 명령을 사용한다.

```cmd
cd /d "C:\Users\user\Documents\GitHub\Gladius\Gladius\Server\TempleAI_Phase1"
run_server.cmd
```

`Uvicorn running on http://127.0.0.1:8765`가 나오면 정상이다. 서버를 사용하는 동안 창을 닫지 않고, 종료할 때는 `Ctrl+C`를 누른다.

## 2. 서버 폴더를 다른 Windows PC로 옮겨 실행

1. 다른 PC에 64비트 Python 3.12 이상을 설치한다. 설치 화면에서 `Add Python to PATH`와 Python Launcher를 활성화한다.
2. `Server\TempleAI_Phase1` 폴더를 통째로 복사한다.
3. `.venv`, `.env`, `server.stdout.log`, `server.stderr.log`는 복사하지 않는다.
4. 새 PC에서 `.env.example`을 복사해 이름을 `.env`로 바꾼다.
5. 새 `.env`에 그 PC에서 사용할 `GEMINI_API_KEY`와 충분히 긴 `TEMPLE_CLIENT_TOKEN`을 입력한다.
6. 외부 접속이 필요 없으면 `run_server.cmd`를 실행한다.
7. 같은 공유기 안의 다른 PC나 Unreal에서 접속해야 하면 `run_server_lan.cmd`를 실행한다.

첫 실행에서는 가상환경 생성과 패키지 설치 때문에 시간이 걸릴 수 있다.

## 3. 같은 네트워크의 다른 PC에서 접속

서버 PC에서 `run_server_lan.cmd`를 실행한다. Windows 방화벽 창이 나오면 개인 네트워크만 허용한다.

서버 PC의 CMD에서 `ipconfig`를 실행해 IPv4 주소를 확인한다. 예를 들어 주소가 `192.168.0.25`라면 다른 PC의 확인 주소는 다음과 같다.

```text
http://192.168.0.25:8765/health
```

Unreal의 Endpoint URL은 다음과 같이 설정한다.

```text
http://192.168.0.25:8765/temple/respond
```

Unreal이 보내는 `X-Temple-Client-Token` 값은 서버 PC의 `.env`에 있는 `TEMPLE_CLIENT_TOKEN`과 같아야 한다.

방화벽 연결이 안 될 때만 관리자 PowerShell에서 다음 규칙을 추가한다.

```powershell
New-NetFirewallRule -DisplayName "TempleAI 8765 Private" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 8765 -Profile Private
```

## 4. 인터넷 밖에서 접속할 때

공유기의 8765 포트를 그대로 전달하지 않는다. 이 서버는 직접 인터넷에 공개할 TLS, 사용자 계정, 충분한 요청량 제한 체계를 갖추지 않았다.

다른 장소의 PC에서 접속해야 한다면 두 PC를 Tailscale 같은 사설 VPN에 연결하거나, 인증과 HTTPS를 제공하는 리버스 프록시 뒤에 서버를 둔다. 이때도 `TEMPLE_CLIENT_TOKEN`을 반드시 사용한다.

## 5. 문제 해결

- `WinError 10048`: 서버가 이미 실행 중이다. 기존 서버를 종료하거나 그대로 사용한다.
- `GEMINI_API_KEY is missing`: `.env`가 `run_server.ps1`과 같은 폴더에 있는지 확인한다.
- `py`를 찾을 수 없음: Python을 다시 설치하면서 Python Launcher와 PATH 옵션을 활성화한다.
- 외부 PC에서 연결되지 않음: `run_server_lan.cmd`, IPv4 주소, 같은 네트워크 여부와 개인 네트워크 방화벽을 확인한다.
- `401 Invalid client token`: Unreal과 서버의 토큰 값이 다르다.
