// Georgy Treshchev 2023.

#include "Codecs/MP3_RuntimeCodec.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "HAL/UnrealMemory.h"

#define INCLUDE_MP3
#include "CodecIncludes.h"
#undef INCLUDE_MP3

bool FMP3_RuntimeCodec::CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	drmp3 MP3;

	if (!drmp3_init_memory(&MP3, AudioData.GetView().GetData(), AudioData.GetView().Num(), nullptr))
	{
		return false;
	}

	drmp3_uninit(&MP3);
	return true;
}

bool FMP3_RuntimeCodec::GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Retrieving header information for MP3 audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to retrieve audio header information in the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

	drmp3 MP3;
	if (!drmp3_init_memory(&MP3, EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to initialize MP3 Decoder"));
		return false;
	}

	const drmp3_uint64 PCMFrameCount = drmp3_get_pcm_frame_count(&MP3);

	{
		HeaderInfo.Duration = static_cast<float>(PCMFrameCount) / MP3.sampleRate;
		HeaderInfo.NumOfChannels = MP3.channels;
		HeaderInfo.SampleRate = MP3.sampleRate;
		HeaderInfo.PCMDataSize = PCMFrameCount * MP3.channels;
		HeaderInfo.AudioFormat = GetAudioFormat();
	}

	drmp3_uninit(&MP3);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved header information for MP3 audio format.\nHeader info: %s"), *HeaderInfo.ToString());
	return true;
}

bool FMP3_RuntimeCodec::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	ensureMsgf(false, TEXT("MP3 codec does not support encoding at the moment"));
	return false;
}

bool FMP3_RuntimeCodec::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Decoding MP3 audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to decode audio data using the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

	drmp3 MP3_Decoder;

	// Initializing MP3 codec
	if (!drmp3_init_memory(&MP3_Decoder, EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize MP3 Decoder"));
		return false;
	}

	const drmp3_uint64 PCMFrameCount = drmp3_get_pcm_frame_count(&MP3_Decoder);

	// Allocating memory for PCM data
	float* TempPCMData = static_cast<float*>(FMemory::Malloc(PCMFrameCount * MP3_Decoder.channels * sizeof(float)));

	if (!TempPCMData)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for MP3 Decoder"));
		return false;
	}

	// Filling in PCM data and getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = drmp3_read_pcm_frames_f32(&MP3_Decoder, PCMFrameCount, TempPCMData);

	// Getting PCM data size
	const int64 TempPCMDataSize = static_cast<int64>(DecodedData.PCMInfo.PCMNumOfFrames * MP3_Decoder.channels);

	DecodedData.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(TempPCMData, TempPCMDataSize);

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(PCMFrameCount) / MP3_Decoder.sampleRate;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = MP3_Decoder.channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = MP3_Decoder.sampleRate;
	}

	drmp3_uninit(&MP3_Decoder);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully decoded MP3 audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString());
	return true;
}
