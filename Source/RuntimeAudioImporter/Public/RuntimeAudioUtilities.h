// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAudioImporterTypes.h"
#include "UObject/Object.h"
#include "RuntimeAudioUtilities.generated.h"

/** Static delegate broadcasting the result of retrieving audio header info */
DECLARE_DELEGATE_TwoParams(FOnGetAudioHeaderInfoResultNative, bool, FRuntimeAudioHeaderInfo);

/** Dynamic delegate broadcasting the result of retrieving audio header info */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGetAudioHeaderInfoResult, bool, bSucceeded, const FRuntimeAudioHeaderInfo&, HeaderInfo);


/** Dynamic delegate broadcasting the result of scanning directory for audio files */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnScanDirectoryForAudioFilesResult, bool, bSucceeded, const TArray<FString>&, AudioFilePaths);

/** Static delegate broadcasting the result of scanning directory for audio files */
DECLARE_DELEGATE_TwoParams(FOnScanDirectoryForAudioFilesResultNative, bool, const TArray<FString>&);


/**
 * Runtime Audio Utilities
 * Contains various functions for working with audio data, including retrieving audio header info, scanning directories for audio files, and more
 */
UCLASS(BlueprintType, Category = "Runtime Audio Utilities")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioUtilities : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get the audio format based on file extension
	 *
	 * @param FilePath File path where to find the format (by extension)
	 * @return The found audio formats (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Utilities|Utilities")
	static TArray<ERuntimeAudioFormat> GetAudioFormats(const FString& FilePath);

	/**
	 * Determine the audio format based on audio data. A more advanced way to get the format
	 *
	 * @param AudioData Audio data array
	 * @return The found audio formats (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Utilities|Utilities")
	static TArray<ERuntimeAudioFormat> GetAudioFormatsAdvanced(const TArray<uint8>& AudioData);

	/**
	 * Retrieve audio header (metadata) information from a file
	 *
	 * @param FilePath The path to the audio file from which header information will be retrieved
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Utilities|Utilities")
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
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Utilities|Utilities")
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
	 * @return The found audio formats (e.g. mp3. flac, etc)
	 */
	static TArray<ERuntimeAudioFormat> GetAudioFormatsAdvanced(const TArray64<uint8>& AudioData);

	/**
	 * Determine audio format based on audio data. A more advanced way to get the format. Suitable for use with 64-bit data size
	 *
	 * @param AudioData Audio data array
	 * @return The found audio formats (e.g. mp3. flac, etc)
	 */
	static TArray<ERuntimeAudioFormat> GetAudioFormatsAdvanced(const FRuntimeBulkDataBuffer<uint8>& AudioData);

	/**
	 * Convert seconds to string (hh:mm:ss or mm:ss depending on the number of seconds)
	 *
	 * @return hh:mm:ss or mm:ss string representation
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Utilities")
	static FString ConvertSecondsToString(int64 Seconds);

	/**
	 * Scan the specified directory for audio files
	 *
	 * @param Directory The directory path to scan for audio files
	 * @param bRecursive Whether to search for files recursively in subdirectories
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Folder"), Category = "Runtime Audio Utilities")
	static void ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResult& Result);

	/**
	 * Scan the specified directory for audio files. Suitable for use in C++
	 *
	 * @param Directory The directory path to scan for audio files
	 * @param bRecursive Whether to search for files recursively in subdirectories
	 * @param Result Delegate broadcasting the result
	 */
	static void ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResultNative& Result);
};
