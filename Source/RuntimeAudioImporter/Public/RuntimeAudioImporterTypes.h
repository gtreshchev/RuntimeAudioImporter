// Georgy Treshchev 2022.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Sound/SoundGroups.h"
#include "RuntimeAudioImporterDefines.h"
#include "Serialization/BulkDataBuffer.h"

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
	Auto UMETA(DisplayName = "Determine format automatically"),
	Mp3 UMETA(DisplayName = "mp3"),
	Wav UMETA(DisplayName = "wav"),
	Flac UMETA(DisplayName = "flac"),
	OggVorbis UMETA(DisplayName = "ogg vorbis"),
	Invalid UMETA(DisplayName = "invalid (not defined format, internal use only)", Hidden)
};

/** Possible RAW (uncompressed) audio formats */
UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ERAWAudioFormat : uint8
{
	Int16 UMETA(DisplayName = "Signed 16-bit PCM"),
	Int32 UMETA(DisplayName = "Signed 32-bit PCM"),
	UInt8 UMETA(DisplayName = "Unsigned 8-bit PCM"),
	Float32 UMETA(DisplayName = "32-bit float")
};

/** Basic sound wave data */
struct FSoundWaveBasicStruct
{
	/** Number of channels */
	uint32 NumOfChannels;

	/** Sample rate (samples per second, sampling frequency) */
	uint32 SampleRate;

	/** Sound wave duration, sec */
	float Duration;

	FSoundWaveBasicStruct()
		: NumOfChannels(0), SampleRate(0), Duration(0)
	{}

	/**
	 * Whether the sound wave data appear to be valid or not
	 */
	bool IsValid() const
	{
		return NumOfChannels > 0 && Duration > 0;
	}

	/**
	 * Converts the basic sound wave struct to a readable format
	 *
	 * @return String representation of the basic sound wave struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Number of channels: %d, sample rate: %d, duration: %f"), NumOfChannels, SampleRate, Duration);
	}
};

/** PCM data buffer structure */
struct FPCMStruct
{
public:
	/** 32-bit float PCM data */
	FBulkDataBuffer<float> PCMData;

	/** Number of PCM frames */
	uint32 PCMNumOfFrames;

	FPCMStruct()
		: PCMNumOfFrames(0)
	{
	}

	/**
	 * Whether the PCM data appear to be valid or not
	 */
	bool IsValid() const
	{
		return PCMData.GetView().GetData() && PCMNumOfFrames > 0 && PCMData.GetView().Num() > 0;
	}

	/**
	 * Converts PCM struct to a readable format
	 *
	 * @return String representation of the PCM Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Validity of PCM data in memory: %s, number of PCM frames: %d, PCM data size: %lld"),
		                       PCMData.GetView().IsValidIndex(0) ? TEXT("Valid") : TEXT("Invalid"), PCMNumOfFrames, static_cast<int64>(PCMData.GetView().Num()));
	}
};

/** Decoded audio information */
struct FDecodedAudioStruct
{
	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** PCM data buffer */
	FPCMStruct PCMInfo;

	/**
	 * Whether the decoded audio data appear to be valid or not
	 */
	bool IsValid() const
	{
		return SoundWaveBasicInfo.IsValid() && PCMInfo.IsValid();
	}

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
	/** Audio data */
	FBulkDataBuffer<uint8> AudioData;

	/** Format of the audio data (e.g. mp3, flac, etc) */
	EAudioFormat AudioFormat;

	/** Base constructor */
	FEncodedAudioStruct()
		: AudioFormat{EAudioFormat::Invalid}
	{
	}

	/** Custom constructor */
	FEncodedAudioStruct(uint8* AudioData, int32 AudioDataSize, EAudioFormat AudioFormat)
		: AudioData(AudioData, AudioDataSize)
	  , AudioFormat(AudioFormat)
	{
	}

	/**
	 * Converts Encoded Audio Struct to a readable format
	 *
	 * @return String representation of the Encoded Audio Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Validity of audio data in memory: %s, audio data size: %lld, audio format: %s"),
		                       AudioData.GetView().IsValidIndex(0) ? TEXT("Valid") : TEXT("Invalid"), static_cast<int64>(AudioData.GetView().Num()),
		                       *UEnum::GetValueAsName(AudioFormat).ToString());
	}
};

/** Compressed sound wave information */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FCompressedSoundWaveInfo
{
	GENERATED_BODY()

	/** Sound group */
	UPROPERTY(BlueprintReadWrite, Category = "Runtime Audio Importer")
	TEnumAsByte<ESoundGroup> SoundGroup;

	/** If set, when played directly (not through a sound cue) the wave will be played looping */
	UPROPERTY(BlueprintReadWrite, Category = "Runtime Audio Importer")
	bool bLooping;

	/** Playback volume of sound 0 to 1 - Default is 1.0 */
	UPROPERTY(BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "Runtime Audio Importer")
	float Volume;

	/** Playback pitch for sound. */
	UPROPERTY(BlueprintReadWrite, meta = (ClampMin = "0.125", ClampMax = "4.0"), Category = "Runtime Audio Importer")
	float Pitch;

	/** Base constructor */
	FCompressedSoundWaveInfo()
		: SoundGroup(ESoundGroup::SOUNDGROUP_Default)
	  , bLooping(false)
	  , Volume(1.f)
	  , Pitch(1.f)
	{
	}
};

/** A line of subtitle text and the time at which it should be displayed. This is the same as FSubtitleCue but editable in Blueprints */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FEditableSubtitleCue
{
	GENERATED_BODY()

	/** The text to appear in the subtitle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	FText Text;

	/** The time at which the subtitle is to be displayed, in seconds relative to the beginning of the line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	float Time;

	/** Base constructor */
	FEditableSubtitleCue()
		: Time(0)
	{
	}
};
