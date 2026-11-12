using UnrealBuildTool;

public class AnimationDataToolsEditor : ModuleRules
{
	public AnimationDataToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AnimationDataTools" });
		PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry" });
	}
}
