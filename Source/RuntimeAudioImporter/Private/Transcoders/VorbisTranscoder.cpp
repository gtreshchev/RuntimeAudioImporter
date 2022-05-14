// Georgy Treshchev 2022.

#include "VorbisTranscoder.h"
#include "RuntimeAudioImporterTypes.h"
#include "Transcoders/RAWTranscoder.h"
#include "GenericPlatform/GenericPlatformProperties.h"

#define INCLUDE_VORBIS
#include "TranscodersIncludes.h"
#undef INCLUDE_VORBIS

bool VorbisTranscoder::CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	int32 ErrorCode;
	stb_vorbis* STBVorbis = stb_vorbis_open_memory(AudioData, AudioDataSize, &ErrorCode, nullptr);

	if (STBVorbis == nullptr)
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to initialize vorbis encoder"));
		return false;
	}

	return true;
}

bool VorbisTranscoder::Encode(const FDecodedAudioStruct& DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Encoding uncompressed audio data to Vorbis audio format.\nDecoded audio info: %s.\nQuality: %d"), *DecodedData.ToString(), Quality));
	
#if PLATFORM_SUPPORTS_VORBIS_CODEC

	TArray<uint8> EncodedAudioData;
	
	const uint32 NumOfFrames = DecodedData.PCMInfo.PCMNumOfFrames;
	const uint32 NumOfChannels = DecodedData.SoundWaveBasicInfo.NumOfChannels;
	const uint32 SampleRate = DecodedData.SoundWaveBasicInfo.SampleRate;

	// Copying decoded data to prevent crash if the task is interrupted
	const TArray<float> CopiedDecodedData{reinterpret_cast<float*>(DecodedData.PCMInfo.PCMData.GetView().GetData()), static_cast<int32>(DecodedData.PCMInfo.PCMData.GetView().Num()) / static_cast<int32>(sizeof(float))};

	// Encoding Vorbis data
	{
		vorbis_info VorbisInfo;
		vorbis_info_init(&VorbisInfo);

		if (vorbis_encode_init_vbr(&VorbisInfo, NumOfChannels, SampleRate, static_cast<float>(Quality) / 100) < 0)
		{
			vorbis_info_clear(&VorbisInfo);
			
			RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to initialize vorbis encoder"));
			return false;
		}

		// Set the comment
		vorbis_comment VorbisComment;
		{
			vorbis_comment_init(&VorbisComment);
			vorbis_comment_add_tag(&VorbisComment, "ENCODER", "RuntimeAudioImporter");
		}

		// Start making a vorbis block
		vorbis_dsp_state VorbisDspState;
		vorbis_block VorbisBlock;
		{
			vorbis_analysis_init(&VorbisDspState, &VorbisInfo);
			vorbis_block_init(&VorbisDspState, &VorbisBlock);
		}

		// Ogg packet stuff
		ogg_packet OggPacket, OggComment, OggCode;
		ogg_page OggPage;
		ogg_stream_state OggStreamState;

		{
			ogg_stream_init(&OggStreamState, 0);
			vorbis_analysis_headerout(&VorbisDspState, &VorbisComment, &OggPacket, &OggComment, &OggCode);
			ogg_stream_packetin(&OggStreamState, &OggPacket);
			ogg_stream_packetin(&OggStreamState, &OggComment);
			ogg_stream_packetin(&OggStreamState, &OggCode);
		}

		// Do stuff until we don't do stuff anymore since we need the data on a separate page
		while (ogg_stream_flush(&OggStreamState, &OggPage))
		{
			EncodedAudioData.Append(static_cast<uint8*>(OggPage.header), OggPage.header_len);
			EncodedAudioData.Append(static_cast<uint8*>(OggPage.body), OggPage.body_len);
		}

		uint32 FramesEncoded{0}, FramesRead{0};
		
		bool bEndOfBitstream{false};

		while (!bEndOfBitstream)
		{
			constexpr uint32 FramesSplitCount = 1024;
			
			// Getting frames for encoding
			uint32 FramesToEncode{NumOfFrames - FramesRead};

			// Analyze buffers
			float** AnalysisBuffer = vorbis_analysis_buffer(&VorbisDspState, FramesToEncode);

			// Make sure we don't read more than FramesSplitCount, since libvorbis can segfault if we read too much at once
			if (FramesToEncode > FramesSplitCount)
			{
				FramesToEncode = FramesSplitCount;
			}

			if (CopiedDecodedData.GetData() == nullptr || AnalysisBuffer == nullptr)
			{
				RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to create analysis buffers"));
				return false;
			}

			// Deinterleave for the encoder
			for (uint32 FrameIndex = 0; FrameIndex < FramesToEncode; ++FrameIndex)
			{
				const float* Frame = CopiedDecodedData.GetData() + (FrameIndex + FramesEncoded) * NumOfChannels;
				
				for (uint32 ChannelIndex = 0; ChannelIndex < NumOfChannels; ++ChannelIndex)
				{
					AnalysisBuffer[ChannelIndex][FrameIndex] = Frame[ChannelIndex];
				}
			}

			// Set how many frames we wrote
			if (vorbis_analysis_wrote(&VorbisDspState, FramesToEncode) < 0)
			{
				RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to read frames"));
				return false;
			}

			// Separate AnalysisBuffer into separate blocks, then chunk those blocks into pages
			while (vorbis_analysis_blockout(&VorbisDspState, &VorbisBlock) == 1)
			{
				// Perform actual analysis
				vorbis_analysis(&VorbisBlock, nullptr);

				// Determine the bitrate on this block
				vorbis_bitrate_addblock(&VorbisBlock);

				// Flush all available vorbis blocks into packets, then append the resulting pages to the output buffer
				while (vorbis_bitrate_flushpacket(&VorbisDspState, &OggPacket))
				{
					ogg_stream_packetin(&OggStreamState, &OggPacket);

					while (!bEndOfBitstream)
					{
						// Write data if we have a page
						if (!ogg_stream_pageout(&OggStreamState, &OggPage))
						{
							break;
						}

						EncodedAudioData.Append(static_cast<uint8*>(OggPage.header), OggPage.header_len);
						EncodedAudioData.Append(static_cast<uint8*>(OggPage.body), OggPage.body_len);

						// End if we need to
						if (ogg_page_eos(&OggPage))
						{
							bEndOfBitstream = true;
						}
					}
				}
			}

			// Increment
			FramesEncoded += FramesToEncode;
			FramesRead += FramesToEncode;
		}

		// Clean up
		ogg_stream_clear(&OggStreamState);
		vorbis_block_clear(&VorbisBlock);
		vorbis_dsp_clear(&VorbisDspState);
		vorbis_comment_clear(&VorbisComment);
		vorbis_info_clear(&VorbisInfo);
	}

	// Filling in the encoded audio data
	{
		EncodedData.AudioData = FBulkDataBuffer<uint8>(static_cast<uint8*>(FMemory::Malloc(EncodedAudioData.Num())), EncodedAudioData.Num());
		EncodedData.AudioFormat = EAudioFormat::OggVorbis;
		FMemory::Memcpy(EncodedData.AudioData.GetView().GetData(), EncodedAudioData.GetData(), EncodedAudioData.Num());
	}
	
	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Successfully encoded uncompressed audio data to Vorbis audio format.\nEncoded audio info: %s"), *EncodedData.ToString()));

	return true;

	#undef FRAMES_SPLIT_COUNT

#else
	RuntimeAudioImporter_TranscoderLogs::PrintError(FString::Printf(TEXT("Your platform (%hs) does not support Vorbis encoding"), FGenericPlatformProperties::IniPlatformName()));
	return false;
#endif
}

