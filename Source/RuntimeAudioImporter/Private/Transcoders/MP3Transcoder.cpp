// Georgy Treshchev 2022.

#include "Transcoders/MP3Transcoder.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"

#define INCLUDE_MP3
#include "TranscodersIncludes.h"
#undef INCLUDE_MP3

bool MP3Transcoder::CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	drmp3 MP3;

	if (!drmp3_init_memory(&MP3, AudioData, AudioDataSize, nullptr))
	{
		return false;
	}

	drmp3_uninit(&MP3);

	return true;
}

bool MP3Transcoder::Decode(FEncodedAudioStruct&& EncodedData, FDecodedAudioStruct& DecodedData)
{
	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Decoding MP3 audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString()));

	drmp3 MP3_Decoder;

	// Initializing transcoding of audio data in memory
	if (!drmp3_init_memory(&MP3_Decoder, EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr))
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Unable to initialize MP3 Decoder"));
		return false;
	}

	// Allocating memory for PCM data
	float* TempPCMData = static_cast<float*>(FMemory::Malloc(drmp3_get_pcm_frame_count(&MP3_Decoder) * MP3_Decoder.channels * sizeof(float)));

	if (!TempPCMData)
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to allocate memory for MP3 Decoder"));
		return false;
	}

	// Filling in PCM data and getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = drmp3_read_pcm_frames_f32(&MP3_Decoder, drmp3_get_pcm_frame_count(&MP3_Decoder), TempPCMData);

	// Getting PCM data size
	const int32 TempPCMDataSize = static_cast<int32>(DecodedData.PCMInfo.PCMNumOfFrames * MP3_Decoder.channels * sizeof(float));

	DecodedData.PCMInfo.PCMData = FBulkDataBuffer<float>(TempPCMData, TempPCMDataSize);

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(drmp3_get_pcm_frame_count(&MP3_Decoder)) / MP3_Decoder.sampleRate;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = MP3_Decoder.channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = MP3_Decoder.sampleRate;
	}

	/** Uninitializing transcoding of audio data in memory */
	drmp3_uninit(&MP3_Decoder);

	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Successfully decoded MP3 audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString()));

	return true;
}
