// Georgy Treshchev 2022.

#include "RuntimeAudioImporter.h"
#include "RuntimeAudioImporterDefines.h"

#define LOCTEXT_NAMESPACE "FRuntimeAudioImporterModule"

void FRuntimeAudioImporterModule::StartupModule()
{
}

void FRuntimeAudioImporterModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterModule, RuntimeAudioImporter)

DEFINE_LOG_CATEGORY(LogRuntimeAudioImporter);
