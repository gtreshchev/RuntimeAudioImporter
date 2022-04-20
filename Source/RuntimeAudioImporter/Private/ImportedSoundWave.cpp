// Georgy Treshchev 2022.

#include "ImportedSoundWave.h"
#include "RuntimeAudioImporterDefines.h"

#include "Async/Async.h"

void UImportedSoundWave::BeginDestroy()
{
	UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Imported sound wave ('%s') data will be cleared because it is being unloaded"), *GetName());

	Super::BeginDestroy();
}

void UImportedSoundWave::ReleaseMemory()
{
	UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Releasing memory for the sound wave '%s'"), *GetName());

	PCMBufferInfo.~FPCMStruct();
}

bool UImportedSoundWave::RewindPlaybackTime(const float PlaybackTime)
{
	if (PlaybackTime > Duration)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to rewind playback time for the imported sound wave '%s' by time '%f' because total length is '%f'"), *GetName(), PlaybackTime, Duration);
		return false;
	}

	return ChangeCurrentFrameCount(PlaybackTime * SampleRate);
}

bool UImportedSoundWave::ChangeCurrentFrameCount(const uint32 NumOfFrames)
{
	if (NumOfFrames < 0 || NumOfFrames > PCMBufferInfo.PCMNumOfFrames)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Cannot change the current frame for the imported sound wave '%s' to frame '%d' because the total number of frames is '%d'"), *GetName(), NumOfFrames, PCMBufferInfo.PCMNumOfFrames);
		return false;
	}

	CurrentNumOfFrames = NumOfFrames;

	// Setting "PlaybackFinishedBroadcast" to "false" in order to re-broadcast the "OnAudioPlaybackFinished" delegate again
	PlaybackFinishedBroadcast = false;

	return true;
}

float UImportedSoundWave::GetPlaybackTime() const
{
	return static_cast<float>(CurrentNumOfFrames) / SampleRate;
}

float UImportedSoundWave::GetDurationConst() const
{
	return Duration;
}


float UImportedSoundWave::GetDuration()
#if ENGINE_MAJOR_VERSION >= 5
const
#endif
{
	return GetDurationConst();
}

float UImportedSoundWave::GetPlaybackPercentage() const
{
	return (GetPlaybackTime() / GetDurationConst()) * 100;
}

bool UImportedSoundWave::IsPlaybackFinished()
{
	return GetPlaybackPercentage() == 100 && PCMBufferInfo.PCMData.GetView().GetData() != nullptr && PCMBufferInfo.PCMNumOfFrames > 0 && PCMBufferInfo.PCMData.GetView().Num() > 0;
}

int32 UImportedSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	// Ensure there is enough number of frames. Lack of frames means audio playback has finished
	if (static_cast<uint32>(CurrentNumOfFrames) >= PCMBufferInfo.PCMNumOfFrames)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			if (!PlaybackFinishedBroadcast)
			{
				UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Playback of the sound wave '%s' has been completed"), *GetName());

				PlaybackFinishedBroadcast = true;

				if (OnAudioPlaybackFinished.IsBound())
				{
					OnAudioPlaybackFinished.Broadcast();
				}
			}
		});

		return 0;
	}

	// Getting the remaining number of samples if the required number of samples is greater than the total available number
	if (static_cast<uint32>(CurrentNumOfFrames) + static_cast<uint32>(NumSamples) / static_cast<uint32>(NumChannels) >= PCMBufferInfo.PCMNumOfFrames)
	{
		NumSamples = (PCMBufferInfo.PCMNumOfFrames - CurrentNumOfFrames) * NumChannels;
	}

	// Retrieving a part of PCM data
	uint8* RetrievedPCMData = PCMBufferInfo.PCMData.GetView().GetData() + (CurrentNumOfFrames * NumChannels * sizeof(float));
	const int32 RetrievedPCMDataSize = NumSamples * sizeof(float);

	// Ensure we got a valid PCM data
	if (RetrievedPCMDataSize <= 0 || !RetrievedPCMData)
	{
		return 0;
	}

	// Filling in OutAudio array with the retrieved PCM data
	OutAudio = TArray<uint8>(RetrievedPCMData, RetrievedPCMDataSize);

	// Increasing CurrentFrameCount for correct iteration sequence
	CurrentNumOfFrames = CurrentNumOfFrames + (NumSamples / NumChannels);

	AsyncTask(ENamedThreads::GameThread, [this, RetrievedPCMData, RetrievedPCMDataSize]()
	{
		if (OnGeneratePCMData.IsBound())
		{
			OnGeneratePCMData.Broadcast(TArray<float>(reinterpret_cast<float*>(RetrievedPCMData), RetrievedPCMDataSize));
		}
	});

	return NumSamples;
}

Audio::EAudioMixerStreamDataFormat::Type UImportedSoundWave::GetGeneratedPCMDataFormat() const
{
	return Audio::EAudioMixerStreamDataFormat::Type::Float;
}
