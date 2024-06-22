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
	/**
	 * Check and request permissions required for audio importing/exporting
	 */
	RUNTIMEAUDIOIMPORTER_API bool CheckAndRequestPermissions();

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
}