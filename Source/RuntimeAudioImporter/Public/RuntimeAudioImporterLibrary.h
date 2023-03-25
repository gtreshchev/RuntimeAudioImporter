// Georgy Treshchev 2023.

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


/** Static delegate broadcasting the result of the audio export to buffer */
DECLARE_DELEGATE_TwoParams(FOnAudioExportToBufferResultNative, bool, const TArray64<uint8>&);

/** Dynamic delegate broadcasting the result of the audio export to buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAudioExportToBufferResult, bool, bSucceeded, const TArray<uint8>&, AudioData);

/** Static delegate broadcasting the result of the audio export to file */
DECLARE_DELEGATE_OneParam(FOnAudioExportToFileResultNative, bool);

/** Dynamic delegate broadcasting the result of the audio export to file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAudioExportToFileResult, bool, bSucceeded);


/** Static delegate broadcasting the result of the RAW data transcoded from buffer */
DECLARE_DELEGATE_TwoParams(FOnRAWDataTranscodeFromBufferResultNative, bool, const TArray64<uint8>&);

/** Dynamic delegate broadcasting the result of the RAW data transcoded from buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnRAWDataTranscodeFromBufferResult, bool, bSucceeded, const TArray<uint8>&, RAWData);


/** Static delegate broadcasting the result of the RAW data transcoded from file */
DECLARE_DELEGATE_OneParam(FOnRAWDataTranscodeFromFileResultNative, bool);

/** Dynamic delegate broadcasting the result of the RAW data transcoded from file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRAWDataTranscodeFromFileResult, bool, bSucceeded);


/** Static delegate broadcasting the result of retrieving audio header info */
DECLARE_DELEGATE_TwoParams(FOnGetAudioHeaderInfoResultNative, bool, const FRuntimeAudioHeaderInfo&);

/** Dynamic delegate broadcasting the result of retrieving audio header info */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGetAudioHeaderInfoResult, bool, bSucceeded, const FRuntimeAudioHeaderInfo&, HeaderInfo);


/** Dynamic delegate broadcasting the result of scanning directory for audio files */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnScanDirectoryForAudioFilesResult, bool, bSucceeded, const TArray<FString>&, AudioFilePaths);

