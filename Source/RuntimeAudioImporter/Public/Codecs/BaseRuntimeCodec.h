// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "RuntimeAudioImporterTypes.h"

/**
 * Base runtime codec
 * To add a new codec, derive from this class and implement the necessary functions
 * Then register the codec as a modular feature within your module, such as in the module's StartupModule function
 * See FRuntimeAudioImporterModule::StartupModule() as a reference
 */
class RUNTIMEAUDIOIMPORTER_API FBaseRuntimeCodec : public IModularFeature
{
public:
	FBaseRuntimeCodec() = default;
	virtual ~FBaseRuntimeCodec() = default;

	/**
	 * Check if the given audio data appears to be valid
	 */
	virtual bool CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData) PURE_VIRTUAL(FBaseRuntimeCodec::CheckAudioFormat, return false;)

	/**
	 * Retrieve audio header information from an encoded source
	 */
	virtual bool GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo) PURE_VIRTUAL(FBaseRuntimeCodec::GetHeaderInfo, return false;)

	/**
	 * Encode uncompressed PCM data into a compressed format
	 */
	virtual bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality) PURE_VIRTUAL(FBaseRuntimeCodec::Encode, return false;)
	/**
	 * Decode compressed audio data into PCM format
	 */
	virtual bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData) PURE_VIRTUAL(FBaseRuntimeCodec::Decode, return false;)

	/**
	 * Retrieve the format applicable to this codec
	 */
	virtual ERuntimeAudioFormat GetAudioFormat() const PURE_VIRTUAL(FBaseRuntimeCodec::GetAudioFormat, return ERuntimeAudioFormat::Invalid;)

	/**
	 * Check if the given extension is supported by this codec
	 */
	virtual bool IsExtensionSupported(const FString& Extension) const PURE_VIRTUAL(FBaseRuntimeCodec::IsExtensionSupported, return false;)
};
