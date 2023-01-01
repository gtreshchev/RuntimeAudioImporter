// Georgy Treshchev 2023.

#include "RuntimeAudioImporterEditor.h"
#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "MetasoundDataReference.h"
#include "MetasoundEditorModule.h"
#endif

#define LOCTEXT_NAMESPACE "FRuntimeAudioImporterEditorModule"

void FRuntimeAudioImporterEditorModule::StartupModule()
{
#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
	using namespace Metasound;
	using namespace Metasound::Editor;
	IMetasoundEditorModule& MetaSoundEditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
	MetaSoundEditorModule.RegisterPinType("ImportedWave");
	MetaSoundEditorModule.RegisterPinType(CreateArrayTypeNameFromElementTypeName("ImportedWave"));
#endif
}

void FRuntimeAudioImporterEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterEditorModule, RuntimeAudioImporterEditor)
