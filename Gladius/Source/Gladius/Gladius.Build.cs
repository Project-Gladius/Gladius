// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gladius : ModuleRules
{
	public Gladius(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "DeveloperSettings" });

		// JsonUtilities: 플레이어 기록을 구조체 그대로 JSON으로 직렬화하는 데 필요
		PrivateDependencyModuleNames.AddRange(new string[] { "LevelSequence", "MovieScene", "HTTP", "Json", "JsonUtilities", "TempleAIClient", "FMODStudio" });

		// 유닛 머리 위 이름표(UMG)에 필요
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMGEditor" });
		}

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
