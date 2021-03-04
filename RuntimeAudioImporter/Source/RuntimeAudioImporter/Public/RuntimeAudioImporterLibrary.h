// Georgy Treshchev 2021.

#pragma once

#include "Sound/SoundWave.h"
#include "Async/Async.h"

#include "PreimportedSoundAsset.h"

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
	LoadFileToArrayError UMETA(DisplayName = "Load file to array error"),

	/** Transcoding PCM to Wav data error */
	PCMToWavDataError UMETA(DisplayName = "PCM to Wav data error"),

	/** Generate compressed data error */
	GenerateCompressedDataError UMETA(DisplayName = "Generate compressed data error"),

	/** Invalid compressed format error */
	InvalidCompressedFormat UMETA(DisplayName = "Invalid compressed format")
};

/** Possible audio formats (extensions) */
UENUM(BlueprintType, Category = "RuntimeAudioImporter")
enum EAudioFormat
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
	Invalid UMETA(DisplayName = "invalid (not defined format, CPP use only)", Hidden)
};

/** List of all supported compression formats */
UENUM(BlueprintType, Category = "RuntimeAudioImporter")
enum ECompressionFormat
{
	/** Raw (Wav) data format. Not recommended because it doesn't work on packaged build */
	RawData UMETA(DisplayName = "Raw Data"),
	
	/** Ogg Vorbis format. It is used, for example, on Android and Windows */
	OggVorbis UMETA(DisplayName = "Ogg Vorbis"),

	/** ADPCM or LPCM formats. LPCM will be used if the compression quality is 100%, otherwise ADPCM */
	ADPCMLPCM UMETA(DisplayName = "ADPCM/LPCM"),

	/** Ogg Opus format. Mostly not used anywhere, but can be used for personal purposes */
	OggOpus UMETA(DisplayName = "Ogg Opus")
};

/** Information about which buffers to fill */
USTRUCT(BlueprintType)
struct FBuffersDetailsStruct
{
	GENERATED_BODY()

	/** Needed compression format */
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<ECompressionFormat> CompressionFormat = OggVorbis;

	/**
	 * Sound compression quality. The range of numbers is from 0 to 100, in percent. How much to compress the sound, where 0 is the maximum loss, 100 is the minimum.
	 * Not used if CompressionFormat is "RawData"
	 */
	UPROPERTY(BlueprintReadWrite)
	int32 SoundCompressionQuality = 100;
};

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

/** Raw PCM Data */
struct FPCMStruct
{
	/** Raw PCM data pointer */
	uint8* RawPCMData;

	/** Number of PCM frames */
	uint64 RawPCMNumOfFrames;

	/** Raw PCM data size */
	uint32 RawPCMDataSize;
};

/** Wav (or Raw) Data */
struct FWAVStruct
{
	/** Wave data pointer */
	uint8* WaveData;

	/** Wave data size */
	int32 WaveDataSize;
};

/** Compressed Data (Ogg Vorbis, ADPCM, etc) */
struct FCompressedStruct
{
	/** Array, which contains compressed format data */
	TArray<uint8> CompressedFormatData;
};

/** Main, mostly in-memory information (like PCM, Wav, etc) */
struct FTranscodingFillStruct
{
	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** Raw PCM Data info */
	FPCMStruct PCMInfo;

	/** Wav (or Raw) Data info */
	FWAVStruct WAVInfo;

	/** Compressed Data info (Ogg Vorbis, ADPCM, etc) */
	FCompressedStruct CompressedInfo;
};

// Forward declaration of the FSoundQualityInfo structure
struct FSoundQualityInfo;

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
                                               RuntimeAudioImporterObjectRef,
                                               const USoundWave*, SoundWaveRef,
                                               const TEnumAsByte < ETranscodingStatus >&, Status);

/**
 * Runtime Audio Importer object
 * Allows you to do various things with audio files in real time, for example, import audio files into a SoundWave engine object
 */
