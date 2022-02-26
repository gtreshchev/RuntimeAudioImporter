// Georgy Treshchev 2022.

#include "Transcoders/OpusTranscoder.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "Serialization/MemoryWriter.h"

#define INCLUDE_OPUS
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_OPUS

#define SAMPLE_SIZE	static_cast<uint32>(sizeof(short))

uint16 GetBestOutputSampleRate(uint8 Quality)
{
	return FMath::GetMappedRangeValueClamped(FVector2D(1, 100), FVector2D(8000, 48000), Quality);
}

int32 GetBitRateFromQuality(uint8 Quality, uint32 NumOfChannels)
{
	const int32 OriginalBitRate = GetBestOutputSampleRate(Quality) * NumOfChannels * SAMPLE_SIZE * 8;
	return static_cast<float>(OriginalBitRate) * FMath::GetMappedRangeValueClamped(FVector2D(1, 100), FVector2D(0.04, 0.25), Quality);
}

void SerializeHeaderData(FMemoryWriter& CompressedData, uint16 SampleRate, uint32 TrueSampleCount, uint8 NumChannels, uint16 NumFrames)
{
	constexpr char* OpusIdentifier = "RuntimeAudioImporterOpus";

	CompressedData.Serialize(OpusIdentifier, FCStringAnsi::Strlen(OpusIdentifier) + 1);
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
	const uint16 OpusSampleRate = GetBestOutputSampleRate(Quality);
	constexpr int32 OpusFrameSizeMs = 60;
	const int32 OpusFrameSizeSamples = (OpusSampleRate * OpusFrameSizeMs) / 1000;
	const uint32 SampleStride = SAMPLE_SIZE * DecodedData.SoundWaveBasicInfo.NumOfChannels;
	const int32 BytesPerFrame = OpusFrameSizeSamples * SampleStride;

	TArray<uint8> DecodedDataCopy = TArray<uint8>(DecodedData.PCMInfo.PCMData, DecodedData.PCMInfo.PCMDataSize);

	const int32 EncSize = opus_encoder_get_size(DecodedData.SoundWaveBasicInfo.NumOfChannels);
	OpusEncoder* Encoder = static_cast<OpusEncoder*>(FMemory::Malloc(EncSize));
	const int32 EncError = opus_encoder_init(Encoder, OpusSampleRate, DecodedData.SoundWaveBasicInfo.NumOfChannels, OPUS_APPLICATION_AUDIO);

	if (EncError != OPUS_OK)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to initialize Opus Encoder"));
		FMemory::Free(Encoder);
		return false;
	}

	const int32 BitRate = GetBitRateFromQuality(Quality, DecodedData.SoundWaveBasicInfo.NumOfChannels);
	opus_encoder_ctl(Encoder, OPUS_SET_BITRATE(BitRate));

	TArray<uint8> EncodedAudioData;

	EncodedAudioData.Empty();
	FMemoryWriter CompressedData(EncodedAudioData);
	int32 SrcBufferOffset = 0;

	int32 FramesToEncode = DecodedDataCopy.Num() / BytesPerFrame;
	const uint32 AccurateSampleCount = DecodedDataCopy.Num() / SampleStride;

	if (DecodedDataCopy.Num() % BytesPerFrame != 0)
	{
		int32 FrameDiff = BytesPerFrame - (DecodedDataCopy.Num() % BytesPerFrame);
		DecodedDataCopy.AddZeroed(FrameDiff);
		FramesToEncode++;
	}

	check(DecodedData.SoundWaveBasicInfo.NumOfChannels <= MAX_uint8);
	check(FramesToEncode <= MAX_uint16);

	SerializeHeaderData(CompressedData, OpusSampleRate, AccurateSampleCount, DecodedData.SoundWaveBasicInfo.NumOfChannels, FramesToEncode);

	TArray<uint8> Uint8RAWBufferFrame;
	Uint8RAWBufferFrame.AddUninitialized(BytesPerFrame);

	while (SrcBufferOffset < DecodedDataCopy.Num())
	{
		const int32 CompressedDataLength = opus_encode(Encoder,
		                                               reinterpret_cast<const opus_int16*>(DecodedDataCopy.GetData() + SrcBufferOffset),
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
			check(CompressedDataLength < MAX_uint16);
			SerialiseFrameData(CompressedData, Uint8RAWBufferFrame.GetData(), CompressedDataLength);
			SrcBufferOffset += BytesPerFrame;
		}
	}

	FMemory::Free(Encoder);

	/** Filling the encoded audio data */
	{
		EncodedData.AudioData = static_cast<uint8*>(FMemory::Malloc(EncodedAudioData.Num()));
		EncodedData.AudioDataSize = EncodedAudioData.Num();
		FMemory::Memmove(EncodedData.AudioData, EncodedAudioData.GetData(), EncodedAudioData.Num());
	}

	return EncodedAudioData.Num() > 0;
}