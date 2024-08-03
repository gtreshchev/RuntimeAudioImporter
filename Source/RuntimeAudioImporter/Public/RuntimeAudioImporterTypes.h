// Georgy Treshchev 2024.

#pragma once

#include "Misc/Build.h"

// Whether to use debug scope lock
#define WITH_DEBUG_SCOPE_LOCK UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT || WITH_EDITOR

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#include "AudioCaptureDeviceInterface.h"
#endif

#include "RuntimeAudioImporterDefines.h"
#include "Engine/EngineBaseTypes.h"
#include "Sound/SoundGroups.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeLock.h"
#if UE_VERSION_OLDER_THAN(5, 0, 0)
#include "Async/Async.h"
#include "Containers/Queue.h"
#include <atomic>
#else
#include "Tasks/Pipe.h"
#endif

#if UE_VERSION_OLDER_THAN(4, 26, 0)
#include "DSP/BufferVectorOperations.h"
#endif

#include "RuntimeAudioImporterTypes.generated.h"

#if UE_VERSION_OLDER_THAN(4, 26, 0)
namespace Audio
{
	using FAlignedFloatBuffer = Audio::AlignedFloatBuffer;
}
#endif

/** MemIsZero isn't available in UE < 5.2 */
#if UE_VERSION_OLDER_THAN(5, 2, 0)
namespace FRAIMemory
{
	static FORCEINLINE bool MemIsZero(const void* Ptr, SIZE_T Count)
	{
		uint8* Start = (uint8*)Ptr;
		uint8* End = Start + Count;
		while (Start < End)
		{
			if ((*Start++) != 0)
			{
				return false;
			}
		}
		return true;
	}
}
#else
using FRAIMemory = FMemory;
#endif

/** FPipe isn't supported in UE earlier than 5.0.0, so the task will be launched via async task */
#if UE_VERSION_OLDER_THAN(5, 0, 0)
namespace UE
{
	namespace Tasks
	{
		enum class ETaskPriority : int8
		{
			High,
			Normal,
			Default,
			ForegroundCount,
			BackgroundHigh,
			BackgroundNormal,
			BackgroundLow
		};

		/**
		 * A pipe for launching tasks with the specified priority. This is a highly simplified version of the pipe from UE >= 5.0.0
		 */
		class FPipe
		{
		public:
			FPipe(const FString& InDebugName)
			{
			}

			void Launch(const FString& InDebugName, TUniqueFunction<void()>&& TaskBody, ETaskPriority Priority = ETaskPriority::Default)
			{
				UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Pipe isn't supported in UE earlier than 5.0.0. Launching task via async task..."));
				TaskQueue.Enqueue({ InDebugName, MoveTemp(TaskBody), Priority });
				if (!bIsTaskRunning) ExecuteNextTask();
			}

			FString GetDebugName() const { return FString(); }

		private:
			struct FTaskInfo
			{
				FString DebugName;
				TUniqueFunction<void()> TaskBody;
				ETaskPriority Priority;
			};

			void ExecuteNextTask()
			{
				if (TaskQueue.IsEmpty()) { bIsTaskRunning = false; return; }
				bIsTaskRunning = true;
				FTaskInfo TaskInfo;
				TaskQueue.Dequeue(TaskInfo);
				Async(EAsyncExecution::TaskGraph, MoveTemp(TaskInfo.TaskBody), [this]() { ExecuteNextTask(); });
			}

			TQueue<FTaskInfo> TaskQueue;
			std::atomic<bool> bIsTaskRunning{false};
		};
	}
}
#endif

// This might be useful for scope lock debugging
#if WITH_DEBUG_SCOPE_LOCK
class FRAIScopeLock : public FScopeLock
{
public:
	~FRAIScopeLock()
	{
		UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Debug scope lock destroyed"));
	}

	FRAIScopeLock(FCriticalSection* InCriticalSection)
		: FScopeLock(InCriticalSection)
	{
		UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Debug scope lock created"));
	}
};
#else
using FRAIScopeLock = FScopeLock;
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
	Custom UMETA(DisplayName = "custom"),
	Invalid UMETA(DisplayName = "invalid", Hidden)
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

/** Possible VAD (Voice Activity Detection) modes */
UENUM(BlueprintType, Category = "Runtime Audio Importer")
enum class ERuntimeVADMode : uint8
{
	Quality UMETA(ToolTip = "High quality, less restrictive in reporting speech"),
	LowBitrate UMETA(ToolTip = "Low bitrate, more restrictive in reporting speech"),
	Aggressive UMETA(ToolTip = "Aggressive, restrictive in reporting speech"),
	VeryAggressive UMETA(ToolTip = "Very aggressive, extremely restrictive in reporting speech")
};

