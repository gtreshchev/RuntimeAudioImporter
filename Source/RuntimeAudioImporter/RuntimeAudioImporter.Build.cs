// Georgy Treshchev 2023.

using UnrealBuildTool;

public class RuntimeAudioImporter : ModuleRules
{
	public RuntimeAudioImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		// Change to toggle MetaSounds support
		bool bEnableMetaSoundSupport = true;

		// MetaSound is only supported in Unreal Engine version >= 5.2
		bEnableMetaSoundSupport &= (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 2) || Target.Version.MajorVersion > 5;

		// Disable if you are not using audio input capture
		bool bEnableCaptureInputSupport = true;

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

		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 2)
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
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("AudioCaptureAndroid");
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
	}
}