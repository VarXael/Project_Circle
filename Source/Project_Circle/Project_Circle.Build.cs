// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_Circle : ModuleRules
{
	public Project_Circle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// Core Runtime Modules
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"Niagara",
			"UMG" // Moved UMG here as it is often needed at runtime for HUDs
		});

		// Audio / Metasound
		PrivateDependencyModuleNames.AddRange(new string[] { 
			"MetasoundEngine", 
			"MetasoundFrontend", "InterchangeNodes"
		});

		// UI - Runtime
		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Slate", 
			"SlateCore" 
		});

		// EDITOR ONLY MODULES
		// These must be wrapped or the game will fail to package.
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { 
				"UnrealEd", 
				"AssetTools", 
				"Blutility", 
				"EditorStyle" 
			});
		}
	}
}