// Georgy Treshchev 2022.

#include "Sound/StreamingSoundWave.h"
#include "RuntimeAudioImporterLibrary.h"
#include "Transcoders/RAWTranscoder.h"

#include "Async/Async.h"

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

	const int32 NewPCMDataSize = PCMBufferInfo->PCMData.GetView().Num() + DecodedAudioInfo.PCMInfo.PCMData.GetView().Num();
	float* NewPCMDataPtr = static_cast<float*>(FMemory::Malloc(NewPCMDataSize * sizeof(float)));

	if (!NewPCMDataPtr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory to populate streaming sound wave audio data"));
		return;
	}

	// Adding new PCM data at the end
	{
		FMemory::Memcpy(NewPCMDataPtr, PCMBufferInfo->PCMData.GetView().GetData(), PCMBufferInfo->PCMData.GetView().Num());
		FMemory::Memcpy(NewPCMDataPtr + (PCMBufferInfo->PCMData.GetView().Num() / sizeof(float)), DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());
	}

	PCMBufferInfo->PCMData = FBulkDataBuffer<float>(NewPCMDataPtr, NewPCMDataSize);
	PCMBufferInfo->PCMNumOfFrames += DecodedAudioInfo.PCMInfo.PCMNumOfFrames;

	Duration += DecodedAudioInfo.SoundWaveBasicInfo.Duration;

	ResetPlaybackFinish();

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully added audio data to streaming sound wave.\nAdded audio info: %s"), *DecodedAudioInfo.ToString());
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

void UStreamingSoundWave::AppendAudioDataFromEncoded(TArray<uint8> AudioData, EAudioFormat AudioFormat)
{
	TWeakObjectPtr<UStreamingSoundWave> ThisPtr(this);
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisPtr, AudioData = MoveTemp(AudioData), AudioFormat]()
	{
		if (!ThisPtr.IsValid())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to append audio data from encoded: the streaming sound wave has been garbage collected"));
			return;
		}

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

		ThisPtr->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void UStreamingSoundWave::AppendAudioDataFromRAW(TArray<uint8> RAWData, ERAWAudioFormat RAWFormat, int32 InSampleRate, int32 NumOfChannels)
{
	TWeakObjectPtr<UStreamingSoundWave> ThisPtr(this);
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisPtr, RAWData = MoveTemp(RAWData), RAWFormat, InSampleRate, NumOfChannels]() mutable
	{
		if (!ThisPtr.IsValid())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to append audio data from encoded: the streaming sound wave has been garbage collected"));
			return;
		}

		uint8* RAWDataPtr{RAWData.GetData()};
		const int32 RAWDataSize{RAWData.Num()};

		float* PCMData{nullptr};
		int32 PCMDataSize{0};

		// Transcoding RAW data to 32-bit float data
		{
			switch (RAWFormat)
			{
			case ERAWAudioFormat::Int16:
				{
					RAWTranscoder::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(RAWDataPtr), RAWDataSize, PCMData, PCMDataSize);
					break;
				}
			case ERAWAudioFormat::Int32:
				{
					RAWTranscoder::TranscodeRAWData<int32, float>(reinterpret_cast<int32*>(RAWDataPtr), RAWDataSize, PCMData, PCMDataSize);
					break;
				}
			case ERAWAudioFormat::UInt8:
				{
					RAWTranscoder::TranscodeRAWData<uint8, float>(RAWDataPtr, RAWDataSize, PCMData, PCMDataSize);
					break;
				}
			case ERAWAudioFormat::Float32:
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
				PCMInfo.PCMData = FBulkDataBuffer<float>(PCMData, PCMDataSize);
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

		ThisPtr->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void UStreamingSoundWave::SetStopSoundOnPlaybackFinish(bool bStop)
{
	bStopSoundOnPlaybackFinish = bStop;
}