UCLASS(BlueprintType, Category = "RuntimeAudioImporter")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioImporterLibrary : public UObject
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

	/** Buffers details info. CPP use only */
	FBuffersDetailsStruct BuffersDetailsInfo = FBuffersDetailsStruct();

	/**
	 * Instantiates a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult events to know when it is in the process of transcoding and imported
	 * @note You must place the returned reference to the RuntimeAudioImporterLibrary in a separate variable so that the UObject is not removed during garbage collection
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV"), Category =
		"RuntimeAudioImporter")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Transcode audio file to SoundWave object
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format Audio file format (extension)
	 * @param BuffersDetails Information about which buffers to fill
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"),
		Category = "RuntimeAudioImporter")
	void ImportAudioFromFile(const FString& FilePath, TEnumAsByte<EAudioFormat> Format,
	                         const FBuffersDetailsStruct& BuffersDetails);

	/**
	 * Import audio file from the preimported sound asset
	 *
	 * @param PreimportedSoundAssetRef PreimportedSoundAsset object reference. Should contains "BaseAudioDataArray" buffer
	 * @param BuffersDetails Information about which buffers to fill
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3"),
		Category = "RuntimeAudioImporter")
	void ImportAudioFromPreimportedSound(UPreimportedSoundAsset* PreimportedSoundAssetRef,
	                                     const FBuffersDetailsStruct& BuffersDetails);


	/**
	* Transcode audio file to USoundWave static object
	*
	* @param AudioDataArray Array of Audio byte data
	* @param Format Audio file format (extension)
	* @param BuffersDetails Information about which buffers to fill
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"),
		Category = "RuntimeAudioImporter")
	void ImportAudioFromBuffer(const TArray<uint8>& AudioDataArray, const TEnumAsByte<EAudioFormat>& Format,
	                           const FBuffersDetailsStruct& BuffersDetails);

	/**
	 * Destroy SoundWave object. After the SoundWave is no longer needed, you need to call this function to free memory
	 *
	 * @param ReadySoundWave SoundWave object reference need to be destroyed
	 * @return Whether the destruction of the SoundWave was successful or not
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "MP3, FLAC, WAV, Destroy"), Category = "RuntimeAudioImporter")
	static bool DestroySoundWave(USoundWave* ReadySoundWave);

private:
	/**
	 * Internal main audio transcoding function
	 *
	 * @param AudioDataArray Array of Audio byte data
	 * @param Format Audio file format (extension)
	 * @param BuffersDetails Information about which buffers to fill
	 */
	void ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioDataArray,
	                                    const TEnumAsByte<EAudioFormat>& Format,
	                                    const FBuffersDetailsStruct& BuffersDetails);

	/**
	 * Define SoundWave object reference
	 *
	 * @return Returns a ready USoundWave object
	 */
	USoundWave* DefineSoundWave();

	/**
	 * Fill SoundWave basic information (e.g. duration, number of channels, etc)
	 *
	 * @param SoundWaveRef SoundWave object reference
	 */
	void FillSoundWaveBasicInfo(USoundWave* SoundWaveRef) const;

	/**
	 * Fill SoundWave Raw (Wave) data buffer
	 *
	 * @param SoundWaveRef SoundWave object reference
	 */
	void FillRawWaveData(USoundWave* SoundWaveRef) const;

	/**
	 * Fill SoundWave compressed data buffer
	 *
	 * @param SoundWaveRef SoundWave object reference
	 */
	void FillCompressedData(USoundWave* SoundWaveRef);

	/**
	 * Transcode Audio from PCM to 16-bit WAV data
	 *
	 * @return Whether the transcoding was successful or not 
	 */
	bool TranscodePCMToCompressedData(USoundWave* SoundWaveToGetData);

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
	 * Transcode Audio from PCM to 16-bit WAV data
	 *
	 * @return Whether the transcoding was successful or not 
	 */
	bool TranscodePCMToWAVData();

	/**
	 * Get Audio Format by extension
	 *
	 * @param FilePath File path, where to find the format (by extension)
	 * @return Returns the found audio format (e.g. mp3. flac, etc) by EAudioFormat Enum
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeAudioImporter")
	static TEnumAsByte<EAudioFormat> GetAudioFormat(const FString& FilePath);

	/**
	 * Generate FSoundQualityInfo Enum based on SoundWave and already defined Sample Rate
	 */
	FSoundQualityInfo GenerateSoundQualityInfo(USoundWave* SoundWaveToGetData) const;

	/**
	 * Get the platform-specific format name based on the base format to find the compressed buffer to fill
	 *
	 * @param Format Base format name (it can be "OGG", "LPCM", etc)
	 */
	static FName GetPlatformSpecificFormat(const FName Format);

	/**
	 * Audio transcoding progress callback
	 * @param Percentage Percentage of importing completion (0-100%)
	 */
	void OnProgress_Internal(int32 Percentage);

	/**
	 * Audio transcoding finished callback
	 * 
	 * @param ReadySoundWave A ready SoundWave object
	 * @param Status TranscodingStatus Enum in case an error occurs
	 */
	void OnResult_Internal(USoundWave* ReadySoundWave, const TEnumAsByte<ETranscodingStatus>& Status);
};
