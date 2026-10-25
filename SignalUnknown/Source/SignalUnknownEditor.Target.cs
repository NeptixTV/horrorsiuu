using UnrealBuildTool;
using System.Collections.Generic;

public class SignalUnknownEditorTarget : TargetRules
{
	public SignalUnknownEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SignalUnknown");
	}
}
