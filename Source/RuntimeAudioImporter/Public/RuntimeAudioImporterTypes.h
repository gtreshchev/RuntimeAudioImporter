// Georgy Treshchev 2022.

#pragma once

#include "Engine/EngineBaseTypes.h"

#include "RuntimeAudioImporterTypes.generated.h"

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

	/** OGG Vorbis format */
	OggVorbis UMETA(DisplayName = "ogg vorbis"),

	/** OGG Opus format */
	OggOpus UMETA(DisplayName = "ogg opus (not supported for decoding yet)"),

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

UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ESoundWaveType : uint8
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
	uint32 NumOfChannels;

	/** Sample rate (samples per second, sampling frequency) */
	uint32 SampleRate;

	/** Sound wave duration, sec */
	float Duration;

	/**
	 * Converts Sound Wave Basic Struct to a readable format
	 *
	 * @return String representation of the Sound Wave Basic Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Number of channels: %d, sample rate: %d, duration: %f"), NumOfChannels, SampleRate, Duration);
	}
};

/** PCM Data buffer structure */
struct FPCMStruct
{
	/** 32-bit float PCM data */
	uint8* PCMData;

	/** Number of PCM frames */
	uint32 PCMNumOfFrames;

	/** PCM data size */
	uint32 PCMDataSize;

	/** Base constructor */
	FPCMStruct()
		: PCMData{nullptr}, PCMNumOfFrames{0}, PCMDataSize{0}
	{
	}

	/**
	 * Converts PCM Struct to a readable format
	 *
	 * @return String representation of the PCM Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Validity of PCM data in memory: %s, number of PCM frames: %d, PCM data size: %d"),
		                       PCMData != nullptr ? TEXT("Valid") : TEXT("Invalid"), PCMNumOfFrames, PCMDataSize);
	}
};

/** Decoded audio information */
struct FDecodedAudioStruct
{
	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** PCM Data buffer */
	FPCMStruct PCMInfo;

	/**
	 * Converts Decoded Audio Struct to a readable format
	 *
	 * @return String representation of the Decoded Audio Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("SoundWave Basic Info:\n%s\n\nPCM Info:\n%s"), *SoundWaveBasicInfo.ToString(), *PCMInfo.ToString());
	}
};

/** Encoded audio information */
struct FEncodedAudioStruct
{
	/** Pointer to memory location of the audio data */
	uint8* AudioData;

	/** Memory size allocated for the audio data */
	int32 AudioDataSize;

	/** Format of the audio data (e.g. mp3, flac, etc) */
	EAudioFormat AudioFormat;

	/** Base constructor */
	FEncodedAudioStruct()
		: AudioData{nullptr}, AudioDataSize{0}, AudioFormat{EAudioFormat::Invalid}
	{
	}

	FEncodedAudioStruct(uint8* AudioData, int32 AudioDataSize, EAudioFormat AudioFormat)
		: AudioData{AudioData}, AudioDataSize{AudioDataSize}, AudioFormat{AudioFormat}
	{
	}

	/**
	 * Converts Encoded Audio Struct to a readable format
	 *
	 * @return String representation of the Encoded Audio Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Validity of audio data in memory: %s, audio data size: %d, audio format: %s"),
		                       AudioData != nullptr ? TEXT("Valid") : TEXT("Invalid"), AudioDataSize, *UEnum::GetValueAsName(AudioFormat).ToString());
	}
};
