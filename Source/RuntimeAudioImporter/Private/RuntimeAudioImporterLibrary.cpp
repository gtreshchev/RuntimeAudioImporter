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
#include "UObject/GCObjectScopeGuard.h"

#include "Launch/Resources/Version.h"

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	return NewObject<URuntimeAudioImporterLibrary>();
}

bool LoadAudioFileToArray(TArray64<uint8>& AudioData, const FString& FilePath)
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25
	TArray<uint8> AudioData32;
	if (!FFileHelper::LoadFileToArray(AudioData32, *FilePath))
	{
		return false;
	}
	AudioData = AudioData32;
#else
	if (!FFileHelper::LoadFileToArray(AudioData, *FilePath))
	{
		return false;
	}
#endif

	// Removing unused two uninitialized bytes
	AudioData.RemoveAt(AudioData.Num() - 2, 2);

	return true;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, EAudioFormat AudioFormat)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, FilePath, AudioFormat]() mutable
	{
		FGCObjectScopeGuard Guard(this);

		if (!FPaths::FileExists(FilePath))
		{
			OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
			return;
		}

		AudioFormat = AudioFormat == EAudioFormat::Auto ? GetAudioFormat(FilePath) : AudioFormat;
		AudioFormat = AudioFormat == EAudioFormat::Invalid ? EAudioFormat::Auto : AudioFormat;

		TArray64<uint8> AudioBuffer;
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
			return;
		}

		ImportAudioFromBuffer(MoveTemp(AudioBuffer), AudioFormat);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAsset)
{
	ImportAudioFromBuffer(PreImportedSoundAsset->AudioDataArray, PreImportedSoundAsset->AudioFormat);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8> AudioData, EAudioFormat AudioFormat)
{
	ImportAudioFromBuffer(TArray64<uint8>(MoveTemp(AudioData)), AudioFormat);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray64<uint8> AudioData, EAudioFormat AudioFormat)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, AudioData = MoveTemp(AudioData), AudioFormat]()
	{
		FGCObjectScopeGuard Guard(this);

		OnProgress_Internal(15);

		if (AudioFormat == EAudioFormat::Invalid)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for import"));
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return;
		}

		uint8* EncodedAudioDataPtr = static_cast<uint8*>(FMemory::Memcpy(FMemory::Malloc(AudioData.Num()), AudioData.GetData(), AudioData.Num()));

		if (!EncodedAudioDataPtr)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to allocate memory for compressed audio data"));
			OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
			return;
		}

		FEncodedAudioStruct EncodedAudioInfo(EncodedAudioDataPtr, AudioData.Num(), AudioFormat);

		OnProgress_Internal(25);

		FDecodedAudioStruct DecodedAudioInfo;
		if (!DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
		{
			OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
			return;
		}

		OnProgress_Internal(65);

		ImportAudioFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, FilePath, RAWFormat, SampleRate, NumOfChannels]() mutable
	{
		if (!FPaths::FileExists(FilePath))
		{
			OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
			return;
		}

		OnProgress_Internal(5);

		TArray64<uint8> AudioBuffer;
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
			return;
		}

		OnProgress_Internal(35);
		ImportAudioFromRAWBuffer(MoveTemp(AudioBuffer), RAWFormat, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	ImportAudioFromRAWBuffer(TArray64<uint8>(MoveTemp(RAWBuffer)), RAWFormat, SampleRate, NumOfChannels);
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray64<uint8> RAWBuffer, ERAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	uint8* RAWData{RAWBuffer.GetData()};
	const int64 RAWDataSize{RAWBuffer.Num()};

	float* PCMData{nullptr};
	int64 PCMDataSize{0};

	OnProgress_Internal(15);

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

	OnProgress_Internal(35);

	if (!PCMData || PCMDataSize <= 0)
	{
		OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
		return;
	}

	ImportAudioFromFloat32Buffer(FRuntimeBulkDataBuffer<float>(PCMData, PCMDataSize), SampleRate, NumOfChannels);
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray<uint8> RAWDataFrom, ERAWAudioFormat RAWFormatFrom, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResult& Result)
{
	TranscodeRAWDataFromBuffer(TArray64<uint8>(MoveTemp(RAWDataFrom)), RAWFormatFrom, RAWFormatTo, FOnRAWDataTranscodeFromBufferResultNative::CreateLambda([Result](bool bSucceeded, const TArray64<uint8>& RAWData)
	{
		Result.ExecuteIfBound(bSucceeded, TArray<uint8>(RAWData));
	}));
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray64<uint8> RAWDataFrom, ERAWAudioFormat RAWFormatFrom, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [RAWDataFrom = MoveTemp(RAWDataFrom), RAWFormatFrom, RAWFormatTo, Result]()
	{
		auto ExecuteResult = [Result](bool bSucceeded, TArray64<uint8>&& AudioData)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, AudioData]()
			{
				Result.ExecuteIfBound(bSucceeded, AudioData);
			});
		};

		TArray64<uint8> IntermediateRAWBuffer;

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

		TArray64<uint8> RAWData_To;

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
	TranscodeRAWDataFromFile(FilePathFrom, RAWFormatFrom, FilePathTo, RAWFormatTo, FOnRAWDataTranscodeFromFileResultNative::CreateLambda([Result](bool bSucceeded)
	{
		Result.ExecuteIfBound(bSucceeded);
	}));
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromFile(const FString& FilePathFrom, ERAWAudioFormat RAWFormatFrom, const FString& FilePathTo, ERAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [FilePathFrom, RAWFormatFrom, FilePathTo, RAWFormatTo, Result]()
	{
		auto ExecuteResult = [Result](bool bSucceeded)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded]()
			{
				Result.ExecuteIfBound(bSucceeded);
			});
		};

		TArray64<uint8> RAWBufferFrom;
		if (!LoadAudioFileToArray(RAWBufferFrom, *FilePathFrom))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when reading RAW data on the path '%s'"), *FilePathFrom);
			ExecuteResult(false);
			return;
		}

		TranscodeRAWDataFromBuffer(MoveTemp(RAWBufferFrom), RAWFormatFrom, RAWFormatTo, FOnRAWDataTranscodeFromBufferResultNative::CreateLambda([Result, ExecuteResult, FilePathTo](bool bSucceeded, const TArray64<uint8>& RAWBufferTo)
		{
			if (!bSucceeded)
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when transcoding RAW data from buffer to save to the path '%s'"), *FilePathTo);
				ExecuteResult(false);
				return;
			}

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
	if (!IsValid(ImportedSoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
		Result.ExecuteIfBound(false);
		return;
	}

	ExportSoundWaveToFile(ImportedSoundWave, SavePath, AudioFormat, Quality, FOnAudioExportToFileResultNative::CreateLambda([Result](bool bSucceeded)
	{
		Result.ExecuteIfBound(bSucceeded);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToFileResultNative& Result)
{
	AudioFormat = AudioFormat == EAudioFormat::Auto ? GetAudioFormat(SavePath) : AudioFormat;

	ExportSoundWaveToBuffer(ImportedSoundWavePtr, AudioFormat, Quality, FOnAudioExportToBufferResultNative::CreateLambda([Result, SavePath](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (!bSucceeded)
		{
			Result.ExecuteIfBound(false);
			return;
		}

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
	if (!IsValid(ImportedSoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
		Result.ExecuteIfBound(false, TArray<uint8>());
		return;
	}

	ExportSoundWaveToBuffer(ImportedSoundWave, AudioFormat, Quality, FOnAudioExportToBufferResultNative::CreateLambda([Result](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (AudioData.Num() > TNumericLimits<int32>::Max())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Array with int32 size (max length: %d) cannot fit int64 size data (retrieved length: %lld)\nA standard byte array can hold a maximum of 2 GB of data"), MAX_int32, AudioData.Num());
			Result.ExecuteIfBound(false, TArray<uint8>());
			return;
		}

		Result.ExecuteIfBound(bSucceeded, TArray<uint8>(AudioData));
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, EAudioFormat AudioFormat, uint8 Quality, const FOnAudioExportToBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ImportedSoundWavePtr, AudioFormat, Quality, Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, TArray64<uint8>&& AudioData)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, AudioData = MoveTemp(AudioData)]()
			{
				Result.ExecuteIfBound(bSucceeded, AudioData);
			});
		};

		if (!ImportedSoundWavePtr.IsValid())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
			ExecuteResult(false, TArray64<uint8>());
			return;
		}

		FGCObjectScopeGuard Guard(ImportedSoundWavePtr.Get());

		FDecodedAudioStruct DecodedAudioInfo;
		{
			FScopeLock Lock(&ImportedSoundWavePtr->DataGuard);

			if (!ImportedSoundWavePtr->GetPCMBuffer().IsValid())
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as the PCM data is invalid"));
				ExecuteResult(false, TArray64<uint8>());
				return;
			}

			{
				DecodedAudioInfo.PCMInfo = ImportedSoundWavePtr->GetPCMBuffer();
				FSoundWaveBasicStruct SoundWaveBasicInfo;
				{
					SoundWaveBasicInfo.NumOfChannels = ImportedSoundWavePtr->NumChannels;
					SoundWaveBasicInfo.SampleRate = ImportedSoundWavePtr->GetSampleRate();
					SoundWaveBasicInfo.Duration = ImportedSoundWavePtr->Duration;
				}
				DecodedAudioInfo.SoundWaveBasicInfo = MoveTemp(SoundWaveBasicInfo);
			}
		}

		FEncodedAudioStruct EncodedAudioInfo;
		{
			EncodedAudioInfo.AudioFormat = AudioFormat;
		}

		if (!EncodeAudioData(MoveTemp(DecodedAudioInfo), EncodedAudioInfo, Quality))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave '%s'"), *ImportedSoundWavePtr->GetName());
			ExecuteResult(false, TArray64<uint8>());
			return;
		}

		ExecuteResult(true, TArray64<uint8>(EncodedAudioInfo.AudioData.GetView().GetData(), EncodedAudioInfo.AudioData.GetView().Num()));
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo)
{
	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, DecodedAudioInfo = MoveTemp(DecodedAudioInfo)]() mutable
		{
			ImportAudioFromDecodedInfo(MoveTemp(DecodedAudioInfo));
		});
		return;
	}

	UImportedSoundWave* ImportedSoundWave = UImportedSoundWave::CreateImportedSoundWave();

	if (!ImportedSoundWave)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while creating the imported sound wave"));
		OnResult_Internal(nullptr, ETranscodingStatus::SoundWaveDeclarationError);
		return;
	}

	FGCObjectScopeGuard ImportedSoundWaveGuard(ImportedSoundWave);

	OnProgress_Internal(75);

	ImportedSoundWave->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("The audio data was successfully imported"));

	OnProgress_Internal(100);

	OnResult_Internal(ImportedSoundWave, ETranscodingStatus::SuccessfulImport);
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

	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to determine audio file format with path '%s' by name"), *FilePath);

	return EAudioFormat::Invalid;
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const TArray<uint8>& AudioData)
{
	return GetAudioFormat(AudioData.GetData(), AudioData.Num());
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const TArray64<uint8>& AudioData)
{
	return GetAudioFormat(AudioData.GetData(), AudioData.Num());
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	return GetAudioFormat(AudioData, static_cast<int64>(AudioDataSize));
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const uint8* AudioData, int64 AudioDataSize)
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

