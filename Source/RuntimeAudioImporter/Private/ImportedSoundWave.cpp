// Georgy Treshchev 2022.

#include "ImportedSoundWave.h"

#include "Async/Async.h"

void UImportedSoundWave::BeginDestroy()
{
	USoundWaveProcedural::BeginDestroy();

	// Releasing memory on sound wave destroy
	ReleaseMemory();
}

void UImportedSoundWave::ReleaseMemory()
{
	if (PCMBufferInfo.PCMData && PCMBufferInfo.PCMNumOfFrames > 0 && PCMBufferInfo.PCMDataSize > 0)
	{
		// Release PCM data on destruction
		FMemory::Free(PCMBufferInfo.PCMData);
	}
	PCMBufferInfo = FPCMStruct();
}

bool UImportedSoundWave::RewindPlaybackTime(const float PlaybackTime)
{
	if (PlaybackTime > GetDuration())
	{
		return false;
	}
	return ChangeCurrentFrameCount(PlaybackTime * SampleRate);
}

bool UImportedSoundWave::ChangeCurrentFrameCount(const int32 NumOfFrames)
{
	if (NumOfFrames < 0 || NumOfFrames > PCMBufferInfo.PCMNumOfFrames)
	{
		return false;
	}
	CurrentNumOfFrames = NumOfFrames;

	// Setting "PlaybackFinishedBroadcast" to "false" in order to rebroadcast the "OnAudioPlaybackFinished" delegate again
	PlaybackFinishedBroadcast = false;

	return true;
}

float UImportedSoundWave::GetPlaybackTime() const
{
	return static_cast<float>(CurrentNumOfFrames) / SampleRate;
}

float UImportedSoundWave::GetDurationConst() const
{
	return static_cast<float>(PCMBufferInfo.PCMNumOfFrames) / SampleRate;
}

float UImportedSoundWave::GetDuration()
{
	return GetDurationConst();
}

float UImportedSoundWave::GetPlaybackPercentage() const
{
	return (GetPlaybackTime() / GetDurationConst()) * 100;
}

bool UImportedSoundWave::IsPlaybackFinished()
{
	return GetPlaybackPercentage() == 100 && PCMBufferInfo.PCMData && PCMBufferInfo.PCMNumOfFrames > 0 && PCMBufferInfo.PCMDataSize > 0;
}

int32 UImportedSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	/** Ensure there is enough number of frames. Lack of frames means audio playback has finished */
	if (CurrentNumOfFrames >= PCMBufferInfo.PCMNumOfFrames)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			if (!PlaybackFinishedBroadcast)
			{
				PlaybackFinishedBroadcast = true;

				if (OnAudioPlaybackFinished.IsBound())
				{
					OnAudioPlaybackFinished.Broadcast();
				}
			}
		});

		return 0;
	}

	/** Getting the remaining number of samples if the required number of samples is greater than the total available number */
	if (CurrentNumOfFrames + NumSamples / NumChannels >= static_cast<int32>(PCMBufferInfo.PCMNumOfFrames))
	{
		NumSamples = (static_cast<int32>(PCMBufferInfo.PCMNumOfFrames) - CurrentNumOfFrames) * NumChannels;
	}

	/** Retrieving a part of PCM data */
	uint8* RetrievedPCMData = PCMBufferInfo.PCMData + (CurrentNumOfFrames * NumChannels * sizeof(float));
	const int32 RetrievedPCMDataSize = NumSamples * sizeof(float);

	/** Ensure we got a valid PCM data */
	if (RetrievedPCMDataSize <= 0 || !RetrievedPCMData)
	{
		return 0;
	}

	/** Filling OutAudio array with the retrieved PCM data */
	OutAudio = TArray<uint8>(RetrievedPCMData, RetrievedPCMDataSize);

	/** Increasing CurrentFrameCount for correct iteration sequence */
	CurrentNumOfFrames = CurrentNumOfFrames + NumSamples / NumChannels;

	AsyncTask(ENamedThreads::GameThread, [=]()
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
