// Georgy Treshchev 2023.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAudioImporterTypes.h"

// TODO: Make FBaseRuntimeCodec an abstract class (currently not possible due to TUniquePtr requiring a non-abstract base class)

/**
 * Base runtime codec
 */
class RUNTIMEAUDIOIMPORTER_API FBaseRuntimeCodec
{
public:
	FBaseRuntimeCodec() = default;
	virtual ~FBaseRuntimeCodec() = default;

	/**
	 * Check if the given audio data appears to be valid
	 */
	virtual bool CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData)
	{
		ensureMsgf(false, TEXT("CheckAudioFormat cannot be called from base runtime codec"));
		return false;
	}

	/**
	 * Retrieve audio header information from an encoded source
	 */
	virtual bool GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo)
	{
		ensureMsgf(false, TEXT("GetHeaderInfo cannot be called from base runtime codec"));
		return false;
	}

	/**
	 * Encode uncompressed PCM data into a compressed format
	 */
	virtual bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality)
	{
		ensureMsgf(false, TEXT("Encode cannot be called from base runtime codec"));
		return false;
	}

	/**
	 * Decode compressed audio data into PCM format
	 */
	virtual bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData)
	{
		ensureMsgf(false, TEXT("Decode cannot be called from base runtime codec"));
		return false;
	}

	/**
	 * Retrieve the format applicable to this codec
	 */
	virtual ERuntimeAudioFormat GetAudioFormat() const
	{
		ensureMsgf(false, TEXT("GetAudioFormat cannot be called from base runtime codec"));
		return ERuntimeAudioFormat::Invalid;
	}
};