namespace VoiceActivityDetector
{
	/**
	 * Converts VAD mode to an integer suitable for the VAD library (libfvad)
	 * @param Mode VAD mode
	 * @return Integer representation of the VAD mode
	 */
	inline int32 GetVADModeInt(ERuntimeVADMode Mode)
	{
		switch (Mode)
		{
		case ERuntimeVADMode::Quality:
			return 0;
		case ERuntimeVADMode::LowBitrate:
			return 1;
		case ERuntimeVADMode::Aggressive:
			return 2;
		case ERuntimeVADMode::VeryAggressive:
			return 3;
		default:
			return -1;
		}
	}
}

/**
 * An alternative to FBulkDataBuffer with consistent data types
 */
template <typename DataType>
class FRuntimeBulkDataBuffer
{
public:
#if UE_VERSION_OLDER_THAN(4, 27, 0)
	using ViewType = TArrayView<DataType>;
#else
	using ViewType = TArrayView64<DataType>;
#endif

	FRuntimeBulkDataBuffer() = default;

	/**
	 * Reserve (pre-allocate) memory for the buffer
	 * This function can only be called if there's no data allocated
	 * 
	 * @param NewCapacity New capacity to reserve
	 * @return True if the memory was successfully reserved, false otherwise
	 */
	bool Reserve(int64 NewCapacity)
	{
		// Reserve function can only be called if there's no data allocated
		if (View.GetData() != nullptr || NewCapacity <= 0)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Reserve function can't be called if there's data allocated or NewCapacity is <= 0 (current capacity: %lld, new capacity: %lld)"), ReservedCapacity, NewCapacity);
			return false;
		}

