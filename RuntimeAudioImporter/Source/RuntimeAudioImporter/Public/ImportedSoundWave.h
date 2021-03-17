// Georgy Treshchev 2021.

#pragma once

#include "Sound/SoundWaveProcedural.h"
#include "ImportedSoundWave.generated.h"

/** PCM Data buffer structure */
struct FPCMStruct
{
	/** Interleaved 32-bit IEEE floating point PCM */
	uint8* PCMData;

	/** Number of PCM frames */
	uint64 PCMNumOfFrames;

	/** Raw PCM data size */
	uint32 PCMDataSize;
};

/**
 * The main sound wave class used to play imported audio from the Runtime Audio Importer
 */
UCLASS(BlueprintType, Category = "RuntimeAudioImporter")
class RUNTIMEAUDIOIMPORTER_API UImportedSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()
public:

	/**
	 * Release PCM data. After the SoundWave is no longer needed, you need to call this function to free memory
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	void ReleaseMemory();

	/**
	 * Rewind the sound for the specified time
	 *
	 * @param PlaybackTime How long to rewind the sound
	 * @return Whether the sound was rewound or not
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	bool RewindPlaybackTime(const float PlaybackTime);

	/**
	 * Change the current number of frames. Usually used to rewind the sound
	 *
	 * @param NumOfFrames The new number of frames from which to continue playing sound
	 * @return Whether the frames were changed or not
	 */
	UFUNCTION(Category = "RuntimeAudioImporter")
	bool ChangeCurrentFrameCount(const int32 NumOfFrames);

	/**
	 * Get the current sound playback time, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	float GetPlaybackTime() const;

	/**
	 * Get duration of a sound, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	float GetDuration() override;

	/**
	 * Get the current sound playback percentage, 0-100%
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	float GetPlaybackPercentage();

	/**
	 * Check if audio playback has finished or not
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	bool IsPlaybackFinished();

	/**
	 * Method for splitting sound into frames and a piece of of PCM data, and returning them to the audio render thread 
	 *
	 * @param OutAudio Retrieved PCM data array
	 * @param NumSamples Required number of samples for retrieval
	 * @return Number of retrieved samples (usually equals to NumSamples)
	 */
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	/**
	 * Getting the format of the retrieved PCM data.
	 *
	 * @note Since we are using Interleaved 32-bit IEEE floating point PCM, there will be no PCM transcoding in the engine, which will improve audio processing performance
	 */
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override;

	/**
	 * PCM buffer. Will be cleared when the object is destroyed
	 */
	FPCMStruct PCMBufferInfo;

protected:

	/**
	 * The current number of frames
	 */
	UPROPERTY() int32 CurrentNumOfFrames = 0;
};
