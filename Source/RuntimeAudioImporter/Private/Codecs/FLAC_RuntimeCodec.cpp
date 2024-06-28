// Georgy Treshchev 2024.

#include "Codecs/FLAC_RuntimeCodec.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "HAL/UnrealMemory.h"

#define INCLUDE_FLAC
#include "CodecIncludes.h"
#undef INCLUDE_FLAC

bool FFLAC_RuntimeCodec::CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	drflac* FLAC = drflac_open_memory(AudioData.GetView().GetData(), AudioData.GetView().Num(), nullptr);

	if (!FLAC)
	{
		return false;
	}

	drflac_close(FLAC);
	return true;
}

bool FFLAC_RuntimeCodec::GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Retrieving header information for FLAC audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to retrieve audio header information in the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

	drflac* FLAC = drflac_open_memory(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr);
	if (!FLAC)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to initialize FLAC Decoder"));
		return false;
	}

	{
		HeaderInfo.Duration = static_cast<float>(FLAC->totalPCMFrameCount) / FLAC->sampleRate;
		HeaderInfo.NumOfChannels = FLAC->channels;
		HeaderInfo.SampleRate = FLAC->sampleRate;
		HeaderInfo.PCMDataSize = FLAC->totalPCMFrameCount * FLAC->channels;
		HeaderInfo.AudioFormat = GetAudioFormat();
	}

	drflac_close(FLAC);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved header information for FLAC audio format.\nHeader info: %s"), *HeaderInfo.ToString());
	return true;
}

bool FFLAC_RuntimeCodec::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	ensureMsgf(false, TEXT("FLAC codec does not support encoding at the moment"));
	return false;
}

bool FFLAC_RuntimeCodec::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Decoding FLAC audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to decode audio data using the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

	// Initializing FLAC codec
	drflac* FLAC_Decoder = drflac_open_memory(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr);

	if (!FLAC_Decoder)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize FLAC Decoder"));
		return false;
	}

	// Allocating memory for PCM data
	float* TempPCMData = static_cast<float*>(FMemory::Malloc(FLAC_Decoder->totalPCMFrameCount * FLAC_Decoder->channels * sizeof(float)));

	if (!TempPCMData)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for FLAC Decoder"));
		drflac_close(FLAC_Decoder);
		return false;
	}

	// Filling in PCM data and getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = drflac_read_pcm_frames_f32(FLAC_Decoder, FLAC_Decoder->totalPCMFrameCount, TempPCMData);

	// Getting PCM data size
	const int64 TempPCMDataSize = static_cast<int64>(DecodedData.PCMInfo.PCMNumOfFrames * FLAC_Decoder->channels);

	DecodedData.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(TempPCMData, TempPCMDataSize);

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(FLAC_Decoder->totalPCMFrameCount) / FLAC_Decoder->sampleRate;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = FLAC_Decoder->channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = FLAC_Decoder->sampleRate;
		DecodedData.SoundWaveBasicInfo.AudioFormat = GetAudioFormat();
	}

	drflac_close(FLAC_Decoder);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully decoded FLAC audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString());
	return true;
}
