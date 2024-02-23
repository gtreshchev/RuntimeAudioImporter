// Georgy Treshchev 2024.

#include "RuntimeAudioImporterEditor.h"

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "MetasoundDataReference.h"
#include "MetasoundEditorModule.h"
#endif

DEFINE_LOG_CATEGORY(LogRuntimeAudioImporterEditor);
#define LOCTEXT_NAMESPACE "FRuntimeAudioImporterEditorModule"

static const FString IOSSettingsSection = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
static const FString AdditionalPlistDataKey = TEXT("AdditionalPlistData");

void FRuntimeAudioImporterEditorModule::StartupModule()
{
#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
	using namespace Metasound;
	using namespace Metasound::Editor;
	IMetasoundEditorModule& MetaSoundEditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
	MetaSoundEditorModule.RegisterPinType("ImportedWave");
	MetaSoundEditorModule.RegisterPinType(CreateArrayTypeNameFromElementTypeName("ImportedWave"));
#endif

	// This is required for the iOS microphone access prompt to show up
	{
		FString AdditionalPlistData;
		GConfig->GetString(*IOSSettingsSection, *AdditionalPlistDataKey, AdditionalPlistData, GEngineIni);
		if (!AdditionalPlistData.Contains(TEXT("NSMicrophoneUsageDescription")))
		{
			AdditionalPlistData += TEXT("<key>NSMicrophoneUsageDescription</key><string>Microphone access for RuntimeAudioImporter recording</string>");
			GConfig->SetString(*IOSSettingsSection, *AdditionalPlistDataKey, *AdditionalPlistData, GEngineIni);
		}
	}
}

void FRuntimeAudioImporterEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterEditorModule, RuntimeAudioImporterEditor)
