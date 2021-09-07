// Georgy Treshchev 2021.

#pragma once

#include "ImportedSoundWave.h"
#include "RuntimeAudioImporterLibrary.generated.h"

/** Possible audio importing results */
UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ETranscodingStatus : uint8
{
	/** Successful import */
	SuccessfulImport UMETA(DisplayName = "Success"),

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
UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class EAudioFormat : uint8
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

UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ERAWAudioFormat : uint8
{
	Int16 UMETA(DisplayName = "Signed 16-bit PCM"),
	Int32 UMETA(DisplayName = "Signed 32-bit PCM"),
	UInt8 UMETA(DisplayName = "Unsigned 8-bit PCM"),
	Float32 UMETA(DisplayName = "32-bit float")
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

/** Main, mostly in-memory information (like PCM, Wav, etc) */
struct FTranscodingFillStruct
{
	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** PCM Data buffer */
	FPCMStruct PCMInfo;
};

// Forward declaration of the UPreImportedSoundAsset class
class UPreImportedSoundAsset;

/** Delegate broadcast to get the audio importer progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgress, const int32, Percentage);

/** Delegate broadcast to get the audio importer result */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResult, class URuntimeAudioImporterLibrary*,
                                               RuntimeAudioImporterObjectRef, UImportedSoundWave*, SoundWaveRef,
                                               const ETranscodingStatus&, Status);

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
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer")
	FOnAudioImporterProgress OnProgress;

	/** Bind to know when the transcoding is complete (even if it fails) */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer")
	FOnAudioImporterResult OnResult;

	/** Transcoding fill info. CPP use only */
	FTranscodingFillStruct TranscodingFillInfo;

	/**
	 * Instantiates a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult delegates to know when it is in the process of importing and imported
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV"),
		Category = "Runtime Audio Importer")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Import audio from file
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format Audio file format (extension)
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"),
		Category = "Runtime Audio Importer")
	void ImportAudioFromFile(const FString& FilePath, EAudioFormat Format);

	/**
	 * Import audio file from the pre-imported sound asset
	 *
	 * @param PreImportedSoundAssetRef PreImportedSoundAsset object reference. Should contain "BaseAudioDataArray" buffer
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3"),
		Category = "Runtime Audio Importer")
	void ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef);

	/**
	 * Import audio from buffer
	 *
	 * @param AudioDataBuffer Buffer of the audio data
	 * @param Format Audio file format (extension)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From Buffer", Keywords =
		"Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV"), Category = "Runtime Audio Importer")
	void ImportAudioFromBuffer_BP(TArray<uint8> AudioDataBuffer, EAudioFormat Format);

	/**
	 * Import audio from buffer
	 *
	 * @param AudioDataBuffer Buffer of the audio data
	 * @param Format Audio file format (extension)
	 */
	void ImportAudioFromBuffer(TArray<uint8>& AudioDataBuffer, const EAudioFormat& Format);

	/**
	 * Import audio from RAW file. Audio data must not have headers and must be uncompressed
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer")
	void ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat Format, const int32 SampleRate = 44100,
								const int32 NumOfChannels = 1);

	/**
	 * Import audio from RAW buffer. Audio data must not have headers and must be uncompressed
	 *
	 * @param RAWBuffer RAW audio buffer
	 * @param Format RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer")
	void ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format, const int32 SampleRate = 44100,
								const int32 NumOfChannels = 1);
	/**
	 * Import audio from 32-bit float PCM data
	 *
	 * @param PCMData Pointer to memory location of the PCM data
	 * @param PCMDataSize Memory size allocated for the PCM data
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	void ImportAudioFromFloat32Buffer(uint8* PCMData, const uint32 PCMDataSize, const int32 SampleRate = 44100,
									const int32 NumOfChannels = 1);

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
	 * Check if the WAV audio data with the RIFF container has a correct byte size.
	 * Made by https://github.com/kass-kass
	 *
	 * @param WavData Buffer of the wav data
	 */
	bool CheckAndFixWavDurationErrors(TArray<uint8>& WavData);


	/**
	 * Transcoding RAW data to interleaved 32-bit floating point data
	 *
	 * @param RAWData Pointer to memory location of the RAW data
	 * @param RAWDataSize Memory size allocated for the RAW data
	 * @param PCMData Returning pointer to memory location of the 32-bit float data
	 * @param PCMDataSize Returning memory size allocated for the 32-bit float data
	 */
	template <typename IntegralType>
	void TranscodeRAWDataTo32FloatData(IntegralType* RAWData, uint32 RAWDataSize, uint8*& PCMData, uint32& PCMDataSize);

	/**
	 * Getting a divisor to transcode data to 32-bit float
	 *
	 * @return Divisor to transcode from the IntegralType to the interleaved 32-bit floating point
	 */
	template <typename IntegralType>
	float GetRawToPcmDivisor();

	/**
	 * Transcode Audio from Audio Data to PCM Data
	 *
	 * @param AudioData Pointer to memory location of the audio data
	 * @param AudioDataSize Memory size allocated for the audio data
	 * @param Format Format of the audio file (e.g. mp3. flac, etc)
	 * @return Whether the transcoding was successful or not
	 */
	bool TranscodeAudioDataArrayToPCMData(const uint8* AudioData, uint32 AudioDataSize, const EAudioFormat& Format);

	/**
	 * Get audio format by extension
	 *
	 * @param FilePath File path where to find the format (by extension)
	 * @return Returns the found audio format (e.g. mp3. flac, etc) by AudioFormat Enum
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer")
	static EAudioFormat GetAudioFormat(const FString& FilePath);

	/**
	 * Audio transcoding progress callback
	 * 
	 * @param Percentage Percentage of importing completion (0-100%)
	 */
	void OnProgress_Internal(const int32& Percentage);

	/**
	 * Audio importing finished callback
	 * 
	 * @param SoundWaveRef A ready SoundWave object
	 * @param Status Importing status
	 */
	void OnResult_Internal(UImportedSoundWave* SoundWaveRef, const ETranscodingStatus& Status);
};
