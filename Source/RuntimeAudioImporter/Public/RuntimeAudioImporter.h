// Georgy Treshchev 2024.

#pragma once

#include "Modules/ModuleManager.h"

class FMP3_RuntimeCodec;
class FWAV_RuntimeCodec;
class FFLAC_RuntimeCodec;
class FVORBIS_RuntimeCodec;
class FBINK_RuntimeCodec;

class FRuntimeAudioImporterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	// Supported default codecs
	TSharedPtr<FMP3_RuntimeCodec> MP3_Codec;
	TSharedPtr<FWAV_RuntimeCodec> WAV_Codec;
	TSharedPtr<FFLAC_RuntimeCodec> FLAC_Codec;
	TSharedPtr<FVORBIS_RuntimeCodec> VORBIS_Codec;
	TSharedPtr<FBINK_RuntimeCodec> BINK_Codec;
};
