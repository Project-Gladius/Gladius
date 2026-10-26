#pragma once

#include "CoreMinimal.h"

// 여러 YouTube API 키를 돌려 쓰는 목록
// 지금 키의 하루 할당량이 바닥나면(또는 키가 잘못됐으면) 다음 키로 넘어가고,
// 바닥난 키는 다음 할당량 초기화 시각(미국 태평양 자정, 넉넉히 08:00 UTC)이 지나면 다시 씀
// 키는 서로 다른 Google Cloud 프로젝트에서 만들어야 함 (같은 프로젝트의 키는 할당량을 같이 씀)
struct GLADIUS_API FYouTubeApiKeyPool
{
	//키 파일 위치: Config/YouTubeApiKeys.txt (한 줄에 키 하나, #으로 시작하는 줄은 주석)
	static FString GetDefaultKeyFilePath();

	//파일에서 키를 읽어 목록 뒤에 추가. 읽은 키 수 반환
	int32 LoadFromFile(const FString& Path);

	//키 하나를 목록 뒤에 추가 (빈 값/중복은 무시)
	void AddKey(const FString& Key);
	bool ReplaceKeys(const TArray<FString>& InKeys);

	//지금 쓸 수 있는 키. 모두 바닥났으면 빈 문자열
	FString GetCurrentKey(const FDateTime& NowUtc);

	//Key가 할당량 초과/사용 불가라고 표시하고 다음 키로 넘어감. 쓸 수 있는 키가 남아 있으면 true
	bool MarkExhausted(const FString& Key, const FDateTime& NowUtc);

	int32 Num() const { return Keys.Num(); }
	int32 NumAvailable(const FDateTime& NowUtc) const;

	//Marked 이후 첫 할당량 초기화 시각
	static FDateTime NextQuotaReset(const FDateTime& MarkedUtc);

	//로그용: 앞 8자리만 보여줌
	static FString Mask(const FString& Key);

private:
	TArray<FString> Keys;
	TMap<FString, FDateTime> ExhaustedUntil;
	int32 CurrentIndex = 0;

	bool IsAvailable(const FString& Key, const FDateTime& NowUtc) const;
};
