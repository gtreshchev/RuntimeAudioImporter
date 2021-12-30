// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

struct FDecodedAudioStruct;
struct FEncodedAudioStruct;

class RUNTIMEAUDIOIMPORTER_API WAVTranscoder
{
public:
	/**
	 * Check if the WAV audio data with the RIFF container has a correct byte size.
	 * Made by https://github.com/kass-kass
	 *
	 * @param WavData Buffer of the wav data
	 */
	static bool CheckAndFixWavDurationErrors(TArray<uint8>& WavData);

	/*static bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData);*/
	static bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData);
};
