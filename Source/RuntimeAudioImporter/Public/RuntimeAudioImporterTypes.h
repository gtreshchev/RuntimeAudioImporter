// Georgy Treshchev 2023.

#pragma once

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#include "AudioCaptureDeviceInterface.h"
#endif
#include "Engine/EngineBaseTypes.h"
#include "Sound/SoundGroups.h"
#include "Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25
#include "DSP/BufferVectorOperations.h"
#endif

#include "RuntimeAudioImporterTypes.generated.h"

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25
namespace Audio
{
	using FAlignedFloatBuffer = Audio::AlignedFloatBuffer;
}
#endif

/** Possible audio importing results */
UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ERuntimeImportStatus : uint8
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
enum class ERuntimeAudioFormat : uint8
{
	Auto UMETA(DisplayName = "Determine format automatically"),
	Mp3 UMETA(DisplayName = "mp3"),
	Wav UMETA(DisplayName = "wav"),
	Flac UMETA(DisplayName = "flac"),
	OggVorbis UMETA(DisplayName = "ogg vorbis"),
	Bink UMETA(DisplayName = "bink"),
	Invalid UMETA(DisplayName = "invalid (not defined format, internal use only)", Hidden)
};

/** Possible RAW (uncompressed, PCM) audio formats */
UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ERuntimeRAWAudioFormat : uint8
{
	Int8 UMETA(DisplayName = "Signed 8-bit integer"),
	UInt8 UMETA(DisplayName = "Unsigned 8-bit integer"),
	Int16 UMETA(DisplayName = "Signed 16-bit integer"),
	UInt16 UMETA(DisplayName = "Unsigned 16-bit integer"),
	Int32 UMETA(DisplayName = "Signed 32-bit integer"),
	UInt32 UMETA(DisplayName = "Unsigned 32-bit integer"),
	Float32 UMETA(DisplayName = "Floating point 32-bit")
};

/**
 * An alternative to FBulkDataBuffer with consistent data types
 */
template <typename DataType>
class FRuntimeBulkDataBuffer
{
public:
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
	using ViewType = TArrayView<DataType>;
#else
	using ViewType = TArrayView64<DataType>;
#endif

	FRuntimeBulkDataBuffer() = default;

	FRuntimeBulkDataBuffer(const FRuntimeBulkDataBuffer& Other)
	{
		*this = Other;
	}

	FRuntimeBulkDataBuffer(FRuntimeBulkDataBuffer&& Other) noexcept
	{
		View = MoveTemp(Other.View);
		Other.View = ViewType();
	}

	FRuntimeBulkDataBuffer(DataType* InBuffer, int64 InNumberOfElements)
		: View(InBuffer, InNumberOfElements)
	{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
		check(InNumberOfElements <= TNumericLimits<int32>::Max())
#endif
	}

	template <typename Allocator>
	explicit FRuntimeBulkDataBuffer(const TArray<DataType, Allocator>& Other)
	{
		const int64 BulkDataSize = Other.Num();

		DataType* BulkData = static_cast<DataType*>(FMemory::Malloc(BulkDataSize * sizeof(DataType)));
		if (!BulkData)
		{
			return;
		}

		FMemory::Memcpy(BulkData, Other.GetData(), BulkDataSize * sizeof(DataType));
		View = ViewType(BulkData, BulkDataSize);
	}

	~FRuntimeBulkDataBuffer()
	{
		FreeBuffer();
	}

	FRuntimeBulkDataBuffer& operator=(const FRuntimeBulkDataBuffer& Other)
	{
		FreeBuffer();

		if (this != &Other)
		{
			const int64 BufferSize = Other.View.Num();

			DataType* BufferCopy = static_cast<DataType*>(FMemory::Malloc(BufferSize * sizeof(DataType)));
			FMemory::Memcpy(BufferCopy, Other.View.GetData(), BufferSize * sizeof(DataType));

			View = ViewType(BufferCopy, BufferSize);
		}

		return *this;
	}

	FRuntimeBulkDataBuffer& operator=(FRuntimeBulkDataBuffer&& Other) noexcept
	{
		if (this != &Other)
		{
			FreeBuffer();

			View = Other.View;
			Other.View = ViewType();
		}

		return *this;
	}

	void Empty()
	{
		FreeBuffer();
		View = ViewType();
	}

	void Reset(DataType* InBuffer, int64 InNumberOfElements)
	{
		FreeBuffer();

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
		check(InNumberOfElements <= TNumericLimits<int32>::Max())
#endif

		View = ViewType(InBuffer, InNumberOfElements);
	}

	const ViewType& GetView() const
	{
		return View;
	}

private:
	void FreeBuffer()
	{
		if (View.GetData() != nullptr)
		{
			FMemory::Free(View.GetData());
			View = ViewType();
		}
	}

	ViewType View;
};

/** Basic sound wave data */
struct FSoundWaveBasicStruct
{
	FSoundWaveBasicStruct()
		: NumOfChannels(0)
	  , SampleRate(0)
	  , Duration(0)
	{
	}

	/** Number of channels */
	uint32 NumOfChannels;

	/** Sample rate (samples per second, sampling frequency) */
	uint32 SampleRate;

	/** Sound wave duration, sec */
	float Duration;

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

