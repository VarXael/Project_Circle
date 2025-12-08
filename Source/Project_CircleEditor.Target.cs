// ==========================================
// FILE: Project_CircleEditor.Target.cs
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_CircleEditor.Target.cs
// ==========================================

using UnrealBuildTool;
using System.Collections.Generic;

public class Project_CircleEditorTarget : TargetRules
{
	public Project_CircleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		
		// FIX: Update to V6
		DefaultBuildSettings = BuildSettingsVersion.V6;
		
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		ExtraModuleNames.Add("Project_Circle");
	}
}