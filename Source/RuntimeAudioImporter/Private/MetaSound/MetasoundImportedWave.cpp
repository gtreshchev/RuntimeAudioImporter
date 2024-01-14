// Georgy Treshchev 2024.

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "MetaSound/MetasoundImportedWave.h"
#include "RuntimeAudioImporterDefines.h"

namespace RuntimeAudioImporter
{
	const FString PluginAuthor = TEXT("Georgy Treshchev");
	const FText PluginNodeMissingPrompt = NSLOCTEXT("RuntimeAudioImporter", "DefaultMissingNodePrompt", "The node was likely removed, renamed, or the RuntimeAudioImporter plugin is not loaded.");

	FImportedWave::FImportedWave(const TUniquePtr<Audio::IProxyData>& InInitData)
	{
		if (InInitData.IsValid())
		{
			if (InInitData->CheckTypeCast<FSoundWaveProxy>())
			{
				UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved proxy data from Imported Sound Wave"));
				SoundWaveProxy = MakeShared<FSoundWaveProxy, ESPMode::ThreadSafe>(InInitData->GetAs<FSoundWaveProxy>());
			}
		}
	}
}
#endif
