// Georgy Treshchev 2024.

#pragma once

#include "RuntimeAudioImporterTypes.h"
#include "Sound/SoundWaveProcedural.h"
#include "Misc/Optional.h"
#include "ImportedSoundWave.generated.h"

class UImportedSoundWave;

/** Static delegate broadcast to track the end of audio playback */
DECLARE_MULTICAST_DELEGATE(FOnAudioPlaybackFinishedNative);

/** Dynamic delegate broadcast to track the end of audio playback */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAudioPlaybackFinished);


/** Static delegate broadcast PCM data during a generation request */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGeneratePCMDataNative, const TArray<float>&);

/** Dynamic delegate broadcast PCM data during a generation request */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGeneratePCMData, const TArray<float>&, PCMData);


/** Static delegate broadcasting the result of preparing a sound wave for MetaSounds */
DECLARE_DELEGATE_OneParam(FOnPrepareSoundWaveForMetaSoundsResultNative, bool);

/** Dynamic delegate broadcasting the result of preparing a sound wave for MetaSounds */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnPrepareSoundWaveForMetaSoundsResult, bool, bSucceeded);


/** Static delegate broadcasting the result of releasing the played audio data */
DECLARE_DELEGATE_OneParam(FOnPlayedAudioDataReleaseResultNative, bool);

/** Dynamic delegate broadcasting the result of releasing the played audio data */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnPlayedAudioDataReleaseResult, bool, bSucceeded);


/** Static delegate broadcast newly populated PCM data */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPopulateAudioDataNative, const TArray<float>&);

/** Dynamic delegate broadcast newly populated PCM data */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPopulateAudioData, const TArray<float>&, PopulatedAudioData);

/** Static delegate broadcast when the PCM data is populated. Same as FOnPopulateAudioDataNative except it doesn't broadcast the audio data */
DECLARE_MULTICAST_DELEGATE(FOnPopulateAudioStateNative);

/** Dynamic delegate broadcast when the PCM data is populated. Same as FOnPopulateAudioData except it doesn't broadcast the audio data */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPopulateAudioState);

/** Static delegate broadcast when a sound wave is duplicated */
DECLARE_DELEGATE_TwoParams(FOnDuplicateSoundWaveNative, bool, UImportedSoundWave*);

/** Dynamic delegate broadcast when a sound wave is duplicated */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnDuplicateSoundWave, bool, bSucceeded, UImportedSoundWave*, DuplicatedSoundWave);

/** Static delegate broadcast the result of stopping the sound wave playback */
DECLARE_DELEGATE_OneParam(FOnStopPlaybackResultNative, bool);

/** Dynamic delegate broadcast the result of stopping the sound wave playback */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnStopPlaybackResult, bool, bSucceeded);


/**
 * Imported sound wave. Assumed to be dynamically populated once from the decoded audio data.
 * Accumulates audio data in 32-bit interleaved floating-point format.
 * Only a single playback is supported at a time (see DuplicateSoundWave for parallel playback)
 * Audio data preparation takes place in the Runtime Audio Importer library
 */
UCLASS(BlueprintType, Category = "Imported Sound Wave")
class RUNTIMEAUDIOIMPORTER_API UImportedSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UImportedSoundWave(const FObjectInitializer& ObjectInitializer);

	/**
	 * Create a new instance of the imported sound wave
	 *
	 * @return Created imported sound wave
	 */
	static UImportedSoundWave* CreateImportedSoundWave();

	//~ Begin USoundWave Interface
	virtual void BeginDestroy() override;
	virtual void Parse(class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances) override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override;
#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
	virtual TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	virtual bool InitAudioResource(FName Format) override;
	virtual bool IsSeekable() const override;
