// Georgy Treshchev 2024.

#pragma once

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "Misc/EngineVersionComparison.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "IAudioProxyInitializer.h"
#include "Sound/SoundWave.h"

namespace RuntimeAudioImporter
{
#if UE_VERSION_OLDER_THAN(5, 2, 0)
	using FProxyDataPtrType = TUniquePtr<Audio::IProxyData>;
#else
	using FProxyDataPtrType = TSharedPtr<Audio::IProxyData>;
#endif
	
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

		FImportedWave(const FProxyDataPtrType& InInitData);

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

		friend FORCEINLINE uint32 GetTypeHash(const FImportedWave& InImportedWave)
		{
			return GetTypeHash(InImportedWave.SoundWaveProxy);
		}
	};
}

DECLARE_METASOUND_DATA_REFERENCE_TYPES(RuntimeAudioImporter::FImportedWave, RUNTIMEAUDIOIMPORTER_API, FImportedWaveTypeInfo, FImportedWaveReadRef, FImportedWaveWriteRef)
#endif
