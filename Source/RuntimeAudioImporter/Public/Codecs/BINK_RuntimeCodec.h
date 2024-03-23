// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "BaseRuntimeCodec.h"

class RUNTIMEAUDIOIMPORTER_API FBINK_RuntimeCodec : public FBaseRuntimeCodec
{
public:
	//~ Begin FBaseRuntimeCodec Interface
	virtual bool CheckAudioFormat(const FRuntimeBulkDataBuffer<uint8>& AudioData) override;
	virtual bool GetHeaderInfo(FEncodedAudioStruct EncodedData, FRuntimeAudioHeaderInfo& HeaderInfo) override;
	virtual bool Encode(FDecodedAudioStruct DecodedData, FEncodedAudioStruct& EncodedData, uint8 Quality) override;
	virtual bool Decode(FEncodedAudioStruct EncodedData, FDecodedAudioStruct& DecodedData) override;
	virtual ERuntimeAudioFormat GetAudioFormat() const override { return ERuntimeAudioFormat::Bink; }
	virtual bool IsExtensionSupported(const FString& Extension) const override
	{
		return Extension.Equals(TEXT("bik"), ESearchCase::IgnoreCase)
		|| Extension.Equals(TEXT("bk2"), ESearchCase::IgnoreCase)
		|| Extension.Equals(TEXT("bink"), ESearchCase::IgnoreCase)
		|| Extension.Equals(TEXT("binka"), ESearchCase::IgnoreCase);
	}
	//~ End FBaseRuntimeCodec Interface
};