		DataType* NewBuffer = static_cast<DataType*>(FMemory::Malloc(NewCapacity * sizeof(DataType)));
		if (!NewBuffer)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate buffer to reserve memory (new capacity: %lld, %lld bytes)"), NewCapacity, NewCapacity * sizeof(DataType));
			return false;
		}

		ReservedCapacity = NewCapacity;
		View = ViewType(NewBuffer, 0);
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Reserving memory for buffer (new capacity: %lld, %lld bytes)"), NewCapacity, NewCapacity * sizeof(DataType));

		return true;
	}

	void Append(const FRuntimeBulkDataBuffer<DataType>& Other)
	{
		Append(Other.GetView().GetData(), Other.GetView().Num());
	}

	void Append(FRuntimeBulkDataBuffer<DataType>&& Other)
	{
		Append(Other.GetView().GetData(), Other.GetView().Num());
		Other.Empty();
	}

	/**
	 * Append data to the buffer from the given buffer
	 * Takes the reserved capacity into account
	 * 
	 * @param InBuffer Buffer to append data from
	 * @param InNumberOfElements Number of elements to append
	 */
	void Append(const DataType* InBuffer, int64 InNumberOfElements)
	{
		if (InNumberOfElements <= 0)
		{
			return;
		}

		// Enough reserved capacity, just memcpy
		if (ReservedCapacity > 0 && InNumberOfElements <= ReservedCapacity)
		{
			FMemory::Memcpy(View.GetData() + View.Num(), InBuffer, InNumberOfElements * sizeof(DataType));
			View = ViewType(View.GetData(), View.Num() + InNumberOfElements);
			int64 NewReservedCapacity = ReservedCapacity - InNumberOfElements;
			NewReservedCapacity = NewReservedCapacity < 0 ? 0 : NewReservedCapacity;
			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Appending data to buffer (previous capacity: %lld, new capacity: %lld)"), ReservedCapacity, NewReservedCapacity);
			ReservedCapacity = NewReservedCapacity;
		}
		// Not enough reserved capacity or no reserved capacity, reallocate entire buffer
		else
		{
			int64 NewCapacity = View.Num() + InNumberOfElements;
			DataType* NewBuffer = static_cast<DataType*>(FMemory::Malloc(NewCapacity * sizeof(DataType)));
			if (!NewBuffer)
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate buffer to append data (new capacity: %lld, current capacity: %lld)"), NewCapacity, View.Num());
				return;
			}

			if (View.Num() > 0)
			{
				FMemory::Memcpy(NewBuffer, View.GetData(), View.Num() * sizeof(DataType));
			}
			FMemory::Memcpy(NewBuffer + View.Num(), InBuffer, InNumberOfElements * sizeof(DataType));

			FreeBuffer();
			View = ViewType(NewBuffer, NewCapacity);
			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Reallocating buffer to append data (new capacity: %lld)"), NewCapacity);
		}
	}

	FRuntimeBulkDataBuffer(const FRuntimeBulkDataBuffer& Other)
	{
		*this = Other;
	}

	FRuntimeBulkDataBuffer(FRuntimeBulkDataBuffer&& Other) noexcept
	{
		View = MoveTemp(Other.View);
		Other.View = ViewType();
		ReservedCapacity = Other.ReservedCapacity;
		Other.ReservedCapacity = 0;
	}

	FRuntimeBulkDataBuffer(DataType* InBuffer, int64 InNumberOfElements)
		: View(InBuffer, InNumberOfElements)
	{
#if UE_VERSION_OLDER_THAN(4, 27, 0)
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
		ReservedCapacity = 0;
	}

	~FRuntimeBulkDataBuffer()
	{
		FreeBuffer();
	}

	FRuntimeBulkDataBuffer& operator=(const FRuntimeBulkDataBuffer& Other)
	{
		if (this != &Other)
		{
			FreeBuffer();

			const int64 BufferSize = Other.View.Num() + Other.ReservedCapacity;

			DataType* BufferCopy = static_cast<DataType*>(FMemory::Malloc(BufferSize * sizeof(DataType)));
			FMemory::Memcpy(BufferCopy, Other.View.GetData(), BufferSize * sizeof(DataType));

			View = ViewType(BufferCopy, BufferSize);
			ReservedCapacity = Other.ReservedCapacity;
		}

		return *this;
	}

	FRuntimeBulkDataBuffer& operator=(FRuntimeBulkDataBuffer&& Other) noexcept
	{
		if (this != &Other)
		{
			FreeBuffer();
			View = MoveTemp(Other.View);
			Other.View = ViewType();
			ReservedCapacity = Other.ReservedCapacity;
			Other.ReservedCapacity = 0;
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
#if UE_VERSION_OLDER_THAN(4, 27, 0)
		check(InNumberOfElements <= TNumericLimits<int32>::Max())
#endif

		View = ViewType(InBuffer, InNumberOfElements);
	}

	const ViewType& GetView() const
	{
		return View;
	}

protected:
	void FreeBuffer()
	{
		if (View.GetData() != nullptr)
		{
			FMemory::Free(View.GetData());
			View = ViewType();
			ReservedCapacity = 0;
		}
	}

	ViewType View;
	int64 ReservedCapacity = 0;
};

/** Basic sound wave data */
struct FSoundWaveBasicStruct
{
	FSoundWaveBasicStruct()
		: NumOfChannels(0)
	  , SampleRate(0)
	  , Duration(0)
	  , AudioFormat(ERuntimeAudioFormat::Invalid)
	{}

	/** Number of channels */
	uint32 NumOfChannels;

	/** Sample rate (samples per second, sampling frequency) */
	uint32 SampleRate;

	/** Sound wave duration, sec */
	float Duration;

	/** Audio format if the original audio data was encoded */
	ERuntimeAudioFormat AudioFormat;

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
	{}

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
	{}

	template <typename Allocator>
	FEncodedAudioStruct(const TArray<uint8, Allocator>& AudioDataArray, ERuntimeAudioFormat AudioFormat)
		: AudioData(AudioDataArray)
	  , AudioFormat(AudioFormat)
	{}

	FEncodedAudioStruct(FRuntimeBulkDataBuffer<uint8> AudioDataBulk, ERuntimeAudioFormat AudioFormat)
		: AudioData(MoveTemp(AudioDataBulk))
	  , AudioFormat(AudioFormat)
	{}

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
	{}

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
	{}

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
	{}

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	FRuntimeAudioInputDeviceInfo(const Audio::FCaptureDeviceInfo& DeviceInfo)
		: DeviceName(DeviceInfo.DeviceName)
#if UE_VERSION_NEWER_THAN(4, 25, 0)
	  , DeviceId(DeviceInfo.DeviceId)
#endif
	  , InputChannels(DeviceInfo.InputChannels)
	  , PreferredSampleRate(DeviceInfo.PreferredSampleRate)
	  , bSupportsHardwareAEC(DeviceInfo.bSupportsHardwareAEC)
	{}
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
	{}

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
	{}

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
