// Georgy Treshchev 2024.

using UnrealBuildTool;
using System.IO;

public class RuntimeAudioImporter : ModuleRules
{
	public RuntimeAudioImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		// Change to toggle MetaSounds support
		bool bEnableMetaSoundSupport = false;

		// MetaSound is only supported in Unreal Engine version >= 5.3
		bEnableMetaSoundSupport &= (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 3) || Target.Version.MajorVersion > 5;

		// Disable if you are not using audio input capture
		bool bEnableCaptureInputSupport = true;
		
		// Disable if you are not using Voice Activity Detection
		bool bEnableVADSupport = true;

		// Bink format is only supported in Unreal Engine version >= 5
		bool bEnableBinkSupport = Target.Version.MajorVersion >= 5;
		
		// Whether to use dr_mp3 or minimp3 for MP3 decoding (minimp3 is used by default)
		// minimp3 is preferred since it supports LAME tags for more precise seeking and length information
		bool bUseDrMp3 = false;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"UEOgg",
			"Vorbis"
		);

		if (Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 4)
		{
			PrivateDependencyModuleNames.Add("VorbisAudioDecoder");
		}

		// This is necessary because the Vorbis module does not include the Unix-specific libvorbis encoder library
		if (Target.Platform != UnrealTargetPlatform.IOS && !Target.IsInPlatformGroup(UnrealPlatformGroup.Android) && Target.Platform != UnrealTargetPlatform.Mac && Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string VorbisLibPath = Path.Combine(Target.UEThirdPartySourceDirectory, "Vorbis", "libvorbis-1.3.2", "lib");
			PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, "Unix",
#if UE_5_2_OR_LATER
				Target.Architecture.LinuxName,
#else
				Target.Architecture,
#endif
				"libvorbisenc.a"));
		}
		
		PublicDefinitions.Add(string.Format("DR_MP3_IMPLEMENTATION={0}", bUseDrMp3 ? "1" : "0"));
		PublicDefinitions.Add(string.Format("MINIMP3_IMPLEMENTATION={0}", bUseDrMp3 ? "0" : "1"));

		PublicDefinitions.AddRange(
			new string[]
			{
				"DR_WAV_IMPLEMENTATION=1",
				"DR_FLAC_IMPLEMENTATION=1"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Core",
				"AudioPlatformConfiguration"
			}
		);

		if (Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 1)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"SignalProcessing"
				}
			);
		}

		if (Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 2)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioExtensions"
				}
			);
		}

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
		}

		PublicDefinitions.Add(string.Format("WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT={0}", (bEnableMetaSoundSupport ? "1" : "0")));

		if (bEnableCaptureInputSupport)
		{
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
			    Target.Platform == UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("AudioCaptureRtAudio");
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PrivateDependencyModuleNames.Add("AudioCaptureAudioUnit");
				PrivateDependencyModuleNames.Add("Core");
				PrivateDependencyModuleNames.Add("AudioCaptureCore");
				PublicFrameworks.AddRange(new string[] { "CoreAudio", "AVFoundation", "AudioToolbox" });
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"AudioCaptureAndroid",
						"AndroidPermission"
					}
				);

				string BuildPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(BuildPath, "RuntimeAudioImporter_AndroidAPL.xml"));
			}

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioMixer",
					"AudioCaptureCore"
				}
			);
		}

        PublicDefinitions.Add(string.Format("WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT={0}", (bEnableCaptureInputSupport ? "1" : "0")));

        if (bEnableVADSupport)
        {
	        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "libfvad", "include"));
	        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "libfvad", "src"));
        }

        PublicDefinitions.Add(string.Format("WITH_RUNTIMEAUDIOIMPORTER_VAD_SUPPORT={0}", (bEnableVADSupport ? "1" : "0")));

		if (bEnableBinkSupport)
		{
			PrivateDependencyModuleNames.Add("BinkAudioDecoder");
			
			PublicSystemIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Include"));

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "binka_ue_encode_win64_static.lib"));
			}

			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PublicAdditionalLibraries.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "libbinka_ue_encode_lnx64_static.a"));
			}

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				if (Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 1)
				{
					PublicAdditionalLibraries.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "libbinka_ue_encode_osx_static.a"));
				}
				else
				{
					PublicAdditionalLibraries.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "libbinka_ue_encode_osx64_static.a"));
				}
			}
		}

        PublicDefinitions.Add(string.Format("WITH_RUNTIMEAUDIOIMPORTER_BINK_DECODE_SUPPORT={0}", (bEnableBinkSupport ? "1" : "0")));
        PublicDefinitions.Add(string.Format("WITH_RUNTIMEAUDIOIMPORTER_BINK_ENCODE_SUPPORT={0}", (bEnableBinkSupport && (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac) ? "1" : "0")));
    }
}