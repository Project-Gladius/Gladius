<!--
    오류 대응 매뉴얼
    26.08.08 파일 생성 및 빌드 실패 관련 내용 작성
-->

# 오류 시 대응 매뉴얼

## 기본적인 프로젝트 세팅

다음의 사항은 기본적으로 지켜져야 하며, 변경될 일도 없습니다.

### Unreal Editor 5.8.x 버전을 사용

패치 버전 변경은 문제없을 것으로 보이나, 마이너 버전이 다르면 문제가 생길 수 있습니다. 쉽게 말해 맨 끝 숫자는 상관없고 5.8이기만 하면 됩니다.

### Visual Studio 2022 및 관련 플러그인 설치

Visual Studio 설치 시, 혹은 이미 설치한 경우 Visual Studio Installer을 실행 및 `수정(M)`을 눌러 `C++를 사용한 게임 개발`이란 이름의 워크로드가 설치(체크)되어 있는지 확인해 주세요.

### 저장소 최신화 여부 수시 확인

git 저장소와 현재 로컬 저장소의 프로젝트가 동기화된 상태인지 항상 확인해 주세요. 프로젝트 실행엔 문제가 없을 수는 있지만 동기화하지 않은 상태에서 작업하시다간 작업 내용을 전부 잃어버릴 수 있습니다.

### 기타 문제가 생길 수도 있는 것

- 프로젝트 경로에 한글이 존재하면 오류가 생길 수 있습니다. 사용자명도 포함되니 주의해주세요. 예시: `C:\Users\(사용자명)\Documents\GitHub\Gladius`
- 다른 버전의 Visual Studio가 중복 설치되어 있는 경우 제거해주세요.

## 이외 오류

### 빌드 실패 관련

1. `Gladius could not be compiled. Try rebuilding from source manually.` 와 같이 불친절한 오류가 뜬 경우.

컴파일을 시작은 했으나 실패했음을 의미합니다. 컴파일조차 안 되는 경우 SDK 관련 문구가 뜰 텐데, Visual Studio 및 워크로드 설치 상태를 확인해 주세요.

저장소 최신화 상태를 확인한 뒤, 다음의 커맨드를 터미널에서 입력하거나:

```
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" GladiusEditor Win64 Development -Project="<본인 경로>/Gladius/Gladius.uproject" -WaitMutex`
```

다음의 위치에서 로그 파일을 찾아보세요.

```
%LOCALAPPDATA%\UnrealBuildTool\Log.txt
```

무슨 오류인지 표시가 될 텐데, 분석이 어려우신 경우
AI의 도움을 받아보시거나, 이형준에게 연락해 주시면 최대한 도와드리겠습니다.
