// Georgy Treshchev 2021.

#pragma once

#include "Sound/SoundWaveProcedural.h"
#include "ImportedSoundWave.generated.h"

/**  */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAudioPlaybackFinished);

/** PCM Data buffer structure */
USTRUCT()
struct FPCMStruct
{
	GENERATED_BODY()

	/** 32-bit float PCM data */
	uint8* PCMData;

	/** Number of PCM frames */
	uint64 PCMNumOfFrames;

	/** PCM data size */
	uint32 PCMDataSize;

	/** Base constructor */
	FPCMStruct()
		: PCMData(nullptr), PCMNumOfFrames(0), PCMDataSize(0)
	{
	}
};

/**
 * The main sound wave class used to play imported audio from the Runtime Audio Importer
 */
UCLASS(BlueprintType, Category = "Imported Sound Wave")
class RUNTIMEAUDIOIMPORTER_API UImportedSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()
public:
	/**
	 * Bind to this delegate to know when the audio playback is finished
	 */
	UPROPERTY(BlueprintAssignable, Category = "Imported Sound Wave")
	FOnAudioPlaybackFinished OnAudioPlaybackFinished;

	/**
	 * Bool to control the behaviour of the OnAudioPlaybackFinished delegate
	 */
	UPROPERTY()
	bool PlaybackFinishedBroadcast = false;

	/**
	 * Begin Destroy override method
	 */
	virtual void BeginDestroy() override;

	/**
	 * Release PCM data. After the SoundWave is no longer needed, you need to call this function to free memory
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave")
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
	bool ChangeCurrentFrameCount(const int32 NumOfFrames);

	/**
	 * Get the current sound wave playback time, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave")
	float GetPlaybackTime() const;

	/**
	 * Get the length of the sound wave, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave")
	virtual float GetDuration() override;

	/**
	 * Get the current sound playback percentage, 0-100%
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave")
	float GetPlaybackPercentage();

	/**
	 * Sampling Rate (samples per second)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Imported Sound Wave")
	int32 SamplingRate;

	/**
	 * Check if audio playback has finished or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave")
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
	 * @note Since we are using 32-bit float, there will be no PCM transcoding in the engine, which will improve audio processing performance
	 */
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override;

	/**
	 * PCM buffer. Will be cleared when the object is destroyed
	 */
	UPROPERTY()
	FPCMStruct PCMBufferInfo;

protected:
	/**
	 * The current number of frames
	 */
	UPROPERTY()
	int32 CurrentNumOfFrames = 0;
};
