using UnrealBuildTool;

public class HorrorHeistTarget : TargetRules
{
	public HorrorHeistTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HorrorHeist");
	}
}
