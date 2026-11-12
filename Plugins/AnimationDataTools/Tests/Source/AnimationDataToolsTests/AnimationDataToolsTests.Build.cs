using UnrealBuildTool;

// Consumer test module for the AnimationDataTools runtime module: it sees only public headers.
public class AnimationDataToolsTests : ModuleRules
{
	public AnimationDataToolsTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HostTestSupport", "AnimationDataTools" });
	}
}
