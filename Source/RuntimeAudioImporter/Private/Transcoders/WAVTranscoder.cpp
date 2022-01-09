// Georgy Treshchev 2022.

#include "Transcoders/WAVTranscoder.h"
#include "RuntimeAudioImporterTypes.h"

#define INCLUDE_WAV
#include "RuntimeAudioImporterIncludes.h"
#undef INCLUDE_WAV

bool WAVTranscoder::CheckAndFixWavDurationErrors(TArray<uint8>& WavData)
{
	drwav wav;
	/** Initializing transcoding of audio data in memory */
	if (!drwav_init_memory(&wav, WavData.GetData(), WavData.Num() - 2, nullptr))
	{
		return false;
	}

	/** Check if the container is RIFF (not Wave64 or any other containers) */
	if (wav.container != drwav_container_riff)
	{
		drwav_uninit(&wav);
		return true;
	}

	/*
	* Get 4-byte field at byte 4, which is the overall file size as uint32, according to RIFF specification.
	* If the field is set to nothing (hex FFFFFFFF), replace the incorrectly set field with the actual size.
	* The field should be (size of file - 8 bytes), as the chunk identifier for the whole file (4 bytes spelling out RIFF at the start of the file), and the chunk length (4 bytes that we're replacing) are excluded.
	*/
	if (BytesToHex(WavData.GetData() + 4, 4) == "FFFFFFFF")
	{
		const int32 ActualFileSize = WavData.Num() - 8;
		FMemory::Memcpy(WavData.GetData() + 4, &ActualFileSize, 4);
	}

	/*
	* Search for the place in the file after the chunk id "data", which is where the data length is stored.
	* First 36 bytes are skipped, as they're always "RIFF", 4 bytes filesize, "WAVE", "fmt ", and 20 bytes of format data.
	*/
	int32 DataSizeLocation = INDEX_NONE;
	for (int32 i = 36; i < WavData.Num() - 4; ++i)
	{
		if (BytesToHex(WavData.GetData() + i, 4) == "64617461" /* hex for string "data" */)
		{
			DataSizeLocation = i + 4;
			break;
		}
	}
	if (DataSizeLocation == INDEX_NONE) // should never happen but just in case
	{
		drwav_uninit(&wav);

		return false;
	}

	/** Same process as replacing full file size, except DataSize counts bytes from end of DataSize int to end of file. */
	if (BytesToHex(WavData.GetData() + DataSizeLocation, 4) == "FFFFFFFF")
	{
		const int32 ActualDataSize = WavData.Num() - DataSizeLocation - 4 /*-4 to not include the DataSize int itself*/;
		FMemory::Memcpy(WavData.GetData() + DataSizeLocation, &ActualDataSize, 4);
	}

	drwav_uninit(&wav);

	return true;
}

bool WAVTranscoder::Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
{
	drwav wav;

	/** Initializing transcoding of audio data in memory */
	if (!drwav_init_memory(&wav, EncodedData.AudioData, EncodedData.AudioDataSize, nullptr))
	{
		return false;
	}

	/** Allocating memory for PCM data */
	DecodedData.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(wav.totalPCMFrameCount * wav.channels * sizeof(float)));

	/** Filling PCM data and getting the number of frames */
	DecodedData.PCMInfo.PCMNumOfFrames = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, reinterpret_cast<float*>(DecodedData.PCMInfo.PCMData));

	/** Getting PCM data size */
	DecodedData.PCMInfo.PCMDataSize = static_cast<uint32>(DecodedData.PCMInfo.PCMNumOfFrames * wav.channels * sizeof(float));

	/** Getting basic audio information */
	{
		DecodedData.SoundWaveBasicInfo.Duration = static_cast<float>(wav.totalPCMFrameCount) / wav.sampleRate;
		DecodedData.SoundWaveBasicInfo.ChannelsNum = wav.channels;
		DecodedData.SoundWaveBasicInfo.SampleRate = wav.sampleRate;
	}

	/** Uninitializing transcoding of audio data in memory */
	drwav_uninit(&wav);

	return true;
}
