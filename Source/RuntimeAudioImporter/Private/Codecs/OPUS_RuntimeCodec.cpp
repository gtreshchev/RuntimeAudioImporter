// Georgy Treshchev 2024.

#include "Codecs/OPUS_RuntimeCodec.h"
#include "Codecs/RAW_RuntimeCodec.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "HAL/UnrealMemory.h"

#define INCLUDE_OPUS
#include "CodecIncludes.h"
#undef INCLUDE_OPUS

bool FOPUS_RuntimeCodec::CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	static constexpr uint8 OGG_SIGN[] = {0x4f, 0x67, 0x67}; // "Ogg" in hex
	static constexpr uint8 OPUS_SIGN[] = {'O', 'p', 'u', 's'}; // "Opus" signature

	if (FMemory::Memcmp(AudioData.GetView().GetData(), OGG_SIGN, sizeof(OGG_SIGN)) != 0)
	{
		return false; // Not an Ogg file
	}

	const uint8* Data = AudioData.GetView().GetData();
	for (size_t i = 0; i < AudioData.GetView().Num() - sizeof(OPUS_SIGN); ++i)
	{
		if (FMemory::Memcmp(&Data[i], OPUS_SIGN, sizeof(OPUS_SIGN)) == 0)
		{
			return true; // Found "Opus" signature
		}
	}

	return false;
}

bool FOPUS_RuntimeCodec::GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
{
	int ErrorCode;
	OggOpusFile* opusFile = op_open_memory(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &ErrorCode);

	if (!opusFile)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to open Opus file for header info, error code: %d (%s)"),
		       static_cast<int32>(ErrorCode), *FString(ANSI_TO_TCHAR(opus_strerror(ErrorCode))));
		return false;
	}

	const OpusHead* OpusHeader = op_head(opusFile, 0);
	if (!OpusHeader)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to read Opus header"));
		op_free(opusFile);
		return false;
	}

	HeaderInfo.Duration = static_cast<float>(op_pcm_total(opusFile, -1)) / OpusHeader->input_sample_rate;
	HeaderInfo.NumOfChannels = OpusHeader->channel_count;
	HeaderInfo.SampleRate = OpusHeader->input_sample_rate;
	HeaderInfo.PCMDataSize = HeaderInfo.Duration * HeaderInfo.SampleRate * HeaderInfo.NumOfChannels;
	HeaderInfo.AudioFormat = GetAudioFormat();

	op_free(opusFile);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully retrieved header information for OPUS audio format.\nHeader info: %s"), *HeaderInfo.ToString());
	return true;
}