#endif
	//~ End USoundWave Interface

	//~ Begin USoundWaveProcedural Interface
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	//~ End USoundWaveProcedural Interface

	/**
	 * Duplicate the sound wave to be able to play it in parallel
	 * 
	 * @param bUseSharedAudioBuffer Whether to use the same audio buffer for the duplicated sound wave or not
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main")
	void DuplicateSoundWave(bool bUseSharedAudioBuffer, const FOnDuplicateSoundWave& Result);

	/**
	 * Duplicate the sound wave to be able to play it in parallel. Suitable for use in C++
	 * 
	 * @param bUseSharedAudioBuffer Whether to use the same audio buffer for the duplicated sound wave or not
	 * @param Result Delegate broadcasting the result
	 */
	virtual void DuplicateSoundWave(bool bUseSharedAudioBuffer, const FOnDuplicateSoundWaveNative& Result);

	/**
	 * Populate audio data from decoded info
	 *
	 * @param DecodedAudioInfo Decoded audio data
	 */
	virtual void PopulateAudioDataFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo);

	/**
	 * Prepare this sound wave to be able to set wave parameter for MetaSounds
	 * 
	 * @param Result Delegate broadcasting the result. Set the wave parameter only after it has been broadcast
	 * @warning This works if bEnableMetaSoundSupport is enabled in RuntimeAudioImporter.Build.cs/RuntimeAudioImporterEditor.Build.cs and only on Unreal Engine version >= 5.2
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|MetaSounds")
	void PrepareSoundWaveForMetaSounds(const FOnPrepareSoundWaveForMetaSoundsResult& Result);

	/**
	 * Prepare this sound wave to be able to set wave parameter for MetaSounds. Suitable for use in C++
	 * 
	 * @param Result Delegate broadcasting the result. Set the wave parameter only after it has been broadcast
	 * @warning This works if bEnableMetaSoundSupport is enabled in RuntimeAudioImporter.Build.cs/RuntimeAudioImporterEditor.Build.cs and only on Unreal Engine version >= 5.2
	 */
	void PrepareSoundWaveForMetaSounds(const FOnPrepareSoundWaveForMetaSoundsResultNative& Result);

	/**
	 * Release sound wave data. Call it manually only if you are sure of it
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Miscellaneous")
	virtual void ReleaseMemory();

	/**
	 * Set whether the sound should loop or not
	 *
	 * @param bLoop Whether the sound should loop or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Properties")
	void SetLooping(bool bLoop);

	/**
	 * Set subtitle cues
	 *
	 * @param InSubtitles Subtitles cues
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Properties")
	void SetSubtitles(UPARAM(DisplayName = "Subtitles") const TArray<FEditableSubtitleCue>& InSubtitles);

	/**
	 * Set sound playback volume
	 *
	 * @param InVolume Volume
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Properties")
	void SetVolume(UPARAM(DisplayName = "Volume") float InVolume = 1);

	/**
	 * Set sound playback pitch
	 *
	 * @param InPitch Pitch
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Properties")
	void SetPitch(UPARAM(DisplayName = "Pitch") float InPitch = 1);

	/**
	 * Rewind the sound for the specified time
	 *
	 * @param PlaybackTime How long to rewind the sound
	 * @return Whether the sound was rewound or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main")
	bool RewindPlaybackTime(float PlaybackTime);

	/**
	 * Thread-unsafe equivalent of RewindPlaybackTime
	 * Should only be used if DataGuard is locked
	 */
	bool RewindPlaybackTime_Internal(float PlaybackTime);

	/**
	 * Set the initial desired sample rate of the sound wave
	 * The sound wave PCM data will always contain this sample rate after the sound wave is populated with audio data
	 *
	 * @note This should be called before the sound wave is populated with any audio data
	 * @param DesiredSampleRate The initial desired sample rate
	 * @return Whether the initial desired sample rate was set or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main")
	bool SetInitialDesiredSampleRate(int32 DesiredSampleRate);

	/**
	 * Set the initial desired number of channels of the sound wave
	 * The sound wave PCM data will always contain this number of channels after the sound wave is populated with audio data
	 *
	 * @note This should be called before the sound wave is populated with any audio data
	 * @param DesiredNumOfChannels The initial desired number of channels
	 * @return Whether the initial desired number of channels was set or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main")
	bool SetInitialDesiredNumOfChannels(int32 DesiredNumOfChannels);

public:

	// TODO: Make this async
	/**
	 * Resample the sound wave to the specified sample rate
	 *
	 * @note This is not thread-safe at the moment
	 * @param NewSampleRate The new sample rate
	 * @return Whether the sound wave was resampled or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main")
	bool ResampleSoundWave(int32 NewSampleRate);

	// TODO: Make this async
	/**
	 * Change the number of channels of the sound wave
	 *
	 * @note This is not thread-safe at the moment
	 * @param NewNumOfChannels The new number of channels
	 * @return Whether the sound wave was mixed or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main")
	bool MixSoundWaveChannels(int32 NewNumOfChannels);

	/**
	 * Stop the sound wave playback
	 * 
	 * @note It is recommended to stop the sound wave playback using external means (e.g., by calling Stop on the audio component) and to use this function only if external means are not available
	 * @warning This function does not work for playback from MetaSounds
	 * @return Whether the sound wave was stopped or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Main", meta = (WorldContext = "WorldContextObject"))
	void StopPlayback(const UObject* WorldContextObject, const FOnStopPlaybackResult& Result);

	/**
	 * Stop the sound wave playback. Suitable for use in C++
	 * 
	 * @note It is recommended to stop the sound wave playback using external means (e.g., by calling Stop on the audio component) and to use this function only if external means are not available
	 * @warning This function does not work for playback from MetaSounds
	 * @return Whether the sound wave was stopped or not
	 */
	void StopPlayback(const UObject* WorldContextObject, const FOnStopPlaybackResultNative& Result);

	/**
	 * Change the number of frames played back. Used to rewind the sound
	 *
	 * @param NumOfFrames The new number of frames from which to continue playing sound
	 * @return Whether the frames were changed or not
	 */
	bool SetNumOfPlayedFrames(uint32 NumOfFrames);

	/**
	 * Thread-unsafe equivalent of SetNumOfPlayedFrames
	 * Should only be used if DataGuard is locked
	 */
	bool SetNumOfPlayedFrames_Internal(uint32 NumOfFrames);

	/**
	 * Get the number of frames played back
	 *
	 * @return The number of frames played back
	 */
	uint32 GetNumOfPlayedFrames() const;

	/**
	 * Thread-unsafe equivalent of GetNumOfPlayedFrames
	 * Should only be used if DataGuard is locked
	 */
	uint32 GetNumOfPlayedFrames_Internal() const;

	/**
	 * Get the current sound wave playback time, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	float GetPlaybackTime() const;

	/**
	 * Thread-unsafe equivalent of GetPlaybackTime
	 * Should only be used if DataGuard is locked
	 */
	float GetPlaybackTime_Internal() const;

	/**
	 * Constant alternative for getting the length of the sound wave, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info", meta = (DisplayName = "Get Duration"))
	float GetDurationConst() const;

	/**
	 * Get the length of the sound wave, in seconds
	 */
	virtual float GetDuration()
