// Georgy Treshchev 2024.

#pragma once

#include "BaseRuntimeCodec.h"

/**
 * A factory for constructing the codecs used for encoding and decoding audio data
 * Codecs are intended to be registered as modular features
 */
class RUNTIMEAUDIOIMPORTER_API FRuntimeCodecFactory
{
public:
	FRuntimeCodecFactory() = default;
	virtual ~FRuntimeCodecFactory() = default;

	/**
	 * Get all available codecs
	 * 
	 * @return An array of all available codecs 
	 */
	virtual TArray<FBaseRuntimeCodec*> GetCodecs();

	/**
	 * Get the codec based on the file path extension
	 *
	 * @param FilePath The file path from which to get the codec
	 * @return The detected codec, or a nullptr if it could not be detected
	 */
	virtual TArray<FBaseRuntimeCodec*> GetCodecs(const FString& FilePath);

	/**
	 * Get the codec based on the audio format
	 *
	 * @param AudioFormat The format from which to get the codec
	 * @return The detected codec, or a nullptr if it could not be detected
	 */
	virtual TArray<FBaseRuntimeCodec*> GetCodecs(ERuntimeAudioFormat AudioFormat);

	/**
	 * Get the codec based on the audio data (slower, but more reliable)
	 *
	 * @param AudioData The audio data from which to get the codec
	 * @return The detected codec, or a nullptr if it could not be detected
	 */
	virtual TArray<FBaseRuntimeCodec*> GetCodecs(const FRuntimeBulkDataBuffer<uint8>& AudioData);

	/**
	 * Get the name of the modular feature
	 * This name should be used when registering the codec as a modular feature
	 *
	 * @return The name of the modular feature
	 */
	static FName GetModularFeatureName()
	{
		static FName CodecFeatureName = FName(TEXT("RuntimeAudioImporterCodec"));
		return CodecFeatureName;
	}
};
