// Georgy Treshchev 2023.

#include "Sound/StreamingSoundWave.h"

#include "RuntimeAudioImporterLibrary.h"
#include "Codecs/RAW_RuntimeCodec.h"

#include "Async/Async.h"
#include "SampleBuffer.h"
#include "UObject/WeakObjectPtrTemplates.h"

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
	NumOfPreAllocatedByteData = 0;
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

	// Check if the number of channels and the sampling rate of the sound wave and the input audio data match
	if (SampleRate != DecodedAudioInfo.SoundWaveBasicInfo.SampleRate || NumChannels != DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels)
	{
		Audio::FAlignedFloatBuffer WaveData(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());

		// Resampling if needed
		if (SampleRate != DecodedAudioInfo.SoundWaveBasicInfo.SampleRate)
		{
			Audio::FAlignedFloatBuffer ResamplerOutputData;
			if (!FRAW_RuntimeCodec::ResampleRAWData(WaveData, GetNumOfChannels(), GetSampleRate(), DecodedAudioInfo.SoundWaveBasicInfo.SampleRate, ResamplerOutputData))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to resample audio data to the sound wave's sample rate. Resampling failed"));
				return;
			}
			WaveData = MoveTemp(ResamplerOutputData);
		}

		// Mixing the channels if needed
		if (NumChannels != DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels)
		{
			Audio::FAlignedFloatBuffer WaveDataTemp;
			if (!FRAW_RuntimeCodec::MixChannelsRAWData(WaveData, DecodedAudioInfo.SoundWaveBasicInfo.SampleRate, GetNumOfChannels(), DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels, WaveDataTemp))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mix audio data to the sound wave's number of channels. Mixing failed"));
				return;
			}
			WaveData = MoveTemp(WaveDataTemp);
		}

		DecodedAudioInfo.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(WaveData);
	}

	// Do not reallocate the entire PCM buffer if it has free space to fill in
	if (static_cast<uint64>(NumOfPreAllocatedByteData) >= DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() * sizeof(float))
	{
		// This should be changed somehow to work with the new calculations
		FMemory::Memcpy(reinterpret_cast<uint8*>(PCMBufferInfo->PCMData.GetView().GetData()) + ((PCMBufferInfo->PCMData.GetView().Num() * sizeof(float)) - NumOfPreAllocatedByteData), DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() * sizeof(float));
		NumOfPreAllocatedByteData -= DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() * sizeof(float);
		NumOfPreAllocatedByteData = NumOfPreAllocatedByteData < 0 ? 0 : NumOfPreAllocatedByteData;
	}
	else
	{
		const int64 NewPCMDataSize = ((PCMBufferInfo->PCMData.GetView().Num() * sizeof(float)) + (DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() * sizeof(float)) - NumOfPreAllocatedByteData) / sizeof(float);
		float* NewPCMDataPtr = static_cast<float*>(FMemory::Malloc(NewPCMDataSize * sizeof(float)));

		if (!NewPCMDataPtr)
		{
			return;
		}

		// Adding new PCM data at the end
		{
			FMemory::Memcpy(NewPCMDataPtr, PCMBufferInfo->PCMData.GetView().GetData(), (PCMBufferInfo->PCMData.GetView().Num() * sizeof(float)) - NumOfPreAllocatedByteData);
			FMemory::Memcpy(reinterpret_cast<uint8*>(NewPCMDataPtr) + ((PCMBufferInfo->PCMData.GetView().Num() * sizeof(float)) - NumOfPreAllocatedByteData), DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() * sizeof(float));
		}

		PCMBufferInfo->PCMData = FRuntimeBulkDataBuffer<float>(NewPCMDataPtr, NewPCMDataSize);
		NumOfPreAllocatedByteData = 0;
	}

	PCMBufferInfo->PCMNumOfFrames += DecodedAudioInfo.PCMInfo.PCMNumOfFrames;
	Duration += DecodedAudioInfo.SoundWaveBasicInfo.Duration;
	ResetPlaybackFinish();

	{
		const bool IsBound = [this]()
		{
			FScopeLock Lock(&OnPopulateAudioData_DataGuard);
			return OnPopulateAudioDataNative.IsBound() || OnPopulateAudioData.IsBound();
		}();
		if (IsBound)
		{
			TArray<float> PCMData(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());
			AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this), PCMData = MoveTemp(PCMData)]() mutable
			{
				if (WeakThis.IsValid())
				{
					FScopeLock Lock(&WeakThis->OnPopulateAudioData_DataGuard);
					if (WeakThis->OnPopulateAudioDataNative.IsBound())
					{
						WeakThis->OnPopulateAudioDataNative.Broadcast(PCMData);
					}
					if (WeakThis->OnPopulateAudioData.IsBound())
					{
						WeakThis->OnPopulateAudioData.Broadcast(PCMData);
					}
				}
				else
				{
					UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to broadcast OnPopulateAudioDataNative and OnPopulateAudioData delegates because the streaming sound wave has been destroyed"));
				}
			});
		}
	}

	{
		const bool IsBound = [this]()
		{
			FScopeLock Lock(&OnPopulateAudioData_DataGuard);
			return OnPopulateAudioStateNative.IsBound() || OnPopulateAudioState.IsBound();
		}();
		if (IsBound)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this)]()
			{
				if (WeakThis.IsValid())
				{
					FScopeLock Lock(&WeakThis->OnPopulateAudioData_DataGuard);
					WeakThis->OnPopulateAudioStateNative.Broadcast();
					WeakThis->OnPopulateAudioState.Broadcast();
				}
				else
				{
					UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to broadcast OnPopulateAudioStateNative and OnPopulateAudioState delegates because the streaming sound wave has been destroyed"));
				}
			});
		}
	}

	AppendAudioTaskQueue.Pop();
	if (FAudioTaskDelegate* AppendAudioTask = AppendAudioTaskQueue.Peek())
	{
		AppendAudioTask->ExecuteIfBound();
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully added audio data to streaming sound wave.\nAdded audio info: %s"), *DecodedAudioInfo.ToString());
}

