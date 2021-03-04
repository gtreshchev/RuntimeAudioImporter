// Georgy Treshchev 2021.

#pragma once

#include "CoreMinimal.h"

struct FSoundQualityInfo;

class RUNTIMEAUDIOIMPORTER_API FOpusCompressor
{
public:
	static bool GenerateCompressedData(const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo,
	                                   TArray<uint8>& OutBuffer);

	static uint16 GetBestOutputSampleRate(int32 SampleRate);

	static bool ResamplePCM(uint32 NumChannels, const TArray<uint8>& InBuffer, uint32 InSampleRate,
	                        TArray<uint8>& OutBuffer, uint32 OutSampleRate);

	static int32 GetBitRateFromQuality(FSoundQualityInfo& QualityInfo);

	static void SerializeHeaderData(FMemoryWriter& CompressedData, uint16 SampleRate, uint32 TrueSampleCount,
	                                uint8 NumChannels, uint16 NumFrames);

	static void SerialiseFrameData(FMemoryWriter& CompressedData, uint8* FrameData, uint16 FrameSize);

	static int32 AddDataChunk(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, int32 ChunkSize);
};
