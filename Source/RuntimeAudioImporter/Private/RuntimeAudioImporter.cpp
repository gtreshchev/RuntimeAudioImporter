// Georgy Treshchev 2023.

#include "RuntimeAudioImporter.h"
#include "RuntimeAudioImporterDefines.h"

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
}

void FRuntimeAudioImporterModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterModule, RuntimeAudioImporter)

DEFINE_LOG_CATEGORY(LogRuntimeAudioImporter);
