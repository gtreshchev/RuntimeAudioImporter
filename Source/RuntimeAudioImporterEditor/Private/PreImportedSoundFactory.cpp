// Georgy Treshchev 2022.

#include "PreImportedSoundFactory.h"
#include "PreImportedSoundAsset.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY(LogPreImportedSoundFactory);

#include "RuntimeAudioImporterLibrary.h"

UPreImportedSoundFactory::UPreImportedSoundFactory()
{
	Formats.Add(TEXT("imp;IMP Pre-imported Audio Format"));
	SupportedClass = StaticClass();
	bCreateNew = false; // turned off for import
	bEditAfterNew = false; // turned off for import
	bEditorImport = true;
	bText = false;
}

bool UPreImportedSoundFactory::FactoryCanImport(const FString& Filename)
{
	const FString FileExtension{FPaths::GetExtension(Filename).ToLower()};
	return FileExtension == TEXT("imp") || URuntimeAudioImporterLibrary::GetAudioFormat(Filename) != EAudioFormat::Invalid;
}

UObject* UPreImportedSoundFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UPreImportedSoundAsset* PreImportedSoundAsset;

	TArray<uint8> AudioDataArray;

	if (FFileHelper::LoadFileToArray(AudioDataArray, *Filename))
	{
		FDecodedAudioStruct DecodedAudioInfo;

		uint8* EncodedAudioDataPtr = static_cast<uint8*>(FMemory::Memcpy(FMemory::Malloc(AudioDataArray.Num() - 2), AudioDataArray.GetData(), AudioDataArray.Num() - 2));
		FEncodedAudioStruct EncodedAudioInfo = FEncodedAudioStruct(EncodedAudioDataPtr, AudioDataArray.Num() - 2, EAudioFormat::Auto);

		if (!URuntimeAudioImporterLibrary::DecodeAudioData(EncodedAudioInfo, DecodedAudioInfo))
		{
			UE_LOG(LogPreImportedSoundFactory, Error, TEXT("Unable to decode the audio file '%s'"), *Filename);
			return nullptr;
		}

		PreImportedSoundAsset = NewObject<UPreImportedSoundAsset>(InParent, UPreImportedSoundAsset::StaticClass(), InName, Flags);
		PreImportedSoundAsset->AudioDataArray = AudioDataArray;
		PreImportedSoundAsset->AudioFormat = EncodedAudioInfo.AudioFormat;
		PreImportedSoundAsset->SourceFilePath = Filename;

		PreImportedSoundAsset->SoundDuration = URuntimeAudioImporterLibrary::ConvertSecondsToString(DecodedAudioInfo.SoundWaveBasicInfo.Duration);
		PreImportedSoundAsset->NumberOfChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
		PreImportedSoundAsset->SampleRate = DecodedAudioInfo.SoundWaveBasicInfo.SampleRate;
	}
	else
	{
		UE_LOG(LogPreImportedSoundFactory, Error, TEXT("Unable to read the audio file '%s'. Check file permissions"), *Filename);
		return nullptr;
	}

	bOutOperationCanceled = false;

	return PreImportedSoundAsset;
}
