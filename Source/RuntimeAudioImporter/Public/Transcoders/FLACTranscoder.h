// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

struct FDecodedAudioStruct;
struct FEncodedAudioStruct;


class RUNTIMEAUDIOIMPORTER_API FLACTranscoder
{
public:
	/**
	 * Check if the given FLAC audio data seems to be valid
	 */
	static bool CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Encode uncompressed data to FLAC format
	 */
	/*static bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData);*/

	/**
	 * Decode compressed FLAC data to PCM format
	 */
	static bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData);
};