void UStreamingSoundWave::ReleaseMemory()
{
	Super::ReleaseMemory();
	NumOfPreAllocatedByteData = 0;
}

void UStreamingSoundWave::ReleasePlayedAudioData(const FOnPlayedAudioDataReleaseResultNative& Result)
{
	FScopeLock Lock(&DataGuard);
	const int64 NewPCMDataSize = (PCMBufferInfo->PCMNumOfFrames - GetNumOfPlayedFrames_Internal()) * NumChannels;

	if (GetNumOfPlayedFrames_Internal() > 0 && NumOfPreAllocatedByteData > 0 && NewPCMDataSize < PCMBufferInfo->PCMData.GetView().Num())
	{
		NumOfPreAllocatedByteData -= (PCMBufferInfo->PCMData.GetView().Num() * sizeof(float)) - (NewPCMDataSize * sizeof(float));
		NumOfPreAllocatedByteData = NumOfPreAllocatedByteData < 0 ? 0 : NumOfPreAllocatedByteData;
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
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = TWeakObjectPtr<UStreamingSoundWave>(this), NumOfBytesToPreAllocate, Result]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->PreAllocateAudioData(NumOfBytesToPreAllocate, Result);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to pre-allocate streaming sound wave audio data as the streaming sound wave has been destroyed"));
			}
		});
		return;
	}
		
	FScopeLock Lock(&DataGuard);

	auto ExecuteResult = [Result](bool bSucceeded)
	{
		AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded]()
		{
			Result.ExecuteIfBound(bSucceeded);
		});
	};

	if (PCMBufferInfo->PCMData.GetView().Num() > 0 || NumOfPreAllocatedByteData > 0)
	{
		ensureMsgf(false, TEXT("Pre-allocation of PCM data can only be applied if the PCM data has not yet been allocated"));
		ExecuteResult(false);
		return;
	}

	float* NewPCMDataPtr = static_cast<float*>(FMemory::Malloc(NumOfBytesToPreAllocate));
	if (!NewPCMDataPtr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory to pre-allocate streaming sound wave audio data with '%lld' number of bytes"), NumOfBytesToPreAllocate);
		ExecuteResult(false);
		return;
	}

	{
		NumOfPreAllocatedByteData = NumOfBytesToPreAllocate;
		PCMBufferInfo->PCMData = FRuntimeBulkDataBuffer<float>(NewPCMDataPtr, NumOfBytesToPreAllocate / sizeof(float));
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully pre-allocated '%lld' number of bytes"), NumOfBytesToPreAllocate);
	ExecuteResult(true);
}

