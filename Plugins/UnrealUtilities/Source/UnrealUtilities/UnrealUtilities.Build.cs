using UnrealBuildTool;

public class UnrealUtilities : ModuleRules
{
	public UnrealUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// GeneralProjectSettings, read by the project version function.
		PrivateDependencyModuleNames.Add("EngineSettings");
	}
}
