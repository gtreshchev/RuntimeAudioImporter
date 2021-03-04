// Georgy Treshchev 2021.

#pragma once

#include "CoreMinimal.h"

struct FSoundQualityInfo;
struct RiffDataChunk
	{
		uint32 ID;
		uint32 DataSize;
		uint8* Data;
	};


class RUNTIMEAUDIOIMPORTER_API FAdvancedPCMCompressor
{
public:
	static bool GenerateCompressedData(const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo,
	                                   TArray<uint8>& OutBuffer);

	static void AddNewChunk(TArray<TArray<uint8>>& OutBuffers, int32 ChunkReserveSize);

	static void AddChunkData(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, int32 ChunkDataSize);
};