	/** 32-bit float PCM data */
	FRuntimeBulkDataBuffer<float> PCMData;

	/** Number of PCM frames */
	uint32 PCMNumOfFrames;
};

/** Decoded audio information */
struct FDecodedAudioStruct
{
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

	/** SoundWave basic info (e.g. duration, number of channels, etc) */
	FSoundWaveBasicStruct SoundWaveBasicInfo;

	/** PCM data buffer */
	FPCMStruct PCMInfo;
};

/** Encoded audio information */
struct FEncodedAudioStruct
{
	FEncodedAudioStruct()
		: AudioFormat(ERuntimeAudioFormat::Invalid)
	{
	}

	template <typename Allocator>
	FEncodedAudioStruct(const TArray<uint8, Allocator>& AudioDataArray, ERuntimeAudioFormat AudioFormat)
		: AudioData(AudioDataArray)
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

	/** Audio data */
	FRuntimeBulkDataBuffer<uint8> AudioData;

	/** Format of the audio data (e.g. mp3, flac, etc) */
	ERuntimeAudioFormat AudioFormat;
};

/** Compressed sound wave information */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FCompressedSoundWaveInfo
{
	GENERATED_BODY()

	FCompressedSoundWaveInfo()
		: SoundGroup(ESoundGroup::SOUNDGROUP_Default)
	  , bLooping(false)
	  , Volume(1.f)
	  , Pitch(1.f)
	{
	}

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
};

/** A line of subtitle text and the time at which it should be displayed. This is the same as FSubtitleCue but editable in Blueprints */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FEditableSubtitleCue
{
	GENERATED_BODY()

	FEditableSubtitleCue()
		: Time(0)
	{
	}

	/** The text to appear in the subtitle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	FText Text;

	/** The time at which the subtitle is to be displayed, in seconds relative to the beginning of the line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	float Time;
};

/** Platform audio input device info */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FRuntimeAudioInputDeviceInfo
{
	GENERATED_BODY()

	FRuntimeAudioInputDeviceInfo()
		: DeviceName("")
	  , DeviceId("")
	  , InputChannels(0)
	  , PreferredSampleRate(0)
	  , bSupportsHardwareAEC(true)
	{
	}

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	FRuntimeAudioInputDeviceInfo(const Audio::FCaptureDeviceInfo& DeviceInfo)
		: DeviceName(DeviceInfo.DeviceName)
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION > 24
	  , DeviceId(DeviceInfo.DeviceId)
#endif
	  , InputChannels(DeviceInfo.InputChannels)
	  , PreferredSampleRate(DeviceInfo.PreferredSampleRate)
	  , bSupportsHardwareAEC(DeviceInfo.bSupportsHardwareAEC)
	{
	}
#endif

	/** The name of the audio device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	FString DeviceName;

	/** ID of the device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	FString DeviceId;

	/** The number of channels supported by the audio device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	int32 InputChannels;

	/** The preferred sample rate of the audio device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	int32 PreferredSampleRate;

	/** Whether or not the device supports Acoustic Echo Canceling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	bool bSupportsHardwareAEC;
};

/** Audio header information */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FRuntimeAudioHeaderInfo
{
	GENERATED_BODY()

	FRuntimeAudioHeaderInfo()
		: Duration(0.f)
	  , NumOfChannels(0)
	  , SampleRate(0)
	  , PCMDataSize(0)
	  , AudioFormat(ERuntimeAudioFormat::Invalid)
	{
	}

	/**
	 * Converts Audio Header Info to a readable format
	 *
	 * @return String representation of the Encoded Audio Struct
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("Duration: %f, number of channels: %d, sample rate: %d, PCM data size: %lld, audio format: %s"),
							   Duration, NumOfChannels, SampleRate, PCMDataSize, *UEnum::GetValueAsName(AudioFormat).ToString());
	}

	/** Audio duration, sec */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Audio Importer")
	float Duration;

	/** Number of channels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Audio Importer")
	int32 NumOfChannels;

	/** Sample rate (samples per second, sampling frequency) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Audio Importer")
	int32 SampleRate;

	/** PCM data size in 32-bit float format */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName = "PCM Data Size", Category = "Runtime Audio Importer")
	int64 PCMDataSize;

	/** Format of the source audio data (e.g. mp3, flac, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime Audio Importer")
	ERuntimeAudioFormat AudioFormat;
};

/** Audio export override options */
USTRUCT(BlueprintType, Category = "Runtime Audio Importer")
struct FRuntimeAudioExportOverrideOptions
{
	GENERATED_BODY()

	FRuntimeAudioExportOverrideOptions()
		: NumOfChannels(-1)
	  , SampleRate(-1)
	{
	}

	bool IsOverriden() const
	{
		return IsNumOfChannelsOverriden() || IsSampleRateOverriden();
	}

	bool IsSampleRateOverriden() const
	{
		return SampleRate != -1;
	}

	bool IsNumOfChannelsOverriden() const
	{
		return NumOfChannels != -1;
	}

	/** Number of channels. Set to -1 to retrieve from source. Mixing if count differs from source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	int32 NumOfChannels;
    
	/** Audio sampling rate (samples per second, sampling frequency). Set to -1 to retrieve from source. Resampling if count differs from source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime Audio Importer")
	int32 SampleRate;
};