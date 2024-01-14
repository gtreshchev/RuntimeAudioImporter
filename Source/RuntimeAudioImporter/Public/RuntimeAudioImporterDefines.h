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
	static bool LoadAudioFileToArray(TArray64<uint8>& AudioData, const FString& FilePath)
	{
#if UE_VERSION_OLDER_THAN(4, 26, 0)
		TArray<uint8> AudioData32;
		if (!FFileHelper::LoadFileToArray(AudioData32, *FilePath))
		{
			return false;
		}
		AudioData = AudioData32;
#else
		if (!FFileHelper::LoadFileToArray(AudioData, *FilePath))
		{
			return false;
		}
#endif

		// Removing unused two uninitialized bytes
		AudioData.RemoveAt(AudioData.Num() - 2, 2);

		return true;
	}
}