#if UE_VERSION_OLDER_THAN(5, 0, 0)
	override;
#else
	const override;
#endif

	/**
	 * Thread-unsafe equivalent of GetDurationConst
	 * Should only be used if DataGuard is locked
	 */
	float GetDurationConst_Internal() const;

	/**
	 * Get sample rate
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	int32 GetSampleRate() const;

	/**
	 * Get number of channels
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	int32 GetNumOfChannels() const;

	/**
	 * Get number of channels
	 * Alias for GetNumOfChannels. Needed since the GetNumOfChannels function sometimes isn't exposed to Blueprints for some reason
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	int32 GetNumberOfChannels() const;

	/**
	 * Get the current sound playback percentage, 0-100%
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	float GetPlaybackPercentage() const;

	/**
	 * Check if audio playback has finished or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	bool IsPlaybackFinished() const;

	/**
	 * Check if the sound wave is currently playing by the audio device or not
	 * @return Whether the sound wave is playing or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info", meta = (WorldContext = "WorldContextObject"))
	bool IsPlaying(const UObject* WorldContextObject) const;

	/**
	 * Thread-unsafe equivalent of IsPlaybackFinished
	 * Should only be used if DataGuard is locked
	 */
	bool IsPlaybackFinished_Internal() const;

	/**
	 * Retrieve audio header (metadata) information. Needed primarily for consistency with the RuntimeAudioImporterLibrary
	 *
	 * @param HeaderInfo Header info, valid only if the return is true
	 * @return Whether the retrieval was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info")
	bool GetAudioHeaderInfo(FRuntimeAudioHeaderInfo& HeaderInfo) const;

protected:
	/**
	 * Makes it possible to broadcast OnAudioPlaybackFinished again
	 */
	void ResetPlaybackFinish();

