// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

struct FDecodedAudioStruct;
struct FEncodedAudioStruct;

class RUNTIMEAUDIOIMPORTER_API MP3Transcoder
{
public:
	static bool CheckAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Encode uncompressed data to MP3 format
	 */
	/*static bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData);*/

	/**
	 * Decode compressed MP3 data to PCM format
	 */
	static bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData);
};
