// Georgy Treshchev 2022.

#include "RuntimeAudioImporterEditor.h"

#define LOCTEXT_NAMESPACE "FRuntimeAudioImporterEditorModule"

void FRuntimeAudioImporterEditorModule::StartupModule()
{
}

void FRuntimeAudioImporterEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogPreImportedSoundFactory);

IMPLEMENT_MODULE(FRuntimeAudioImporterEditorModule, RuntimeAudioImporterEditor)
