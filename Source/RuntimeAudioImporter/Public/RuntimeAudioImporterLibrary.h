// Georgy Treshchev 2022.

#pragma once

#include "Sound/ImportedSoundWave.h"
#include "RuntimeAudioImporterTypes.h"
#include "RuntimeAudioImporterLibrary.generated.h"

class UPreImportedSoundAsset;
class URuntimeAudioImporterLibrary;

/** Static delegate broadcasting the audio importer progress */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgressNative, int32 Percentage);

/** Dynamic delegate broadcasting the audio importer progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgress, int32, Percentage);

/** Static delegate broadcasting the audio importer result */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResultNative, URuntimeAudioImporterLibrary* Importer, UImportedSoundWave* ImportedSoundWave, ETranscodingStatus Status);

/** Dynamic delegate broadcasting the audio importer result */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResult, URuntimeAudioImporterLibrary*, Importer, UImportedSoundWave*, ImportedSoundWave, ETranscodingStatus, Status);


/** Static delegate broadcasting the result of the audio export to buffer */
DECLARE_DELEGATE_TwoParams(FOnAudioExportToBufferResultNative, bool, const TArray<uint8>&);

/** Dynamic delegate broadcasting the result of the audio export to buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAudioExportToBufferResult, bool, bSucceeded, const TArray<uint8>&, AudioData);

/** Static delegate broadcasting the result of the audio export to file */
DECLARE_DELEGATE_OneParam(FOnAudioExportToFileResultNative, bool);

/** Dynamic delegate broadcasting the result of the audio export to file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAudioExportToFileResult, bool, bSucceeded);


/** Static delegate broadcasting the result of the RAW data transcoded from buffer */
DECLARE_DELEGATE_TwoParams(FOnRAWDataTranscodeFromBufferResultNative, bool, const TArray<uint8>&);

/** Dynamic delegate broadcasting the result of the RAW data transcoded from buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnRAWDataTranscodeFromBufferResult, bool, bSucceeded, UPARAM(DisplayName = "RAW Data") const TArray<uint8>&, RAWData);


/** Static delegate broadcasting the result of the RAW data transcoded from file */
DECLARE_DELEGATE_OneParam(FOnRAWDataTranscodeFromFileResultNative, bool);

