// Georgy Treshchev 2022.

#pragma once

#include "ImportedSoundWave.h"
#include "RuntimeAudioImporterTypes.h"
#include "RuntimeAudioImporterLibrary.generated.h"

// Forward declaration of the UPreImportedSoundAsset class
class UPreImportedSoundAsset;

/** Delegate broadcast to get the audio importer progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgress, const int32, Percentage);

/** Delegate broadcast to get the audio importer result */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResult, class URuntimeAudioImporterLibrary*, RuntimeAudioImporterObjectRef, UImportedSoundWave*, SoundWaveRef, const ETranscodingStatus&, Status);

/**
 * Runtime Audio Importer object
 * Designed primarily for importing audio files in real time
 */
UCLASS(BlueprintType, Category = "Runtime Audio Importer")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioImporterLibrary : public UObject
{
	GENERATED_BODY()

public:
	/** Bind to know when the transcoding is on progress */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer|Delegates")
	FOnAudioImporterProgress OnProgress;

	/** Bind to know when the transcoding is complete (even if it fails) */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer")
	FOnAudioImporterResult OnResult;

	/** Decoded audio info. CPP use only */
	FDecodedAudioStruct DecodedAudioInfo;

	/**
	 * Instantiates a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult delegates to know when it is in the process of importing and imported
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV, OGG, VORBIS, OPUS, IMPORT"), Category = "Runtime Audio Importer")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Import audio from file
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format Audio file format (extension)
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromFile(const FString& FilePath, EAudioFormat Format);

	/**
	 * Import audio file from the pre-imported sound asset
	 *
	 * @param PreImportedSoundAssetRef PreImportedSoundAsset object reference. Should contain "BaseAudioDataArray" buffer
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef);

	/**
	 * Import audio from buffer
	 *
	 * @param AudioDataBuffer Buffer of the audio data
	 * @param Format Audio file format (extension)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From Buffer", Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"), Category = "Runtime Audio Importer|Advanced")
	void ImportAudioFromBuffer_BP(TArray<uint8> AudioDataBuffer, EAudioFormat Format);

	/**
	 * Import audio from buffer
	 *
	 * @param AudioDataBuffer Buffer of the audio data
	 * @param Format Audio file format (extension)
	 */
	void ImportAudioFromBuffer(TArray<uint8>& AudioDataBuffer, EAudioFormat Format);

	/**
	 * Import audio from RAW file. Audio data must not have headers and must be uncompressed
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW File"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat Format, const int32 SampleRate = 44100, const int32 NumOfChannels = 1);

	/**
	 * Import audio from RAW buffer. Audio data must not have headers and must be uncompressed
	 *
	 * @param RAWBuffer RAW audio buffer
	 * @param Format RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW Buffer"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format, const int32 SampleRate = 44100, const int32 NumOfChannels = 1);
	
	/**
	 * Import audio from 32-bit float PCM data
	 *
	 * @param PCMData Pointer to memory location of the PCM data
	 * @param PCMDataSize Memory size allocated for the PCM data
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	void ImportAudioFromFloat32Buffer(uint8* PCMData, const uint32 PCMDataSize, const int32 SampleRate = 44100, const int32 NumOfChannels = 1);


	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From RAW data for transcoding
	 * @param FormatFrom Original format
	 * @param RAWData_To Transcoded RAW data with the specified format
	 * @param FormatTo Required format
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From Buffer"), Category = "Runtime Audio Importer|Transcode")
	void TranscodeRAWDataFromBuffer(TArray<uint8> RAWData_From, ERAWAudioFormat FormatFrom, TArray<uint8>& RAWData_To, ERAWAudioFormat FormatTo);

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param FilePathFrom Path to file with RAW data for transcoding
	 * @param FormatFrom Original format
	 * @param FilePathTo File path for saving RAW data
	 * @param FormatTo Required format
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From File"), Category = "Runtime Audio Importer|Transcode")
	bool TranscodeRAWDataFromFile(const FString& FilePathFrom, ERAWAudioFormat FormatFrom, const FString& FilePathTo, ERAWAudioFormat FormatTo);


private:
	/**
	 * Internal main audio importing method
	 *
	 * @param AudioDataBuffer Buffer of the audio data
	 * @param Format Audio file format (extension)
	 */
	void ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioDataBuffer, const EAudioFormat& Format);


	/**
	 * Create Imported Sound Wave and finish importing.
	 *
	 * @note At this point, TranscodingFillInfo should be filled
	 */
	void CreateSoundWaveAndFinishImport();

	/**
	 * Define SoundWave object reference
	 *
	 * @param SoundWaveRef SoundWave object reference to define
	 * @return Whether the defining was successful or not
	 */
	bool DefineSoundWave(UImportedSoundWave* SoundWaveRef);

	/**
	 * Fill SoundWave basic information (e.g. duration, number of channels, etc)
	 *
	 * @param SoundWaveRef SoundWave object reference
	 */
	void FillSoundWaveBasicInfo(UImportedSoundWave* SoundWaveRef) const;

	/**
	 * Fill SoundWave PCM data buffer
	 *
	 * @param SoundWaveRef SoundWave object reference
	 */
	void FillPCMData(UImportedSoundWave* SoundWaveRef) const;

	/**
	 * Transcode Audio from Audio Data to PCM Data
	 *
	 * @param EncodedAudioInfo Encoded audio data
	 * @return Whether the transcoding was successful or not
	 */
	bool DecodeAudioData(const FEncodedAudioStruct& EncodedAudioInfo);

	/**
	 * Get audio format by extension
	 *
	 * @param FilePath File path where to find the format (by extension)
	 * @return Returns the found audio format (e.g. mp3. flac, etc) by AudioFormat Enum
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utility")
	static EAudioFormat GetAudioFormat(const FString& FilePath);

	/**
	 * Audio transcoding progress callback
	 * 
	 * @param Percentage Percentage of importing completion (0-100%)
	 */
	void OnProgress_Internal(int32 Percentage);

	/**
	 * Audio importing finished callback
	 * 
	 * @param SoundWaveRef A ready SoundWave object
	 * @param Status Importing status
	 */
	void OnResult_Internal(UImportedSoundWave* SoundWaveRef, ETranscodingStatus Status);
};
