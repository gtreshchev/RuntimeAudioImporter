// Georgy Treshchev 2022.

using System;
using System.IO;
using UnrealBuildTool;

public class RuntimeAudioImporter : ModuleRules
{
	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/")); }
	}

	public RuntimeAudioImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"UEOgg",
			"Vorbis",
			"libOpus"
		);

		PublicDefinitions.AddRange(
			new string[]
			{
				"DR_WAV_IMPLEMENTATION=1",
				"DR_MP3_IMPLEMENTATION=1",
				"DR_FLAC_IMPLEMENTATION=1"
			}
		);

		PrivateIncludePaths.Add(ThirdPartyPath);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Core"
			}
		);

#if !UE_5_00_OR_LATER
		PrivateDependencyModuleNames.Add("AudioPlatformConfiguration");
#endif
	}
}