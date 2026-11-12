using UnrealBuildTool;

// Consumer test module: it sees only the plugin's public headers.
public class UnrealUtilitiesTests : ModuleRules
{
	public UnrealUtilitiesTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "EngineSettings", "HostTestSupport", "UnrealUtilities" });
	}
}
