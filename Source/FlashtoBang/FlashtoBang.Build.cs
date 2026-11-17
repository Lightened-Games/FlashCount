// Flash to Bang - game module.

using UnrealBuildTool;

public class FlashtoBang : ModuleRules
{
	public FlashtoBang(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "DeveloperSettings", "ProceduralMeshComponent", "EngineCameras"
		});

		// FactoryRecord (game-audio capture for the recorded showcase).
		PrivateDependencyModuleNames.Add("AudioMixer");

		// Editor-only helpers used by the content script (BlockingVolume brushes).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.Add(ModuleDirectory);
	}
}
