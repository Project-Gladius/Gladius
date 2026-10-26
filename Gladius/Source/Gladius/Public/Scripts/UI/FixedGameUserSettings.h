#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "FixedGameUserSettings.generated.h"

UCLASS(Config=GameUserSettings)
class GLADIUS_API UFixedGameUserSettings : public UGameUserSettings
{
 GENERATED_BODY()
public:
 virtual void SetToDefaults() override;
 virtual void LoadSettings(bool bForceReload = false) override;
 virtual void ValidateSettings() override;
 virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
private:
 void EnforceFixedWindow();
};
