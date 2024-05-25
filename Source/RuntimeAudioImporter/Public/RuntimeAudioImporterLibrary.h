// Georgy Treshchev 2024.

#pragma once

#include "Sound/ImportedSoundWave.h"
#include "RuntimeAudioImporterTypes.h"
#include "RuntimeAudioImporterLibrary.generated.h"

class UPreImportedSoundAsset;
class URuntimeAudioImporterLibrary;

/** Static delegate broadcasting the audio importer progress */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgressNative, int32);

/** Dynamic delegate broadcasting the audio importer progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgress, int32, Percentage);

/** Static delegate broadcasting the audio importer result */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResultNative, URuntimeAudioImporterLibrary*, UImportedSoundWave*, ERuntimeImportStatus);

/** Dynamic delegate broadcasting the audio importer result */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResult, URuntimeAudioImporterLibrary*, Importer, UImportedSoundWave*, ImportedSoundWave, ERuntimeImportStatus, Status);

/** Static delegate broadcasting the result of the conversion from SoundWave to ImportedSoundWave */
DECLARE_DELEGATE_TwoParams(FOnRegularToAudioImporterSoundWaveConvertResultNative, bool, UImportedSoundWave*);

/** Dynamic delegate broadcasting the result of the conversion from SoundWave to ImportedSoundWave */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnRegularToAudioImporterSoundWaveConvertResult, bool, bSucceeded, UImportedSoundWave*, ImportedSoundWave);


/**
 * Runtime Audio Importer library
 * Various functions related to working with audio data, including importing audio files, manually encoding and decoding audio data, and more
 */
