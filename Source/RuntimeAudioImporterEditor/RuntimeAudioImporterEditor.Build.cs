// Georgy Treshchev 2024.

using UnrealBuildTool;

public class RuntimeAudioImporterEditor : ModuleRules
{
	public RuntimeAudioImporterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		// Change to toggle MetaSounds support
		bool bEnableMetaSoundSupport = false;

		// MetaSound is only supported in Unreal Engine version >= 5.3
		bEnableMetaSoundSupport &= (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 3) || Target.Version.MajorVersion > 5;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
		
		// To access settings for adding the permissions needed for importing/exporting audio files
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AndroidRuntimeSettings",
				"IOSRuntimeSettings"
			}
		);

		if (bEnableMetaSoundSupport)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MetasoundGraphCore",
					"MetasoundFrontend",
					"MetasoundEditor"
				}
			);
		}
	}
}