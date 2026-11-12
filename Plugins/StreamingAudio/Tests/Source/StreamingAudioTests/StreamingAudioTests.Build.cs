using UnrealBuildTool;

// Consumer test module: it sees only the plugin's public headers.
public class StreamingAudioTests : ModuleRules
{
	public StreamingAudioTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "HostTestSupport", "StreamingAudio" });
	}
}
