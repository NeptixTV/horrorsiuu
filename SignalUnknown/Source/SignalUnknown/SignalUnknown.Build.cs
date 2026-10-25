using UnrealBuildTool;

public class SignalUnknown : ModuleRules
{
	public SignalUnknown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allows includes like "Core/SUTypes.h" from every file of the module.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings",
			"ProceduralMeshComponent",
			"CableComponent",
			"AIModule",
			"NavigationSystem",
			"AudioMixer",
			"ImageWrapper",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
			"NetCore"
		});
	}
}
