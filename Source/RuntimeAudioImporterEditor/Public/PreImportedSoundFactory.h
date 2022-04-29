// Georgy Treshchev 2022.

#pragma once

#include "CoreMinimal.h"

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

#include "Factories/Factory.h"
#include "PreImportedSoundFactory.generated.h"

/** Declaring custom logging */
DECLARE_LOG_CATEGORY_EXTERN(LogPreImportedSoundFactory, Log, All);

/**
 * Factory for pre-importing audio files. Supports all formats from EAudioFormat, but OGG Vorbis is recommended due to its smaller size and better quality
 */
UCLASS()
class RUNTIMEAUDIOIMPORTEREDITOR_API UPreImportedSoundFactory : public UFactory
{
	GENERATED_BODY()

public:
	/** Default constructor */
	UPreImportedSoundFactory();

	//~ Begin UFactory Interface.
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	//~ end UFactory Interface.
};
