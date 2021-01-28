// Respirant 2020.
#pragma once

#include "Sound/SoundWave.h"
#include "Async/Async.h"
#include "RuntimeAudioImporterLibrary.generated.h"

/**
 * Possible audio importing results
 */
UENUM(BlueprintType, Category = "RuntimeAudioImporter")
enum TranscodingStatus
{
	SuccessTranscoding			UMETA(DisplayName = "Success"),
	FailedToOpenStream			UMETA(DisplayName = "Failed to open stream"),
	UnsupportedBitDepth			UMETA(DisplayName = "Unsupported bit depth (Unreal Engine only supports 16 bit)"),
	InvalidWAVEData				UMETA(DisplayName = "Invalid WAVE data"),
	InvalidBitrate				UMETA(DisplayName = "Invalid bitrate (Actual bit rate does not match the container size)"),
	InvalidSubformat			UMETA(DisplayName = "InvalidSubformat (Subformat identifier not recognized)"),
	UnrecognizedReadWaveError   UMETA(DisplayName = "Unrecognized ReadWave error"),
	USoundWaveDeclarationError	UMETA(DisplayName = "USoundWave declaration error"),
	InvalidUSoundWave			UMETA(DisplayName = "Invalid USoundWave (The USoundWave object contains incorrect data)"),
	InvalidAudioFormat			UMETA(DisplayName = "Invalid audio format (Can't determine the format of the audio file)"),
	AudioDoesNotExist			UMETA(DisplayName = "The audio file does not exist"),
	ZeroDurationError			UMETA(DisplayName = "Transcoded audio has zero length")
};

/**
 * Possible audio formats
 */
UENUM(BlueprintType, Category = "RuntimeAudioImporter")
enum AudioFormat
{
	MP3							UMETA(DisplayName = "mp3"),
	WAV							UMETA(DisplayName = "wav"),
	FLAC						UMETA(DisplayName = "flac"),
	INVALID						UMETA(DisplayName = "invalid (not defined format, internal use only)", Hidden)
};

