// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAudioImporterLibrary.h"
#include "UObject/Object.h"
#include "RuntimeAudioExporter.generated.h"

/** Static delegate broadcasting the result of the audio export to buffer */
DECLARE_DELEGATE_TwoParams(FOnAudioExportToBufferResultNative, bool, const TArray64<uint8>&);

/** Dynamic delegate broadcasting the result of the audio export to buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAudioExportToBufferResult, bool, bSucceeded, const TArray<uint8>&, AudioData);

/** Static delegate broadcasting the result of the audio export to file */
DECLARE_DELEGATE_OneParam(FOnAudioExportToFileResultNative, bool);

/** Dynamic delegate broadcasting the result of the audio export to file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAudioExportToFileResult, bool, bSucceeded);


/**
 * Runtime Audio Exporter
 * Contains functions for exporting audio data to a file or a buffer
 */
UCLASS(BlueprintType, Category = "Runtime Audio Exporter")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioExporter : public UObject
{
	GENERATED_BODY()

public:
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
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Exporter")
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
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Exporter")
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
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Export Sound Wave To RAW File"), Category = "Runtime Audio Exporter")
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
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Export Sound Wave To RAW Buffer"), Category = "Runtime Audio Exporter")
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
};
