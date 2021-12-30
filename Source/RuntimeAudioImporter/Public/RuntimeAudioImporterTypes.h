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

/** PCM Data buffer structure */
struct FPCMStruct
{
	/** 32-bit float PCM data */
	uint8* PCMData;

	/** Number of PCM frames */
	uint64 PCMNumOfFrames;

	/** PCM data size */
	uint32 PCMDataSize;

	/** Base constructor */
	FPCMStruct()
		: PCMData(nullptr), PCMNumOfFrames(0), PCMDataSize(0)
	{
	}
};

/** Decoded audio information */
struct FDecodedAudioStruct
{
	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** PCM Data buffer */
	FPCMStruct PCMInfo;
};

/** Encoded audio information */
struct FEncodedAudioStruct
{
	/** Pointer to memory location of the audio data */
	uint8* AudioData;

	/** Memory size allocated for the audio data */
	uint32 AudioDataSize;

	/** Format of the audio data (e.g. mp3, flac, etc) */
	EAudioFormat AudioFormat;

	/** Base constructor */
	FEncodedAudioStruct()
		: AudioData(nullptr), AudioDataSize(0), AudioFormat(EAudioFormat::Invalid)
	{
	}

	FEncodedAudioStruct(uint8* InAudioData, uint32 InAudioDataSize, EAudioFormat InAudioFormat)
		: AudioData(InAudioData), AudioDataSize(InAudioDataSize), AudioFormat(InAudioFormat)
	{
	}
};
