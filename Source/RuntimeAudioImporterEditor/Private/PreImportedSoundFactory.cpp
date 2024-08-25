// Georgy Treshchev 2024.

#include "PreImportedSoundFactory.h"
#include "RuntimeAudioImporterEditor.h"
#include "PreImportedSoundAsset.h"
#include "Misc/FileHelper.h"
#include "RuntimeAudioImporterLibrary.h"
#include "RuntimeAudioUtilities.h"
#include "Codecs/BaseRuntimeCodec.h"
#include "Codecs/RuntimeCodecFactory.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "PreImportedSoundFactory"

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
	return FileExtension == TEXT("imp") || URuntimeAudioUtilities::GetAudioFormats(Filename).Num() > 0;
}

UObject* UPreImportedSoundFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	TArray64<uint8> AudioData;

	if (!RuntimeAudioImporter::LoadAudioFileToArray(AudioData, *Filename))
	{
		FMessageLog("Import").Error(FText::Format(LOCTEXT("PreImportedSoundFactory_ReadError", "Unable to read the audio file '{0}'. Check file permissions'"), FText::FromString(Filename)));
		return nullptr;
	}

	FRuntimeBulkDataBuffer<uint8> BulkDataBuffer(AudioData);

	FRuntimeCodecFactory CodecFactory;
	TArray<FBaseRuntimeCodec*> RuntimeCodecs = CodecFactory.GetCodecs(BulkDataBuffer);

	if (RuntimeCodecs.Num() == 0)
	{
		FMessageLog("Import").Error(FText::Format(LOCTEXT("PreImportedSoundFactory_CodecError", "Unable to determine the audio codec for the file '{0}'. Make sure the file is not corrupted'"), FText::FromString(Filename)));
		return nullptr;
	}

	FText ErrorText = LOCTEXT("PreImportedSoundFactory_UnknownError", "An unknown error occurred while importing the sound asset");
	for (FBaseRuntimeCodec* RuntimeCodec : RuntimeCodecs)
	{
		FRuntimeAudioHeaderInfo HeaderInfo;
		FEncodedAudioStruct EncodedAudioInfo = FEncodedAudioStruct(BulkDataBuffer, RuntimeCodec->GetAudioFormat());
	
		if (!RuntimeCodec->GetHeaderInfo(MoveTemp(EncodedAudioInfo), HeaderInfo))
		{
			ErrorText = FText::Format(LOCTEXT("PreImportedSoundFactory_HeaderError", "Unable to get the header info for the file '{0}'. Make sure the file is not corrupted'"), FText::FromString(Filename));
			continue;
		}

		UPreImportedSoundAsset* PreImportedSoundAsset = NewObject<UPreImportedSoundAsset>(InParent, UPreImportedSoundAsset::StaticClass(), InName, Flags);
		if (!PreImportedSoundAsset)
		{
			ErrorText = FText::Format(LOCTEXT("PreImportedSoundFactory_CreateError", "Unable to create the sound asset '{0}'"), FText::FromString(Filename));
			continue;
		}

		PreImportedSoundAsset->AudioDataArray = AudioData;
		PreImportedSoundAsset->AudioFormat = HeaderInfo.AudioFormat;
		PreImportedSoundAsset->SourceFilePath = Filename;

		PreImportedSoundAsset->SoundDuration = URuntimeAudioUtilities::ConvertSecondsToString(HeaderInfo.Duration);
		PreImportedSoundAsset->NumberOfChannels = HeaderInfo.NumOfChannels;
		PreImportedSoundAsset->SampleRate = HeaderInfo.SampleRate;

		bOutOperationCanceled = false;

		UE_LOG(LogRuntimeAudioImporterEditor, Log, TEXT("Successfully imported sound asset '%s'"), *Filename);

		return PreImportedSoundAsset;
	}
	
	FMessageLog("Import").Error(ErrorText);
	return nullptr;
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
		UE_LOG(LogRuntimeAudioImporterEditor, Log, TEXT("The sound asset '%s' cannot be re-imported because the object is corrupted"), *PreImportedSoundAsset->SourceFilePath);
		return EReimportResult::Failed;
	}

	if (PreImportedSoundAsset->SourceFilePath.IsEmpty() || !FPaths::FileExists(PreImportedSoundAsset->SourceFilePath))
	{
		UE_LOG(LogRuntimeAudioImporterEditor, Log, TEXT("The sound asset '%s' cannot be re-imported because the path to the source file cannot be found"), *PreImportedSoundAsset->SourceFilePath);
		return EReimportResult::Failed;
	}

	bool OutCanceled = false;
	if (ImportObject(Obj->GetClass(), Obj->GetOuter(), *Obj->GetName(), RF_Public | RF_Standalone, PreImportedSoundAsset->SourceFilePath, nullptr, OutCanceled))
	{
		UE_LOG(LogRuntimeAudioImporterEditor, Log, TEXT("Successfully re-imported sound asset '%s'"), *PreImportedSoundAsset->SourceFilePath);
		return EReimportResult::Succeeded;
	}

	return OutCanceled ? EReimportResult::Cancelled : EReimportResult::Failed;
}

int32 UPreImportedSoundFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
