// Respirant 2020.

using System;
using System.IO;
using UnrealBuildTool;

public class RuntimeAudioImporter : ModuleRules
{
    public RuntimeAudioImporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


        PublicDefinitions.AddRange(
            new string[] {
                "DR_WAV_IMPLEMENTATION=1",
                "DR_MP3_IMPLEMENTATION=1",
                "DR_FLAC_IMPLEMENTATION=1"
            }
            );

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));

        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
				// ... add other public dependencies that you statically link with here ...
			}
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );
    }
}
