// Georgy Treshchev 2023.

#include "Sound/StreamingSoundWave.h"
#include "RuntimeAudioImporterLibrary.h"
#include "Codecs/RAW_RuntimeCodec.h"

#include "Async/Async.h"
#include "UObject/GCObjectScopeGuard.h"

UStreamingSoundWave::UStreamingSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlaybackFinishedBroadcast = true;

	// No need to stop the sound after the end of streaming sound wave playback, assuming the PCM data can be filled after that
	// (except if this is overridden in SetStopSoundOnPlaybackFinish)
	bStopSoundOnPlaybackFinish = false;

	// No need to loop streaming sound wave by default
	bLooping = false;

	// It is necessary to populate the sample rate and the number of channels to make the streaming wave playable even if there is no audio data
	// (since the audio data may be filled in after the sound wave starts playing)
	{
		SetSampleRate(44100);
		NumChannels = 2;
	}

	bFilledInitialAudioData = false;
	NumOfPreAllocatedPCMData = 0;
}

void UStreamingSoundWave::PopulateAudioDataFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo)
{
	FScopeLock Lock(&DataGuard);

	if (!DecodedAudioInfo.IsValid())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to continue populating the audio data because the decoded info is invalid"));
		return;
	}

	// Update the initial audio data if it hasn't already been filled in
	if (!bFilledInitialAudioData)
	{
		SetSampleRate(DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
		NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
		bFilledInitialAudioData = true;
	}

	// TODO: Convert the sample rate and number of channels for the new PCM data to match the previous PCM data if the data is different
	if (SampleRate != DecodedAudioInfo.SoundWaveBasicInfo.SampleRate || NumChannels != DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("The sample rate and/or number of channels is different from what was appended to the audio data last time. Mapping such data to previously added audio data is not yet supported.\nPrevious audio channels: %d, new audio channels: %d; previous sample rate: %d, new sample rate: %d"),
		       NumChannels, DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels, SampleRate, DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
		return;
	}

	// Do not reallocate the entire PCM buffer if it has free space to fill in
	if (NumOfPreAllocatedPCMData >= DecodedAudioInfo.PCMInfo.PCMData.GetView().Num())
	{
		FMemory::Memcpy(PCMBufferInfo->PCMData.GetView().GetData() + ((PCMBufferInfo->PCMData.GetView().Num() - NumOfPreAllocatedPCMData) / sizeof(float)), DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());
		NumOfPreAllocatedPCMData -= DecodedAudioInfo.PCMInfo.PCMData.GetView().Num();
		NumOfPreAllocatedPCMData = NumOfPreAllocatedPCMData < 0 ? 0 : NumOfPreAllocatedPCMData;
	}
	else
	{
		const int64 NewPCMDataSize = (PCMBufferInfo->PCMData.GetView().Num() + DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() - NumOfPreAllocatedPCMData);
		float* NewPCMDataPtr = static_cast<float*>(FMemory::Malloc(NewPCMDataSize * sizeof(float)));

		if (!NewPCMDataPtr)
		{
			return;
		}

		// Adding new PCM data at the end
		{
			FMemory::Memcpy(NewPCMDataPtr, PCMBufferInfo->PCMData.GetView().GetData(), PCMBufferInfo->PCMData.GetView().Num() - NumOfPreAllocatedPCMData);
			FMemory::Memcpy(NewPCMDataPtr + ((PCMBufferInfo->PCMData.GetView().Num() - NumOfPreAllocatedPCMData) / sizeof(float)), DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());
		}

		PCMBufferInfo->PCMData = FRuntimeBulkDataBuffer<float>(NewPCMDataPtr, NewPCMDataSize);
		NumOfPreAllocatedPCMData = 0;
	}

	PCMBufferInfo->PCMNumOfFrames += DecodedAudioInfo.PCMInfo.PCMNumOfFrames;

	Duration += DecodedAudioInfo.SoundWaveBasicInfo.Duration;

	ResetPlaybackFinish();

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully added audio data to streaming sound wave.\nAdded audio info: %s"), *DecodedAudioInfo.ToString());
}

void UStreamingSoundWave::ReleaseMemory()
{
	Super::ReleaseMemory();
	NumOfPreAllocatedPCMData = 0;
}

void UStreamingSoundWave::ReleasePlayedAudioData(const FOnPlayedAudioDataReleaseResultNative& Result)
{
	FScopeLock Lock(&DataGuard);
	const int64 NewPCMDataSize = (PCMBufferInfo->PCMNumOfFrames - GetNumOfPlayedFrames_Internal()) * NumChannels * sizeof(float);

	if (GetNumOfPlayedFrames_Internal() > 0 && NumOfPreAllocatedPCMData > 0 && NewPCMDataSize < PCMBufferInfo->PCMData.GetView().Num())
	{
		NumOfPreAllocatedPCMData -= PCMBufferInfo->PCMData.GetView().Num() - NewPCMDataSize;
		NumOfPreAllocatedPCMData = NumOfPreAllocatedPCMData < 0 ? 0 : NumOfPreAllocatedPCMData;
	}
	Super::ReleasePlayedAudioData(Result);
}

UStreamingSoundWave* UStreamingSoundWave::CreateStreamingSoundWave()
{
	if (!IsInGameThread())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to create a sound wave outside of the game thread"));
		return nullptr;
	}

	return NewObject<UStreamingSoundWave>();
}

