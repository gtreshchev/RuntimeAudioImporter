// Georgy Treshchev 2023.

#include "PreImportedSoundFactory.h"
#include "PreImportedSoundAsset.h"
#include "Misc/FileHelper.h"
#include "RuntimeAudioImporterLibrary.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "PreImportedSoundFactory"
DEFINE_LOG_CATEGORY(LogPreImportedSoundFactory);

UPreImportedSoundFactory::UPreImportedSoundFactory()
{
	Formats.Add(TEXT("imp;Runtime Audio Importer any supported format (mp3, wav, flac and ogg)"));

	// Removed for consistency with non-RuntimeAudioImporter modules
	/*
	Formats.Add(TEXT("mp3;MPEG-2 Audio"));
	Formats.Add(TEXT("wav;Wave Audio File"));
	Formats.Add(TEXT("flac;Free Lossless Audio Codec"));
	Formats.Add(TEXT("ogg;OGG Vorbis bitstream format"));
	*/

	SupportedClass = StaticClass();
	bCreateNew = false; // turned off for import
	bEditAfterNew = false; // turned off for import
	bEditorImport = true;
	bText = false;
}

bool UPreImportedSoundFactory::FactoryCanImport(const FString& Filename)
{
	const FString FileExtension{FPaths::GetExtension(Filename).ToLower()};
	return FileExtension == TEXT("imp") || URuntimeAudioImporterLibrary::GetAudioFormat(Filename) != ERuntimeAudioFormat::Invalid;
}

UObject* UPreImportedSoundFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	TArray<uint8> AudioData;

	if (!FFileHelper::LoadFileToArray(AudioData, *Filename))
	{
		FMessageLog("Import").Error(FText::Format(LOCTEXT("PreImportedSoundFactory_ReadError", "Unable to read the audio file '{0}'. Check file permissions'"), FText::FromString(Filename)));
		return nullptr;
	}

	// Removing unused two uninitialized bytes
	AudioData.RemoveAt(AudioData.Num() - 2, 2);

	FDecodedAudioStruct DecodedAudioInfo;
	FEncodedAudioStruct EncodedAudioInfo = FEncodedAudioStruct(AudioData, ERuntimeAudioFormat::Auto);

	if (!URuntimeAudioImporterLibrary::DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
	{
		FMessageLog("Import").Error(FText::Format(LOCTEXT("PreImportedSoundFactory_DecodeError", "Unable to decode the audio file '{0}'. Make sure the file is not corrupted'"), FText::FromString(Filename)));
		return nullptr;
	}

	UPreImportedSoundAsset* PreImportedSoundAsset = NewObject<UPreImportedSoundAsset>(InParent, UPreImportedSoundAsset::StaticClass(), InName, Flags);
	PreImportedSoundAsset->AudioDataArray = AudioData;
	PreImportedSoundAsset->AudioFormat = EncodedAudioInfo.AudioFormat;
	PreImportedSoundAsset->SourceFilePath = Filename;

	PreImportedSoundAsset->SoundDuration = URuntimeAudioImporterLibrary::ConvertSecondsToString(DecodedAudioInfo.SoundWaveBasicInfo.Duration);
	PreImportedSoundAsset->NumberOfChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
	PreImportedSoundAsset->SampleRate = DecodedAudioInfo.SoundWaveBasicInfo.SampleRate;

	bOutOperationCanceled = false;

	UE_LOG(LogPreImportedSoundFactory, Log, TEXT("Successfully imported sound asset '%s'"), *Filename);

	return PreImportedSoundAsset;
}

bool UPreImportedSoundFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (const UPreImportedSoundAsset* PreImportedSoundAsset = Cast<UPreImportedSoundAsset>(Obj))
	{
		OutFilenames.Add(PreImportedSoundAsset->SourceFilePath);
		return true;
	}

	return false;
}

void UPreImportedSoundFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UPreImportedSoundAsset* PreImportedSoundAsset = Cast<UPreImportedSoundAsset>(Obj);

	if (PreImportedSoundAsset && ensure(NewReimportPaths.Num() == 1))
	{
		PreImportedSoundAsset->SourceFilePath = NewReimportPaths[0];
	}
}

EReimportResult::Type UPreImportedSoundFactory::Reimport(UObject* Obj)
{
	const UPreImportedSoundAsset* PreImportedSoundAsset = Cast<UPreImportedSoundAsset>(Obj);

	if (!PreImportedSoundAsset)
	{
		UE_LOG(LogPreImportedSoundFactory, Log, TEXT("The sound asset '%s' cannot be re-imported because the object is corrupted"), *PreImportedSoundAsset->SourceFilePath);
		return EReimportResult::Failed;
	}

	if (PreImportedSoundAsset->SourceFilePath.IsEmpty() || !FPaths::FileExists(PreImportedSoundAsset->SourceFilePath))
	{
		UE_LOG(LogPreImportedSoundFactory, Log, TEXT("The sound asset '%s' cannot be re-imported because the path to the source file cannot be found"), *PreImportedSoundAsset->SourceFilePath);
		return EReimportResult::Failed;
	}

	bool OutCanceled = false;
	if (ImportObject(Obj->GetClass(), Obj->GetOuter(), *Obj->GetName(), RF_Public | RF_Standalone, PreImportedSoundAsset->SourceFilePath, nullptr, OutCanceled))
	{
		UE_LOG(LogPreImportedSoundFactory, Log, TEXT("Successfully re-imported sound asset '%s'"), *PreImportedSoundAsset->SourceFilePath);
		return EReimportResult::Succeeded;
	}

	return OutCanceled ? EReimportResult::Cancelled : EReimportResult::Failed;
}

int32 UPreImportedSoundFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
