// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"
#include "ImportedSoundWave.h"
#include "StreamingSoundWave.generated.h"

/**
 * Streaming sound wave. Can append audio data dynamically, including during playback
 * It will live indefinitely, even if the sound wave has finished playing, until SetStopSoundOnPlaybackFinish is called.
 * Audio data is always accumulated, clear memory manually via ReleaseMemory if necessary.
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
	 * Append audio data to the end of existing data from encoded audio data
	 *
	 * @param AudioData Audio data array
	 * @param AudioFormat Audio format
	 */
	UFUNCTION(BlueprintCallable, Category = "Streaming Sound Wave|Append")
	void AppendAudioDataFromEncoded(TArray<uint8> AudioData, EAudioFormat AudioFormat);

	/**
	 * Append audio data to the end of existing data from RAW audio data
	 *
	 * @param RAWData RAW audio buffer
	 * @param RAWFormat RAW audio format
	 * @param InSampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Append Audio Data From RAW"), Category = "Streaming Sound Wave|Append")
	void AppendAudioDataFromRAW(UPARAM(DisplayName = "RAW Data") TArray<uint8> RAWData, UPARAM(DisplayName = "RAW Format") ERAWAudioFormat RAWFormat, UPARAM(DisplayName = "Sample Rate") int32 InSampleRate = 44100, int32 NumOfChannels = 1);
	
	/**
	 * Set whether the sound should stop after playback is complete or not (play "blank sound"). False by default
	 * Setting it to True also makes the sound wave eligible for garbage collection after it has finished playing
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Streaming Sound Wave|Import")
	void SetStopSoundOnPlaybackFinish(bool bStop);

	//~ Begin UImportedSoundWave Interface
	virtual void PopulateAudioDataFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo) override;
	//~ End UImportedSoundWave Interface

private:
	/** Whether the initial audio data is filled in or not */
	bool bFilledInitialAudioData;
};