UCLASS(BlueprintType, Category = "Runtime Audio Importer")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioImporterLibrary : public UObject
{
	GENERATED_BODY()

public:
	/** Bind to know when audio import is on progress. Suitable for use in C++ */
	FOnAudioImporterProgressNative OnProgressNative;

	/** Bind to know when audio import is on progress */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer|Delegates")
	FOnAudioImporterProgress OnProgress;

	/** Bind to know when audio import is complete (even if it fails). Suitable for use in C++ */
	FOnAudioImporterResultNative OnResultNative;

	/** Bind to know when audio import is complete (even if it fails) */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer|Delegates")
	FOnAudioImporterResult OnResult;

	/**
	 * Tries to retrieve audio data from a given regular sound wave
	 * 
	 * @param SoundWave The sound wave from which to obtain audio data
	 * @param OutDecodedAudioInfo The decoded audio information. Populated only if the function returns true
	 * @return True if the audio data was successfully retrieved
	 */
	static bool TryToRetrieveSoundWaveData(USoundWave* SoundWave, FDecodedAudioStruct& OutDecodedAudioInfo);

	/**
	 * Instantiate a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult delegates
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Import audio from a file
	 *
	 * @param FilePath Path to the audio file to import
	 * @param AudioFormat Audio format
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromFile(const FString& FilePath, ERuntimeAudioFormat AudioFormat);

	/**
	 * Import audio from a pre-imported sound asset
	 *
	 * @param PreImportedSoundAsset PreImportedSoundAsset object reference
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis, BINK"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAsset);

	/**
	 * Import audio from a buffer
	 *
	 * @param AudioData Audio data array
	 * @param AudioFormat Audio format
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis, BINK"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromBuffer(TArray<uint8> AudioData, ERuntimeAudioFormat AudioFormat);

	/**
	 * Import audio from a buffer. Suitable for use with 64-bit data size
	 *
	 * @param AudioData Audio data array
	 * @param AudioFormat Audio format
	 */
	void ImportAudioFromBuffer(TArray64<uint8> AudioData, ERuntimeAudioFormat AudioFormat);

	/**
	 * Import audio from a RAW file. The audio data must not have headers and must be uncompressed
	 *
	 * @param FilePath Path to the audio file to import
	 * @param RAWFormat RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW File", Keywords = "PCM, RAW"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWFile(const FString& FilePath, UPARAM(DisplayName = "RAW Format") ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Import audio from a RAW buffer. The audio data must not have headers and must be uncompressed
	 *
	 * @param RAWBuffer RAW audio buffer
	 * @param RAWFormat RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW Buffer", Keywords = "PCM, RAW"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWBuffer(UPARAM(DisplayName = "RAW Buffer") TArray<uint8> RAWBuffer, UPARAM(DisplayName = "RAW Format") ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Import audio from a RAW buffer. The audio data must not have headers and must be uncompressed. Suitable for use with 64-bit data size
	 *
	 * @param RAWBuffer The RAW audio buffer
	 * @param RAWFormat RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	void ImportAudioFromRAWBuffer(TArray64<uint8> RAWBuffer, ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Converts a regular SoundWave to an inherited sound wave of type ImportedSoundWave used in RuntimeAudioImporter
	 * Experimental feature, use with caution
	 *
	 * @param SoundWave The regular USoundWave to convert
	 * @param ImportedSoundWaveClass The subclass of UImportedSoundWave to create and convert to
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Convert")
	static void ConvertRegularToImportedSoundWave(USoundWave* SoundWave, TSubclassOf<UImportedSoundWave> ImportedSoundWaveClass, const FOnRegularToAudioImporterSoundWaveConvertResult& Result);

	/**
	 * Converts a regular SoundWave to an inherited sound wave of type ImportedSoundWave used in RuntimeAudioImporter. Suitable for use in C++
	 * Experimental feature, use with caution
	 *
	 * @param SoundWave The regular USoundWave to convert
	 * @param ImportedSoundWaveClass The subclass of UImportedSoundWave to create and convert to
	 * @param Result Delegate broadcasting the result
	 */
	static void ConvertRegularToImportedSoundWave(USoundWave* SoundWave, TSubclassOf<UImportedSoundWave> ImportedSoundWaveClass, const FOnRegularToAudioImporterSoundWaveConvertResultNative& Result);

	/**
	 * Decode compressed audio data to uncompressed.
	 *
	 * @param EncodedAudioInfo The encoded audio data
	 * @param DecodedAudioInfo The decoded audio data
	 * @return Whether the decoding was successful or not
	 */
	static bool DecodeAudioData(FEncodedAudioStruct&& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo);

	/**
	 * Encode uncompressed audio data to compressed.
	 *
	 * @param DecodedAudioInfo The decoded audio data
	 * @param EncodedAudioInfo The encoded audio data
	 * @param Quality The quality of the encoded audio data, from 0 to 100
	 * @return Whether the encoding was successful or not
	 */
	static bool EncodeAudioData(FDecodedAudioStruct&& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality);

	/**
	 * Import audio from 32-bit float PCM data
	 *
	 * @param PCMData PCM data
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	void ImportAudioFromFloat32Buffer(FRuntimeBulkDataBuffer<float>&& PCMData, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Create Imported Sound Wave and finish importing.
	 *
	 * @param DecodedAudioInfo Decoded audio data
	 */
	void ImportAudioFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo);

	/**
	 * Resample and mix channels in decoded audio info
	 * 
	 * @param DecodedAudioInfo Decoded audio data
	 * @param NewSampleRate New sample rate
	 * @param NewNumOfChannels New number of channels
	 * @return True if the resampling and mixing was successful
	 */
	static bool ResampleAndMixChannelsInDecodedInfo(FDecodedAudioStruct& DecodedAudioInfo, uint32 NewSampleRate, uint32 NewNumOfChannels);

protected:
	/**
	 * Audio transcoding progress callback
	 * 
	 * @param Percentage Percentage of importing completion (0-100%)
	 */
	void OnProgress_Internal(int32 Percentage);

	/**
	 * Audio importing finished callback
	 * 
	 * @param ImportedSoundWave Reference to the imported sound wave
	 * @param Status Importing status
	 */
	void OnResult_Internal(UImportedSoundWave* ImportedSoundWave, ERuntimeImportStatus Status);
};