bool VorbisTranscoder::Decode(const FEncodedAudioStruct& EncodedData, FDecodedAudioStruct& DecodedData)
{
	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Decoding Vorbis audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString()));
	
	int32 ErrorCode;
	stb_vorbis* Vorbis_Decoder = stb_vorbis_open_memory(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &ErrorCode, nullptr);

	if (Vorbis_Decoder == nullptr)
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Unable to initialize OGG Vorbis Decoder"));
		return false;
	}

	const int32 SamplesLimit{Vorbis_Decoder->channels * 4096};
	const int32 NumOfChannels{Vorbis_Decoder->channels};
	const int32 SampleRate{static_cast<int32>(Vorbis_Decoder->sample_rate)};

	int32 SamplesOffset = 0;
	int32 NumOfFrames = 0;

	int32 TotalSamples = SamplesLimit;

	int16* Int16RAWBuffer = static_cast<int16*>(FMemory::Malloc(TotalSamples * sizeof(int16)));
	if (Int16RAWBuffer == nullptr)
	{
		RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to allocate memory for OGG Vorbis Decoder"));
		stb_vorbis_close(Vorbis_Decoder);
		return false;
	}

	while (true)
	{
		const int32 CurrentFrames = stb_vorbis_get_frame_short_interleaved(Vorbis_Decoder, NumOfChannels, Int16RAWBuffer + SamplesOffset, TotalSamples - SamplesOffset);

		if (CurrentFrames == 0) break;

		NumOfFrames += CurrentFrames;
		SamplesOffset += CurrentFrames * NumOfChannels;

		if (SamplesOffset + SamplesLimit > TotalSamples)
		{
			TotalSamples *= 2;

			int16* Int16RAWBufferFrame = static_cast<int16*>(FMemory::Realloc(Int16RAWBuffer, TotalSamples * sizeof(int16)));

			if (Int16RAWBufferFrame == nullptr)
			{
				RuntimeAudioImporter_TranscoderLogs::PrintError(TEXT("Failed to allocate memory for OGG Vorbis Decoder"));

				FMemory::Free(Int16RAWBuffer);
				stb_vorbis_close(Vorbis_Decoder);

				return false;
			}

			Int16RAWBuffer = Int16RAWBufferFrame;
		}
	}

	stb_vorbis_close(Vorbis_Decoder);


	// Getting the number of frames
	DecodedData.PCMInfo.PCMNumOfFrames = NumOfFrames;

	// Getting PCM data size
	const int32 TempPCMDataSize = DecodedData.PCMInfo.PCMNumOfFrames * NumOfChannels * 2;

	// Transcoding int16 to float format
	{
		float* TempFloatBuffer = static_cast<float*>(FMemory::Malloc(DecodedData.PCMInfo.PCMNumOfFrames * NumOfChannels * 2 * sizeof(float)));
		int32 TempFloatSize;

		RAWTranscoder::TranscodeRAWData<int16, float>(Int16RAWBuffer, TempPCMDataSize, TempFloatBuffer, TempFloatSize);
		DecodedData.PCMInfo.PCMData = FBulkDataBuffer<uint8>(reinterpret_cast<uint8*>(TempFloatBuffer), TempFloatSize);
	}
	
	FMemory::Free(Int16RAWBuffer);

	// Getting basic audio information
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedData.PCMInfo.PCMNumOfFrames) / SampleRate;
		DecodedData.SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
		DecodedData.SoundWaveBasicInfo.SampleRate = SampleRate;
	}
	
	RuntimeAudioImporter_TranscoderLogs::PrintLog(FString::Printf(TEXT("Successfully decoded Vorbis audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString()));

	return true;
}