public:
	/** Bind to this delegate to know when the audio playback is finished. Suitable for use in C++ */
	FOnAudioPlaybackFinishedNative OnAudioPlaybackFinishedNative;

	/** Bind to this delegate to know when the audio playback is finished */
	UPROPERTY(BlueprintAssignable, Category = "Imported Sound Wave|Delegates")
	FOnAudioPlaybackFinished OnAudioPlaybackFinished;

	/** Bind to this delegate to receive PCM data during playback (may be useful for analyzing audio data). Suitable for use in C++ */
	FOnGeneratePCMDataNative OnGeneratePCMDataNative;

	/** Bind to this delegate to receive PCM data during playback (may be useful for analyzing audio data) */
	UPROPERTY(BlueprintAssignable, Category = "Imported Sound Wave|Delegates")
	FOnGeneratePCMData OnGeneratePCMData;

protected:
	/** Data guard (mutex) for thread safety */
	mutable FCriticalSection OnGeneratePCMData_DataGuard;
	
public:
	/** Bind to this delegate to obtain audio data every time it is populated. Suitable for use in C++ */
	FOnPopulateAudioDataNative OnPopulateAudioDataNative;

	/** Bind to this delegate to obtain audio data every time it is populated */
	UPROPERTY(BlueprintAssignable, Category = "Imported Sound Wave|Delegates")
	FOnPopulateAudioData OnPopulateAudioData;

protected:
	/** Data guard (mutex) for thread safety */
	mutable FCriticalSection OnPopulateAudioData_DataGuard;

public:
	/** Bind to this delegate to know when the audio data is populated. Same as OnPopulateAudioDataNative except it doesn't broadcast the audio data. Suitable for use in C++ */
	FOnPopulateAudioStateNative OnPopulateAudioStateNative;

	/** Bind to this delegate to know when the audio data is populated. Same as OnPopulateAudioData except it doesn't broadcast the audio data */
	UPROPERTY(BlueprintAssignable, Category = "Imported Sound Wave|Delegates")
	FOnPopulateAudioState OnPopulateAudioState;

	/**
	 * Retrieve the PCM buffer, completely thread-safe. Suitable for use in Blueprints
	 *
	 * @return PCM buffer in 32-bit float format
	 */
	UFUNCTION(BlueprintCallable, Category = "Imported Sound Wave|Info", meta = (DisplayName = "Get PCM Buffer"))
	TArray<float> GetPCMBufferCopy();

	/**
	 * Get immutable PCM buffer. Use DataGuard to make it thread safe
	 * Use PopulateAudioDataFromDecodedInfo to populate it
	 *
	 * @return PCM buffer in 32-bit float format
	 */
	const FPCMStruct& GetPCMBuffer() const;

	/**
	 * Get audio format of the audio imported into the sound wave
	 * @return Audio format
	 */
	UFUNCTION(BlueprintPure, Category = "Imported Sound Wave|Info")
	ERuntimeAudioFormat GetAudioFormat() const;

	/** Data guard (mutex) for thread safety */
	mutable TSharedPtr<FCriticalSection> DataGuard;

protected:
	/** Bool to control the behaviour of the OnAudioPlaybackFinished delegate */
	bool PlaybackFinishedBroadcast;

	/** The number of frames played. Increments during playback, should not be > PCMBufferInfo.PCMNumOfFrames */
	uint32 PlayedNumOfFrames;

	/** Contains PCM data for sound wave playback */
	TSharedPtr<FPCMStruct> PCMBufferInfo;

	/** Whether to stop the sound at the end of playback or not. Sound wave will not be garbage collected if playback was completed while this parameter is set to false */
	bool bStopSoundOnPlaybackFinish;

	/** Audio format of the audio imported into the sound wave */
	ERuntimeAudioFormat ImportedAudioFormat;

	/** Initial desired sample rate of the sound wave (see SetInitialDesiredSampleRate) */
	TOptional<uint32> InitialDesiredSampleRate;

	/** Initial desired number of channels of the sound wave (see SetInitialDesiredNumChannels) */
	TOptional<uint32> InitialDesiredNumOfChannels;
};
