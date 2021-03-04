// Georgy Treshchev 2021.

#include "PreimportedSoundFactory.h"

#include "PreimportedSoundAsset.h" //maybe #include "RuntimeAudioImporter/Public/PreimportedSoundAsset.h"
#include "Misc/FileHelper.h"

/** Defining custom logging */
DEFINE_LOG_CATEGORY(LogPreimportedSoundFactory)

#include "ThirdParty/dr_mp3.h"

/**
* Replacing standard CPP memory functions (malloc, realloc, free) with memory management functions that are maintained by Epic as the engine evolves.
*/
void* Unreal_Malloc(size_t sz, void* pUserData)
{
	return FMemory::Malloc(sz);
}

void* Unreal_Realloc(void* p, size_t sz, void* pUserData)
{
	return FMemory::Realloc(p, sz);
}

void Unreal_Free(void* p, void* pUserData)
{
	FMemory::Free(p);
}

UPreimportedSoundFactory::UPreimportedSoundFactory()
{
	Formats.Add(TEXT("mp3;MP3 Preimported Audio Format"));
	Formats.Add(TEXT("imp;IMP (the same as MP3) Preimported Audio Format"));
	SupportedClass = UPreimportedSoundAsset::StaticClass();
	bCreateNew = false; // turned off for import
	bEditAfterNew = false; // turned off for import
	bEditorImport = true;
	bText = false;
}


bool UPreimportedSoundFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename).Equals(TEXT("mp3")) || FPaths::GetExtension(Filename).Equals(TEXT("imp"));
}

UObject* UPreimportedSoundFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName,
                                                     EObjectFlags Flags, const FString& Filename, const TCHAR* Parms,
                                                     FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UPreimportedSoundAsset* PreimportedSoundAsset = nullptr;

	TArray<uint8> AudioDataArray;

	if (FFileHelper::LoadFileToArray(AudioDataArray, *Filename))
	{
		drmp3_allocation_callbacks allocationCallbacksDecoding;
		allocationCallbacksDecoding.pUserData = nullptr;
		allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
		allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
		allocationCallbacksDecoding.onFree = Unreal_Free;

		drmp3 mp3;

		if (!drmp3_init_memory(&mp3, AudioDataArray.GetData(), AudioDataArray.Num() - 2, &allocationCallbacksDecoding))
		{
			UE_LOG(LogPreimportedSoundFactory, Log, TEXT("The file you are trying to import is not an MP3 file."));
			return nullptr;
		}

		PreimportedSoundAsset = NewObject<UPreimportedSoundAsset>(InParent, InClass, InName, Flags);
		PreimportedSoundAsset->AudioDataArray = AudioDataArray;
		PreimportedSoundAsset->SourceFilePath = Filename;

		PreimportedSoundAsset->SoundDuration = ConvertSecToFormattedDuration(
			static_cast<int32>(drmp3_get_pcm_frame_count(&mp3)) / mp3.sampleRate);
		PreimportedSoundAsset->NumberOfChannels = mp3.channels;
		PreimportedSoundAsset->SampleRate = mp3.sampleRate;

		drmp3_uninit(&mp3);
	}
	else
	{
		UE_LOG(LogPreimportedSoundFactory, Log, TEXT("Unable to read the audio file. Check file permissions."));
		return nullptr;
	}

	bOutOperationCanceled = false;

	return PreimportedSoundAsset;
}

FString UPreimportedSoundFactory::ConvertSecToFormattedDuration(int32 Seconds)
{
	FString FinalString;

	const int32 NewHours = Seconds / 3600;
	if (NewHours > 0)
	{
		FinalString += ((NewHours < 10) ? TEXT("0") + FString::FromInt(NewHours) : FString::FromInt(NewHours)) +
			TEXT(":");
	}

	Seconds = Seconds % 3600;

	const int32 NewMinutes = Seconds / 60;
	if (NewMinutes > 0)
	{
		FinalString += ((NewMinutes < 10) ? TEXT("0") + FString::FromInt(NewMinutes) : FString::FromInt(NewMinutes)) +
			TEXT(":");
	}

	const int32 NewSeconds = Seconds % 60;
	if (NewSeconds > 0)
	{
		FinalString += (NewSeconds < 10) ? TEXT("0") + FString::FromInt(NewSeconds) : FString::FromInt(NewSeconds);
	}

	return FinalString;
}
