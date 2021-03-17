// Georgy Treshchev 2021.

#include "ImportedSoundWave.h"

void UImportedSoundWave::ReleaseMemory()
{
	if (PCMBufferInfo.PCMData)
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
	return true;
}

float UImportedSoundWave::GetPlaybackTime() const
{
	return static_cast<float>(CurrentNumOfFrames) / SampleRate;
}

float UImportedSoundWave::GetDuration()
{
	return static_cast<float>(PCMBufferInfo.PCMNumOfFrames) / SampleRate;
}

float UImportedSoundWave::GetPlaybackPercentage()
{
	return (GetPlaybackTime() / GetDuration()) * 100;
}

bool UImportedSoundWave::IsPlaybackFinished()
{
	return GetPlaybackPercentage() == 100 && PCMBufferInfo.PCMData && PCMBufferInfo.PCMNumOfFrames > 0 && PCMBufferInfo.PCMDataSize > 0;
}

int32 UImportedSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	// Ensure there is enough number of frames
	if (CurrentNumOfFrames >= static_cast<int32>(PCMBufferInfo.PCMNumOfFrames))
	{
		return 0;
	}

	// Getting the remaining number of samples if the required number of samples is greater than the total available number
	if (CurrentNumOfFrames + NumSamples / NumChannels > PCMBufferInfo.PCMNumOfFrames)
	{
		NumSamples = (PCMBufferInfo.PCMNumOfFrames - CurrentNumOfFrames) * NumChannels;
	}

	// Retrieving a part of PCM data 
	const uint8* RetrievedPCMData = PCMBufferInfo.PCMData + (CurrentNumOfFrames * NumChannels * sizeof(float));
	const int32 RetrievedPCMDataSize = NumSamples * NumChannels * sizeof(int32);

	// Ensure we got a valid PCM data
	if (RetrievedPCMDataSize <= 0 || !RetrievedPCMData)
	{
		return 0;
	}

	// Filling OutAudio array with the retrieved PCM data
	OutAudio = TArray<uint8>(RetrievedPCMData, RetrievedPCMDataSize);

	//Increasing CurrentFrameCount for correct iteration sequence
	CurrentNumOfFrames = CurrentNumOfFrames + NumSamples / NumChannels;

	return NumSamples;
}

Audio::EAudioMixerStreamDataFormat::Type UImportedSoundWave::GetGeneratedPCMDataFormat() const
{
	return Audio::EAudioMixerStreamDataFormat::Type::Float;
}
