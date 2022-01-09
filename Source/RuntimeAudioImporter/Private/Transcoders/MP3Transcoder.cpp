// Georgy Treshchev 2022.

#include "Transcoders/MP3Transcoder.h"
#include "RuntimeAudioImporterTypes.h"

#define INCLUDE_MP3
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_MP3

bool MP3Transcoder::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	drmp3 mp3;

	/** Initializing transcoding of audio data in memory */
	if (!drmp3_init_memory(&mp3, EncodedData.AudioData, EncodedData.AudioDataSize, nullptr))
	{
		return false;
	}

	/** Allocating memory for PCM data */
	DecodedData.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(drmp3_get_pcm_frame_count(&mp3) * mp3.channels * sizeof(float)));

	/** Filling PCM data and getting the number of frames */
	DecodedData.PCMInfo.PCMNumOfFrames = drmp3_read_pcm_frames_f32(&mp3, drmp3_get_pcm_frame_count(&mp3), reinterpret_cast<float*>(DecodedData.PCMInfo.PCMData));

	/** Getting PCM data size */
	DecodedData.PCMInfo.PCMDataSize = static_cast<uint32>(DecodedData.PCMInfo.PCMNumOfFrames * mp3.channels * sizeof(float));

	/** Getting basic audio information */
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(drmp3_get_pcm_frame_count(&mp3)) / mp3.sampleRate;
		DecodedData.SoundWaveBasicInfo.ChannelsNum = mp3.channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = mp3.sampleRate;
	}

	/** Uninitializing transcoding of audio data in memory */
	drmp3_uninit(&mp3);

	return true;
}
