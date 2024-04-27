// Georgy Treshchev 2024.

#include "Codecs/VORBIS_RuntimeCodec.h"
#include "RuntimeAudioImporterTypes.h"
#include "Codecs/RAW_RuntimeCodec.h"
#include "HAL/PlatformProperties.h"
#include "HAL/UnrealMemory.h"

#if UE_VERSION_OLDER_THAN(5, 4, 0)
#include "VorbisAudioInfo.h"
#else
#include "Decoders/VorbisAudioInfo.h"
#endif

#include "Interfaces/IAudioFormat.h"
#ifndef WITH_OGGVORBIS
#define WITH_OGGVORBIS 0
#endif

#define INCLUDE_VORBIS
#include "CodecIncludes.h"
#undef INCLUDE_VORBIS

bool FVORBIS_RuntimeCodec::CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
#if WITH_OGGVORBIS
	FVorbisAudioInfo AudioInfo;
	FSoundQualityInfo SoundQualityInfo;

	if (!AudioInfo.ReadCompressedInfo(AudioData.GetView().GetData(), AudioData.GetView().Num(), &SoundQualityInfo) || SoundQualityInfo.SampleDataSize == 0)
	{
		return false;
	}

	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support VORBIS decoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}

bool FVORBIS_RuntimeCodec::GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Retrieving header information for VORBIS audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to retrieve audio header information in the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

#if WITH_OGGVORBIS
	FVorbisAudioInfo AudioInfo;
	FSoundQualityInfo SoundQualityInfo;

	if (!AudioInfo.ReadCompressedInfo(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &SoundQualityInfo) || SoundQualityInfo.SampleDataSize == 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to read VORBIS compressed info"));
		return false;
	}

	{
		HeaderInfo.Duration = SoundQualityInfo.Duration;
		HeaderInfo.SampleRate = SoundQualityInfo.SampleRate;
		HeaderInfo.NumOfChannels = SoundQualityInfo.NumChannels;
		HeaderInfo.PCMDataSize = SoundQualityInfo.SampleDataSize / sizeof(int16);
		HeaderInfo.AudioFormat = GetAudioFormat();
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved header information for VORBIS audio format.\nHeader info: %s"), *HeaderInfo.ToString());
	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support VORBIS decoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}

bool FVORBIS_RuntimeCodec::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Encoding uncompressed audio data to VORBIS audio format.\nDecoded audio info: %s.\nQuality: %d"), *DecodedData.ToString(), Quality);

#if PLATFORM_SUPPORTS_VORBIS_CODEC
	TArray<uint8> EncodedAudioData;

	const uint32 NumOfFrames = DecodedData.PCMInfo.PCMNumOfFrames;
	const uint32 NumOfChannels = DecodedData.SoundWaveBasicInfo.NumOfChannels;
	const uint32 SampleRate = DecodedData.SoundWaveBasicInfo.SampleRate;

	// Encoding Vorbis data
	{
		vorbis_info VorbisInfo;
		vorbis_info_init(&VorbisInfo);

		if (vorbis_encode_init_vbr(&VorbisInfo, NumOfChannels, SampleRate, static_cast<float>(Quality) / 100) < 0)
		{
			vorbis_info_clear(&VorbisInfo);
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to initialize VORBIS encoder"));
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

		auto CleanUpVORBIS = [&]()
		{
			ogg_stream_clear(&OggStreamState);
			vorbis_block_clear(&VorbisBlock);
			vorbis_dsp_clear(&VorbisDspState);
			vorbis_comment_clear(&VorbisComment);
			vorbis_info_clear(&VorbisInfo);
		};

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

			if (!DecodedData.PCMInfo.PCMData.GetView().GetData() || !AnalysisBuffer)
			{
				CleanUpVORBIS();
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to create VORBIS analysis buffers"));
				return false;
			}

			// Deinterleave for the encoder
			for (uint32 FrameIndex = 0; FrameIndex < FramesToEncode; ++FrameIndex)
			{
				const float* Frame = DecodedData.PCMInfo.PCMData.GetView().GetData() + (FrameIndex + FramesEncoded) * NumOfChannels;

				for (uint32 ChannelIndex = 0; ChannelIndex < NumOfChannels; ++ChannelIndex)
				{
					AnalysisBuffer[ChannelIndex][FrameIndex] = Frame[ChannelIndex];
				}
			}

			// Set how many frames we wrote
			if (vorbis_analysis_wrote(&VorbisDspState, FramesToEncode) < 0)
			{
				CleanUpVORBIS();
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to read VORBIS frames"));
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
		CleanUpVORBIS();
	}

	// Populating the encoded audio data
	{
		EncodedData.AudioData = FRuntimeBulkDataBuffer<uint8>(EncodedAudioData);
		EncodedData.AudioFormat = ERuntimeAudioFormat::OggVorbis;
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully encoded uncompressed audio data to VORBIS audio format.\nEncoded audio info: %s"), *EncodedData.ToString());
	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support VORBIS encoding"), FPlatformProperties::IniPlatformName());
	return false;
#endif
}

bool FVORBIS_RuntimeCodec::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Decoding VORBIS audio data to uncompressed audio format.\nEncoded audio info: %s"), *EncodedData.ToString());

	ensureAlwaysMsgf(EncodedData.AudioFormat == GetAudioFormat(), TEXT("Attempting to decode audio data using the '%s' codec, but the data format is encoded in '%s'"),
	                 *UEnum::GetValueAsString(GetAudioFormat()), *UEnum::GetValueAsString(EncodedData.AudioFormat));

#if WITH_OGGVORBIS
	FVorbisAudioInfo AudioInfo;
	FSoundQualityInfo SoundQualityInfo;

	// Parse the audio header for the relevant information
	if (!AudioInfo.ReadCompressedInfo(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &SoundQualityInfo))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to read VORBIS compressed info"));
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
		DecodedData.SoundWaveBasicInfo.AudioFormat = GetAudioFormat();
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully decoded VORBIS audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString());
	return true;
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Your platform (%hs) does not support VORBIS decoding"), FPlatformProperties::IniPlatformName());
#endif
}
