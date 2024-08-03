// Georgy Treshchev 2024.

#include "Sound/StreamingSoundWave.h"

#include "RuntimeAudioImporterLibrary.h"
#include "Codecs/RAW_RuntimeCodec.h"

#include "Async/Async.h"
#include "SampleBuffer.h"
#include "VAD/RuntimeVoiceActivityDetector.h"
#include "UObject/WeakObjectPtrTemplates.h"

UStreamingSoundWave::UStreamingSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AudioTaskPipe = MakeUnique<UE::Tasks::FPipe>(*FString::Printf(TEXT("AudioTaskPipe_%s"), *GetName()));
	ensureMsgf(AudioTaskPipe, TEXT("AudioTaskPipe is not initialized. This will cause issues with audio data appending"));

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
}

bool UStreamingSoundWave::ToggleVAD(bool bVAD)
{
	VADInstance = bVAD ? NewObject<URuntimeVoiceActivityDetector>() : nullptr;
	return true;
}

bool UStreamingSoundWave::ResetVAD()
{
	if (VADInstance)
	{
		return VADInstance->ResetVAD();
	}
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to reset VAD as the VAD instance is not valid"));
	return false;
}

bool UStreamingSoundWave::SetVADMode(ERuntimeVADMode Mode)
{
	if (VADInstance)
	{
		return VADInstance->SetVADMode(Mode);
	}
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to set VAD mode as the VAD instance is not valid"));
	return false;
}

void UStreamingSoundWave::PopulateAudioDataFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo)
{
#if WITH_RUNTIMEAUDIOIMPORTER_VAD_SUPPORT
	// Process VAD if necessary
	if (VADInstance)
	{
		bool bDetected = VADInstance->ProcessVAD(TArray<float>(reinterpret_cast<const float*>(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData()), static_cast<int32>(DecodedAudioInfo.PCMInfo.PCMData.GetView().Num())), DecodedAudioInfo.SoundWaveBasicInfo.SampleRate, DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels);
		if (!bDetected)
		{
			UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("VAD detected silence, skipping audio data append"));
			return;
		}
		UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("VAD detected voice, appending audio data"));
	}
#endif

	{
		FRAIScopeLock Lock(&*DataGuard);
		if (!DecodedAudioInfo.IsValid())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to continue populating the audio data because the decoded info is invalid"));
			return;
		}

		// Whether the audio data has been populated with PCM buffer
		const bool bHasPreviouslyPopulatedRealPCMData = PCMBufferInfo->PCMData.GetView().Num() > 0;

		// Make sure the sample rate and the number of channels match the previously populated audio data
		if (bHasPreviouslyPopulatedRealPCMData)
		{
			URuntimeAudioImporterLibrary::ResampleAndMixChannelsInDecodedInfo(DecodedAudioInfo, SampleRate, NumChannels);
		}

		// If the sound wave has not yet been filled in with audio data and the initial desired sample rate and the number of channels are set, resample and mix the channels
		else if (InitialDesiredSampleRate.IsSet() || InitialDesiredNumOfChannels.IsSet())
		{
			URuntimeAudioImporterLibrary::ResampleAndMixChannelsInDecodedInfo(DecodedAudioInfo,
				InitialDesiredSampleRate.IsSet() ? InitialDesiredSampleRate.GetValue() : DecodedAudioInfo.SoundWaveBasicInfo.SampleRate,
				InitialDesiredNumOfChannels.IsSet() ? InitialDesiredNumOfChannels.GetValue() : DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels);
		}

		// Update the initial audio data if it hasn't already been filled in
		if (!bHasPreviouslyPopulatedRealPCMData)
		{
			SetSampleRate(DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
			NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
		}

		PCMBufferInfo->PCMData.Append(DecodedAudioInfo.PCMInfo.PCMData);

		PCMBufferInfo->PCMNumOfFrames += DecodedAudioInfo.PCMInfo.PCMNumOfFrames;
		Duration += DecodedAudioInfo.SoundWaveBasicInfo.Duration;
		ResetPlaybackFinish();
	}

	{
		const bool IsBound = [this]()
		{
			FRAIScopeLock Lock(&OnPopulateAudioData_DataGuard);
			return OnPopulateAudioDataNative.IsBound() || OnPopulateAudioData.IsBound();
		}();
		if (IsBound)
		{
			TArray<float> PCMData(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());
			AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this), PCMData = MoveTemp(PCMData)]() mutable
			{
				if (WeakThis.IsValid())
				{
					FRAIScopeLock Lock(&WeakThis->OnPopulateAudioData_DataGuard);
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
			FRAIScopeLock Lock(&OnPopulateAudioData_DataGuard);
			return OnPopulateAudioStateNative.IsBound() || OnPopulateAudioState.IsBound();
		}();
		if (IsBound)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this)]()
			{
				if (WeakThis.IsValid())
				{
					FRAIScopeLock Lock(&WeakThis->OnPopulateAudioData_DataGuard);
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

	UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Successfully added audio data to streaming sound wave.\nAdded audio info: %s"), *DecodedAudioInfo.ToString());
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
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = MakeWeakObjectPtr(this), NumOfBytesToPreAllocate, Result]()
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
		
	FRAIScopeLock Lock(&*DataGuard);

	auto ExecuteResult = [Result](bool bSucceeded)
	{
		AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded]()
		{
			Result.ExecuteIfBound(bSucceeded);
		});
	};

	PCMBufferInfo->PCMData.Reserve(NumOfBytesToPreAllocate / sizeof(float));

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully pre-allocated '%lld' number of bytes"), NumOfBytesToPreAllocate);
	ExecuteResult(true);
}

void UStreamingSoundWave::AppendAudioDataFromEncoded(TArray<uint8> AudioData, ERuntimeAudioFormat AudioFormat)
{
	if (IsInGameThread())
	{
		AudioTaskPipe->Launch(AudioTaskPipe->GetDebugName(), [WeakThis = MakeWeakObjectPtr(this), AudioData = MoveTemp(AudioData), AudioFormat]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AppendAudioDataFromEncoded(MoveTemp(AudioData), AudioFormat);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to append audio data to streaming sound wave as the streaming sound wave has been destroyed"));
			}
		}, UE::Tasks::ETaskPriority::BackgroundHigh);
		return;
	}

	FEncodedAudioStruct EncodedAudioInfo(MoveTemp(AudioData), AudioFormat);
	FDecodedAudioStruct DecodedAudioInfo;
	if (!URuntimeAudioImporterLibrary::DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to decode audio data to populate streaming sound wave audio data"));
		return;
	}

	PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
}

void UStreamingSoundWave::AppendAudioDataFromRAW(TArray<uint8> RAWData, ERuntimeRAWAudioFormat RAWFormat, int32 InSampleRate, int32 NumOfChannels)
{
	if (IsInGameThread())
	{
		AudioTaskPipe->Launch(AudioTaskPipe->GetDebugName(), [WeakThis = MakeWeakObjectPtr(this), RAWData = MoveTemp(RAWData), RAWFormat, InSampleRate, NumOfChannels]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AppendAudioDataFromRAW(MoveTemp(RAWData), RAWFormat, InSampleRate, NumOfChannels);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to append RAW audio data to streaming sound wave as the streaming sound wave has been destroyed"));
			}
		}, UE::Tasks::ETaskPriority::BackgroundHigh);
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

	PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
}

void UStreamingSoundWave::SetStopSoundOnPlaybackFinish(bool bStop)
{
	bStopSoundOnPlaybackFinish = bStop;
}
