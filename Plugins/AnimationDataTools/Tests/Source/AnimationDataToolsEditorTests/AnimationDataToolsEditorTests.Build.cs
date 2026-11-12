using UnrealBuildTool;

// Consumer test module for the AnimationDataToolsEditor module: it sees only public headers.
public class AnimationDataToolsEditorTests : ModuleRules
{
	public AnimationDataToolsEditorTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "UnrealEd", "Blutility", "Json", "JsonUtilities", "HostTestSupport", "AnimationDataTools", "AnimationDataToolsEditor" });
	}
}
