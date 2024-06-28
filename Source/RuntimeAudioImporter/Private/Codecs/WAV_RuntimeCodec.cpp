// Georgy Treshchev 2024.

#include "Codecs/WAV_RuntimeCodec.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "HAL/UnrealMemory.h"

#define INCLUDE_WAV
#include "CodecIncludes.h"
#include "Codecs/RAW_RuntimeCodec.h"
#undef INCLUDE_WAV

namespace
{
	/**
	 * Check and fix the WAV audio data with the correct byte size in the RIFF container
	 * Made by https://github.com/kass-kass
	 */
	bool CheckAndFixWavDurationErrors(const FRuntimeBulkDataBuffer<uint8>& WavData)
	{
		drwav WAV;

		// Initializing WAV codec
		if (!drwav_init_memory(&WAV, WavData.GetView().GetData(), WavData.GetView().Num(), nullptr))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize WAV Decoder"));
			return false;
		}

		// Check if the container is RIFF (not Wave64 or any other containers)
		if (WAV.container != drwav_container_riff)
		{
			drwav_uninit(&WAV);
			return true;
		}

		// Get 4-byte field at byte 4, which is the overall file size as uint32, according to RIFF specification.
		// If the field is set to nothing (hex FFFFFFFF), replace the incorrectly set field with the actual size.
		// The field should be (size of file - 8 bytes), as the chunk identifier for the whole file (4 bytes spelling out RIFF at the start of the file), and the chunk length (4 bytes that we're replacing) are excluded.
		if (BytesToHex(WavData.GetView().GetData() + 4, 4) == "FFFFFFFF")
		{
			const int32 ActualFileSize = WavData.GetView().Num() - 8;
			FMemory::Memcpy(WavData.GetView().GetData() + 4, &ActualFileSize, 4);
		}

		// Search for the place in the file after the chunk id "data", which is where the data length is stored.
		// First 36 bytes are skipped, as they're always "RIFF", 4 bytes filesize, "WAVE", "fmt ", and 20 bytes of format data.
		uint32 DataSizeLocation = INDEX_NONE;
		for (uint32 Index = 36; Index < static_cast<uint32>(WavData.GetView().Num()) - 4; ++Index)
		{
			// "64617461" - hex for string "data"
			if (BytesToHex(WavData.GetView().GetData() + Index, 4) == "64617461")
			{
				DataSizeLocation = Index + 4;
				break;
			}
		}

		// Should never happen, but just in case
		if (DataSizeLocation == INDEX_NONE)
		{
			drwav_uninit(&WAV);
			return false;
		}

		// Same process as replacing full file size, except DataSize counts bytes from end of DataSize int to end of file.
		if (BytesToHex(WavData.GetView().GetData() + DataSizeLocation, 4) == "FFFFFFFF")
		{
			// -4 to not include the DataSize int itself
			const uint32 ActualDataSize = WavData.GetView().Num() - DataSizeLocation - 4;

			FMemory::Memcpy(WavData.GetView().GetData() + DataSizeLocation, &ActualDataSize, 4);
		}

		drwav_uninit(&WAV);
		return true;
	}
}

bool FWAV_RuntimeCodec::CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	drwav WAV;

	CheckAndFixWavDurationErrors(AudioData);

	if (!drwav_init_memory(&WAV, AudioData.GetView().GetData(), AudioData.GetView().Num(), nullptr))
	{
		return false;
	}

	drwav_uninit(&WAV);
	return true;
}

bool FWAV_RuntimeCodec::GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Retrieving header information for WAV audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to retrieve audio header information in the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

	drwav WAV;
	if (!drwav_init_memory(&WAV, EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to initialize WAV Decoder"));
		return false;
	}

	{
		HeaderInfo.Duration = static_cast<float>(WAV.totalPCMFrameCount) / WAV.sampleRate;
		HeaderInfo.NumOfChannels = WAV.channels;
		HeaderInfo.SampleRate = WAV.sampleRate;
		HeaderInfo.PCMDataSize = WAV.totalPCMFrameCount * WAV.channels;
		HeaderInfo.AudioFormat = GetAudioFormat();
	}

	drwav_uninit(&WAV);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved header information for WAV audio format.\nHeader info: %s"), *HeaderInfo.ToString());
	return true;
}

bool FWAV_RuntimeCodec::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Encoding uncompressed audio data to WAV audio format.\nDecoded audio info: %s."), *DecodedData.ToString());

	drwav WAV_Encoder;

	drwav_data_format WAV_Format;
	{
		WAV_Format.container = drwav_container_riff;
		WAV_Format.format = DR_WAVE_FORMAT_PCM;
		WAV_Format.channels = DecodedData.SoundWaveBasicInfo.NumOfChannels;
		WAV_Format.sampleRate = DecodedData.SoundWaveBasicInfo.SampleRate;
		WAV_Format.bitsPerSample = 16;
	}

	void* CompressedData = nullptr;
	size_t CompressedDataLen;

	if (!drwav_init_memory_write(&WAV_Encoder, &CompressedData, &CompressedDataLen, &WAV_Format, nullptr))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize WAV Encoder"));
		return false;
	}

	int16* TempInt16BBuffer;
	FRAW_RuntimeCodec::TranscodeRAWData<float, int16>(DecodedData.PCMInfo.PCMData.GetView().GetData(), DecodedData.PCMInfo.PCMData.GetView().Num(), TempInt16BBuffer);

	drwav_write_pcm_frames(&WAV_Encoder, DecodedData.PCMInfo.PCMNumOfFrames, TempInt16BBuffer);
	drwav_uninit(&WAV_Encoder);
	FMemory::Free(TempInt16BBuffer);

	// Populating the encoded audio data
	{
		EncodedData.AudioData = FRuntimeBulkDataBuffer<uint8>(static_cast<uint8*>(CompressedData), static_cast<int64>(CompressedDataLen));
		EncodedData.AudioFormat = ERuntimeAudioFormat::Wav;
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully encoded uncompressed audio data to WAV audio format.\nEncoded audio info: %s"), *EncodedData.ToString());
	return true;
}

bool FWAV_RuntimeCodec::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Decoding WAV audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to decode audio data using the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

	if (!CheckAndFixWavDurationErrors(EncodedData.AudioData))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while fixing WAV audio data duration error"));
		return false;
	}

	drwav WAV_Decoder;

	// Initializing WAV codec
	if (!drwav_init_memory(&WAV_Decoder, EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), nullptr))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize WAV Decoder"));
		return false;
	}

	// Allocating memory for PCM data
	float* TempPCMData = static_cast<float*>(FMemory::Malloc(WAV_Decoder.totalPCMFrameCount * WAV_Decoder.channels * sizeof(float)));
	if (!TempPCMData)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for WAV Decoder"));
		drwav_uninit(&WAV_Decoder);
		return false;
	}

	// Filling PCM data and getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = drwav_read_pcm_frames_f32(&WAV_Decoder, WAV_Decoder.totalPCMFrameCount, TempPCMData);

	// Getting PCM data size
	const int64 TempPCMDataSize = static_cast<int64>(DecodedData.PCMInfo.PCMNumOfFrames * WAV_Decoder.channels);

	DecodedData.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(TempPCMData, TempPCMDataSize);

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(WAV_Decoder.totalPCMFrameCount) / WAV_Decoder.sampleRate;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = WAV_Decoder.channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = WAV_Decoder.sampleRate;
		DecodedData.SoundWaveBasicInfo.AudioFormat = GetAudioFormat();
	}

	drwav_uninit(&WAV_Decoder);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully decoded WAV audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString());
	return true;
}
