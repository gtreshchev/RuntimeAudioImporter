// Georgy Treshchev 2022.

#include "RuntimeAudioImporterLibrary.h"

#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "PreImportedSoundAsset.h"

#include "Transcoders/MP3Transcoder.h"
#include "Transcoders/WAVTranscoder.h"
#include "Transcoders/FlacTranscoder.h"
#include "Transcoders/VorbisTranscoder.h"
#include "Transcoders/RAWTranscoder.h"

#include "Misc/FileHelper.h"
#include "Async/Async.h"

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	return NewObject<URuntimeAudioImporterLibrary>();
}

bool LoadAudioFileToArray(TArray<uint8>& AudioData, const FString& FilePath)
{
	// Filling AudioBuffer with a binary file
	if (!FFileHelper::LoadFileToArray(AudioData, *FilePath))
	{
		return false;
	}

	// Removing unused two uninitialized bytes
	AudioData.RemoveAt(AudioData.Num() - 2, 2);

	return true;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, EAudioFormat AudioFormat)
{
	TWeakObjectPtr<URuntimeAudioImporterLibrary> ThisPtr(this);
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisPtr, FilePath, AudioFormat]() mutable
	{
		if (!ThisPtr.IsValid())
		{
			return;
		}

		// Checking if the file exists
		if (!FPaths::FileExists(FilePath))
		{
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
			return;
		}

		// Getting the audio format
		AudioFormat = AudioFormat == EAudioFormat::Auto ? GetAudioFormat(FilePath) : AudioFormat;
		AudioFormat = AudioFormat == EAudioFormat::Invalid ? EAudioFormat::Auto : AudioFormat;

		TArray<uint8> AudioBuffer;

		// Filling AudioBuffer with a binary file
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
			return;
		}

		ThisPtr->ImportAudioFromBuffer(MoveTemp(AudioBuffer), AudioFormat);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	TWeakObjectPtr<URuntimeAudioImporterLibrary> ThisPtr(this);
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisPtr, FilePath, RAWFormat, SampleRate, NumOfChannels]() mutable
	{
		if (!ThisPtr.IsValid())
		{
			return;
		}

		if (!FPaths::FileExists(FilePath))
		{
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
			return;
		}

		ThisPtr->OnProgress_Internal(5);

		TArray<uint8> AudioBuffer;
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
			return;
		}

		ThisPtr->OnProgress_Internal(35);
		ThisPtr->ImportAudioFromRAWBuffer(MoveTemp(AudioBuffer), RAWFormat, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	uint8* RAWData{RAWBuffer.GetData()};
	const int32 RAWDataSize{RAWBuffer.Num()};

	float* PCMData{nullptr};
	int32 PCMDataSize{0};

	// Transcoding RAW data to 32-bit float data
	{
		switch (RAWFormat)
		{
		case ERAWAudioFormat::Int16:
			{
				RAWTranscoder::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(RAWData), RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::Int32:
			{
				RAWTranscoder::TranscodeRAWData<int32, float>(reinterpret_cast<int32*>(RAWData), RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::UInt8:
			{
				RAWTranscoder::TranscodeRAWData<uint8, float>(RAWData, RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::Float32:
			{
				PCMDataSize = RAWDataSize;
				PCMData = static_cast<float*>(FMemory::Memcpy(FMemory::Malloc(PCMDataSize), RAWData, RAWDataSize));
				break;
			}
		}
	}

	if (!PCMData || PCMDataSize <= 0)
	{
		OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
		return;
	}

	ImportAudioFromFloat32Buffer(FBulkDataBuffer<uint8>(reinterpret_cast<uint8*>(PCMData), PCMDataSize), SampleRate, NumOfChannels);
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAsset)
{
	ImportAudioFromBuffer(PreImportedSoundAsset->AudioDataArray, PreImportedSoundAsset->AudioFormat);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8> AudioData, EAudioFormat AudioFormat)
{
	TWeakObjectPtr<URuntimeAudioImporterLibrary> ThisPtr(this);
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisPtr, AudioData = MoveTemp(AudioData), AudioFormat]()
	{
		if (!ThisPtr.IsValid())
		{
			return;
		}

		ThisPtr->OnProgress_Internal(5);

		if (AudioFormat == EAudioFormat::Invalid)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for import"));
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return;
		}

		uint8* EncodedAudioDataPtr = static_cast<uint8*>(FMemory::Memcpy(FMemory::Malloc(AudioData.Num()), AudioData.GetData(), AudioData.Num()));

		if (!EncodedAudioDataPtr)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for compressed audio data"));
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
			return;
		}

		FEncodedAudioStruct EncodedAudioInfo(EncodedAudioDataPtr, AudioData.Num(), AudioFormat);

		ThisPtr->OnProgress_Internal(10);

		FDecodedAudioStruct DecodedAudioInfo;
		if (!DecodeAudioData(EncodedAudioInfo, DecodedAudioInfo))
		{
			ThisPtr->OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
			return;
		}

		ThisPtr->OnProgress_Internal(65);

		// Finalizing import
		ThisPtr->ImportAudioFromDecodedInfo(DecodedAudioInfo);
	});
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray<uint8> RAWDataFrom, ERAWAudioFormat RAWFormatFrom, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResult& Result)
{
	TranscodeRAWDataFromBuffer(MoveTemp(RAWDataFrom), RAWFormatFrom, RAWFormatTo, FOnRAWDataTranscodeFromBufferResultNative::CreateLambda([Result](bool bSucceed, const TArray<uint8>& RAWData)
	{
		Result.ExecuteIfBound(bSucceed, RAWData);
	}));
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray<uint8> RAWDataFrom, ERAWAudioFormat RAWFormatFrom, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [RAWDataFrom = MoveTemp(RAWDataFrom), RAWFormatFrom, RAWFormatTo, Result]()
	{
		auto ExecuteResult = [Result](bool bSucceed, TArray<uint8>&& AudioData)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceed, AudioData]()
			{
				Result.ExecuteIfBound(bSucceed, AudioData);
			});
		};

		TArray<uint8> IntermediateRAWBuffer;

		// Transcoding of all formats to unsigned 8-bit PCM format (intermediate)
		switch (RAWFormatFrom)
		{
		case ERAWAudioFormat::Int16:
			{
				RAWTranscoder::TranscodeRAWData<int16, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERAWAudioFormat::Int32:
			{
				RAWTranscoder::TranscodeRAWData<int32, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERAWAudioFormat::UInt8:
			{
				IntermediateRAWBuffer = RAWDataFrom;
				break;
			}
		case ERAWAudioFormat::Float32:
			{
				RAWTranscoder::TranscodeRAWData<float, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		}

		TArray<uint8> RAWData_To;

		// Transcoding unsigned 8-bit PCM to the specified format
		switch (RAWFormatTo)
		{
		case ERAWAudioFormat::Int16:
			{
				RAWTranscoder::TranscodeRAWData<uint8, int16>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERAWAudioFormat::Int32:
			{
				RAWTranscoder::TranscodeRAWData<uint8, int32>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERAWAudioFormat::UInt8:
			{
				RAWData_To = IntermediateRAWBuffer;
				break;
			}
		case ERAWAudioFormat::Float32:
			{
				RAWTranscoder::TranscodeRAWData<uint8, float>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		}

		ExecuteResult(true, MoveTemp(RAWData_To));
	});
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromFile(const FString& FilePathFrom, ERAWAudioFormat RAWFormatFrom, const FString& FilePathTo, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResult& Result)
{
	TranscodeRAWDataFromFile(FilePathFrom, RAWFormatFrom, FilePathTo, RAWFormatTo, FOnRAWDataTranscodeFromFileResultNative::CreateLambda([Result](bool bSucceed)
	{
		Result.ExecuteIfBound(bSucceed);
	}));
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromFile(const FString& FilePathFrom, ERAWAudioFormat RAWFormatFrom, const FString& FilePathTo, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [FilePathFrom, RAWFormatFrom, FilePathTo, RAWFormatTo, Result]()
	{
		auto ExecuteResult = [Result](bool bSucceed)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceed]()
			{
				Result.ExecuteIfBound(bSucceed);
			});
		};

		TArray<uint8> RAWBufferFrom;

		// Loading a file into a byte array
		if (!LoadAudioFileToArray(RAWBufferFrom, *FilePathFrom))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when reading RAW data on the path '%s'"), *FilePathFrom);
			ExecuteResult(false);
			return;
		}

		TranscodeRAWDataFromBuffer(MoveTemp(RAWBufferFrom), RAWFormatFrom, RAWFormatTo, FOnRAWDataTranscodeFromBufferResultNative::CreateLambda([Result, ExecuteResult, FilePathTo](bool bSucceed, const TArray<uint8>& RAWBufferTo)
		{
			if (!bSucceed)
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when transcoding RAW data from buffer to save to the path '%s'"), *FilePathTo);
				return;
			}

			// Writing a file to a specified location
			if (!FFileHelper::SaveArrayToFile(RAWBufferTo, *FilePathTo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving RAW data to the path '%s'"), *FilePathTo);
				ExecuteResult(false);
				return;
			}

			ExecuteResult(true);
		}));
	});
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToFileResult& Result)
{
	ExportSoundWaveToFile(ImportedSoundWave, SavePath, AudioFormat, Quality, FOnAudioExportToFileResultNative::CreateLambda([Result](bool bSucceed)
	{
		Result.ExecuteIfBound(bSucceed);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToFileResultNative& Result)
{
	AudioFormat = AudioFormat == EAudioFormat::Auto ? GetAudioFormat(SavePath) : AudioFormat;

	// Exporting a sound wave to a buffer
	ExportSoundWaveToBuffer(ImportedSoundWave, AudioFormat, Quality, FOnAudioExportToBufferResultNative::CreateLambda([Result, SavePath](bool bSucceed, const TArray<uint8>& AudioData)
	{
		if (!bSucceed)
		{
			return;
		}

		// Writing encoded data to specified location
		if (!FFileHelper::SaveArrayToFile(AudioData, *SavePath))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving audio data to the path '%s'"), *SavePath);
			Result.ExecuteIfBound(false);
			return;
		}

		Result.ExecuteIfBound(true);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToBuffer(UImportedSoundWave* ImportedSoundWave, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToBufferResult& Result)
{
	ExportSoundWaveToBuffer(ImportedSoundWave, AudioFormat, Quality, FOnAudioExportToBufferResultNative::CreateLambda([Result](bool bSucceed, const TArray<uint8>& AudioData)
	{
		Result.ExecuteIfBound(bSucceed, AudioData);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToBuffer(UImportedSoundWave* ImportedSoundWave, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ImportedSoundWave, AudioFormat, Quality, Result]()
	{
		auto ExecuteResult = [Result](bool bSucceed, TArray<uint8>&& AudioData)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceed, AudioData]()
			{
				Result.ExecuteIfBound(bSucceed, AudioData);
			});
		};

		if (!ImportedSoundWave)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
			ExecuteResult(false, TArray<uint8>());
			return;
		}

		// Filling in decoded audio info
		FDecodedAudioStruct DecodedAudioInfo;
		{
			DecodedAudioInfo.PCMInfo = ImportedSoundWave->PCMBufferInfo;
			FSoundWaveBasicStruct SoundWaveBasicInfo;
			{
				SoundWaveBasicInfo.NumOfChannels = ImportedSoundWave->NumChannels;
				SoundWaveBasicInfo.SampleRate = ImportedSoundWave->SamplingRate;
				SoundWaveBasicInfo.Duration = ImportedSoundWave->Duration;
			}
			DecodedAudioInfo.SoundWaveBasicInfo = SoundWaveBasicInfo;
		}

		FEncodedAudioStruct EncodedAudioInfo;
		{
			EncodedAudioInfo.AudioFormat = AudioFormat;
		}

		// Encoding to the specified format
		if (!EncodeAudioData(DecodedAudioInfo, EncodedAudioInfo, Quality))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave '%s'"), *ImportedSoundWave->GetName());
			ExecuteResult(false, TArray<uint8>());
			return;
		}

		ExecuteResult(true, TArray<uint8>(EncodedAudioInfo.AudioData.GetView().GetData(), EncodedAudioInfo.AudioData.GetView().Num()));
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromDecodedInfo(const FDecodedAudioStruct& DecodedAudioInfo)
{
	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		TWeakObjectPtr<URuntimeAudioImporterLibrary> ThisPtr(this);
		AsyncTask(ENamedThreads::GameThread, [ThisPtr, DecodedAudioInfo]() { if (ThisPtr.IsValid()) ThisPtr->ImportAudioFromDecodedInfo(DecodedAudioInfo); });
		return;
	}

	UImportedSoundWave* ImportedSoundWave = CreateImportedSoundWave();

	if (!ImportedSoundWave)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while creating the imported sound wave"));
		OnResult_Internal(nullptr, ETranscodingStatus::SoundWaveDeclarationError);
		return;
	}

	DefineSoundWave(ImportedSoundWave, DecodedAudioInfo);

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("The audio data was successfully imported. Information about imported data:\n%s"), *DecodedAudioInfo.ToString());
	OnProgress_Internal(100);
	OnResult_Internal(ImportedSoundWave, ETranscodingStatus::SuccessfulImport);
}

void URuntimeAudioImporterLibrary::DefineSoundWave(UImportedSoundWave* ImportedSoundWave, const FDecodedAudioStruct& DecodedAudioInfo)
{
	OnProgress_Internal(70);

	// Filling in a sound wave basic information (e.g. duration, number of channels, etc)
	FillSoundWaveBasicInfo(ImportedSoundWave, DecodedAudioInfo);

	OnProgress_Internal(75);

	// Filling in PCM data buffer
	FillPCMData(ImportedSoundWave, DecodedAudioInfo);

	OnProgress_Internal(95);
}

void URuntimeAudioImporterLibrary::FillSoundWaveBasicInfo(UImportedSoundWave* ImportedSoundWave, const FDecodedAudioStruct& DecodedAudioInfo)
{
	ImportedSoundWave->Duration = DecodedAudioInfo.SoundWaveBasicInfo.Duration;
	ImportedSoundWave->SetSampleRate(DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
	ImportedSoundWave->SamplingRate = DecodedAudioInfo.SoundWaveBasicInfo.SampleRate;
	ImportedSoundWave->NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
	ImportedSoundWave->SoundGroup = SOUNDGROUP_Default;

	if (ImportedSoundWave->NumChannels == 4)
	{
		ImportedSoundWave->bIsAmbisonics = 1;
	}

	ImportedSoundWave->bProcedural = true;
	ImportedSoundWave->DecompressionType = EDecompressionType::DTYPE_Procedural;
}

void URuntimeAudioImporterLibrary::FillPCMData(UImportedSoundWave* ImportedSoundWave, const FDecodedAudioStruct& DecodedAudioInfo)
{
	ImportedSoundWave->PCMBufferInfo = DecodedAudioInfo.PCMInfo;
	ImportedSoundWave->RawPCMDataSize = DecodedAudioInfo.PCMInfo.PCMData.GetView().Num();
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const FString& FilePath)
{
	const FString& Extension{FPaths::GetExtension(FilePath, false).ToLower()};

	if (Extension == TEXT("mp3"))
	{
		return EAudioFormat::Mp3;
	}

	if (Extension == TEXT("wav") || Extension == TEXT("wave"))
	{
		return EAudioFormat::Wav;
	}

	if (Extension == TEXT("flac"))
	{
		return EAudioFormat::Flac;
	}

	if (Extension == TEXT("ogg") || Extension == TEXT("oga") || Extension == TEXT("sb0"))
	{
		return EAudioFormat::OggVorbis;
	}

	UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to determine audio file format with path '%s' by name"), *FilePath);

	return EAudioFormat::Invalid;
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const TArray<uint8>& AudioData)
{
	return GetAudioFormat(AudioData.GetData(), AudioData.Num());
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	if (MP3Transcoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::Mp3;
	}

	if (WAVTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::Wav;
	}

	if (FlacTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::Flac;
	}

	if (VorbisTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::OggVorbis;
	}

	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to determine audio data format"));

	return EAudioFormat::Invalid;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFloat32Buffer(FBulkDataBuffer<uint8>&& PCMData, int32 SampleRate, int32 NumOfChannels)
{
	FDecodedAudioStruct DecodedAudioInfo;

	// Filling in the required information
	{
		DecodedAudioInfo.PCMInfo.PCMData = MoveTemp(PCMData);
		DecodedAudioInfo.PCMInfo.PCMNumOfFrames = DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() / sizeof(float) / NumOfChannels;

		DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
		DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = SampleRate;
		DecodedAudioInfo.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SampleRate;
	}

	OnProgress_Internal(50);

	// Finalizing import
	ImportAudioFromDecodedInfo(DecodedAudioInfo);
}

FString URuntimeAudioImporterLibrary::ConvertSecondsToString(int32 Seconds)
{
	FString FinalString;

	const int32 NewHours = Seconds / 3600;
	if (NewHours > 0)
	{
		FinalString += ((NewHours < 10) ? TEXT("0") + FString::FromInt(NewHours) : FString::FromInt(NewHours)) + TEXT(":");
	}

	Seconds = Seconds % 3600;

	const int32 NewMinutes = Seconds / 60;
	FinalString += ((NewMinutes < 10) ? TEXT("0") + FString::FromInt(NewMinutes) : FString::FromInt(NewMinutes)) + TEXT(":");

	const int32 NewSeconds = Seconds % 60;
	FinalString += (NewSeconds < 10) ? TEXT("0") + FString::FromInt(NewSeconds) : FString::FromInt(NewSeconds);

	return FinalString;
}

bool URuntimeAudioImporterLibrary::DecodeAudioData(FEncodedAudioStruct& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo)
{
	if (EncodedAudioInfo.AudioFormat == EAudioFormat::Auto)
	{
		EncodedAudioInfo.AudioFormat = GetAudioFormat(EncodedAudioInfo.AudioData.GetView().GetData(), EncodedAudioInfo.AudioData.GetView().Num());
	}

	if (EncodedAudioInfo.AudioFormat == EAudioFormat::Wav && !WAVTranscoder::CheckAndFixWavDurationErrors(EncodedAudioInfo.AudioData))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while fixing Wav audio data duration error"));
		return false;
	}

	switch (EncodedAudioInfo.AudioFormat)
	{
	case EAudioFormat::Mp3:
		{
			if (!MP3Transcoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Mp3 audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::Wav:
		{
			if (!WAVTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Wav audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::Flac:
		{
			if (!FlacTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Flac audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::OggVorbis:
		{
			if (!VorbisTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Vorbis audio data"));
				return false;
			}
			break;
		}
	default:
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for decoding"));
			return false;
		}
	}

	return true;
}

bool URuntimeAudioImporterLibrary::EncodeAudioData(const FDecodedAudioStruct& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality)
{
	if (EncodedAudioInfo.AudioFormat == EAudioFormat::Auto || EncodedAudioInfo.AudioFormat == EAudioFormat::Invalid)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for encoding"));
		return false;
	}

	switch (EncodedAudioInfo.AudioFormat)
	{
	case EAudioFormat::Mp3:
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("MP3 format is not currently supported for encoding"));
			break;
		}
	case EAudioFormat::Wav:
		{
			if (!WAVTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo, FWAVEncodingFormat(EWAVEncodingFormat::FORMAT_IEEE_FLOAT, 32)))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding Wav audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::Flac:
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Flac format is not currently supported for encoding"));
			break;
		}
	case EAudioFormat::OggVorbis:
		{
			if (!VorbisTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo, Quality))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding Vorbis audio data"));
				return false;
			}
			break;
		}
	default:
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for encoding"));
			return false;
		}
	}

	return true;
}

UImportedSoundWave* URuntimeAudioImporterLibrary::CreateImportedSoundWave() const
{
	if (!IsInGameThread())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to create a sound wave outside of the game thread"));
		return nullptr;
	}

	return NewObject<UImportedSoundWave>();
}

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	TWeakObjectPtr<URuntimeAudioImporterLibrary> ThisPtr(this);

	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [ThisPtr, Percentage]() { if (ThisPtr.IsValid()) ThisPtr->OnProgress_Internal(Percentage); });
		return;
	}

	if (ThisPtr->OnProgress.IsBound())
	{
		ThisPtr->OnProgress.Broadcast(Percentage);
	}

	if (ThisPtr->OnProgressNative.IsBound())
	{
		ThisPtr->OnProgressNative.Broadcast(Percentage);
	}
}

void URuntimeAudioImporterLibrary::OnResult_Internal(UImportedSoundWave* ImportedSoundWave, ETranscodingStatus Status)
{
	TWeakObjectPtr<URuntimeAudioImporterLibrary> ThisPtr(this);

	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [ThisPtr, ImportedSoundWave, Status]() { if (ThisPtr.IsValid()) ThisPtr->OnResult_Internal(ImportedSoundWave, Status); });
		return;
	}

	bool bBroadcasted{false};

	if (OnResultNative.IsBound())
	{
		bBroadcasted = true;
		ThisPtr->OnResultNative.Broadcast(this, ImportedSoundWave, Status);
	}

	if (OnResult.IsBound())
	{
		bBroadcasted = true;
		ThisPtr->OnResult.Broadcast(this, ImportedSoundWave, Status);
	}

	if (!bBroadcasted)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("You did not bind to the delegate to get the result of the import"));
	}
}
