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
	 * Decode compressed MP3 data to PCM format
	 */
	static bool Decode(const FEncodedAudioStruct& EncodedData, FDecodedAudioStruct& DecodedData);
};
