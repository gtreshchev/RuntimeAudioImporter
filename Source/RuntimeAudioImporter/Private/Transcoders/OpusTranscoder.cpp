// Georgy Treshchev 2022.

#include "Transcoders/OpusTranscoder.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "Serialization/MemoryWriter.h"
#include "GenericPlatform/GenericPlatformProperties.h"

#define INCLUDE_OPUS
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_OPUS

#define SAMPLE_SIZE	static_cast<uint32>(sizeof(short))

uint16 GetBestOutputSampleRate(uint8 Quality)
{
	return FMath::GetMappedRangeValueClamped(FVector2D(1, 100), FVector2D(8000, 48000), Quality);
}

uint32 GetBitRateFromQuality(uint8 Quality, uint32 NumOfChannels)
{
	const uint32 OriginalBitRate = GetBestOutputSampleRate(Quality) * NumOfChannels * SAMPLE_SIZE * 8;
	return static_cast<float>(OriginalBitRate) * FMath::GetMappedRangeValueClamped(FVector2D(1, 100), FVector2D(0.04, 0.25), Quality);
}

void SerializeHeaderData(FMemoryWriter& CompressedData, uint16 SampleRate, uint32 TrueSampleCount, uint8 NumChannels, uint16 NumFrames)
{
	const char* OpusIdentifier = "RuntimeAudioImporterOpus";

	CompressedData.Serialize(const_cast<char*>(OpusIdentifier), FCStringAnsi::Strlen(OpusIdentifier) + 1);
	CompressedData.Serialize(&SampleRate, sizeof(uint16));
	CompressedData.Serialize(&TrueSampleCount, sizeof(uint32));
	CompressedData.Serialize(&NumChannels, sizeof(uint8));
	CompressedData.Serialize(&NumFrames, sizeof(uint16));
}

void SerialiseFrameData(FMemoryWriter& CompressedData, uint8* FrameData, uint16 FrameSize)
{
	CompressedData.Serialize(&FrameSize, sizeof(uint16));
	CompressedData.Serialize(FrameData, FrameSize);
}

bool OpusTranscoder::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Encoding uncompressed audio data to Opus audio format.\nDecoded audio info: %s.\nQuality: %d"),
		   *DecodedData.ToString(), Quality);
	
#if WITH_OPUS
	const uint16 OpusSampleRate{GetBestOutputSampleRate(Quality)};
	constexpr uint32 OpusFrameSizeMs{60};
	const uint32 OpusFrameSizeSamples = (OpusSampleRate * OpusFrameSizeMs) / 1000;
	const uint32 SampleStride = SAMPLE_SIZE * DecodedData.SoundWaveBasicInfo.NumOfChannels;
	const uint32 BytesPerFrame = OpusFrameSizeSamples * SampleStride;

	// Copying decoded data to prevent crash if the task is interrupted
	TArray<uint8> CopiedDecodedData{TArray<uint8>(DecodedData.PCMInfo.PCMData, DecodedData.PCMInfo.PCMDataSize)};

	const uint32 EncoderSize = opus_encoder_get_size(DecodedData.SoundWaveBasicInfo.NumOfChannels);
	OpusEncoder* Encoder{static_cast<OpusEncoder*>(FMemory::Malloc(EncoderSize))};
	const int32 EncoderError = opus_encoder_init(Encoder, OpusSampleRate, DecodedData.SoundWaveBasicInfo.NumOfChannels, OPUS_APPLICATION_AUDIO);

	if (EncoderError != OPUS_OK)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize Opus Encoder: error code %d"), EncoderError);
		FMemory::Free(Encoder);
		return false;
	}

	const uint32 BitRate{GetBitRateFromQuality(Quality, DecodedData.SoundWaveBasicInfo.NumOfChannels)};
	opus_encoder_ctl(Encoder, OPUS_SET_BITRATE(BitRate));

	TArray<uint8> EncodedAudioData;
	EncodedAudioData.Empty();
	FMemoryWriter CompressedData(EncodedAudioData);
	
	uint32 SrcBufferOffset = 0;

	uint32 FramesToEncode = CopiedDecodedData.Num() / BytesPerFrame;
	const uint32 AccurateSampleCount = CopiedDecodedData.Num() / SampleStride;

	if (CopiedDecodedData.Num() % BytesPerFrame != 0)
	{
		uint32 FrameDifference = BytesPerFrame - (CopiedDecodedData.Num() % BytesPerFrame);
		CopiedDecodedData.AddZeroed(FrameDifference);
		FramesToEncode++;
	}

	if (DecodedData.SoundWaveBasicInfo.NumOfChannels > MAX_uint8)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Failed to encode Opus data: the number of channels (%d) is more than the max supported value (%d)"), DecodedData.SoundWaveBasicInfo.NumOfChannels, MAX_uint8);
		FMemory::Free(Encoder);
		return false;
	}

	if (FramesToEncode > MAX_uint16)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Failed to encode Opus data: the number of frames to encode (%d) is more than the max supported value (%d)"), FramesToEncode, MAX_uint16);
		FMemory::Free(Encoder);
		return false;
	}

	SerializeHeaderData(CompressedData, OpusSampleRate, AccurateSampleCount, DecodedData.SoundWaveBasicInfo.NumOfChannels, FramesToEncode);

	TArray<uint8> Uint8RAWBufferFrame;
	Uint8RAWBufferFrame.AddUninitialized(BytesPerFrame);

	while (SrcBufferOffset < static_cast<uint32>(CopiedDecodedData.Num()))
	{
		const int32 CompressedDataLength = opus_encode(Encoder,
		                                               reinterpret_cast<const opus_int16*>(CopiedDecodedData.GetData() + SrcBufferOffset),
		                                               OpusFrameSizeSamples, Uint8RAWBufferFrame.GetData(), Uint8RAWBufferFrame.Num());

		if (CompressedDataLength < 0)
		{
			const char* ErrorStr = opus_strerror(CompressedDataLength);
			UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Failed to encode Opus data: [%d] %s"), CompressedDataLength, ANSI_TO_TCHAR(ErrorStr));

			FMemory::Free(Encoder);
			return false;
		}
		else
		{
			if (CompressedDataLength > MAX_uint16)
			{
				UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Failed to encode Opus data: Compressed data length (%d) is more than max uint16 value (%d)"), CompressedDataLength, MAX_uint16);
				FMemory::Free(Encoder);
				return false;
			}

			SerialiseFrameData(CompressedData, Uint8RAWBufferFrame.GetData(), CompressedDataLength);
			SrcBufferOffset += BytesPerFrame;
		}
	}

	FMemory::Free(Encoder);

	// Filling the encoded audio data
	{
		EncodedData.AudioData = static_cast<uint8*>(FMemory::Malloc(EncodedAudioData.Num()));
		EncodedData.AudioDataSize = EncodedAudioData.Num();
		EncodedData.AudioFormat = EAudioFormat::OggOpus;
		FMemory::Memmove(EncodedData.AudioData, EncodedAudioData.GetData(), EncodedAudioData.Num());
	}
	
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully encoded uncompressed audio data to Opus audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	return true;

#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%s) does not support Opus encoding"), FGenericPlatformProperties::IniPlatformName());
	return false;
#endif
}
