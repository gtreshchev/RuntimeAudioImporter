// Georgy Treshchev 2022.

#include "Transcoders/VorbisTranscoder.h"
#include "RuntimeAudioImporterTypes.h"
#include "Transcoders/RAWTranscoder.h"

#define INCLUDE_VORBIS
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_VORBIS

bool VorbisTranscoder::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	short* DecodedBufferInt16Buffer;
	int32 SampleRate, NumOfChannels;

	/** Decoding the data */
	const int32 NumOfFrames = stb_vorbis_decode_memory(EncodedData.AudioData, EncodedData.AudioDataSize, &NumOfChannels, &SampleRate, &DecodedBufferInt16Buffer);

	/** Checking if we got a valid data */
	if (DecodedBufferInt16Buffer == nullptr || NumOfFrames == -1)
	{
		return false;
	}

	/** Getting the number of frames */
	DecodedData.PCMInfo.PCMNumOfFrames = NumOfFrames;

	/** Getting PCM data size */
	DecodedData.PCMInfo.PCMDataSize = static_cast<uint32>(DecodedData.PCMInfo.PCMNumOfFrames * NumOfChannels * 2);

	/** Transcoding int16 to float format */
	{
		float* TempFloatBuffer = new float[DecodedData.PCMInfo.PCMNumOfFrames * NumOfChannels * 2];
		uint32 TempFloatSize;
		
		RAWTranscoder::TranscodeRAWData<int16, float>(DecodedBufferInt16Buffer, DecodedData.PCMInfo.PCMDataSize, TempFloatBuffer, TempFloatSize);
		DecodedData.PCMInfo.PCMData = reinterpret_cast<uint8*>(TempFloatBuffer);
	}

	/** Getting basic audio information */
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedData.PCMInfo.PCMNumOfFrames) / SampleRate;
		DecodedData.SoundWaveBasicInfo.ChannelsNum = NumOfChannels;
		DecodedData.SoundWaveBasicInfo.SampleRate = SampleRate;
	}

	return true;
}
