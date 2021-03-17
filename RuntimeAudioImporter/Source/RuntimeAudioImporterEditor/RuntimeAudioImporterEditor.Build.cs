// Georgy Treshchev 2021.

using UnrealBuildTool;

public class RuntimeAudioImporterEditor : ModuleRules
{
	public RuntimeAudioImporterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDefinitions.AddRange(
            new string[] {
	            "DR_MP3_IMPLEMENTATION=1"
            }
            );

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RuntimeAudioImporter"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd"
			}
			);
		
	}
}