/** Dynamic delegate broadcasting the result of the RAW data transcoded from file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRAWDataTranscodeFromFileResult, bool, bSucceeded);


/**
 * Runtime Audio Importer library
 * Various functions related to transcoding audio data, such as importing audio files, manually encoding / decoding audio data and more
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
	 * Instantiates a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult delegates
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Import audio from file
	 *
	 * @param FilePath Path to the audio file to import
	 * @param AudioFormat Audio format
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromFile(const FString& FilePath, EAudioFormat AudioFormat);

	/**
	 * Import audio file from the pre-imported sound asset
	 *
	 * @param PreImportedSoundAsset PreImportedSoundAsset object reference
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAsset);

	/**
	 * Import audio from buffer
	 *
	 * @param AudioData Audio data array
	 * @param AudioFormat Audio format
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromBuffer(TArray<uint8> AudioData, EAudioFormat AudioFormat);

	/**
	 * Import audio from RAW file. Audio data must not have headers and must be uncompressed
	 *
	 * @param FilePath Path to the audio file to import
	 * @param RAWFormat RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW File"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWFile(const FString& FilePath, UPARAM(DisplayName = "RAW Format") ERAWAudioFormat RAWFormat, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Import audio from RAW buffer. Audio data must not have headers and must be uncompressed
	 *
	 * @param RAWBuffer RAW audio buffer
	 * @param RAWFormat RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW Buffer"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWBuffer(UPARAM(DisplayName = "RAW Buffer") TArray<uint8> RAWBuffer, UPARAM(DisplayName = "RAW Format") ERAWAudioFormat RAWFormat, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Transcoding one RAW Data format to another from buffer
	 *
	 * @param RAWDataFrom RAW data for transcoding
	 * @param RAWFormatFrom Original format
	 * @param RAWFormatTo Required format
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From Buffer"), Category = "Runtime Audio Importer|Transcode")
	static void TranscodeRAWDataFromBuffer(UPARAM(DisplayName = "RAW Data From") TArray<uint8> RAWDataFrom, UPARAM(DisplayName = "RAW Format From") ERAWAudioFormat RAWFormatFrom, UPARAM(DisplayName = "RAW Format To") ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResult& Result);

	/**
	 * Transcoding one RAW Data format to another from buffer
	 *
	 * @param RAWDataFrom RAW data for transcoding
	 * @param RAWFormatFrom Original format
	 * @param RAWFormatTo Required format
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeRAWDataFromBuffer(UPARAM(DisplayName = "RAW Data From") TArray<uint8> RAWDataFrom, UPARAM(DisplayName = "RAW Format From") ERAWAudioFormat RAWFormatFrom, UPARAM(DisplayName = "RAW Format To") ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResultNative& Result);

	/**
	 * Transcoding one RAW Data format to another from file. Suitable for use in C++
	 *
	 * @param FilePathFrom Path to file with RAW data for transcoding
	 * @param RAWFormatFrom Original format
	 * @param FilePathTo File path for saving RAW data
	 * @param RAWFormatTo Required format
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From File"), Category = "Runtime Audio Importer|Transcode")
	static void TranscodeRAWDataFromFile(const FString& FilePathFrom, UPARAM(DisplayName = "RAW Format From") ERAWAudioFormat RAWFormatFrom, const FString& FilePathTo, UPARAM(DisplayName = "RAW Format To") ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResult& Result);

	/**
	 * Transcoding one RAW Data format to another from file. Suitable for use in C++
	 *
	 * @param FilePathFrom Path to file with RAW data for transcoding
	 * @param RAWFormatFrom Original format
	 * @param FilePathTo File path for saving RAW data
	 * @param RAWFormatTo Required format
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeRAWDataFromFile(const FString& FilePathFrom, UPARAM(DisplayName = "RAW Format From") ERAWAudioFormat RAWFormatFrom, const FString& FilePathTo, UPARAM(DisplayName = "RAW Format To") ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResultNative& Result);

	/**
	 * Export the imported sound wave to file
	 *
	 * @param ImportedSoundWave Reference to the imported sound wave
	 * @param AudioFormat Required format to export Please note that some formats are not supported
	 * @param SavePath Path to save the file
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Export")
	static void ExportSoundWaveToFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToFileResult& Result);

	/**
	 * Export the imported sound wave to file. Suitable for use in C++
	 *
	 * @param ImportedSoundWavePtr Weak pointer to the imported sound wave
	 * @param AudioFormat Required format to export Please note that some formats are not supported
	 * @param SavePath Path to save the file
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @param Result Delegate broadcasting the result
	 */
	static void ExportSoundWaveToFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToFileResultNative& Result);

	/**
	 * Export the imported sound wave to buffer
	 *
	 * @param ImportedSoundWave Reference to the imported sound wave
	 * @param AudioFormat Required format to export Please note that some formats are not supported
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Export")
	static void ExportSoundWaveToBuffer(UImportedSoundWave* ImportedSoundWave, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToBufferResult& Result);

	/**
	 * Export the imported sound wave to buffer. Suitable for use in C++
	 *
	 * @param ImportedSoundWavePtr Weak pointer to the imported sound wave
	 * @param AudioFormat Required format to export Please note that some formats are not supported
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @param Result Delegate broadcasting the result
	 */
	static void ExportSoundWaveToBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToBufferResultNative& Result);

	/**
	 * Get audio format by extension
	 *
	 * @param FilePath File path where to find the format (by extension)
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static EAudioFormat GetAudioFormat(const FString& FilePath);

	/**
	 * Determine audio format based on audio data. A more advanced way to get the format
	 *
	 * @param AudioData Audio data array
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static EAudioFormat GetAudioFormatAdvanced(const TArray<uint8>& AudioData);

	/**
	 * Convert seconds to string (hh:mm:ss or mm:ss depending on the number of seconds)
	 *
	 * @return hh:mm:ss or mm:ss string representation
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static FString ConvertSecondsToString(int32 Seconds);

	/**
	 * Decode compressed audio data to uncompressed
	 *
	 * @param EncodedAudioInfo Encoded audio data
	 * @param DecodedAudioInfo Decoded audio data
	 * @return Whether the decoding was successful or not
	 */
	static bool DecodeAudioData(FEncodedAudioStruct&& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo);

	/**
	 * Encode uncompressed audio data to compressed
	 *
	 * @param DecodedAudioInfo Decoded audio data
	 * @param EncodedAudioInfo Encoded audio data
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @return Whether the encoding was successful or not
	 */
	static bool EncodeAudioData(FDecodedAudioStruct&& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality);

	/**
	 * Determine audio format based on audio data
	 *
	 * @param AudioData Pointer to in-memory audio data
	 * @param AudioDataSize Size of in-memory audio data
	 */
	static EAudioFormat GetAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Import audio from 32-bit float PCM data
	 *
	 * @param PCMData PCM data
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	void ImportAudioFromFloat32Buffer(FBulkDataBuffer<float>&& PCMData, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Create Imported Sound Wave and finish importing.
	 *
	 * @param DecodedAudioInfo Decoded audio data
	 */
	void ImportAudioFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo);

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
	void OnResult_Internal(UImportedSoundWave* ImportedSoundWave, ETranscodingStatus Status);
};