bool FOPUS_RuntimeCodec::Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
{
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Encoding uncompressed audio data to OPUS audio format.\nDecoded audio info: %s.\nQuality: %d"), *DecodedData.ToString(), Quality);

	// Preskip duration recommended for .opus files
	static constexpr float PreskipDuration = 0.08f;

	// Channel layout mapping based on number of channels
	static const struct ChannelLayout
	{
		int32 StreamCount;
		int32 CoupledStreamCount;
		uint8 Mapping[8];
	} ChannelLayouts[8] = {
			{1, 0, {0}}, // 1: mono
			{1, 1, {0, 1}}, // 2: stereo
			{2, 1, {0, 1, 2}}, // 3: 1-d surround
			{2, 2, {0, 1, 2, 3}}, // 4: quadraphonic surround
			{3, 2, {0, 1, 4, 2, 3}}, // 5: 5-channel surround
			{4, 2, {0, 1, 4, 5, 2, 3}}, // 6: 5.1 surround
			{4, 3, {0, 1, 4, 6, 2, 3, 5}}, // 7: 6.1 surround
			{5, 3, {0, 1, 6, 7, 2, 3, 4, 5}} // 8: 7.1 surround
		};

	// Supported Opus sample rates
	//static const TArray<uint32> SupportedSampleRates = { 8000, 12000, 16000, 24000, 48000 };

	uint32 NumOfChannels = DecodedData.SoundWaveBasicInfo.NumOfChannels;
	uint32 SampleRate = DecodedData.SoundWaveBasicInfo.SampleRate;
	Audio::FAlignedFloatBuffer ProcessedPCMData = Audio::FAlignedFloatBuffer(DecodedData.PCMInfo.PCMData.GetView().GetData(), DecodedData.PCMInfo.PCMData.GetView().Num());

	// Mix channels if more than 2
	// TODO: Support more than 2 channels
	if (NumOfChannels > 2)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("More than 2 channels detected (%d) which is not supported for OPUS encoding at the moment. Mixing to stereo"), NumOfChannels);

		Audio::FAlignedFloatBuffer RemixedPCMData;
		if (!FRAW_RuntimeCodec::MixChannelsRAWData(ProcessedPCMData, SampleRate, NumOfChannels, 2, RemixedPCMData))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to mix channels for OPUS encoding (%d -> 2)"), NumOfChannels);
			return false;
		}

		ProcessedPCMData = MoveTemp(RemixedPCMData);
		NumOfChannels = 2;

		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Audio data has been mixed to stereo for OPUS encoding"));
	}

	// Resample to 48kHz if not
	// TODO: Support other sample rates
	if (SampleRate != 48000)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Sample rate is not 48kHz (%d) which is not supported for OPUS encoding at the moment. Resampling to 48kHz"), SampleRate);

		constexpr uint32 DestinationSampleRate = 48000;
		Audio::FAlignedFloatBuffer ResampledPCMData;
		if (!FRAW_RuntimeCodec::ResampleRAWData(ProcessedPCMData, NumOfChannels, SampleRate, DestinationSampleRate, ResampledPCMData))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to resample audio data for OPUS encoding (%d -> %d)"), SampleRate, DestinationSampleRate);
			return false;
		}

		ProcessedPCMData = MoveTemp(ResampledPCMData);
		SampleRate = DestinationSampleRate;

		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Audio data has been resampled to the desired sample rate '%d'"), SampleRate);
	}

	const uint32 NumOfFrames = ProcessedPCMData.Num() / NumOfChannels;

	TArray<uint8> EncodedAudioData;

	// Opus encoder initialization
	OpusEncoder* OpusEnc = nullptr;
	int OpusError;

	OpusEnc = opus_encoder_create(
		SampleRate,
		NumOfChannels,
		OPUS_APPLICATION_AUDIO,
		&OpusError
	);

	if (OpusError != OPUS_OK)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to create OPUS encoder: %d"), static_cast<int32>(OpusError));
		return false;
	}

	// Configure encoder
	{
		// Variable bit rate encoding
		int32 UseVbr = 1;
		opus_encoder_ctl(OpusEnc, OPUS_SET_VBR(UseVbr));

		// Disable constrained VBR
		int32 UseCVbr = 0;
		opus_encoder_ctl(OpusEnc, OPUS_SET_VBR_CONSTRAINT(UseCVbr));

		// Complexity (1-10)
		int32 Complexity = FMath::Clamp(static_cast<int32>(Quality / 10), 0, 10);
		opus_encoder_ctl(OpusEnc, OPUS_SET_COMPLEXITY(Complexity));

		// Disable forward error correction for this use case
		int32 InbandFEC = 0;
		opus_encoder_ctl(OpusEnc, OPUS_SET_INBAND_FEC(InbandFEC));

		// Set bitrate
		int BitrateKbps = FMath::Clamp(
			static_cast<int>((Quality / 100.0f) * 320), // Max 320 kbps
			12, // Minimum 12 kbps
			320 // Maximum 320 kbps
		);
		opus_encoder_ctl(OpusEnc, OPUS_SET_BITRATE(BitrateKbps * 1000));
	}

	// Ogg stream initialization
	ogg_stream_state OggStreamState;
	uint32_t SerialNumber = FMath::Rand();
	ogg_stream_init(&OggStreamState, SerialNumber);

	// Generate Opus header packet
	{
		TArray<uint8> HeaderPacket;

		// Magic number
		uint8 MagicNumber[8] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
		HeaderPacket.Append(MagicNumber, 8);

		uint8 Version = 1;
		HeaderPacket.Add(Version);

		uint8 ChannelCount = static_cast<uint8>(NumOfChannels);
		HeaderPacket.Add(ChannelCount);

		// Preskip (fixed at 10ms, about 480 samples at 48kHz)
		uint16 Preskip = static_cast<uint16>(FMath::FloorToInt(PreskipDuration * SampleRate));
		HeaderPacket.Append(reinterpret_cast<uint8*>(&Preskip), sizeof(uint16));

		// Sample rate
		HeaderPacket.Append((uint8*)(&SampleRate), sizeof(uint32));

		// Output gain (0 for now)
		int16 OutputGain = 0;
		HeaderPacket.Append(reinterpret_cast<uint8*>(&OutputGain), sizeof(int16));

		// Channel mapping
		uint8 ChannelMapping = NumOfChannels > 2 ? 1 : 0;
		HeaderPacket.Add(ChannelMapping);

		if (ChannelMapping > 0)
		{
			const ChannelLayout& Layout = ChannelLayouts[NumOfChannels - 1];
			HeaderPacket.Add(static_cast<uint8>(Layout.StreamCount));
			HeaderPacket.Add(static_cast<uint8>(Layout.CoupledStreamCount));
			HeaderPacket.Append(Layout.Mapping, NumOfChannels);
		}

		// Create Ogg packet
		ogg_packet HeaderOggPacket;
		HeaderOggPacket.packet = HeaderPacket.GetData();
		HeaderOggPacket.bytes = HeaderPacket.Num();
		HeaderOggPacket.b_o_s = 1; // Beginning of stream
		HeaderOggPacket.e_o_s = 0;
		HeaderOggPacket.granulepos = 0;
		HeaderOggPacket.packetno = 0;

		ogg_stream_packetin(&OggStreamState, &HeaderOggPacket);
	}

	// Generate comment packet
	{
		TArray<uint8> CommentPacket;
		uint8 MagicNumber[8] = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'};
		CommentPacket.Append(MagicNumber, 8);

		// Vendor string
		const char* VendorString = "RuntimeAudioImporter";
		uint32 VendorStringLength = strlen(VendorString);
		CommentPacket.Append(reinterpret_cast<uint8*>(&VendorStringLength), sizeof(uint32));
		CommentPacket.Append(reinterpret_cast<const uint8*>(VendorString), VendorStringLength);

		// No user comments
		uint32 CommentListLength = 0;
		CommentPacket.Append(reinterpret_cast<uint8*>(&CommentListLength), sizeof(uint32));

		// Create Ogg packet
		ogg_packet CommentOggPacket;
		CommentOggPacket.packet = CommentPacket.GetData();
		CommentOggPacket.bytes = CommentPacket.Num();
		CommentOggPacket.b_o_s = 0;
		CommentOggPacket.e_o_s = 0;
		CommentOggPacket.granulepos = 0;
		CommentOggPacket.packetno = 1;

		ogg_stream_packetin(&OggStreamState, &CommentOggPacket);
	}

	// Flush initial headers
	ogg_page OggPage;
	while (ogg_stream_flush(&OggStreamState, &OggPage))
	{
		EncodedAudioData.Append(static_cast<uint8*>(OggPage.header), OggPage.header_len);
		EncodedAudioData.Append(static_cast<uint8*>(OggPage.body), OggPage.body_len);
	}

	// Encoding parameters
	const uint32 FrameSize = static_cast<int>(SampleRate * 0.02);
	TArray<float> EncodingBuffer;
	EncodingBuffer.SetNum(NumOfChannels * FrameSize);

	uint32 FramesProcessed = 0;
	int64 GranulePos = 0;
	int PacketIndex = 2; // Start at 2 after header and comment packets

	while (FramesProcessed < NumOfFrames)
	{
		// Determine how many frames to process
		uint32 FramesToProcess = FMath::Min<uint32>(FrameSize, NumOfFrames - FramesProcessed);

		// Prepare input buffer (deinterleave)
		for (uint32 FrameIndex = 0; FrameIndex < FramesToProcess; ++FrameIndex)
		{
			const float* SourceFrame = ProcessedPCMData.GetData() + (FramesProcessed + FrameIndex) * NumOfChannels;

			for (uint32 ChannelIndex = 0; ChannelIndex < NumOfChannels; ++ChannelIndex)
			{
				EncodingBuffer[FrameIndex * NumOfChannels + ChannelIndex] = SourceFrame[ChannelIndex];
			}
		}

		// Ensure buffer is filled if last chunk is smaller
		if (FramesToProcess < FrameSize)
		{
			FMemory::Memzero(
				EncodingBuffer.GetData() + (FramesToProcess * NumOfChannels),
				(FrameSize - FramesToProcess) * NumOfChannels * sizeof(float)
			);
		}

		// Encode frame
		TArray<uint8> CompressedBuffer;
		CompressedBuffer.SetNum(4096); // Large enough buffer for compressed data

		int32 CompressedSize = opus_encode_float(
			OpusEnc,
			EncodingBuffer.GetData(),
			static_cast<int>(FrameSize),
			CompressedBuffer.GetData(),
			CompressedBuffer.Num()
		);

		if (CompressedSize < 0)
		{
			const char* ErrorStr = opus_strerror(CompressedSize);
			UE_LOG(LogRuntimeAudioImporter, Error,
			       TEXT("Opus encoding failed: error %d: %s"),
			       CompressedSize,
			       *FString(ANSI_TO_TCHAR(ErrorStr))
			);
			opus_encoder_destroy(OpusEnc);
			return false;
		}

		// Prepare Ogg packet
		ogg_packet OpusPacket;
		OpusPacket.packet = CompressedBuffer.GetData();
		OpusPacket.bytes = CompressedSize;
		OpusPacket.b_o_s = 0;
		OpusPacket.e_o_s = (FramesProcessed + FramesToProcess >= NumOfFrames) ? 1 : 0;

		// Granule position calculation
		GranulePos += FramesToProcess;

		OpusPacket.granulepos = GranulePos;
		OpusPacket.packetno = PacketIndex++;

		// Submit packet to Ogg stream
		ogg_stream_packetin(&OggStreamState, &OpusPacket);

		// Get Ogg pages
		while (true)
		{
			int Result = ogg_stream_pageout(&OggStreamState, &OggPage);
			if (Result == 0) break;

			EncodedAudioData.Append(static_cast<uint8*>(OggPage.header), OggPage.header_len);
			EncodedAudioData.Append(static_cast<uint8*>(OggPage.body), OggPage.body_len);
		}

		// Update processed frames
		FramesProcessed += FramesToProcess;
	}

	// Final flush
	while (ogg_stream_flush(&OggStreamState, &OggPage))
	{
		EncodedAudioData.Append(static_cast<uint8*>(OggPage.header), OggPage.header_len);
		EncodedAudioData.Append(static_cast<uint8*>(OggPage.body), OggPage.body_len);
	}

	// Clean up
	opus_encoder_destroy(OpusEnc);
	ogg_stream_clear(&OggStreamState);

	// Populate encoded data
	{
		EncodedData.AudioData = FRuntimeBulkDataBuffer<uint8>(EncodedAudioData);
		EncodedData.AudioFormat = ERuntimeAudioFormat::OggOpus;
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully encoded uncompressed audio data to OPUS audio format.\nEncoded audio info: %s"), *EncodedData.ToString());
	return true;
}

