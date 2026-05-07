using UnrealBuildTool;

public class TaskEvidence : ModuleRules
{
	public TaskEvidence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Json"
		});
	}
}
