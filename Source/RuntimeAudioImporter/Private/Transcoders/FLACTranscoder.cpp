// Georgy Treshchev 2022.

#include "Transcoders/FLACTranscoder.h"
#include "RuntimeAudioImporterTypes.h"

#define INCLUDE_FLAC
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_FLAC

bool FLACTranscoder::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	/** Initializing transcoding of audio data in memory */
	drflac* pFlac{drflac_open_memory(EncodedData.AudioData, EncodedData.AudioDataSize, nullptr)};
	if (pFlac == nullptr)
	{
		return false;
	}

	/** Allocating memory for PCM data */
	DecodedData.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(pFlac->totalPCMFrameCount * pFlac->channels * sizeof(float)));

	/** Filling PCM data and getting the number of frames */
	DecodedData.PCMInfo.PCMNumOfFrames = drflac_read_pcm_frames_f32(pFlac, pFlac->totalPCMFrameCount, reinterpret_cast<float*>(DecodedData.PCMInfo.PCMData));

	/** Getting PCM data size */
	DecodedData.PCMInfo.PCMDataSize = static_cast<uint32>(DecodedData.PCMInfo.PCMNumOfFrames * pFlac->channels * sizeof(float));

	/** Getting basic audio information */
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(pFlac->totalPCMFrameCount) / pFlac->sampleRate;
		DecodedData.SoundWaveBasicInfo.ChannelsNum = pFlac->channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = pFlac->sampleRate;
	}

	/** Uninitializing transcoding of audio data in memory */
	drflac_close(pFlac);

	return true;
}
