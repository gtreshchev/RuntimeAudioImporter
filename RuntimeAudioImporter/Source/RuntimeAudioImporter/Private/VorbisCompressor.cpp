// Georgy Treshchev 2021.

#include "VorbisCompressor.h"
#include "Interfaces/IAudioFormat.h"
#include "Serialization/MemoryWriter.h"
#include "VorbisAudioInfo.h"


#if PLATFORM_SUPPORTS_VORBIS_CODEC
THIRD_PARTY_INCLUDES_START
#include "vorbis/vorbisenc.h"
#include "vorbis/vorbisfile.h"
THIRD_PARTY_INCLUDES_END
#endif

#define VORBIS_QUALITY_MODIFIER		0.85

#define SAMPLES_TO_READ		1024
#define SAMPLE_SIZE			( ( uint32 )sizeof( short ) )


bool FVorbisCompressor::GenerateCompressedData(const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo,
                                               TArray<uint8>& CompressedDataStore)
{
#if WITH_OGGVORBIS
	{
		short ReadBuffer[SAMPLES_TO_READ * SAMPLE_SIZE * 2];

		ogg_stream_state Stream_State;
		ogg_page Ogg_Page;
		ogg_packet Ogg_Packet;
		vorbis_info Vorbis_Info;
		vorbis_comment Vorbis_Comment;
		vorbis_dsp_state Dsp_State;
		vorbis_block Vorbis_Block;
		uint32 i;
		bool bEndOfBitstream;

		CompressedDataStore.Empty();
		FMemoryWriter CompressedData(CompressedDataStore);
		uint32 BufferOffset = 0;

		float CompressionQuality = (float)(QualityInfo.Quality * VORBIS_QUALITY_MODIFIER) / 100.0f;
		CompressionQuality = FMath::Clamp(CompressionQuality, -0.1f, 1.0f);

		vorbis_info_init(&Vorbis_Info);

		if (vorbis_encode_init_vbr(&Vorbis_Info, QualityInfo.NumChannels, QualityInfo.SampleRate, CompressionQuality))
		{
			return false;
		}

		vorbis_comment_init( &Vorbis_Comment );
		vorbis_comment_add_tag( &Vorbis_Comment, "ENCODER", "RuntimeAudioImporter" );

		vorbis_analysis_init(&Dsp_State, &Vorbis_Info);
		vorbis_block_init(&Dsp_State, &Vorbis_Block);

		ogg_stream_init(&Stream_State, 0);

		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout(&Dsp_State, &Vorbis_Comment, &header, &header_comm, &header_code);
		ogg_stream_packetin(&Stream_State, &header);
		ogg_stream_packetin(&Stream_State, &header_comm);
		ogg_stream_packetin(&Stream_State, &header_code);

		while (true)
		{
			int result = ogg_stream_flush(&Stream_State, &Ogg_Page);
			if (result == 0)
			{
				break;
			}

			CompressedData.Serialize(Ogg_Page.header, Ogg_Page.header_len);
			CompressedData.Serialize(Ogg_Page.body, Ogg_Page.body_len);
		}

		bEndOfBitstream = false;
		while (!bEndOfBitstream)
		{
			uint32 BytesToRead = FMath::Min(SAMPLES_TO_READ * QualityInfo.NumChannels * SAMPLE_SIZE,
			                                QualityInfo.SampleDataSize - BufferOffset);
			FMemory::Memcpy(ReadBuffer, SrcBuffer.GetData() + BufferOffset, BytesToRead);
			BufferOffset += BytesToRead;

			if (BytesToRead == 0)
			{
				vorbis_analysis_wrote(&Dsp_State, 0);
			}
			else
			{
				float** buffer = vorbis_analysis_buffer(&Dsp_State, SAMPLES_TO_READ);

				if (QualityInfo.NumChannels == 1)
				{
					for (i = 0; i < BytesToRead / SAMPLE_SIZE; i++)
					{
						buffer[0][i] = (ReadBuffer[i]) / 32768.0f;
					}
				}
				else
				{
					for (i = 0; i < BytesToRead / (SAMPLE_SIZE * 2); i++)
					{
						buffer[0][i] = (ReadBuffer[i * 2]) / 32768.0f;
						buffer[1][i] = (ReadBuffer[i * 2 + 1]) / 32768.0f;
					}
				}

				vorbis_analysis_wrote(&Dsp_State, i);
			}

			while (vorbis_analysis_blockout(&Dsp_State, &Vorbis_Block) == 1)
			{
				vorbis_analysis(&Vorbis_Block, NULL);
				vorbis_bitrate_addblock(&Vorbis_Block);

				while (vorbis_bitrate_flushpacket(&Dsp_State, &Ogg_Packet))
				{
					ogg_stream_packetin(&Stream_State, &Ogg_Packet);

					while (!bEndOfBitstream)
					{
						int result = ogg_stream_pageout(&Stream_State, &Ogg_Page);
						if (result == 0)
						{
							break;
						}
						CompressedData.Serialize(Ogg_Page.header, Ogg_Page.header_len);
						CompressedData.Serialize(Ogg_Page.body, Ogg_Page.body_len);

						if (ogg_page_eos(&Ogg_Page))
						{
							bEndOfBitstream = true;
						}
					}
				}
			}
		}

		ogg_stream_clear(&Stream_State);
		vorbis_block_clear(&Vorbis_Block);
		vorbis_dsp_clear(&Dsp_State);
		vorbis_comment_clear(&Vorbis_Comment);
		vorbis_info_clear(&Vorbis_Info);
	}
	return CompressedDataStore.Num() > 0;
#else
		return false;
#endif		// WITH_OGGVOBVIS
}


TArray<int32> FVorbisCompressor::GetChannelOrder(int32 NumChannels)
{
	TArray<int32> ChannelOrder;

	switch (NumChannels)
	{
	case 6:
		{
			for (int32 i = 0; i < NumChannels; i++)
			{
				ChannelOrder.Add(VorbisChannelInfo::Order[NumChannels - 1][i]);
			}
			break;
		}

	default:
		{
			for (int32 i = 0; i < NumChannels; i++)
			{
				ChannelOrder.Add(i);
			}
			break;
		}
	}

	return ChannelOrder;
}


int32 FVorbisCompressor::GetMinimumSizeForInitialChunk(FName Format, const TArray<uint8>& SrcBuffer)
{
	return 6 * 1024;
}

void FVorbisCompressor::AddNewChunk(TArray<TArray<uint8>>& OutBuffers, int32 ChunkReserveSize)
{
	TArray<uint8>& NewBuffer = *new(OutBuffers) TArray<uint8>;
	NewBuffer.Empty(ChunkReserveSize);
}

void FVorbisCompressor::AddChunkData(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, int32 ChunkDataSize)
{
	TArray<uint8>& TargetBuffer = OutBuffers[OutBuffers.Num() - 1];
	TargetBuffer.Append(ChunkData, ChunkDataSize);
}
