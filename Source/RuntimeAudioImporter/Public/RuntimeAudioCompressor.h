// Georgy Treshchev 2022.

#pragma once

#include "ImportedSoundWave.h"
#include "RuntimeAudioImporterTypes.h"
#include "RuntimeAudioCompressor.generated.h"

/** Static delegate broadcast the compressed sound wave */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSoundWaveCompressedResultNative, bool bSuccess, USoundWave* SoundWaveRef);

/** Dynamic delegate broadcast the compressed sound wave */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoundWaveCompressedResult, bool, bSuccess, USoundWave*, SoundWaveRef);

/**
 * Runtime Audio Compressor
 * Utilized for sound compression
 */
UCLASS(BlueprintType, Category = "Runtime Audio Importer")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioCompressor : public UObject
{
	GENERATED_BODY()

public:
	/** Bind to know when audio compression is complete (even if it fails). Recommended for C++ only */
	FOnSoundWaveCompressedResultNative OnResultNative;
	
	/** Bind to know when audio compression is complete (even if it fails). Recommended for Blueprints only */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer|Delegates")
	FOnSoundWaveCompressedResult OnResult;

	/**
	 * Instantiates a RuntimeAudioCompressor object
	 *
	 * @return The RuntimeAudioCompressor object. Bind to it's OnResult delegate
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, Compress"), Category = "Runtime Audio Importer")
	static URuntimeAudioCompressor* CreateRuntimeAudioCompressor();

	/**
	 * Compress ImportedSoundWave to regular SoundWave. This greatly reduces the size of the audio data in memory and can improve performance
	 *
	 * @param ImportedSoundWaveRef Reference to the imported sound wave
	 * @param CompressedSoundWaveInfo Basic information for filling a sound wave (partially taken from the standard Sound Wave asset)
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @param bFillCompressedBuffer Whether to fill the compressed buffer. It is supposed to be true to reduce memory
	 * @param bFillPCMBuffer Whether to fill PCM buffer. Mainly used for in-engine previews. It is recommended not to enable to save memory
	 * @param bFillRAWWaveBuffer Whether to fill RAW Wave buffer. It is recommended not to enable to save memory
	 *
	 * @note Some unique features will be missing, such as the "OnGeneratePCMData" delegate. But at the same time, you do not need to manually rewind the sound wave through "RewindPlaybackTime", but use traditional methods
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	void CompressSoundWave(UImportedSoundWave* ImportedSoundWaveRef, FCompressedSoundWaveInfo CompressedSoundWaveInfo, uint8 Quality, bool bFillCompressedBuffer, bool bFillPCMBuffer, bool bFillRAWWaveBuffer);

private:
	/**
	 * Audio compression finished callback
	 * 
	 * @param SoundWaveRef Reference to the compressed sound wave
	 */
	void BroadcastResult(USoundWave* SoundWaveRef);
};
