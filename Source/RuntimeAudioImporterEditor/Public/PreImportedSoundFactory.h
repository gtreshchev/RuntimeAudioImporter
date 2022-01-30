// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

#include "Factories/Factory.h"
#include "PreImportedSoundFactory.generated.h"

/** Declaring custom logging */
DECLARE_LOG_CATEGORY_EXTERN(LogPreImportedSoundFactory, Error, All);

/**
 * Factory for pre-importing audio files. Only MP3 is available for pre-import because it takes up the least amount of memory.
 */
UCLASS()
class RUNTIMEAUDIOIMPORTEREDITOR_API UPreImportedSoundFactory : public UFactory
{
	GENERATED_BODY()

public:
	UPreImportedSoundFactory();

	/**
	 * Check whether the audio file can be imported.
	 *
	 * @param Filename The name of the importing audio file
	 * @note Only MP3 is available for pre-import because it takes up the least amount of memory
	 */
	virtual bool FactoryCanImport(const FString& Filename) override;

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

private:
	/**
	 * Convert seconds to string (hh:mm:ss or mm:ss depending on the number of seconds)
	 *
	 * @param Seconds Audio duration in seconds
	 */
	static FString ConvertSecondsToString(int32 Seconds);
};
