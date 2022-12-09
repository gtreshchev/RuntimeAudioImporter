// Georgy Treshchev 2022.

using UnrealBuildTool;

public class RuntimeAudioImporter : ModuleRules
{
	public RuntimeAudioImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		// Change to toggle MetaSounds support
		bool bEnableMetaSoundSupport = false;

		// MetaSound is only supported in Unreal Engine version >= 5.2
		bEnableMetaSoundSupport &= (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 2) || Target.Version.MajorVersion > 5;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"UEOgg",
			"Vorbis"
		);

		PublicDefinitions.AddRange(
			new string[]
			{
				"DR_WAV_IMPLEMENTATION=1",
				"DR_MP3_IMPLEMENTATION=1",
				"DR_FLAC_IMPLEMENTATION=1"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Core"
			}
		);

		if (bEnableMetaSoundSupport)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MetasoundEngine",
					"MetasoundFrontend",
					"MetasoundGraphCore"
				}
			);

			PublicDefinitions.Add("WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT=0");
		}
	}
}