void UStreamingSoundWave::AppendAudioDataFromEncoded(TArray<uint8> AudioData, ERuntimeAudioFormat AudioFormat)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = TWeakObjectPtr<UStreamingSoundWave>(this), AudioData = MoveTemp(AudioData), AudioFormat]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AppendAudioDataFromEncoded(MoveTemp(AudioData), AudioFormat);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to append audio data to streaming sound wave as the streaming sound wave has been destroyed"));
			}
		});
		return;
	}

	FAudioTaskDelegate AppendAudioDataTask = FAudioTaskDelegate::CreateWeakLambda(this, [this, AudioData = MoveTemp(AudioData), AudioFormat]() mutable
	{
		FEncodedAudioStruct EncodedAudioInfo(MoveTemp(AudioData), AudioFormat);
		FDecodedAudioStruct DecodedAudioInfo;
		if (!URuntimeAudioImporterLibrary::DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to decode audio data to populate streaming sound wave audio data"));
			return;
		}

		PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});

	if (!AppendAudioTaskQueue.IsEmpty())
	{
		AppendAudioTaskQueue.Enqueue(AppendAudioDataTask);
	}
	else
	{
		AppendAudioDataTask.ExecuteIfBound();
	}
}

void UStreamingSoundWave::AppendAudioDataFromRAW(TArray<uint8> RAWData, ERuntimeRAWAudioFormat RAWFormat, int32 InSampleRate, int32 NumOfChannels)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = TWeakObjectPtr<UStreamingSoundWave>(this), RAWData = MoveTemp(RAWData), RAWFormat, InSampleRate, NumOfChannels]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AppendAudioDataFromRAW(MoveTemp(RAWData), RAWFormat, InSampleRate, NumOfChannels);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to append RAW audio data to streaming sound wave as the streaming sound wave has been destroyed"));
			}
		});
		return;
	}

	uint8* ByteDataPtr = RAWData.GetData();
	const int64 ByteDataSize = RAWData.Num();

	float* Float32DataPtr = nullptr;
	int64 NumOfSamples = 0;

	// Transcoding RAW data to 32-bit float data
	{
		switch (RAWFormat)
		{
		case ERuntimeRAWAudioFormat::Int8:
			{
				NumOfSamples = ByteDataSize / sizeof(int8);
				FRAW_RuntimeCodec::TranscodeRAWData<int8, float>(reinterpret_cast<int8*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt8:
			{
				NumOfSamples = ByteDataSize / sizeof(uint8);
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, float>(ByteDataPtr, NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::Int16:
			{
				NumOfSamples = ByteDataSize / sizeof(int16);
				FRAW_RuntimeCodec::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt16:
			{
				NumOfSamples = ByteDataSize / sizeof(uint16);
				FRAW_RuntimeCodec::TranscodeRAWData<uint16, float>(reinterpret_cast<uint16*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt32:
			{
				NumOfSamples = ByteDataSize / sizeof(uint32);
				FRAW_RuntimeCodec::TranscodeRAWData<uint32, float>(reinterpret_cast<uint32*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::Int32:
			{
				NumOfSamples = ByteDataSize / sizeof(int32);
				FRAW_RuntimeCodec::TranscodeRAWData<int32, float>(reinterpret_cast<int32*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::Float32:
			{
				NumOfSamples = ByteDataSize / sizeof(float);
				Float32DataPtr = static_cast<float*>(FMemory::Memcpy(FMemory::Malloc(ByteDataSize), ByteDataPtr, ByteDataSize));
				break;
			}
		}
	}

	if (!Float32DataPtr || NumOfSamples <= 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to transcode RAW data to decoded audio info"))
		return;
	}

	FDecodedAudioStruct DecodedAudioInfo;
	{
		FPCMStruct PCMInfo;
		{
			PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(Float32DataPtr, NumOfSamples);
			PCMInfo.PCMNumOfFrames = NumOfSamples / NumOfChannels;
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

	FAudioTaskDelegate AppendAudioDataTask = FAudioTaskDelegate::CreateWeakLambda(this, [this, DecodedAudioInfo = MoveTemp(DecodedAudioInfo)]() mutable
	{
		PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});

	if (!AppendAudioTaskQueue.IsEmpty())
	{
		AppendAudioTaskQueue.Enqueue(AppendAudioDataTask);
	}
	else
	{
		AppendAudioDataTask.ExecuteIfBound();
	}
}

void UStreamingSoundWave::SetStopSoundOnPlaybackFinish(bool bStop)
{
	bStopSoundOnPlaybackFinish = bStop;
}
