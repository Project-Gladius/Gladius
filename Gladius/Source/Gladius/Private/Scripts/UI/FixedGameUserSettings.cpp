#include "Scripts/UI/FixedGameUserSettings.h"

void UFixedGameUserSettings::EnforceFixedWindow()
{
 SetScreenResolution(FIntPoint(1920,1080));
 SetFullscreenMode(EWindowMode::Windowed);
 ConfirmVideoMode();
}
void UFixedGameUserSettings::SetToDefaults()
{
 Super::SetToDefaults(); EnforceFixedWindow();
}
void UFixedGameUserSettings::LoadSettings(bool bForceReload)
{
 Super::LoadSettings(bForceReload); EnforceFixedWindow();
}
void UFixedGameUserSettings::ValidateSettings()
{
 Super::ValidateSettings(); EnforceFixedWindow();
}
void UFixedGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
 EnforceFixedWindow(); Super::ApplySettings(false);
}
