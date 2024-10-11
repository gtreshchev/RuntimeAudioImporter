// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRuntimeAudioImporter, Log, All);

namespace RuntimeAudioImporter
{
#if WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT
	/**
	 * Check and request permissions required for audio importing/exporting
	 * @param AllRequiredPermissions List of all required permissions. If no permissions are provided, only those necessary for the RuntimeAudioImporter will be requested
	 */
	RUNTIMEAUDIOIMPORTER_API bool CheckAndRequestPermissions(TArray<FString> AllRequiredPermissions = TArray<FString>());

	/**
	 * Load audio file to TArray64<uint8>
	 */
	RUNTIMEAUDIOIMPORTER_API bool LoadAudioFileToArray(TArray64<uint8>& AudioData, const FString& FilePath);

	/**
	 * Save audio file from array (Perfect forwarding to FFileHelper::SaveArrayToFile)
	 */
	template<typename T>
	static bool SaveAudioFileFromArray(T&& AudioData, const FString& FilePath)
	{
		CheckAndRequestPermissions();
		return FFileHelper::SaveArrayToFile(Forward<T>(AudioData), *FilePath);
	}
#endif
}