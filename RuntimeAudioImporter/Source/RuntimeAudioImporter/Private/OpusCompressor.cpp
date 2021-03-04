// Georgy Treshchev 2021.

#include "OpusCompressor.h"
#include "Interfaces/IAudioFormat.h"
#include "Serialization/MemoryWriter.h"
#include "OpusAudioInfo.h"

#define OUTSIDE_SPEEX

THIRD_PARTY_INCLUDES_START
#include "opus_multistream.h"
#include "speex_resampler.h"
THIRD_PARTY_INCLUDES_END


#define SAMPLE_SIZE			( ( uint32 )sizeof( short ) )

bool FOpusCompressor::GenerateCompressedData(const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo,
                                             TArray<uint8>& CompressedDataStore)
{
	const uint16 OpusSampleRate = GetBestOutputSampleRate(QualityInfo.SampleRate);
	const int32 OpusFrameSizeMs = 60;
	const int32 OpusFrameSizeSamples = (OpusSampleRate * OpusFrameSizeMs) / 1000;
	const uint32 SampleStride = SAMPLE_SIZE * QualityInfo.NumChannels;
	const int32 BytesPerFrame = OpusFrameSizeSamples * SampleStride;

	TArray<uint8> SrcBufferCopy;
	if (QualityInfo.SampleRate != OpusSampleRate)
	{
		if (!ResamplePCM(QualityInfo.NumChannels, SrcBuffer, QualityInfo.SampleRate, SrcBufferCopy, OpusSampleRate))
		{
			return false;
		}
	}
	else
	{
		SrcBufferCopy = SrcBuffer;
	}

	const int32 EncSize = opus_encoder_get_size(QualityInfo.NumChannels);
	OpusEncoder* Encoder = static_cast<OpusEncoder*>(FMemory::Malloc(EncSize));
	const int32 EncError = opus_encoder_init(Encoder, OpusSampleRate, QualityInfo.NumChannels, OPUS_APPLICATION_AUDIO);
	if (EncError != OPUS_OK)
	{
		FMemory::Free(Encoder);
		return false;
	}

	const int32 BitRate = GetBitRateFromQuality(QualityInfo);
	opus_encoder_ctl(Encoder, OPUS_SET_BITRATE(BitRate));

	CompressedDataStore.Empty();
	FMemoryWriter CompressedData(CompressedDataStore);
	int32 SrcBufferOffset = 0;

	int32 FramesToEncode = SrcBufferCopy.Num() / BytesPerFrame;
	const uint32 AccurateSampleCount = SrcBufferCopy.Num() / SampleStride;

	if (SrcBufferCopy.Num() % BytesPerFrame != 0)
	{
		int32 FrameDiff = BytesPerFrame - (SrcBufferCopy.Num() % BytesPerFrame);
		SrcBufferCopy.AddZeroed(FrameDiff);
		FramesToEncode++;
	}

	check(QualityInfo.NumChannels <= MAX_uint8);
	check(FramesToEncode <= MAX_uint16);
	SerializeHeaderData(CompressedData, OpusSampleRate, AccurateSampleCount, QualityInfo.NumChannels, FramesToEncode);

	TArray<uint8> TempCompressedData;
	TempCompressedData.AddUninitialized(BytesPerFrame);

	while (SrcBufferOffset < SrcBufferCopy.Num())
	{
		const int32 CompressedDataLength = opus_encode(
			Encoder, reinterpret_cast<const opus_int16*>(SrcBufferCopy.GetData() + SrcBufferOffset),
			OpusFrameSizeSamples, TempCompressedData.GetData(), TempCompressedData.Num());

		if (CompressedDataLength < 0)
		{
			const char* ErrorStr = opus_strerror(CompressedDataLength);
			UE_LOG(LogAudio, Warning, TEXT("Failed to encode: [%d] %s"), CompressedDataLength, ANSI_TO_TCHAR(ErrorStr));

			FMemory::Free(Encoder);

			CompressedDataStore.Empty();
			return false;
		}
		else
		{
			check(CompressedDataLength < MAX_uint16);
			SerialiseFrameData(CompressedData, TempCompressedData.GetData(), CompressedDataLength);
			SrcBufferOffset += BytesPerFrame;
		}
	}

	FMemory::Free(Encoder);

	return CompressedDataStore.Num() > 0;
}

