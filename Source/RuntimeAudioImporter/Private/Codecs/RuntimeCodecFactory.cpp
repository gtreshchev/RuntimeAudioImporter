// Georgy Treshchev 2023.

#include "Codecs/RuntimeCodecFactory.h"
#include "Codecs/BaseRuntimeCodec.h"

#include "Codecs/MP3_RuntimeCodec.h"
#include "Codecs/WAV_RuntimeCodec.h"
#include "Codecs/FLAC_RuntimeCodec.h"
#include "Codecs/VORBIS_RuntimeCodec.h"
#include "Codecs/BINK_RuntimeCodec.h"

#include "Misc/Paths.h"

#include "RuntimeAudioImporterDefines.h"

TUniquePtr<FBaseRuntimeCodec> FRuntimeCodecFactory::GetCodec(const FString& FilePath)
{
	const FString Extension = FPaths::GetExtension(FilePath, false).ToLower();

	if (Extension == TEXT("mp3"))
	{
		return MakeUnique<FMP3_RuntimeCodec>();
	}
	if (Extension == TEXT("wav") || Extension == TEXT("wave"))
	{
		return MakeUnique<FWAV_RuntimeCodec>();
	}
	if (Extension == TEXT("flac"))
	{
		return MakeUnique<FFLAC_RuntimeCodec>();
	}
	if (Extension == TEXT("ogg") || Extension == TEXT("oga") || Extension == TEXT("sb0"))
	{
		return MakeUnique<FVORBIS_RuntimeCodec>();
	}
	if (Extension == TEXT("bink") || Extension == TEXT("binka") || Extension == TEXT("bnk"))
	{
		return MakeUnique<FBINK_RuntimeCodec>();
	}

	UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Failed to determine the audio codec for '%s' using its file name"), *FilePath);
	return nullptr;
}

TUniquePtr<FBaseRuntimeCodec> FRuntimeCodecFactory::GetCodec(ERuntimeAudioFormat AudioFormat)
{
	switch (AudioFormat)
	{
	case ERuntimeAudioFormat::Mp3:
		return MakeUnique<FMP3_RuntimeCodec>();
	case ERuntimeAudioFormat::Wav:
		return MakeUnique<FWAV_RuntimeCodec>();
	case ERuntimeAudioFormat::Flac:
		return MakeUnique<FFLAC_RuntimeCodec>();
	case ERuntimeAudioFormat::OggVorbis:
		return MakeUnique<FVORBIS_RuntimeCodec>();
	case ERuntimeAudioFormat::Bink:
		return MakeUnique<FBINK_RuntimeCodec>();
	default:
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to determine the audio codec for the %s format"), *UEnum::GetValueAsString(AudioFormat));
		return nullptr;
	}
}

TUniquePtr<FBaseRuntimeCodec> FRuntimeCodecFactory::GetCodec(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	TUniquePtr<FBaseRuntimeCodec> MP3_Codec = MakeUnique<FMP3_RuntimeCodec>();
	if (MP3_Codec->CheckAudioFormat(AudioData))
	{
		return MoveTemp(MP3_Codec);
	}

	TUniquePtr<FBaseRuntimeCodec> FLAC_Codec = MakeUnique<FFLAC_RuntimeCodec>();
	if (FLAC_Codec->CheckAudioFormat(AudioData))
	{
		return MoveTemp(FLAC_Codec);
	}

	TUniquePtr<FBaseRuntimeCodec> VORBIS_Codec = MakeUnique<FVORBIS_RuntimeCodec>();
	if (VORBIS_Codec->CheckAudioFormat(AudioData))
	{
		return MoveTemp(VORBIS_Codec);
	}

	TUniquePtr<FBaseRuntimeCodec> BINK_Codec = MakeUnique<FBINK_RuntimeCodec>();
	if (BINK_Codec->CheckAudioFormat(AudioData))
	{
		return MoveTemp(BINK_Codec);
	}

	TUniquePtr<FBaseRuntimeCodec> WAV_Codec = MakeUnique<FWAV_RuntimeCodec>();
	if (WAV_Codec->CheckAudioFormat(AudioData))
	{
		return MoveTemp(WAV_Codec);
	}

	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to determine the audio codec based on the audio data of size %lld bytes"), static_cast<int64>(AudioData.GetView().Num()));
	return nullptr;
}
