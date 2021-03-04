// Georgy Treshchev 2021.

#pragma once

#include "CoreMinimal.h"

struct FSoundQualityInfo;

class RUNTIMEAUDIOIMPORTER_API FVorbisCompressor
{
public:
	static bool GenerateCompressedData(const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo,
	                                   TArray<uint8>& OutBuffer);

	static int32 GetMinimumSizeForInitialChunk(FName Format, const TArray<uint8>& SrcBuffer);

	static TArray<int32> GetChannelOrder(int32 NumChannels);

	static void AddNewChunk(TArray<TArray<uint8>>& OutBuffers, int32 ChunkReserveSize);

	static void AddChunkData(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, int32 ChunkDataSize);
};