uint16 FOpusCompressor::GetBestOutputSampleRate(int32 SampleRate)
{
	static const uint16 ValidSampleRates[] =
	{
		0,
		8000,
		12000,
		16000,
		24000,
		48000,
	};

	for (int32 Index = UE_ARRAY_COUNT(ValidSampleRates) - 2; Index >= 0; Index--)
	{
		if (SampleRate > ValidSampleRates[Index])
		{
			return ValidSampleRates[Index + 1];
		}
	}
	check(0);
	return 0;
}

bool FOpusCompressor::ResamplePCM(uint32 NumChannels, const TArray<uint8>& InBuffer, uint32 InSampleRate,
                                  TArray<uint8>& OutBuffer, uint32 OutSampleRate)
{
	int32 err = 0;
	SpeexResamplerState* resampler = speex_resampler_init(NumChannels, InSampleRate, OutSampleRate,
	                                                      SPEEX_RESAMPLER_QUALITY_DESKTOP, &err);
	if (err != RESAMPLER_ERR_SUCCESS)
	{
		speex_resampler_destroy(resampler);
		return false;
	}

	const uint32 SampleStride = SAMPLE_SIZE * NumChannels;
	const float Duration = (float)InBuffer.Num() / (InSampleRate * SampleStride);
	const int32 SafeCopySize = (Duration + 1) * OutSampleRate * SampleStride;
	OutBuffer.Empty(SafeCopySize);
	OutBuffer.AddUninitialized(SafeCopySize);
	uint32 InSamples = InBuffer.Num() / SampleStride;
	uint32 OutSamples = OutBuffer.Num() / SampleStride;

	if (NumChannels == 1)
	{
		err = speex_resampler_process_int(resampler, 0, (const short*)(InBuffer.GetData()), &InSamples,
		                                  (short*)(OutBuffer.GetData()), &OutSamples);
	}
	else
	{
		err = speex_resampler_process_interleaved_int(resampler, (const short*)(InBuffer.GetData()), &InSamples,
		                                              (short*)(OutBuffer.GetData()), &OutSamples);
	}

	speex_resampler_destroy(resampler);
	if (err != RESAMPLER_ERR_SUCCESS)
	{
		return false;
	}

	const int32 WrittenBytes = (int32)(OutSamples * SampleStride);
	if (WrittenBytes < OutBuffer.Num())
	{
		OutBuffer.SetNum(WrittenBytes, true);
	}

	return true;
}

int32 FOpusCompressor::GetBitRateFromQuality(FSoundQualityInfo& QualityInfo)
{
	const int32 OriginalBitRate = QualityInfo.SampleRate * QualityInfo.NumChannels * SAMPLE_SIZE * 8;
	return static_cast<float>(OriginalBitRate) * FMath::GetMappedRangeValueClamped(
		FVector2D(1, 100), FVector2D(0.04, 0.25), QualityInfo.Quality);
}

void FOpusCompressor::SerializeHeaderData(FMemoryWriter& CompressedData, uint16 SampleRate, uint32 TrueSampleCount,
                                          uint8 NumChannels, uint16 NumFrames)
{
	const char* OpusIdentifier = OPUS_ID_STRING;
	CompressedData.Serialize((void*)OpusIdentifier, FCStringAnsi::Strlen(OpusIdentifier) + 1);
	CompressedData.Serialize(&SampleRate, sizeof(uint16));
	CompressedData.Serialize(&TrueSampleCount, sizeof(uint32));
	CompressedData.Serialize(&NumChannels, sizeof(uint8));
	CompressedData.Serialize(&NumFrames, sizeof(uint16));
}

void FOpusCompressor::SerialiseFrameData(FMemoryWriter& CompressedData, uint8* FrameData, uint16 FrameSize)
{
	CompressedData.Serialize(&FrameSize, sizeof(uint16));
	CompressedData.Serialize(FrameData, FrameSize);
}

int32 FOpusCompressor::AddDataChunk(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, int32 ChunkSize)
{
	TArray<uint8>& NewBuffer = *new(OutBuffers) TArray<uint8>;
	NewBuffer.Empty(ChunkSize);
	NewBuffer.AddUninitialized(ChunkSize);
	FMemory::Memcpy(NewBuffer.GetData(), ChunkData, ChunkSize);
	return ChunkSize;
}
