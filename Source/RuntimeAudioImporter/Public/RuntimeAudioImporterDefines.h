﻿// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRuntimeAudioImporter, Log, All);

/**
 * In some transcoders, it is not possible to directly call UE_LOG due to some variable intersections, which is why this wrapper is needed
 */
namespace RuntimeAudioImporter_TranscoderLogs
{
	static void PrintLog(const FString& LogString)
	{
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("%s"), *LogString);
	}
	
	static void PrintWarning(const FString& WarningString)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("%s"), *WarningString);
	}

	static void PrintError(const FString& ErrorString)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("%s"), *ErrorString);
	}
}