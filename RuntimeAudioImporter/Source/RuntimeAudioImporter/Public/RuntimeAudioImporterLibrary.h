// Georgy Treshchev 2021.

#pragma once

#include "Async/Async.h"
#include "ImportedSoundWave.h"
#include "RuntimeAudioImporterLibrary.generated.h"

/** Possible audio importing results */
UENUM(BlueprintType, Category = "RuntimeAudioImporter")
enum ETranscodingStatus
{
	/** Success importing */
	SuccessImporting UMETA(DisplayName = "Success Importing"),

	/** Failed to read Audio Data Array */
	FailedToReadAudioDataArray UMETA(DisplayName = "Failed to read Audio Data Array"),

	/** SoundWave declaration error */
	SoundWaveDeclarationError UMETA(DisplayName = "SoundWave declaration error"),

	/** Invalid audio format (Can't determine the format of the audio file) */
	InvalidAudioFormat UMETA(DisplayName = "Invalid audio format"),

	/** The audio file does not exist */
	AudioDoesNotExist UMETA(DisplayName = "Audio does not exist"),

	/** Load file to array error */
	LoadFileToArrayError UMETA(DisplayName = "Load file to array error")
};

/** Possible audio formats (extensions) */
UENUM(BlueprintType, Category = "RuntimeAudioImporter")enum EAudioFormat
{
	/** Determine format automatically */
	Auto UMETA(DisplayName = "Determine format automatically"),

	/** MP3 format */
	Mp3 UMETA(DisplayName = "mp3"),

	/** WAV format */
	Wav UMETA(DisplayName = "wav"),

	/** FLAC format */
	Flac UMETA(DisplayName = "flac"),

	/** Invalid format */
	Invalid UMETA(DisplayName = "invalid (not defined format, CPP use only)", Hidden)};

/** Basic SoundWave data. CPP use only. */
struct FSoundWaveBasicStruct
{
	/** Number of channels */
	int32 ChannelsNum;

	/** Sample rate (samples per second, sampling frequency) */
	uint32 SampleRate;

	/** Sound wave duration, sec */
	float Duration;
};

/** Main, mostly in-memory information (like PCM, Wav, etc) */
struct FTranscodingFillStruct
{
	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** float PCM info */
	FPCMStruct PCMInfo;
};

// Forward declaration of the UPreimportedSoundAsset class
class UPreimportedSoundAsset;

/**
 * Declare delegate which will be called during the transcoding process
 *
 * @param Percentage Percentage of importing completed (0-100%)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgress, const int32, Percentage);

/**
 * Declare a delegate that will be called on the transcoding result
 * 
 * @param RuntimeAudioImporterObject Runtime Audio Importer object reference
 * @param ReadySoundWave Ready SoundWave object reference
 * @param Status TranscodingStatus Enum in case an error occurs
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnResult, class URuntimeAudioImporterLibrary*,
                                               RuntimeAudioImporterObjectRef, UImportedSoundWave*, SoundWaveRef,
                                               const TEnumAsByte < ETranscodingStatus >&, Status);

/**
 * Runtime Audio Importer object
 * Allows you to do various things with audio files in real time, for example, import audio files into a SoundWave engine object
 */
UCLASS(BlueprintType, Category = "RuntimeAudioImporter")class RUNTIMEAUDIOIMPORTER_API
	URuntimeAudioImporterLibrary : public UObject
{
	GENERATED_BODY()
public:

	/** Bind to know when the transcoding is on progress */
	UPROPERTY(BlueprintAssignable, Category = "RuntimeAudioImporter")
	FOnProgress OnProgress;

	/** Bind to know when the transcoding is complete (even if it fails) */
	UPROPERTY(BlueprintAssignable, Category = "RuntimeAudioImporter")
	FOnResult OnResult;

	/** Transcoding fill info. CPP use only */
	FTranscodingFillStruct TranscodingFillInfo = FTranscodingFillStruct();

	/**
	 * Instantiates a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult delegates to know when it is in the process of importing and imported
	 * @note You must place the returned RuntimeAudioImporterLibrary reference in a separate variable so that this object will not be removed during the garbage collection
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV"), Category =
		"RuntimeAudioImporter")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Import audio from file
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format Audio file format (extension)
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"),
		Category = "RuntimeAudioImporter")
	void ImportAudioFromFile(const FString& FilePath,
	                         TEnumAsByte<EAudioFormat> Format);

	/**
	 * Import audio file from the preimported sound asset
	 *
	 * @param PreimportedSoundAssetRef PreimportedSoundAsset object reference. Should contains "BaseAudioDataArray" buffer
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3"), Category =
		"RuntimeAudioImporter")
	void ImportAudioFromPreimportedSound(UPreimportedSoundAsset* PreimportedSoundAssetRef);

	/**
	* Import audio data to SoundWave static object
	*
	* @param AudioDataArray Array of Audio byte data
	* @param Format Audio file format (extension)
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"),
		Category = "RuntimeAudioImporter")
	void ImportAudioFromBuffer(const TArray<uint8>& AudioDataArray,
	                           const TEnumAsByte<EAudioFormat>& Format);
private:
	/**
	 * Internal main audio importing method
	 *
	 * @param AudioDataArray Array of Audio byte data
	 * @param Format Audio file format (extension)
	 */
	void ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioDataArray, const TEnumAsByte<EAudioFormat>& Format);

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
	 * @param AudioData Pointer to memory location of Audio byte data
	 * @param AudioDataSize Memory size allocated for Audio byte data
	 * @param Format Format of the audio file (e.g. mp3. flac, etc)
	 * @return Whether the transcoding was successful or not
	 */
	bool TranscodeAudioDataArrayToPCMData(const uint8* AudioData, uint32 AudioDataSize,
	                                      TEnumAsByte<EAudioFormat> Format);

	/**
	 * Get audio format by extension
	 *
	 * @param FilePath File path, where to find the format (by extension)
	 * @return Returns the found audio format (e.g. mp3. flac, etc) by EAudioFormat Enum
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	static TEnumAsByte<EAudioFormat> GetAudioFormat(
		const FString& FilePath);

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
	void OnResult_Internal(UImportedSoundWave* SoundWaveRef, const TEnumAsByte<ETranscodingStatus>& Status);
};