void UStreamingSoundWave::PreAllocateAudioData(int64 NumOfBytesToPreAllocate, const FOnPreAllocateAudioDataResult& Result)
{
	PreAllocateAudioData(NumOfBytesToPreAllocate, FOnPreAllocateAudioDataResultNative::CreateWeakLambda(this, [Result](bool bSucceeded)
	{
		Result.ExecuteIfBound(bSucceeded);
	}));
}

void UStreamingSoundWave::PreAllocateAudioData(int64 NumOfBytesToPreAllocate, const FOnPreAllocateAudioDataResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, NumOfBytesToPreAllocate, Result]()
	{
		FGCObjectScopeGuard Guard(this);
		FScopeLock Lock(&DataGuard);

		auto ExecuteResult = [Result](bool bSucceeded)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded]()
			{
				Result.ExecuteIfBound(bSucceeded);
			});
		};

		if (PCMBufferInfo->PCMData.GetView().Num() > 0 || NumOfPreAllocatedPCMData > 0)
		{
			ensureMsgf(false, TEXT("Pre-allocation of PCM data can only be applied if the PCM data has not yet been allocated"));
			ExecuteResult(false);
			return;
		}

		float* NewPCMDataPtr = static_cast<float*>(FMemory::Malloc(NumOfBytesToPreAllocate));
		const int64 NewPCMDataSize = NumOfBytesToPreAllocate / sizeof(float);

		if (!NewPCMDataPtr)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory to pre-allocate streaming sound wave audio data"));
			ExecuteResult(false);
			return;
		}

		NumOfPreAllocatedPCMData = NewPCMDataSize;

		PCMBufferInfo->PCMData = FRuntimeBulkDataBuffer<float>(NewPCMDataPtr, NewPCMDataSize);

		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully pre-allocated '%lld' number of bytes"), NumOfBytesToPreAllocate);

		ExecuteResult(true);
	});
}

void UStreamingSoundWave::AppendAudioDataFromEncoded(TArray<uint8> AudioData, ERuntimeAudioFormat AudioFormat)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, AudioData = MoveTemp(AudioData), AudioFormat]()
	{
		FGCObjectScopeGuard Guard(this);

		uint8* EncodedAudioDataPtr = static_cast<uint8*>(FMemory::Memcpy(FMemory::Malloc(AudioData.Num()), AudioData.GetData(), AudioData.Num()));

		if (!EncodedAudioDataPtr)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory to populate streaming sound wave audio data"));
			return;
		}

		FEncodedAudioStruct EncodedAudioInfo(EncodedAudioDataPtr, AudioData.Num(), AudioFormat);

		FDecodedAudioStruct DecodedAudioInfo;
		if (!URuntimeAudioImporterLibrary::DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to decode audio data to populate streaming sound wave audio data"));
			return;
		}

		PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void UStreamingSoundWave::AppendAudioDataFromRAW(TArray<uint8> RAWData, ERuntimeRAWAudioFormat RAWFormat, int32 InSampleRate, int32 NumOfChannels)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, RAWData = MoveTemp(RAWData), RAWFormat, InSampleRate, NumOfChannels]() mutable
	{
		FGCObjectScopeGuard Guard(this);

		uint8* RAWDataPtr{RAWData.GetData()};
		const int32 RAWDataSize{RAWData.Num()};

		float* PCMData{nullptr};
		int64 PCMDataSize{0};

		// Transcoding RAW data to 32-bit float data
		{
			switch (RAWFormat)
			{
			case ERuntimeRAWAudioFormat::Int16:
				{
					FRAW_RuntimeCodec::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(RAWDataPtr), RAWDataSize, PCMData, PCMDataSize);
					break;
				}
			case ERuntimeRAWAudioFormat::Int32:
				{
					FRAW_RuntimeCodec::TranscodeRAWData<int32, float>(reinterpret_cast<int32*>(RAWDataPtr), RAWDataSize, PCMData, PCMDataSize);
					break;
				}
			case ERuntimeRAWAudioFormat::UInt8:
				{
					FRAW_RuntimeCodec::TranscodeRAWData<uint8, float>(RAWDataPtr, RAWDataSize, PCMData, PCMDataSize);
					break;
				}
			case ERuntimeRAWAudioFormat::Float32:
				{
					PCMDataSize = RAWDataSize;
					PCMData = static_cast<float*>(FMemory::Memcpy(FMemory::Malloc(PCMDataSize), RAWDataPtr, RAWDataSize));
					break;
				}
			}
		}

		if (!PCMData || PCMDataSize <= 0)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to transcode RAW data to decoded audio info"))
			return;
		}

		FDecodedAudioStruct DecodedAudioInfo;
		{
			FPCMStruct PCMInfo;
			{
				PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(PCMData, PCMDataSize);
				PCMInfo.PCMNumOfFrames = PCMDataSize / sizeof(float) / NumOfChannels;
			}
			DecodedAudioInfo.PCMInfo = MoveTemp(PCMInfo);

			FSoundWaveBasicStruct SoundWaveBasicInfo;
			{
				SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
				SoundWaveBasicInfo.SampleRate = InSampleRate;
				SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / InSampleRate;
			}
			DecodedAudioInfo.SoundWaveBasicInfo = MoveTemp(SoundWaveBasicInfo);
		}

		PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void UStreamingSoundWave::SetStopSoundOnPlaybackFinish(bool bStop)
{
	bStopSoundOnPlaybackFinish = bStop;
}