bool FOPUS_RuntimeCodec::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	int ErrorCode;
	OggOpusFile* OpusFile = op_open_memory(EncodedData.AudioData.GetView().GetData(), EncodedData.AudioData.GetView().Num(), &ErrorCode);

	if (!OpusFile)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to open Opus file for decoding, error code: %d (%s)"),
		       static_cast<int32>(ErrorCode), *FString(ANSI_TO_TCHAR(opus_strerror(ErrorCode))));
		return false;
	}

	const OpusHead* OpusHeader = op_head(OpusFile, 0);
	const uint32 NumOfChannels = OpusHeader->channel_count;
	const size_t TotalFrames = op_pcm_total(OpusFile, -1);
	const size_t DecodedBufferSize = TotalFrames * NumOfChannels;

	// Allocate memory for the PCM data
	float* DecodedPCMData = static_cast<float*>(FMemory::Malloc(DecodedBufferSize * sizeof(float)));
	if (!DecodedPCMData)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for PCM data"));
		op_free(OpusFile);
		return false;
	}

	// Decode the audio data
	size_t PCMNumOfFrames = 0;
	while (PCMNumOfFrames < TotalFrames)
	{
		int32 FramesDecoded = op_read_float(OpusFile, DecodedPCMData + PCMNumOfFrames * NumOfChannels, TotalFrames - PCMNumOfFrames, nullptr);
		if (FramesDecoded <= 0)
		{
			break;
		}

		PCMNumOfFrames += FramesDecoded;
	}

	// Assign decoded PCM data to the DecodedData struct
	DecodedData.PCMInfo.PCMNumOfFrames = PCMNumOfFrames;
	DecodedData.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(DecodedPCMData, PCMNumOfFrames * NumOfChannels);

	// Basic sound wave information
	DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(TotalFrames) / OpusHeader->input_sample_rate;
	DecodedData.SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
	DecodedData.SoundWaveBasicInfo.SampleRate = OpusHeader->input_sample_rate;
	DecodedData.SoundWaveBasicInfo.AudioFormat = GetAudioFormat();

	op_free(OpusFile);
	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully decoded OPUS audio data to uncompressed audio format.\nDecoded audio info: %s"), *DecodedData.ToString());
	return true;
}
