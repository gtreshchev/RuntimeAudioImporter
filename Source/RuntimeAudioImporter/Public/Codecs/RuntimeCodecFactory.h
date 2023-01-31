// Georgy Treshchev 2023.

#pragma once

#include "BaseRuntimeCodec.h"

/**
 * A factory for constructing the codecs used for encoding and decoding audio data
 */
class RUNTIMEAUDIOIMPORTER_API FRuntimeCodecFactory
{
public:
	FRuntimeCodecFactory() = default;
	virtual ~FRuntimeCodecFactory() = default;

	/**
	 * Get the codec based on the file path extension
	 *
	 * @param FilePath The file path from which to get the codec
	 * @return The detected codec, or a nullptr if it could not be detected
	 */
	virtual TUniquePtr<FBaseRuntimeCodec> GetCodec(const FString& FilePath);

	/**
	 * Get the codec based on the audio format
	 *
	 * @param AudioFormat The format from which to get the codec
	 * @return The detected codec, or a nullptr if it could not be detected
	 */
	virtual TUniquePtr<FBaseRuntimeCodec> GetCodec(ERuntimeAudioFormat AudioFormat);

	/**
	 * Get the codec based on the audio data (slower, but more reliable)
	 *
	 * @param AudioData The audio data from which to get the codec
	 * @return The detected codec, or a nullptr if it could not be detected
	 */
	virtual TUniquePtr<FBaseRuntimeCodec> GetCodec(const FRuntimeBulkDataBuffer<uint8>& AudioData);
};
