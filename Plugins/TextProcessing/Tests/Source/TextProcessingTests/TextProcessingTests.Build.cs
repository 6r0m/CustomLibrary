using UnrealBuildTool;

// Consumer test module: it sees only the plugin's public headers.
public class TextProcessingTests : ModuleRules
{
	public TextProcessingTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "HostTestSupport", "TextProcessing" });
	}
}
