// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAudioImporterTypes.h"


struct FDecodedAudioStruct;
struct FEncodedAudioStruct;

class RUNTIMEAUDIOIMPORTER_API OpusTranscoder
{
public:

	/**
	 * Check if the given Opus audio data seems to be valid
	 */
	//static bool CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Encode uncompressed data to Opus format
	 */
	static bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality);

	/**
	 * Decode compressed Opus data to PCM format
	 */
	//static bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData);
};
