// Georgy Treshchev 2024.

#include "RuntimeAudioImporter.h"
#include "RuntimeAudioImporterDefines.h"
#include "Codecs/RuntimeCodecFactory.h"
#include "Features/IModularFeatures.h"

#include "Codecs/MP3_RuntimeCodec.h"
#include "Codecs/WAV_RuntimeCodec.h"
#include "Codecs/FLAC_RuntimeCodec.h"
#include "Codecs/VORBIS_RuntimeCodec.h"
#include "Codecs/BINK_RuntimeCodec.h"

#define LOCTEXT_NAMESPACE "FRuntimeAudioImporterModule"

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundWave.h"
#include "MetaSound/MetasoundImportedWave.h"
#include "Sound/ImportedSoundWave.h"

REGISTER_METASOUND_DATATYPE(RuntimeAudioImporter::FImportedWave, "ImportedWave", Metasound::ELiteralType::UObjectProxy, UImportedSoundWave);
#endif

void FRuntimeAudioImporterModule::StartupModule()
{
#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
	FModuleManager::Get().LoadModuleChecked("MetasoundEngine");
	FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
#endif

	MP3_Codec = MakeShared<FMP3_RuntimeCodec>();
	WAV_Codec = MakeShared<FWAV_RuntimeCodec>();
	FLAC_Codec = MakeShared<FFLAC_RuntimeCodec>();
	VORBIS_Codec = MakeShared<FVORBIS_RuntimeCodec>();
	BINK_Codec = MakeShared<FBINK_RuntimeCodec>();
	
	// Register codecs with the modular feature system
	IModularFeatures::Get().RegisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), MP3_Codec.Get());
	IModularFeatures::Get().RegisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), WAV_Codec.Get());
	IModularFeatures::Get().RegisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), FLAC_Codec.Get());
	IModularFeatures::Get().RegisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), VORBIS_Codec.Get());
	IModularFeatures::Get().RegisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), BINK_Codec.Get());
}

void FRuntimeAudioImporterModule::ShutdownModule()
{
	// Unregister codecs with the modular feature system
	IModularFeatures::Get().UnregisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), MP3_Codec.Get());
	IModularFeatures::Get().UnregisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), WAV_Codec.Get());
	IModularFeatures::Get().UnregisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), FLAC_Codec.Get());
	IModularFeatures::Get().UnregisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), VORBIS_Codec.Get());
	IModularFeatures::Get().UnregisterModularFeature(FRuntimeCodecFactory::GetModularFeatureName(), BINK_Codec.Get());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterModule, RuntimeAudioImporter)

DEFINE_LOG_CATEGORY(LogRuntimeAudioImporter);
