// Georgy Treshchev 2021.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "PreimportedSoundFactory.generated.h"

/** Declaring custom logging */
DECLARE_LOG_CATEGORY_EXTERN(LogPreimportedSoundFactory, Error, All);

/**
 * Factory for pre-importing audio files. Only MP3 is available for pre-import because it takes up the least amount of memory.
 */
UCLASS()
class RUNTIMEAUDIOIMPORTEREDITOR_API UPreimportedSoundFactory : public UFactory
{
	GENERATED_BODY()

public:

	UPreimportedSoundFactory();

	/**
	 * Check whether the audio file can be imported.
	 *
	 * @param Filename The name of the importing audio file
	 * @note Only MP3 is available for pre-import because it takes up the least amount of memory.
	 */
	virtual bool FactoryCanImport(const FString& Filename) override;

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	                                   const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	                                   bool& bOutOperationCanceled) override;

private:

	/**
	 * Convert sound duration from seconds to formatted string (hh:mm:ss)
	 *
	 * @param Seconds Audio duration in seconds
	 */
	static FString ConvertSecToFormattedDuration(int32 Seconds);
};
