// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

struct FDecodedAudioStruct;
struct FEncodedAudioStruct;

class RUNTIMEAUDIOIMPORTER_API VorbisTranscoder
{
public:
	/**
	 * Check if the given Vorbis audio data seems to be valid
	 */
	static bool CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Encode uncompressed data to Vorbis format
	 */
	static bool Encode(const FDecodedAudioStruct& DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality);

	/**
	 * Decode compressed Vorbis data to PCM format
	 */
	static bool Decode(const FEncodedAudioStruct& EncodedData, FDecodedAudioStruct& DecodedData);
};
