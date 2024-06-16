// Georgy Treshchev 2024.

#include "RuntimeAudioImporterEditor.h"

#include "Misc/EngineVersionComparison.h"

#include "AndroidRuntimeSettings.h"
#include "IOSRuntimeSettings.h"

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "MetasoundDataReference.h"
#include "MetasoundEditorModule.h"
#endif

DEFINE_LOG_CATEGORY(LogRuntimeAudioImporterEditor);
#define LOCTEXT_NAMESPACE "FRuntimeAudioImporterEditorModule"

static const FString AndroidReadExternalStoragePermission = TEXT("android.permission.READ_EXTERNAL_STORAGE");
static const FString AndroidWriteExternalStoragePermission = TEXT("android.permission.WRITE_EXTERNAL_STORAGE");
static const FString AndroidReadMediaAudioPermission = TEXT("android.permission.READ_MEDIA_AUDIO");
static const FString AndroidRecordAudioPermission = TEXT("android.permission.RECORD_AUDIO");

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
		UIOSRuntimeSettings* IOSRuntimeSettings = GetMutableDefault<UIOSRuntimeSettings>();
		if (!IOSRuntimeSettings)
		{
			UE_LOG(LogRuntimeAudioImporterEditor, Error, TEXT("Failed to get IOSRuntimeSettings to add permissions"));
			return;
		}

		if (!IOSRuntimeSettings->AdditionalPlistData.Contains(TEXT("NSMicrophoneUsageDescription")))
		{
			IOSRuntimeSettings->AdditionalPlistData += TEXT("<key>NSMicrophoneUsageDescription</key><string>Microphone access for RuntimeAudioImporter recording</string>");
#if UE_VERSION_OLDER_THAN(5, 0, 0)
			IOSRuntimeSettings->UpdateDefaultConfigFile();
#else
			IOSRuntimeSettings->TryUpdateDefaultConfigFile();
#endif
		}
		else
		{
			UE_LOG(LogRuntimeAudioImporterEditor, Log, TEXT("NSMicrophoneUsageDescription already exists in AdditionalPlistData"));
		}
	}

	// This is required to access the external storage and record audio on Android
	{
		UAndroidRuntimeSettings* AndroidRuntimeSettings = GetMutableDefault<UAndroidRuntimeSettings>();
		if (!AndroidRuntimeSettings)
		{
			UE_LOG(LogRuntimeAudioImporterEditor, Error, TEXT("Failed to get AndroidRuntimeSettings to add permissions"));
			return;
		}

		TArray<FString> NewExtraPermissions = AndroidRuntimeSettings->ExtraPermissions;

		if (!AndroidRuntimeSettings->ExtraPermissions.Contains(AndroidReadExternalStoragePermission))
		{
			NewExtraPermissions.Add(AndroidReadExternalStoragePermission);
		}
		if (!AndroidRuntimeSettings->ExtraPermissions.Contains(AndroidWriteExternalStoragePermission))
		{
			NewExtraPermissions.Add(AndroidWriteExternalStoragePermission);
		}
		if (!AndroidRuntimeSettings->ExtraPermissions.Contains(AndroidReadMediaAudioPermission))
		{
			NewExtraPermissions.Add(AndroidReadMediaAudioPermission);
		}
		if (!AndroidRuntimeSettings->ExtraPermissions.Contains(AndroidRecordAudioPermission))
		{
			NewExtraPermissions.Add(AndroidRecordAudioPermission);
		}
		if (NewExtraPermissions.Num() > AndroidRuntimeSettings->ExtraPermissions.Num())
		{
			AndroidRuntimeSettings->ExtraPermissions = NewExtraPermissions;
#if UE_VERSION_OLDER_THAN(5, 0, 0)
			AndroidRuntimeSettings->UpdateDefaultConfigFile();
#else
			AndroidRuntimeSettings->TryUpdateDefaultConfigFile();
#endif
		}
	}
}

void FRuntimeAudioImporterEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterEditorModule, RuntimeAudioImporterEditor)
