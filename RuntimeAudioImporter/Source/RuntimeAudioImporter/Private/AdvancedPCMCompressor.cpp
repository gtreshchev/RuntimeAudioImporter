// Georgy Treshchev 2021.

#include "AdvancedPCMCompressor.h"
#include "Interfaces/IAudioFormat.h"
#include "Serialization/MemoryWriter.h"
#include "ADPCMAudioInfo.h"


#define FORMAT_FOURCC(ch0, ch1, ch2, ch3)\
	((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |\
	((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))



namespace BaseDataOperations
{
	template <uint32 N>
	void GenerateWaveFile(RiffDataChunk (& RiffDataChunks)[N], TArray<uint8>& CompressedDataStore)
	{
		uint32 RiffDataSize = sizeof(uint32);

		for (uint32 Scan = 0; Scan < N; ++Scan)
		{
			const RiffDataChunk& Chunk = RiffDataChunks[Scan];
			RiffDataSize += (sizeof(Chunk.ID) + sizeof(Chunk.DataSize) + Chunk.DataSize);
		}

		const uint32 OutputDataSize = RiffDataSize + sizeof(uint32) + sizeof(uint32);

		CompressedDataStore.Empty(OutputDataSize);
		FMemoryWriter CompressedData(CompressedDataStore);

		uint32 rID = FORMAT_FOURCC('R', 'I', 'F', 'F');
		CompressedData.Serialize(&rID, sizeof(rID));
		CompressedData.Serialize(&RiffDataSize, sizeof(RiffDataSize));

		rID = FORMAT_FOURCC('W', 'A', 'V', 'E');
		CompressedData.Serialize(&rID, sizeof(rID));

		for (uint32 Scan = 0; Scan < N; ++Scan)
		{
			RiffDataChunk& Chunk = RiffDataChunks[Scan];

			CompressedData.Serialize(&Chunk.ID, sizeof(Chunk.ID));
			CompressedData.Serialize(&Chunk.DataSize, sizeof(Chunk.DataSize));
			CompressedData.Serialize(Chunk.Data, Chunk.DataSize);
		}
	}

	template <typename T, uint32 B>
	T SignExtend(const T ValueToExtend)
	{
		struct
		{
			T ExtendedValue:B;
		} SignExtender;
		return SignExtender.ExtendedValue = ValueToExtend;
	}

	template <typename T>
	T ReadFromByteStream(const uint8* ByteStream, int32& ReadIndex, bool bLittleEndian = true)
	{
		T ValueRaw = 0;

		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}

		return ValueRaw;
	}

	template <typename T>
	void WriteToByteStream(T Value, uint8* ByteStream, int32& WriteIndex, bool bLittleEndian = true)
	{
		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
	}

	template <typename T>
	T ReadFromArray(const T* ElementArray, int32& ReadIndex, int32 NumElements, int32 IndexStride = 1)
	{
		T OutputValue = 0;

		if (ReadIndex >= 0 && ReadIndex < NumElements)
		{
			OutputValue = ElementArray[ReadIndex];
			ReadIndex += IndexStride;
		}

		return OutputValue;
	}
} // end namespace



namespace LPCM
{
	void GenerateCompressedData(const TArray<uint8>& InputPCMData, TArray<uint8>& CompressedDataStore,
	                            const FSoundQualityInfo& QualityInfo)
	{
		WaveFormatHeader Format;
		Format.nChannels = static_cast<uint16>(QualityInfo.NumChannels);
		Format.nSamplesPerSec = QualityInfo.SampleRate;
		Format.nBlockAlign = static_cast<uint16>(Format.nChannels * sizeof(int16));
		Format.nAvgBytesPerSec = Format.nBlockAlign * QualityInfo.SampleRate;
		Format.wBitsPerSample = 16;
		Format.wFormatTag = WAVE_FORMAT_LPCM;

		RiffDataChunk RiffDataChunks[2];
		RiffDataChunks[0].ID = FORMAT_FOURCC('f', 'm', 't', ' ');
		RiffDataChunks[0].DataSize = sizeof(Format);
		RiffDataChunks[0].Data = reinterpret_cast<uint8*>(&Format);

		RiffDataChunks[1].ID = FORMAT_FOURCC('d', 'a', 't', 'a');
		RiffDataChunks[1].DataSize = InputPCMData.Num();
		RiffDataChunks[1].Data = const_cast<uint8*>(InputPCMData.GetData());

		BaseDataOperations::GenerateWaveFile(RiffDataChunks, CompressedDataStore);
	}
} // end namespace LPCM

namespace ADPCM
{
	template <typename T>
	static void GetAdaptationTable(T (& OutAdaptationTable)[NUM_ADAPTATION_TABLE])
	{
		static T AdaptationTable[] =
		{
			230, 230, 230, 230, 307, 409, 512, 614,
			768, 614, 512, 409, 307, 230, 230, 230
		};

		FMemory::Memcpy(&OutAdaptationTable, AdaptationTable, sizeof(AdaptationTable));
	}

	struct FAdaptationContext
	{
		int32 AdaptationTable[NUM_ADAPTATION_TABLE];
		int32 AdaptationCoefficient1[NUM_ADAPTATION_COEFF];
		int32 AdaptationCoefficient2[NUM_ADAPTATION_COEFF];

		int32 AdaptationDelta;
		int32 Coefficient1;
		int32 Coefficient2;
		int32 Sample1;
		int32 Sample2;

		FAdaptationContext() :
			AdaptationDelta(0),
			Coefficient1(0),
			Coefficient2(0),
			Sample1(0),
			Sample2(0)
		{
			GetAdaptationTable(AdaptationTable);
			GetAdaptationCoefficients(AdaptationCoefficient1, AdaptationCoefficient2);
		}
	};

	uint8 EncodeNibble(FAdaptationContext& Context, int16 NextSample)
	{
		int32 PredictedSample = (Context.Sample1 * Context.Coefficient1 + Context.Sample2 * Context.Coefficient2) / 256;
		int32 ErrorDelta = (NextSample - PredictedSample) / Context.AdaptationDelta;
		ErrorDelta = FMath::Clamp(ErrorDelta, -8, 7);

		PredictedSample += (Context.AdaptationDelta * ErrorDelta);
		PredictedSample = FMath::Clamp(PredictedSample, -32768, 32767);

		int8 SmallDelta = static_cast<int8>(ErrorDelta);
		const uint8 EncodedNibble = reinterpret_cast<uint8&>(SmallDelta) & 0x0F;

		Context.Sample2 = Context.Sample1;
		Context.Sample1 = static_cast<int16>(PredictedSample);
		Context.AdaptationDelta = (Context.AdaptationDelta * Context.AdaptationTable[EncodedNibble]) / 256;
		Context.AdaptationDelta = FMath::Max(Context.AdaptationDelta, 16);

		return EncodedNibble;
	}

	int32 EncodeBlock(const int16* InputPCMSamples, int32 SampleStride, int32 NumSamples, int32 BlockSize,
	                  uint8* EncodedADPCMData)
	{
		FAdaptationContext Context;
		int32 ReadIndex = 0;
		int32 WriteIndex = 0;

		const uint8 CoefficientIndex = 0;
		Context.AdaptationDelta = Context.AdaptationTable[0];

		Context.Sample2 = BaseDataOperations::ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride);
		Context.Sample1 = BaseDataOperations::ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride);
		Context.Coefficient1 = Context.AdaptationCoefficient1[CoefficientIndex];
		Context.Coefficient2 = Context.AdaptationCoefficient2[CoefficientIndex];

		BaseDataOperations::WriteToByteStream<uint8>(CoefficientIndex, EncodedADPCMData, WriteIndex);
		BaseDataOperations::WriteToByteStream<int16>(Context.AdaptationDelta, EncodedADPCMData, WriteIndex);
		BaseDataOperations::WriteToByteStream<int16>(Context.Sample1, EncodedADPCMData, WriteIndex);
		BaseDataOperations::WriteToByteStream<int16>(Context.Sample2, EncodedADPCMData, WriteIndex);

		while (WriteIndex < BlockSize)
		{
			EncodedADPCMData[WriteIndex] = EncodeNibble(
				Context, BaseDataOperations::ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride)) << 4;
			EncodedADPCMData[WriteIndex++] |= EncodeNibble(
				Context, BaseDataOperations::ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride));
		}

		return WriteIndex;
	}

	void GenerateCompressedData(const TArray<uint8>& InputPCMData, TArray<uint8>& CompressedDataStore,
	                            const FSoundQualityInfo& QualityInfo)
	{
		const int32 SourceSampleStride = QualityInfo.NumChannels;

		const int32 SourceNumSamples = QualityInfo.SampleDataSize / 2;
		const int32 SourceNumSamplesPerChannel = SourceNumSamples / QualityInfo.NumChannels;

		const int32 CompressedNumSamplesPerByte = 2;
		const int32 PreambleSamples = 2;
		const int32 BlockSize = 512;
		const int32 PreambleSize = 2 * PreambleSamples + 3;
		const int32 CompressedSamplesPerBlock = (BlockSize - PreambleSize) * CompressedNumSamplesPerByte +
			PreambleSamples;
		const int32 NumBlocksPerChannel = (SourceNumSamplesPerChannel + CompressedSamplesPerBlock - 1) /
			CompressedSamplesPerBlock;

		const uint32 EncodedADPCMDataSize = NumBlocksPerChannel * BlockSize * QualityInfo.NumChannels;
		uint8* EncodedADPCMData = static_cast<uint8*>(FMemory::Malloc(EncodedADPCMDataSize));
		FMemory::Memzero(EncodedADPCMData, EncodedADPCMDataSize);

		const int16* InputPCMSamples = reinterpret_cast<const int16*>(InputPCMData.GetData());
		uint8* EncodedADPCMChannelData = EncodedADPCMData;

		for (uint32 ChannelIndex = 0; ChannelIndex < QualityInfo.NumChannels; ++ChannelIndex)
		{
			const int16* ChannelPCMSamples = InputPCMSamples + ChannelIndex;
			int32 SourceSampleOffset = 0;
			int32 DestDataOffset = 0;

			for (int32 BlockIndex = 0; BlockIndex < NumBlocksPerChannel; ++BlockIndex)
			{
				EncodeBlock(ChannelPCMSamples + SourceSampleOffset, SourceSampleStride,
				            SourceNumSamples - SourceSampleOffset, BlockSize, EncodedADPCMChannelData + DestDataOffset);

				SourceSampleOffset += CompressedSamplesPerBlock * SourceSampleStride;
				DestDataOffset += BlockSize;
			}

			EncodedADPCMChannelData += DestDataOffset;
		}

		ADPCMFormatHeader Format;
		Format.BaseFormat.nChannels = static_cast<uint16>(QualityInfo.NumChannels);
		Format.BaseFormat.nSamplesPerSec = QualityInfo.SampleRate;
		Format.BaseFormat.nBlockAlign = static_cast<uint16>(BlockSize);
		Format.BaseFormat.wBitsPerSample = 4;
		Format.BaseFormat.wFormatTag = WAVE_FORMAT_ADPCM;
		Format.wSamplesPerBlock = static_cast<uint16>(CompressedSamplesPerBlock);
		Format.BaseFormat.nAvgBytesPerSec = ((Format.BaseFormat.nSamplesPerSec / Format.wSamplesPerBlock) * Format.
			BaseFormat.nBlockAlign);
		Format.wNumCoef = NUM_ADAPTATION_COEFF;
		Format.SamplesPerChannel = SourceNumSamplesPerChannel;
		Format.BaseFormat.cbSize = sizeof(Format) - sizeof(Format.BaseFormat);

		RiffDataChunk RiffDataChunks[2];
		RiffDataChunks[0].ID = FORMAT_FOURCC('f', 'm', 't', ' ');
		RiffDataChunks[0].DataSize = sizeof(Format);
		RiffDataChunks[0].Data = reinterpret_cast<uint8*>(&Format);

		RiffDataChunks[1].ID = FORMAT_FOURCC('d', 'a', 't', 'a');
		RiffDataChunks[1].DataSize = EncodedADPCMDataSize;
		RiffDataChunks[1].Data = EncodedADPCMData;

		BaseDataOperations::GenerateWaveFile(RiffDataChunks, CompressedDataStore);
		FMemory::Free(EncodedADPCMData);
	}
} // end namespace ADPCM


bool FAdvancedPCMCompressor::GenerateCompressedData(const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo,
                                                    TArray<uint8>& OutBuffer)
{
	if (QualityInfo.Quality == 100)
	{
		LPCM::GenerateCompressedData(SrcBuffer, OutBuffer, QualityInfo);
	}
	else
	{
		ADPCM::GenerateCompressedData(SrcBuffer, OutBuffer, QualityInfo);
	}

	return OutBuffer.Num() > 0;
}

void FAdvancedPCMCompressor::AddNewChunk(TArray<TArray<uint8>>& OutBuffers, int32 ChunkReserveSize)
{
	TArray<uint8>& NewBuffer = *new(OutBuffers) TArray<uint8>;
	NewBuffer.Empty(ChunkReserveSize);
}

void FAdvancedPCMCompressor::AddChunkData(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData,
                                          int32 ChunkDataSize)
{
	TArray<uint8>& TargetBuffer = OutBuffers[OutBuffers.Num() - 1];
	TargetBuffer.Append(ChunkData, ChunkDataSize);
}
