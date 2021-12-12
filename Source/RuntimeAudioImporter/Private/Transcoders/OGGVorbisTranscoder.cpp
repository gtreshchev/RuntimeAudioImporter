#include "Transcoders/OGGVorbisTranscoder.h"
#include "RuntimeAudioImporterTypes.h"

#define INCLUDE_STB_VORBIS
#include "RuntimeAudioImporterDefines.h"
#undef INCLUDE_STB_VORBIS

bool OGGVorbisTranscoder::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	stb_vorbis* STB_Vorbis{stb_vorbis_open_memory(EncodedData.AudioData, EncodedData.AudioDataSize, nullptr, nullptr)};

	if (STB_Vorbis == nullptr)
	{
		return false;
	}

	const stb_vorbis_info& info{stb_vorbis_get_info(STB_Vorbis)};

	const int32 SampleCount = stb_vorbis_stream_length_in_samples(STB_Vorbis);

	DecodedData.PCMInfo.PCMData = reinterpret_cast<uint8*>(new float[SampleCount * info.channels]);

	DecodedData.PCMInfo.PCMNumOfFrames = 0;

	TArray<float> PCMData_Array;

	while (true)
	{
		float** DecodedBufferInFrame;
		const int32 NumOfSamplesInFrame =
			stb_vorbis_get_frame_float(STB_Vorbis, nullptr, &DecodedBufferInFrame);
		if (NumOfSamplesInFrame == 0)
		{
			break;
		}

		for (int32 CurrentSampleInFrame = 0; CurrentSampleInFrame < NumOfSamplesInFrame; ++CurrentSampleInFrame)
		{
			for (int32 Channel = 0; Channel < info.channels; ++Channel)
			{
				PCMData_Array.Emplace(DecodedBufferInFrame[Channel][CurrentSampleInFrame]);
			}
		}

		/** Filling the number of frames */
		DecodedData.PCMInfo.PCMNumOfFrames += NumOfSamplesInFrame;
	}

	FMemory::Memcpy(DecodedData.PCMInfo.PCMData, PCMData_Array.GetData(), PCMData_Array.Num() - 2);

	/** Clearing unused buffer */
	PCMData_Array.Empty();

	/** Uninitializing transcoding of audio data in memory */
	stb_vorbis_close(STB_Vorbis);

	/** Getting PCM data size */
	DecodedData.PCMInfo.PCMDataSize = static_cast<uint32>(DecodedData.PCMInfo.PCMNumOfFrames *
		info.channels * sizeof(float));

	/** Getting basic audio information */
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedData.PCMInfo.PCMNumOfFrames) / info.
			sample_rate;
		DecodedData.SoundWaveBasicInfo.ChannelsNum = info.channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = info.sample_rate;
	}

	return true;
}
