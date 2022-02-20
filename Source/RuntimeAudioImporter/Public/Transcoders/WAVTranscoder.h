// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

struct FDecodedAudioStruct;
struct FEncodedAudioStruct;

/**
 * All possible WAV formats
 */
enum class EWavEncodingFormat : uint8
{
	FORMAT_PCM,
	FORMAT_ADPCM,
	FORMAT_IEEE_FLOAT,
	FORMAT_ALAW,
	FORMAT_MULAW,
	FORMAT_DVI_ADPCM,
	FORMAT_EXTENSIBLE
};

/**
 * Information on how to encode WAV data
 */
struct FWavEncodingFormat
{
	EWavEncodingFormat Format;
	uint32 BitsPerSample;

	FWavEncodingFormat(EWavEncodingFormat Format, uint32 BitsPerSample)
		: Format(Format), BitsPerSample(BitsPerSample)
	{
	}
};

class RUNTIMEAUDIOIMPORTER_API WAVTranscoder
{
public:
	/**
	 * Check if the WAV audio data with the RIFF container has a correct byte size
	 * Made by https://github.com/kass-kass
	 *
	 * @param WavData Buffer of the wav data
	 */
	static bool CheckAndFixWavDurationErrors(TArray<uint8>& WavData);
	
	/**
	 * Check if the given WAV audio data seems to be valid
	 */
	static bool CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Encode uncompressed data to WAV format
	 */
	static bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, FWavEncodingFormat Format);

	/**
	 * Decode compressed WAV data to PCM format
	 */
	static bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData);
};
