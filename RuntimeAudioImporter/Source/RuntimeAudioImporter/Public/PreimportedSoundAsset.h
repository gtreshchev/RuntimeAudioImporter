// Georgy Treshchev 2021.

#pragma once

#include "CoreMinimal.h"
#include "PreimportedSoundAsset.generated.h"

/**
 * Pre-imported asset which collects MP3 audio data. Used if you want to load the MP3 file into the editor in advance
 */
UCLASS(BlueprintType, Category = "RuntimeAudioImporter")
class RUNTIMEAUDIOIMPORTER_API UPreimportedSoundAsset : public UObject
{
	GENERATED_BODY()
public:

	/** Audio data array. Only MP3 data is intended to be here */
	UPROPERTY()
	TArray<uint8> AudioDataArray;

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
