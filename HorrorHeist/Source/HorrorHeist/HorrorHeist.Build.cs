using UnrealBuildTool;

public class HorrorHeist : ModuleRules
{
	public HorrorHeist(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Sub-folders are included relative to the module root, e.g. #include "Player/HHCharacter.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"ApplicationCore",
			"AudioMixer",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"RenderCore",
			"EngineSettings",
			"CoreOnline"
		});

		// Voice capture / VoIP (push-to-talk) lives in the online subsystem utils.
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
	}
}
