// Georgy Treshchev 2023.

#include "Codecs/BINK_RuntimeCodec.h"
#include "RuntimeAudioImporterTypes.h"
#include "Codecs/RAW_RuntimeCodec.h"
#include "HAL/PlatformProperties.h"
#include "HAL/UnrealMemory.h"

#if WITH_RUNTIMEAUDIOIMPORTER_BINK_DECODE_SUPPORT
#include "BinkAudioInfo.h"
#include "Interfaces/IAudioFormat.h"
#endif

#define INCLUDE_BINK
#include "CodecIncludes.h"
#undef INCLUDE_BINK


/**
 * Taken from AudioFormatBink.cpp
 */
namespace
{
	uint8 GetCompressionLevelFromQualityIndex(int32 InQualityIndex)
	{
		// Bink goes from 0 (best) to 9 (worst), but is basically unusable below 4 
		static constexpr float BinkLowest = 4;
		static constexpr float BinkHighest = 0;

		// Map Quality 1 (lowest) to 40 (highest)
		static constexpr float QualityLowest = 1;
		static constexpr float QualityHighest = 40;

		return FMath::GetMappedRangeValueClamped(FVector2D(QualityLowest, QualityHighest), FVector2D(BinkLowest, BinkHighest), InQualityIndex);
	}

	void* BinkAlloc(size_t Bytes)
	{
		return FMemory::Malloc(Bytes, 16);
	}

	void BinkFree(void* Ptr)
	{
		FMemory::Free(Ptr);
	}
}

bool FBINK_RuntimeCodec::CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
#if WITH_RUNTIMEAUDIOIMPORTER_BINK_DECODE_SUPPORT
	FBinkAudioInfo AudioInfo;
	FSoundQualityInfo SoundQualityInfo;

	if (!AudioInfo.ReadCompressedInfo(AudioData.GetView().GetData(), AudioData.GetView().Num(), &SoundQualityInfo) || SoundQualityInfo.SampleDataSize == 0)
	{
		return false;
	}

	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support BINK decoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}

bool FBINK_RuntimeCodec::GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Retrieving header information for the BINK audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to retrieve audio header information in the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

#if WITH_RUNTIMEAUDIOIMPORTER_BINK_DECODE_SUPPORT
	FBinkAudioInfo AudioInfo;
	FSoundQualityInfo SoundQualityInfo;

	if (!AudioInfo.ReadCompressedInfo(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &SoundQualityInfo) || SoundQualityInfo.SampleDataSize == 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to read BINK compressed info"));
		return false;
	}

	{
		HeaderInfo.Duration = SoundQualityInfo.Duration;
		HeaderInfo.SampleRate = SoundQualityInfo.SampleRate;
		HeaderInfo.NumOfChannels = SoundQualityInfo.NumChannels;
		HeaderInfo.PCMDataSize = SoundQualityInfo.SampleDataSize / sizeof(int16);
		HeaderInfo.AudioFormat = GetAudioFormat();
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved header information for FLAC audio format.\nHeader info: %s"), *HeaderInfo.ToString());
	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support BINK decoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}

bool FBINK_RuntimeCodec::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Encoding uncompressed audio data to BINK audio format.\nDecoded audio info: %s.\nQuality: %d"), *DecodedData.ToString(), Quality);

#if WITH_RUNTIMEAUDIOIMPORTER_BINK_ENCODE_SUPPORT
	const uint8 CompressionLevel = GetCompressionLevelFromQualityIndex(Quality);

	int16* TempInt16Buffer;
	FRAW_RuntimeCodec::TranscodeRAWData<float, int16>(DecodedData.PCMInfo.PCMData.GetView().GetData(), DecodedData.PCMInfo.PCMData.GetView().Num(), TempInt16Buffer);

	void* CompressedData = nullptr;
	uint32_t CompressedDataLen = 0;
	UECompressBinkAudio(static_cast<void*>(TempInt16Buffer), DecodedData.PCMInfo.PCMData.GetView().Num() * sizeof(int16), DecodedData.SoundWaveBasicInfo.SampleRate, DecodedData.SoundWaveBasicInfo.NumOfChannels, CompressionLevel, 1, BinkAlloc, BinkFree, &CompressedData, &CompressedDataLen);

	if (CompressedDataLen <= 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to decode BINK audio data: the uncompressed data is empty"));
		return false;
	}

	// Populating the encoded audio data
	{
		EncodedData.AudioData = FRuntimeBulkDataBuffer<uint8>(static_cast<uint8*>(CompressedData), CompressedDataLen);
		EncodedData.AudioFormat = ERuntimeAudioFormat::Bink;
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully encoded uncompressed audio data to BINK audio format.\nEncoded audio info: %s"), *EncodedData.ToString());
	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support BINK encoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}

bool FBINK_RuntimeCodec::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Decoding BINK audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to decode audio data using the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

#if WITH_RUNTIMEAUDIOIMPORTER_BINK_DECODE_SUPPORT
	FBinkAudioInfo AudioInfo;
	FSoundQualityInfo SoundQualityInfo;

	// Parse the audio header for the relevant information
	if (!AudioInfo.ReadCompressedInfo(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &SoundQualityInfo))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to read BINK compressed info"));
		return false;
	}

	TArray64<uint8> PCMData;

	// Decompress all the sample data
	PCMData.Empty(SoundQualityInfo.SampleDataSize);
	PCMData.AddZeroed(SoundQualityInfo.SampleDataSize);
	AudioInfo.ExpandFile(PCMData.GetData(), &SoundQualityInfo);

	// Getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = PCMData.Num() / SoundQualityInfo.NumChannels / sizeof(int16);

	const int64 NumOfSamples = PCMData.Num() / sizeof(int16);

	// Transcoding int16 to float format
	{
		float* TempFloatBuffer;
		FRAW_RuntimeCodec::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(PCMData.GetData()), NumOfSamples, TempFloatBuffer);
		DecodedData.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(TempFloatBuffer, NumOfSamples);
	}

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = SoundQualityInfo.Duration;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = SoundQualityInfo.NumChannels;
		DecodedData.SoundWaveBasicInfo.SampleRate = SoundQualityInfo.SampleRate;
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully decoded BINK audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString());
	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support BINK decoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}
