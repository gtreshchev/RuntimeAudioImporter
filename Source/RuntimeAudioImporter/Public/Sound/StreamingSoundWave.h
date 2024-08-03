// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "ImportedSoundWave.h"
#include "Delegates/Delegate.h"
#include "Containers/Queue.h"
#include "StreamingSoundWave.generated.h"

class URuntimeVoiceActivityDetector;

/** Static delegate broadcast the result of audio data pre-allocation */
DECLARE_DELEGATE_OneParam(FOnPreAllocateAudioDataResultNative, bool);

/** Dynamic delegate broadcast the result of audio data pre-allocation */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnPreAllocateAudioDataResult, bool, bSucceeded);

/**
 * Streaming sound wave. Can append audio data dynamically, including during playback.
 * It will live indefinitely, even if the sound wave has finished playing, until SetStopSoundOnPlaybackFinish is called.
 * Audio data is always accumulated, clear memory manually via ReleaseMemory or ReleasePlayedAudioData if necessary.
 */
UCLASS(BlueprintType, Category = "Streaming Sound Wave")
class RUNTIMEAUDIOIMPORTER_API UStreamingSoundWave : public UImportedSoundWave
{
	GENERATED_BODY()

public:
	UStreamingSoundWave(const FObjectInitializer& ObjectInitializer);

	/**
	 * Create a new instance of the streaming sound wave
	 *
	 * @return Created streaming sound wave
	 */
	UFUNCTION(BlueprintCallable, Category = "Streaming Sound Wave|Main")
	static UStreamingSoundWave* CreateStreamingSoundWave();

	/**
	 * Pre-allocate PCM data, to avoid reallocating memory each time audio data is appended
	 *
	 * @param NumOfBytesToPreAllocate Number of bytes to pre-allocate
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Streaming Sound Wave|Allocation")
	void PreAllocateAudioData(int64 NumOfBytesToPreAllocate, const FOnPreAllocateAudioDataResult& Result);

	/**
	 * Pre-allocate PCM data, to avoid reallocating memory each time audio data is appended. Suitable for use in C++
	 *
	 * @param NumOfBytesToPreAllocate Number of bytes to pre-allocate
	 * @param Result Delegate broadcasting the result
	 */
	void PreAllocateAudioData(int64 NumOfBytesToPreAllocate, const FOnPreAllocateAudioDataResultNative& Result);

	/**
	 * Append audio data to the end of existing data from encoded audio data
	 *
	 * @param AudioData Audio data array
	 * @param AudioFormat Audio format
	 */
	UFUNCTION(BlueprintCallable, Category = "Streaming Sound Wave|Append")
	void AppendAudioDataFromEncoded(TArray<uint8> AudioData, ERuntimeAudioFormat AudioFormat);

	/**
	 * Append audio data to the end of existing data from RAW audio data
	 *
	 * @param RAWData RAW audio buffer
	 * @param RAWFormat RAW audio format
	 * @param InSampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Append Audio Data From RAW"), Category = "Streaming Sound Wave|Append")
	void AppendAudioDataFromRAW(UPARAM(DisplayName = "RAW Data") TArray<uint8> RAWData, UPARAM(DisplayName = "RAW Format") ERuntimeRAWAudioFormat RAWFormat, UPARAM(DisplayName = "Sample Rate") int32 InSampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Set whether the sound should stop after playback is complete or not (play "blank sound"). False by default
	 * Setting it to True also makes the sound wave eligible for garbage collection after it has finished playing
	 */
	UFUNCTION(BlueprintCallable, Category = "Streaming Sound Wave|Import")
	void SetStopSoundOnPlaybackFinish(bool bStop);

	/**
	 * Toggles whether the audio capture should be filtered by VAD (Voice Activity Detection)
	 * If VAD is enabled, only audio data with voice activity will be captured
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Voice Activity Detector Enable Toggle"), Category = "Streaming Sound Wave|VAD")
	bool ToggleVAD(bool bVAD);

	/**
	 * Reinitializes a VAD (Voice Activity Detector) instance, clearing all state and resetting mode and sample rate to defaults
	 *
	 * @note This function can be called only if VAD is enabled (see ToggleVAD)
	 * @return Whether VAD is enabled or not
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Voice Activity Detector Reset"), Category = "Streaming Sound Wave|VAD")
	bool ResetVAD();

	/**
	 * Changes the operating ("aggressiveness") mode of a VAD (Voice Activity Detector) instance
	 * A more aggressive (higher mode) VAD is more restrictive in reporting speech
	 * In other words, the probability of detecting voice activity increases with a higher mode
	 * However, this also increases the rate of missed detections
	 * VeryAggressive is used by default
	 *
	 * @note This function can be called only if VAD is enabled (see ToggleVAD)
	 * @param Mode The VAD mode to set
	 * @return Whether the VAD mode was successfully set
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VAD Mode", Keywords = "Voice Activity Detector Mode"), Category = "Streaming Sound Wave|VAD")
	bool SetVADMode(ERuntimeVADMode Mode);

	//~ Begin UImportedSoundWave Interface
	virtual void PopulateAudioDataFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo) override;
	//~ End UImportedSoundWave Interface

protected:
	/** The audio task pipe (enforces sequential asynchronous execution of audio tasks as opposed to parallel which is possible with the default async task graph) */
	TUniquePtr<UE::Tasks::FPipe> AudioTaskPipe;

	/** The VAD (Voice Activity Detector) instance. Is valid only if VAD is enabled (see ToggleVAD) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Streaming Sound Wave|VAD")
	URuntimeVoiceActivityDetector* VADInstance;
};
