// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_Circle : ModuleRules
{
	public Project_Circle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "MetasoundEngine", "MetasoundFrontend" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EditorStyle", "Blutility", "UMG", "UnrealEd", "AssetTools" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayAbilities", "GameplayTags", "GameplayTasks" });
		
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
