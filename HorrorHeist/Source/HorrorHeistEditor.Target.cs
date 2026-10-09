using UnrealBuildTool;

public class HorrorHeistEditorTarget : TargetRules
{
	public HorrorHeistEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HorrorHeist");
	}
}