void URuntimeAudioImporterLibrary::ImportAudioFromFloat32Buffer(FRuntimeBulkDataBuffer<float>&& PCMData, int32 SampleRate, int32 NumOfChannels)
{
	FDecodedAudioStruct DecodedAudioInfo;
	{
		DecodedAudioInfo.PCMInfo.PCMData = MoveTemp(PCMData);
		DecodedAudioInfo.PCMInfo.PCMNumOfFrames = DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() / sizeof(float) / NumOfChannels;

		DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
		DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = SampleRate;
		DecodedAudioInfo.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SampleRate;
	}

	OnProgress_Internal(65);

	ImportAudioFromDecodedInfo(MoveTemp(DecodedAudioInfo));
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

bool URuntimeAudioImporterLibrary::DecodeAudioData(FEncodedAudioStruct&& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo)
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
			if (!MP3Transcoder::Decode(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Mp3 audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::Wav:
		{
			if (!WAVTranscoder::Decode(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Wav audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::Flac:
		{
			if (!FlacTranscoder::Decode(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Flac audio data"));
				return false;
			}
			break;
		}
	case EAudioFormat::OggVorbis:
		{
			if (!VorbisTranscoder::Decode(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
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

bool URuntimeAudioImporterLibrary::EncodeAudioData(FDecodedAudioStruct&& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality)
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
			if (!WAVTranscoder::Encode(MoveTemp(DecodedAudioInfo), EncodedAudioInfo, FWAVEncodingFormat(EWAVEncodingFormat::FORMAT_IEEE_FLOAT, 32)))
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
			if (!VorbisTranscoder::Encode(MoveTemp(DecodedAudioInfo), EncodedAudioInfo, Quality))
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

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, Percentage]()
		{
			OnProgress_Internal(Percentage);
		});
		return;
	}

	if (OnProgress.IsBound())
	{
		OnProgress.Broadcast(Percentage);
	}

	if (OnProgressNative.IsBound())
	{
		OnProgressNative.Broadcast(Percentage);
	}
}

void URuntimeAudioImporterLibrary::OnResult_Internal(UImportedSoundWave* ImportedSoundWave, ETranscodingStatus Status)
{
	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, ImportedSoundWave, Status]()
		{
			OnResult_Internal(ImportedSoundWave, Status);
		});
		return;
	}

	bool bBroadcasted{false};

	if (OnResultNative.IsBound())
	{
		bBroadcasted = true;
		OnResultNative.Broadcast(this, ImportedSoundWave, Status);
	}

	if (OnResult.IsBound())
	{
		bBroadcasted = true;
		OnResult.Broadcast(this, ImportedSoundWave, Status);
	}

	if (!bBroadcasted)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("You did not bind to the delegate to get the result of the import"));
	}
}
