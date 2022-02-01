// Georgy Treshchev 2022.

#include "Transcoders/VorbisTranscoder.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "Transcoders/RAWTranscoder.h"

#define INCLUDE_VORBIS
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_VORBIS

bool VorbisTranscoder::CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	int32 ErrorCode;
	stb_vorbis* STBVorbis = stb_vorbis_open_memory(AudioData, AudioDataSize, &ErrorCode, nullptr);

	if (STBVorbis == nullptr)
	{
		return false;
	}

	return true;
}

bool VorbisTranscoder::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	int32 ErrorCode;
	stb_vorbis* STBVorbis = stb_vorbis_open_memory(EncodedData.AudioData, EncodedData.AudioDataSize, &ErrorCode, nullptr);

	if (STBVorbis == nullptr)
	{
		//UE_INTERNAL_LOG_IMPL(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize OGG Vorbis Decoder"));
		return false;
	}

	const int32 SamplesLimit{STBVorbis->channels * 4096};
	const int32 NumOfChannels{STBVorbis->channels};
	const int32 SampleRate{static_cast<int32>(STBVorbis->sample_rate)};

	int32 SamplesOffset = 0;
	int32 NumOfFrames = 0;

	int32 TotalSamples = SamplesLimit;

	int16* Int16RAWBuffer = static_cast<int16*>(FMemory::Malloc(TotalSamples * sizeof(int16)));
	if (Int16RAWBuffer == nullptr)
	{
		//UE_INTERNAL_LOG_IMPL(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for OGG Vorbis Decoder"));
		stb_vorbis_close(STBVorbis);
		return false;
	}

	while (true)
	{
		const int32 CurrentFrames = stb_vorbis_get_frame_short_interleaved(STBVorbis, NumOfChannels, Int16RAWBuffer + SamplesOffset, TotalSamples - SamplesOffset);

		if (CurrentFrames == 0) break;

		NumOfFrames += CurrentFrames;
		SamplesOffset += CurrentFrames * NumOfChannels;

		if (SamplesOffset + SamplesLimit > TotalSamples)
		{
			TotalSamples *= 2;

			int16* Int16RAWBufferFrame = static_cast<int16*>(FMemory::Realloc(Int16RAWBuffer, TotalSamples * sizeof(int16)));

			if (Int16RAWBufferFrame == nullptr)
			{
				//UE_INTERNAL_LOG_IMPL(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for OGG Vorbis Decoder"));

				FMemory::Free(Int16RAWBuffer);
				stb_vorbis_close(STBVorbis);

				return false;
			}

			Int16RAWBuffer = Int16RAWBufferFrame;
		}
	}

	stb_vorbis_close(STBVorbis);


	/** Getting the number of frames */
	DecodedData.PCMInfo.PCMNumOfFrames = NumOfFrames;

	/** Getting PCM data size */
	DecodedData.PCMInfo.PCMDataSize = DecodedData.PCMInfo.PCMNumOfFrames * NumOfChannels * 2;

	/** Transcoding int16 to float format */
	{
		float* TempFloatBuffer = new float[DecodedData.PCMInfo.PCMNumOfFrames * NumOfChannels * 2];
		int32 TempFloatSize;

		RAWTranscoder::TranscodeRAWData<int16, float>(Int16RAWBuffer, DecodedData.PCMInfo.PCMDataSize, TempFloatBuffer, TempFloatSize);
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
