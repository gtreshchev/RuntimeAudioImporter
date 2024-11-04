// Georgy Treshchev 2024.

#include "RuntimeAudioImporterEditor.h"

#include "Misc/EngineVersionComparison.h"

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT && !PLATFORM_LINUX
#if PLATFORM_APPLE
//#include "IOSRuntimeSettings.h"
#else
#include "AndroidRuntimeSettings.h"
#endif
#endif

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

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT && !PLATFORM_LINUX && PLATFORM_APPLE
	// This is required for the iOS microphone access prompt to show up

	FString AdditionalPlistData;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("AdditionalPlistData"), AdditionalPlistData, GEngineIni);
	if (!AdditionalPlistData.Contains(TEXT("NSMicrophoneUsageDescription")))
	{
		AdditionalPlistData += TEXT("<key>NSMicrophoneUsageDescription</key><string>Microphone access for RuntimeAudioImporter recording</string>");
		GConfig->SetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("AdditionalPlistData"), *AdditionalPlistData, GEngineIni);
	}

	// Commented out because UIOSRuntimeSettings is not available when building for Mac, but only for iOS, so use GConfig instead
	/*{
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
	}*/
#endif

#if (WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT || WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT) && !PLATFORM_LINUX && !PLATFORM_APPLE
	// This is required to access the external storage and record audio on Android
	{
		UAndroidRuntimeSettings* AndroidRuntimeSettings = GetMutableDefault<UAndroidRuntimeSettings>();
		if (!AndroidRuntimeSettings)
		{
			UE_LOG(LogRuntimeAudioImporterEditor, Error, TEXT("Failed to get AndroidRuntimeSettings to add permissions"));
			return;
		}

		TArray<FString> NewExtraPermissions = AndroidRuntimeSettings->ExtraPermissions;

#if WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT
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
#endif
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
		if (!AndroidRuntimeSettings->ExtraPermissions.Contains(AndroidRecordAudioPermission))
		{
			NewExtraPermissions.Add(AndroidRecordAudioPermission);
		}
#endif
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
#endif
}

void FRuntimeAudioImporterEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeAudioImporterEditorModule, RuntimeAudioImporterEditor)
