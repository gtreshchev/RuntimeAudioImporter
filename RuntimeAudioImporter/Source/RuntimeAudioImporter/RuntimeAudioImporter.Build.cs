// Georgy Treshchev 2021.
using System;
using System.IO;
using UnrealBuildTool;

public class RuntimeAudioImporter : ModuleRules
{
	public RuntimeAudioImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//PublicDefinitions.Add("WITH_OGGVORBIS=0");

		PublicDefinitions.AddRange(
            new string[] {
                "DR_WAV_IMPLEMENTATION=1",
                "DR_MP3_IMPLEMENTATION=1",
                "DR_FLAC_IMPLEMENTATION=1"
            }
            );


        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AudioPlatformConfiguration"	
			}
			);
		
		
		
		
		
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"libOpus"
			);
		
	
		PublicDefinitions.Add("WITH_OGGVORBIS");
	}
}
