// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "BaseRuntimeCodec.h"

class RUNTIMEAUDIOIMPORTER_API FMP3_RuntimeCodec : public FBaseRuntimeCodec
{
public:
	//~ Begin FBaseRuntimeCodec Interface
	virtual bool CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData) override;
	virtual bool GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo) override;
	virtual bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality) override;
	virtual bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData) override;
	virtual ERuntimeAudioFormat GetAudioFormat() const override { return ERuntimeAudioFormat::Mp3; }
	virtual bool IsExtensionSupported(const FString& Extension) const override
	{
		return Extension.Equals(TEXT("mp3"), ESearchCase::IgnoreCase)
		|| Extension.Equals(TEXT("mpeg"), ESearchCase::IgnoreCase)
		|| Extension.Equals(TEXT("mpga"), ESearchCase::IgnoreCase)
		|| Extension.Equals(TEXT("mpg"), ESearchCase::IgnoreCase);
	}
	//~ End FBaseRuntimeCodec Interface
};
