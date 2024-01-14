// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAudioImporterTypes.h"
#include "PreImportedSoundAsset.generated.h"

/**
 * Pre-imported asset which collects MP3 audio data. Used if you want to load the MP3 file into the editor in advance
 */
UCLASS(BlueprintType, Category = "Pre Imported Sound Asset")
class RUNTIMEAUDIOIMPORTER_API UPreImportedSoundAsset : public UObject
{
	GENERATED_BODY()

public:
	UPreImportedSoundAsset();

	/** Audio data array */
	UPROPERTY()
	TArray<uint8> AudioDataArray;

	/** Audio data format */
	UPROPERTY(Category = "Info", VisibleAnywhere, Meta = (DisplayName = "Audio format"))
	ERuntimeAudioFormat AudioFormat;

	/** Information about the basic details of an audio file. Used only for convenience in the editor */
#if WITH_EDITORONLY_DATA
	UPROPERTY(Category = "File Path", VisibleAnywhere, Meta = (DisplayName = "Source file path"))
	FString SourceFilePath;

	UPROPERTY(Category = "Info", VisibleAnywhere, Meta = (DisplayName = "Sound duration"))
	FString SoundDuration;

	UPROPERTY(Category = "Info", VisibleAnywhere, Meta = (DisplayName = "Number of channels"))
	int32 NumberOfChannels;

	UPROPERTY(Category = "Info", VisibleAnywhere, Meta = (DisplayName = "Sample rate"))
	int32 SampleRate;
#endif
};
