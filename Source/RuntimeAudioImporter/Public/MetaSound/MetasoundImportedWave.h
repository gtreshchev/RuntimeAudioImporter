// Georgy Treshchev 2024.

#pragma once

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "IAudioProxyInitializer.h"
#include "Sound/SoundWave.h"

namespace RuntimeAudioImporter
{
	extern const FString RUNTIMEAUDIOIMPORTER_API PluginAuthor;
	extern const FText RUNTIMEAUDIOIMPORTER_API PluginNodeMissingPrompt;

	/**
	 * FImportedWave is an alternative to FWaveAsset to hold proxy data obtained from UImportedSoundWave
	 */
	class RUNTIMEAUDIOIMPORTER_API FImportedWave
	{
		FSoundWaveProxyPtr SoundWaveProxy;

	public:
		FImportedWave() = default;
		FImportedWave(const FImportedWave&) = default;
		FImportedWave& operator=(const FImportedWave& Other) = default;

		FImportedWave(const TUniquePtr<Audio::IProxyData>& InInitData);

		bool IsSoundWaveValid() const
		{
			return SoundWaveProxy.IsValid();
		}

		const FSoundWaveProxyPtr& GetSoundWaveProxy() const
		{
			return SoundWaveProxy;
		}

		const FSoundWaveProxy* operator->() const
		{
			return SoundWaveProxy.Get();
		}

		FSoundWaveProxy* operator->()
		{
			return SoundWaveProxy.Get();
		}
	};
}

DECLARE_METASOUND_DATA_REFERENCE_TYPES(RuntimeAudioImporter::FImportedWave, RUNTIMEAUDIOIMPORTER_API, FImportedWaveTypeInfo, FImportedWaveReadRef, FImportedWaveWriteRef)
#endif