/**
 * Declare delegate which will be called during the transcoding process
 *
 * @param Percentage							Percentage of download completed (0-100%)
 * @note										The impementation of dispatcher is not fully right. The data is taken approximately, and will not show exactly the correct percentage of transcoding progress
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgress, const int32, Percentage);

/**
 * Declare a delegate that will be called on the transcoding result
 *
 * @param ReadySoundWave						Ready USoundWave Object
 * @param Status								Reference to TranscodingStatus Enumerator in case an error occurs
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResult, const USoundWave*, ReadySoundWave, const TEnumAsByte < TranscodingStatus >, Status);

UCLASS(BlueprintType, Category = "RuntimeFilesDownloader")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioImporterLibrary : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Bind to know when the transcoding is on progress.
	 */
	UPROPERTY(BlueprintAssignable, Category = "RuntimeAudioImporter")
		FOnProgress OnProgress;

	/**
	 * Bind to know when the transcoding is complete (even if it fails).
	 */
	UPROPERTY(BlueprintAssignable, Category = "RuntimeAudioImporter")
		FOnResult OnResult;

	/**
	 * Instantiates a RuntimeAudioImporter object.
	 *
	 * @return									The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult events to know when it is in the process of transcoding and has been transcoded.
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV"), Category = "RuntimeAudioImporter")
		static URuntimeAudioImporterLibrary * CreateRuntimeAudioImporter();

	/**
	 * Transcode audio file to USoundWave static object
	 *
	 * @param filePath							Path to the audio file to import
	 * @param Format							Audio file format (extension)
	 * @param DefineFormatAutomatically			Whether to define format (extension) automatically or not
	 * @return									Returns itself.
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"), Category = "RuntimeAudioImporter")
		URuntimeAudioImporterLibrary * ImportAudioFromFile(const FString & filePath, TEnumAsByte < AudioFormat > Format, bool DefineFormatAutomatically);

	/**
	 * Destroy USoundWave static object. After the Soundwave is no longer needed, you need to call this function to free memory
	 *
	 * @param ReadySoundWave					SoundWave Object need to be destroyed
	 * @return									Whether the descruction of USoundWave was successful or not
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "MP3, FLAC, WAV, Destroy"), Category = "RuntimeAudioImporter")
		static bool DestroySoundWave(USoundWave * ReadySoundWave);

	/**
	 * Export (save) SoundWave object to .WAV audio file to device memory. So far beta and it is not recommended to use due to the fact that the engine can clear the sound data itself
	 *
	 * @param SoundWaveToExport					SoundWave object need to be exported
	 * @param PathToExport						Path where to save audio file
	 * @return									Whether the save was successfull or not
	 * @warning									It is very important that USoundWave should not used by the engine so that it can be read from physical memory.
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Exporter, Export, Save, Runtime, WAV"), Category = "RuntimeAudioImporter")
		static bool ExportSoundWaveToFile(USoundWave * SoundWaveToExport, FString PathToExport);

private:
	/**
	 * Internal main audio transcoding function
	 *
	 * @param filePath							Path to the audio file to import
	 * @param Format							Audio file format (extension)
	 * @param DefineFormatAutomatically			Whether to define format (extension) automatically or not
	 * @return									Returns itself
	 */
	URuntimeAudioImporterLibrary * GetUSoundWaveFromAudioFile_Internal(const FString & filePath, TEnumAsByte < AudioFormat > Format, bool DefineFormatAutomatically);

	/**
	 * Get USoundWave static object reference from 16-bit WAV data
	 *
	 * @param WaveData							Reference to memory location of 16-bit WAV byte data
	 * @param WaveDataSize						Memory size allocated for 16-bit WAV byte data
	 * @param Status							Reference to TranscodingStatus Enumerator in case an error occurs
	 * @return									Returns a ready USoundWave object
	 */
	class USoundWave* GetSoundWaveObject(const uint8 * WaveData, int32 WaveDataSize, TEnumAsByte < TranscodingStatus > & Status);

	/**
	 * Transcode Audio from File to PCM Data
	 *
	 * @param filePath							Path to the audio file from which to receive PCM data
	 * @param Format							Format of the audio file (e.g. mp3. flac, etc)
	 * @param Status							Reference to TranscodingStatus Enumerator in case an error occurs
	 * @param framesToWrite						PCM frames ready to be written
	 * @param pSampleData						Ready PCM sample data for which memory has been allocated
	 * @param channels							Quantity of channels (e.g. 1, 2, etc)
	 * @param sampleRate						Sample rate of the audio
	 * @return									Whether the transcoding was successful or not
	 */
	bool TranscodeAudioFileToPCMData(const char* filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus > & Status, uint64 & framesToWrite, int16_t * &pSampleData, uint32 & channels, uint32 & sampleRate);

	/**
	 * Transcode Audio from PCM to 16-bit WAV data
	 *
	 * @param WaveData							Reference to memory location of 16-bit WAV byte data
	 * @param WaveDataSize						Memory size allocated for 16-bit WAV byte data
	 * @param framesToWrite						PCM frames which will be written for WAV data
	 * @param pSampleData						Ready WAV sample data for which memory has been allocated
	 * @param channels							Quantity of channels (e.g. 1, 2, etc)
	 * @param sampleRate						Sample rate of the audio
	 * @return									Whether the transcoding was successful or not
	 */
	bool TranscodePCMToWAVData(void*& WaveData, size_t & WaveDataSize, uint64 framesToWrite, int16_t * &pSampleData, uint32 channels, uint32 sampleRate);

	/**
	 * @param filePath							File path, where to find the format (by extension)
	 * @return									Returns the found audio format (e.g. mp3. flac, etc) by AudioFormat Enumerator
	 */
	static TEnumAsByte<AudioFormat> GetAudioFormat(const FString & filePath);

	/**
	 * Audio transcoding progress callback
	 * @param Percentage						Percentage of download completion (0-100%)
	 * @note									The impementation of dispatcher is not fully right. The data is taken approximately, and will not show exactly the correct percentage of transcoding progress
	 */
	void OnProgress_Internal(int32 Percentage);

	/**
	 * Audio transcoding finished callback
	 * @param ReadySoundWave					A ready USoundWave object
	 * @param Status							Reference to TranscodingStatus Enumerator
	 */
	void OnResult_Internal(USoundWave * ReadySoundWave, TEnumAsByte < TranscodingStatus > Status);
};
