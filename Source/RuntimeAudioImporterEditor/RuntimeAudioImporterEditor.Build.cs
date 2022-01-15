// Georgy Treshchev 2022.

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
				"RuntimeAudioImporter",
				"CoreUObject",
				"Engine"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd"
			}
		);
		
	}
}