/** Static delegate broadcasting the result of scanning directory for audio files */
DECLARE_DELEGATE_TwoParams(FOnScanDirectoryForAudioFilesResultNative, bool, const TArray<FString>&);


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
	 * Transcode one RAW Data format into another from buffer
	 *
	 * @param RAWDataFrom The RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From Buffer"), Category = "Runtime Audio Importer|Transcode")
	static void TranscodeRAWDataFromBuffer(UPARAM(DisplayName = "RAW Data From") TArray<uint8> RAWDataFrom, UPARAM(DisplayName = "RAW Format From") ERuntimeRAWAudioFormat RAWFormatFrom, UPARAM(DisplayName = "RAW Format To") ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResult& Result);

	/**
	 * Transcode one RAW data format into another from buffer. Suitable for use with 64-bit data size
	 *
	 * @param RAWDataFrom The RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeRAWDataFromBuffer(TArray64<uint8> RAWDataFrom, ERuntimeRAWAudioFormat RAWFormatFrom, ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResultNative& Result);

	/**
	 * Transcode one RAW data format into another from file
	 *
	 * @param FilePathFrom Path to file with the RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param FilePathTo File path for saving RAW data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From File"), Category = "Runtime Audio Importer|Transcode")
	static void TranscodeRAWDataFromFile(const FString& FilePathFrom, UPARAM(DisplayName = "RAW Format From") ERuntimeRAWAudioFormat RAWFormatFrom, const FString& FilePathTo, UPARAM(DisplayName = "RAW Format To") ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResult& Result);

	/**
	 * Transcode one RAW data format into another from file. Suitable for use in C++
	 *
	 * @param FilePathFrom Path to file with the RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param FilePathTo File path for saving RAW data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeRAWDataFromFile(const FString& FilePathFrom, UPARAM(DisplayName = "RAW Format From") ERuntimeRAWAudioFormat RAWFormatFrom, const FString& FilePathTo, UPARAM(DisplayName = "RAW Format To") ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResultNative& Result);

	/**
	 * Export the imported sound wave to a file
	 *
	 * @param ImportedSoundWave Imported sound wave to be exported
	 * @param AudioFormat The desired audio format for the exported file. Note that some formats may not be supported
	 * @param SavePath The path where the exported file will be saved
	 * @param Quality The quality of the encoded audio data, from 0 to 100
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Export")
	static void ExportSoundWaveToFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResult& Result);

	/**
	 * Export the imported sound wave into file. Suitable for use in C++
	 *
	 * @param ImportedSoundWavePtr Imported sound wave to be exported
	 * @param AudioFormat The desired audio format for the exported file. Note that some formats may not be supported
	 * @param SavePath The path where the exported file will be saved
	 * @param Quality The quality of the encoded audio data, from 0 to 100
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	static void ExportSoundWaveToFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResultNative& Result);

	/**
	 * Export the imported sound wave into a buffer
	 *
	 * @param ImportedSoundWave Imported sound wave to be exported
	 * @param AudioFormat The desired audio format for the exported file. Note that some formats may not be supported
	 * @param Quality The quality of the encoded audio data, from 0 to 100
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Export")
	static void ExportSoundWaveToBuffer(UImportedSoundWave* ImportedSoundWave, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResult& Result);

	/**
	 * Export the imported sound wave into a buffer. Suitable for use in C++
	 *
	 * @param ImportedSoundWavePtr Imported sound wave to be exported
	 * @param AudioFormat The desired audio format for the exported file. Note that some formats may not be supported
	 * @param Quality The quality of the encoded audio data, from 0 to 100
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	static void ExportSoundWaveToBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResultNative& Result);

	/**
	 * Export the imported sound wave into a RAW file
	 *
	 * @param ImportedSoundWave Imported sound wave to be exported
	 * @param RAWFormat Required RAW format for exporting
	 * @param SavePath Path to save the file
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Export Sound Wave To RAW File"), Category = "Runtime Audio Importer|Export")
	static void ExportSoundWaveToRAWFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, UPARAM(DisplayName = "RAW Format") ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResult& Result);

	/**
	 * Export the imported sound wave into a RAW file. Suitable for use in C++
	 *
	 * @param ImportedSoundWavePtr Imported sound wave to be exported
	 * @param RAWFormat Required RAW format for exporting
	 * @param SavePath Path to save the file
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	static void ExportSoundWaveToRAWFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResultNative& Result);

	/**
	 * Export the imported sound wave into a RAW buffer
	 *
	 * @param ImportedSoundWave Imported sound wave to be exported
	 * @param RAWFormat Required RAW format for exporting
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Export Sound Wave To RAW Buffer"), Category = "Runtime Audio Importer|Export")
	static void ExportSoundWaveToRAWBuffer(UImportedSoundWave* ImportedSoundWave, UPARAM(DisplayName = "RAW Format") ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResult& Result);

	/**
	 * Export the imported sound wave into a RAW buffer. Suitable for use with 64-bit data size
	 *
	 * @param ImportedSoundWavePtr Imported sound wave to be exported
	 * @param RAWFormat Required RAW format for exporting
	 * @param OverrideOptions Override options for the export
	 * @param Result Delegate broadcasting the result
	 */
	static void ExportSoundWaveToRAWBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResultNative& Result);

	/**
	 * Get the audio format based on file extension
	 *
	 * @param FilePath File path where to find the format (by extension)
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static ERuntimeAudioFormat GetAudioFormat(const FString& FilePath);

	/**
	 * Determine the audio format based on audio data. A more advanced way to get the format
	 *
	 * @param AudioData Audio data array
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static ERuntimeAudioFormat GetAudioFormatAdvanced(const TArray<uint8>& AudioData);

	/**
	 * Retrieve audio header (metadata) information from a file
	 *
	 * @param FilePath The path to the audio file from which header information will be retrieved
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static void GetAudioHeaderInfoFromFile(const FString& FilePath, const FOnGetAudioHeaderInfoResult& Result);

	/**
	 * Retrieve audio header (metadata) information from a file. Suitable for use in C++
	 *
	 * @param FilePath The path to the audio file from which header information will be retrieved
	 * @param Result Delegate broadcasting the result
	 */
	static void GetAudioHeaderInfoFromFile(const FString& FilePath, const FOnGetAudioHeaderInfoResultNative& Result);

	/**
	 * Retrieve audio header (metadata) information from a buffer
	 *
	 * @param AudioData The audio data from which the header information will be retrieved
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static void GetAudioHeaderInfoFromBuffer(TArray<uint8> AudioData, const FOnGetAudioHeaderInfoResult& Result);

	/**
	 * Retrieve audio header (metadata) information from a buffer. Suitable for use with 64-bit data size
	 *
	 * @param AudioData The audio data from which the header information will be retrieved
	 * @param Result Delegate broadcasting the result
	 */
	static void GetAudioHeaderInfoFromBuffer(TArray64<uint8> AudioData, const FOnGetAudioHeaderInfoResultNative& Result);

	/**
	 * Determine audio format based on audio data. A more advanced way to get the format. Suitable for use with 64-bit data size
	 *
	 * @param AudioData Audio data array
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	static ERuntimeAudioFormat GetAudioFormatAdvanced(const TArray64<uint8>& AudioData);

	/**
	 * Determine audio format based on audio data. A more advanced way to get the format. Suitable for use with 64-bit data size
	 *
	 * @param AudioData Audio data array
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	static ERuntimeAudioFormat GetAudioFormatAdvanced(const FRuntimeBulkDataBuffer<uint8>& AudioData);

	/**
	 * Convert seconds to string (hh:mm:ss or mm:ss depending on the number of seconds)
	 *
	 * @return hh:mm:ss or mm:ss string representation
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static FString ConvertSecondsToString(int64 Seconds);

	/**
	 * Scan the specified directory for audio files
	 *
	 * @param Directory The directory path to scan for audio files
	 * @param bRecursive Whether to search for files recursively in subdirectories
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Folder"), Category = "Runtime Audio Importer|Utilities")
	static void ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResult& Result);

	/**
	 * Scan the specified directory for audio files. Suitable for use in C++
	 *
	 * @param Directory The directory path to scan for audio files
	 * @param bRecursive Whether to search for files recursively in subdirectories
	 * @param Result Delegate broadcasting the result
	 */
	static void ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResultNative& Result);

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
