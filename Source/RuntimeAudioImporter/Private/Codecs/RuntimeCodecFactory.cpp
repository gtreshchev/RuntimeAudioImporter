// Georgy Treshchev 2024.

#include "Codecs/RuntimeCodecFactory.h"
#include "Codecs/BaseRuntimeCodec.h"
#include "Features/IModularFeatures.h"
#include "Misc/Paths.h"

#include "RuntimeAudioImporterDefines.h"

TArray<FBaseRuntimeCodec*> FRuntimeCodecFactory::GetCodecs()
{
#if UE_VERSION_NEWER_THAN(5, 1, 0)
	IModularFeatures::FScopedLockModularFeatureList ScopedLockModularFeatureList;
#endif
	TArray<FBaseRuntimeCodec*> AvailableCodecs = IModularFeatures::Get().GetModularFeatureImplementations<FBaseRuntimeCodec>(GetModularFeatureName());
	return AvailableCodecs;
}

TArray<FBaseRuntimeCodec*> FRuntimeCodecFactory::GetCodecs(const FString& FilePath)
{
	TArray<FBaseRuntimeCodec*> Codecs;
	const FString Extension = FPaths::GetExtension(FilePath, false);

	for (FBaseRuntimeCodec* Codec : GetCodecs())
	{
		if (Codec->IsExtensionSupported(Extension))
		{
			Codecs.Add(Codec);
		}
	}

	if (Codecs.Num() == 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Failed to determine the audio codec for '%s' using its file name"), *FilePath);
	}

	return Codecs;
}

TArray<FBaseRuntimeCodec*> FRuntimeCodecFactory::GetCodecs(ERuntimeAudioFormat AudioFormat)
{
	TArray<FBaseRuntimeCodec*> Codecs;
	for (FBaseRuntimeCodec* Codec : GetCodecs())
	{
		if (Codec->GetAudioFormat() == AudioFormat)
		{
			Codecs.Add(Codec);
		}
	}

	if (Codecs.Num() == 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to determine the audio codec for the %s format"), *UEnum::GetValueAsString(AudioFormat));
	}

	return Codecs;
}

TArray<FBaseRuntimeCodec*> FRuntimeCodecFactory::GetCodecs(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	TArray<FBaseRuntimeCodec*> Codecs;
	for (FBaseRuntimeCodec* Codec : GetCodecs())
	{
		if (Codec->CheckAudioFormat(AudioData))
		{
			Codecs.Add(Codec);
		}
	}

	if (Codecs.Num() == 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to determine the audio codec based on the audio data of size %lld bytes"), static_cast<int64>(AudioData.GetView().Num()));
	}

	return Codecs;
}
