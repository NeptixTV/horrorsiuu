using UnrealBuildTool;
using System.Collections.Generic;

public class SignalUnknownTarget : TargetRules
{
	public SignalUnknownTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SignalUnknown");
	}
}
