#include "Scripts/Time/RealTimeSettings.h"
#include "Engine/World.h"

bool URealTimeSettings::MatchesMap(const UWorld* World, const TSoftObjectPtr<UWorld>& Map)
{
    if (!World || Map.IsNull()) return false;
    FString Package = World->GetOutermost()->GetName();
    // PIE prefixes only the final package name, not its directory.
    if (!World->StreamingLevelsPrefix.IsEmpty())
        Package.ReplaceInline(*World->StreamingLevelsPrefix, TEXT(""));
    return Package == Map.ToSoftObjectPath().GetLongPackageName();
}
