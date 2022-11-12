// Georgy Treshchev 2022.

#include "Transcoders/FlacTranscoder.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"

#define INCLUDE_FLAC
#include "TranscodersIncludes.h"
#undef INCLUDE_FLAC

bool FlacTranscoder::CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	drflac* FLAC{drflac_open_memory(AudioData, AudioDataSize, nullptr)};

	if (!FLAC)
	{
		return false;
	}

	drflac_close(FLAC);

	return true;
}

bool FlacTranscoder::Decode(FEncodedAudioStruct&& EncodedData, FDecodedAudioStruct& DecodedData)
{
	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Decoding Flac audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString()));

	// Initializing transcoding of audio data in memory
	drflac* FLAC_Decoder{drflac_open_memory(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr)};

	if (!FLAC_Decoder)
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Unable to initialize FLAC Decoder"));
		return false;
	}

	// Allocating memory for PCM data
	float* TempPCMData = static_cast<float*>(FMemory::Malloc(FLAC_Decoder->totalPCMFrameCount * FLAC_Decoder->channels * sizeof(float)));

	if (!TempPCMData)
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to allocate memory for FLAC Decoder"));
		return false;
	}

	// Filling in PCM data and getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = drflac_read_pcm_frames_f32(FLAC_Decoder, FLAC_Decoder->totalPCMFrameCount, TempPCMData);

	// Getting PCM data size
	const int32 TempPCMDataSize = static_cast<int32>(DecodedData.PCMInfo.PCMNumOfFrames * FLAC_Decoder->channels * sizeof(float));

	DecodedData.PCMInfo.PCMData = FBulkDataBuffer<float>(TempPCMData, TempPCMDataSize);

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(FLAC_Decoder->totalPCMFrameCount) / FLAC_Decoder->sampleRate;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = FLAC_Decoder->channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = FLAC_Decoder->sampleRate;
	}

	// Uninitializing transcoding of audio data in memory
	drflac_close(FLAC_Decoder);

	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Successfully decoded Flac audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString()));

	return true;
}
