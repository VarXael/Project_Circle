// ==========================================
// FILE: Project_Circle.Target.cs
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle.Target.cs
// ==========================================

using UnrealBuildTool;
using System.Collections.Generic;

public class Project_CircleTarget : TargetRules
{
	public Project_CircleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		
		// FIX: Update to V6 to match UE 5.7 defaults. 
		// This fixes the "UndefinedIdentifierWarningLevel" error.
		DefaultBuildSettings = BuildSettingsVersion.V6;
		
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		ExtraModuleNames.Add("Project_Circle");
	}
}