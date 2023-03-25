// Georgy Treshchev 2023.

#include "RuntimeAudioImporterLibrary.h"

#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "PreImportedSoundAsset.h"

#include "Codecs/RAW_RuntimeCodec.h"

#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"
#include "Codecs/RuntimeCodecFactory.h"
#include "UObject/GCObjectScopeGuard.h"

#include "SampleBuffer.h"

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

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, ERuntimeAudioFormat AudioFormat)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, FilePath, AudioFormat]() mutable
	{
		FGCObjectScopeGuard Guard(this);

		if (!FPaths::FileExists(FilePath))
		{
			OnResult_Internal(nullptr, ERuntimeImportStatus::AudioDoesNotExist);
			return;
		}

		AudioFormat = AudioFormat == ERuntimeAudioFormat::Auto ? GetAudioFormat(FilePath) : AudioFormat;
		AudioFormat = AudioFormat == ERuntimeAudioFormat::Invalid ? ERuntimeAudioFormat::Auto : AudioFormat;

		TArray64<uint8> AudioBuffer;
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			OnResult_Internal(nullptr, ERuntimeImportStatus::LoadFileToArrayError);
			return;
		}

		ImportAudioFromBuffer(MoveTemp(AudioBuffer), AudioFormat);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAsset)
{
	ImportAudioFromBuffer(PreImportedSoundAsset->AudioDataArray, PreImportedSoundAsset->AudioFormat);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8> AudioData, ERuntimeAudioFormat AudioFormat)
{
	ImportAudioFromBuffer(TArray64<uint8>(MoveTemp(AudioData)), AudioFormat);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray64<uint8> AudioData, ERuntimeAudioFormat AudioFormat)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, AudioData = MoveTemp(AudioData), AudioFormat]()
	{
		FGCObjectScopeGuard Guard(this);

		OnProgress_Internal(15);

		if (AudioFormat == ERuntimeAudioFormat::Invalid)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for import"));
			OnResult_Internal(nullptr, ERuntimeImportStatus::InvalidAudioFormat);
			return;
		}

		FEncodedAudioStruct EncodedAudioInfo(AudioData, AudioFormat);

		OnProgress_Internal(25);

		FDecodedAudioStruct DecodedAudioInfo;
		if (!DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
		{
			OnResult_Internal(nullptr, ERuntimeImportStatus::FailedToReadAudioDataArray);
			return;
		}

		OnProgress_Internal(65);

		ImportAudioFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, FilePath, RAWFormat, SampleRate, NumOfChannels]() mutable
	{
		if (!FPaths::FileExists(FilePath))
		{
			OnResult_Internal(nullptr, ERuntimeImportStatus::AudioDoesNotExist);
			return;
		}

		OnProgress_Internal(5);

		TArray64<uint8> AudioBuffer;
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			OnResult_Internal(nullptr, ERuntimeImportStatus::LoadFileToArrayError);
			return;
		}

		OnProgress_Internal(35);
		ImportAudioFromRAWBuffer(MoveTemp(AudioBuffer), RAWFormat, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	ImportAudioFromRAWBuffer(TArray64<uint8>(MoveTemp(RAWBuffer)), RAWFormat, SampleRate, NumOfChannels);
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray64<uint8> RAWBuffer, ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	uint8* ByteDataPtr = RAWBuffer.GetData();
	const int64 ByteDataSize = RAWBuffer.Num();

	float* Float32DataPtr = nullptr;
	int64 NumOfSamples = 0;

	OnProgress_Internal(15);

	// Transcoding RAW data to 32-bit float data
	{
		switch (RAWFormat)
		{
		case ERuntimeRAWAudioFormat::Int8:
			{
				NumOfSamples = ByteDataSize / sizeof(int8);
				FRAW_RuntimeCodec::TranscodeRAWData<int8, float>(reinterpret_cast<int8*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt8:
			{
				NumOfSamples = ByteDataSize / sizeof(uint8);
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, float>(ByteDataPtr, NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::Int16:
			{
				NumOfSamples = ByteDataSize / sizeof(int16);
				FRAW_RuntimeCodec::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt16:
			{
				NumOfSamples = ByteDataSize / sizeof(uint16);
				FRAW_RuntimeCodec::TranscodeRAWData<uint16, float>(reinterpret_cast<uint16*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt32:
			{
				NumOfSamples = ByteDataSize / sizeof(uint32);
				FRAW_RuntimeCodec::TranscodeRAWData<uint32, float>(reinterpret_cast<uint32*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::Int32:
			{
				NumOfSamples = ByteDataSize / sizeof(int32);
				FRAW_RuntimeCodec::TranscodeRAWData<int32, float>(reinterpret_cast<int32*>(ByteDataPtr), NumOfSamples, Float32DataPtr);
				break;
			}
		case ERuntimeRAWAudioFormat::Float32:
			{
				NumOfSamples = ByteDataSize / sizeof(float);
				Float32DataPtr = static_cast<float*>(FMemory::Memcpy(FMemory::Malloc(ByteDataSize), ByteDataPtr, ByteDataSize));
				break;
			}
		}
	}

	OnProgress_Internal(35);
	if (!Float32DataPtr || NumOfSamples <= 0)
	{
		OnResult_Internal(nullptr, ERuntimeImportStatus::FailedToReadAudioDataArray);
		return;
	}

	ImportAudioFromFloat32Buffer(FRuntimeBulkDataBuffer<float>(Float32DataPtr, NumOfSamples), SampleRate, NumOfChannels);
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray<uint8> RAWDataFrom, ERuntimeRAWAudioFormat RAWFormatFrom, ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResult& Result)
{
	TranscodeRAWDataFromBuffer(TArray64<uint8>(MoveTemp(RAWDataFrom)), RAWFormatFrom, RAWFormatTo, FOnRAWDataTranscodeFromBufferResultNative::CreateLambda([Result](bool bSucceeded, const TArray64<uint8>& RAWData)
	{
		Result.ExecuteIfBound(bSucceeded, TArray<uint8>(RAWData));
	}));
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray64<uint8> RAWDataFrom, ERuntimeRAWAudioFormat RAWFormatFrom, ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [RAWDataFrom = MoveTemp(RAWDataFrom), RAWFormatFrom, RAWFormatTo, Result]() mutable
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
		case ERuntimeRAWAudioFormat::Int8:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<int8, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt8:
			{
				IntermediateRAWBuffer = MoveTemp(RAWDataFrom);
				break;
			}
		case ERuntimeRAWAudioFormat::Int16:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<int16, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt16:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint16, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERuntimeRAWAudioFormat::Int32:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<int32, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt32:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint32, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		case ERuntimeRAWAudioFormat::Float32:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<float, uint8>(RAWDataFrom, IntermediateRAWBuffer);
				break;
			}
		}

		TArray64<uint8> RAWData_To;

		// Transcoding unsigned 8-bit PCM to the specified format
		switch (RAWFormatTo)
		{
		case ERuntimeRAWAudioFormat::Int8:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, int8>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt8:
			{
				RAWData_To = MoveTemp(IntermediateRAWBuffer);
				break;
			}
		case ERuntimeRAWAudioFormat::Int16:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, int16>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt16:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, uint16>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERuntimeRAWAudioFormat::Int32:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, int32>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERuntimeRAWAudioFormat::UInt32:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, uint32>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		case ERuntimeRAWAudioFormat::Float32:
			{
				FRAW_RuntimeCodec::TranscodeRAWData<uint8, float>(IntermediateRAWBuffer, RAWData_To);
				break;
			}
		}

		ExecuteResult(true, MoveTemp(RAWData_To));
	});
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromFile(const FString& FilePathFrom, ERuntimeRAWAudioFormat RAWFormatFrom, const FString& FilePathTo, ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResult& Result)
{
	TranscodeRAWDataFromFile(FilePathFrom, RAWFormatFrom, FilePathTo, RAWFormatTo, FOnRAWDataTranscodeFromFileResultNative::CreateLambda([Result](bool bSucceeded)
	{
		Result.ExecuteIfBound(bSucceeded);
	}));
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromFile(const FString& FilePathFrom, ERuntimeRAWAudioFormat RAWFormatFrom, const FString& FilePathTo, ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResultNative& Result)
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

void URuntimeAudioImporterLibrary::ExportSoundWaveToFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResult& Result)
{
	if (!IsValid(ImportedSoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
		Result.ExecuteIfBound(false);
		return;
	}

	ExportSoundWaveToFile(ImportedSoundWave, SavePath, AudioFormat, Quality, OverrideOptions, FOnAudioExportToFileResultNative::CreateLambda([Result](bool bSucceeded)
	{
		Result.ExecuteIfBound(bSucceeded);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResultNative& Result)
{
	AudioFormat = AudioFormat == ERuntimeAudioFormat::Auto ? GetAudioFormat(SavePath) : AudioFormat;
	ExportSoundWaveToBuffer(ImportedSoundWavePtr, AudioFormat, Quality, OverrideOptions, FOnAudioExportToBufferResultNative::CreateLambda([Result, SavePath](bool bSucceeded, const TArray64<uint8>& AudioData)
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

void URuntimeAudioImporterLibrary::ExportSoundWaveToBuffer(UImportedSoundWave* ImportedSoundWave, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResult& Result)
{
	if (!IsValid(ImportedSoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
		Result.ExecuteIfBound(false, TArray<uint8>());
		return;
	}

	ExportSoundWaveToBuffer(ImportedSoundWave, AudioFormat, Quality, OverrideOptions, FOnAudioExportToBufferResultNative::CreateLambda([Result](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (AudioData.Num() > TNumericLimits<int32>::Max())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Array with int32 size (max length: %d) cannot fit int64 size data (retrieved length: %lld)\nA standard byte array can hold a maximum of 2 GB of data"), TNumericLimits<int32>::Max(), AudioData.Num());
			Result.ExecuteIfBound(false, TArray<uint8>());
			return;
		}

		Result.ExecuteIfBound(bSucceeded, TArray<uint8>(AudioData));
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ImportedSoundWavePtr, AudioFormat, Quality, OverrideOptions, Result]
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
					SoundWaveBasicInfo.NumOfChannels = ImportedSoundWavePtr->GetNumOfChannels();
					SoundWaveBasicInfo.SampleRate = ImportedSoundWavePtr->GetSampleRate();
					SoundWaveBasicInfo.Duration = ImportedSoundWavePtr->GetDurationConst_Internal();
				}
				DecodedAudioInfo.SoundWaveBasicInfo = MoveTemp(SoundWaveBasicInfo);
			}
		}

		FEncodedAudioStruct EncodedAudioInfo;
		{
			EncodedAudioInfo.AudioFormat = AudioFormat;
		}

		// Check if the number of channels and the sampling rate of the sound wave and desired override options are not the same
		if (OverrideOptions.IsOverriden() && (ImportedSoundWavePtr->GetSampleRate() != OverrideOptions.SampleRate || ImportedSoundWavePtr->GetNumOfChannels() != OverrideOptions.NumOfChannels))
		{
			Audio::FAlignedFloatBuffer WaveData(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());

			// Resampling if needed
			if (OverrideOptions.IsSampleRateOverriden() && ImportedSoundWavePtr->GetSampleRate() != OverrideOptions.SampleRate)
			{
				Audio::FAlignedFloatBuffer ResamplerOutputData;
				if (!FRAW_RuntimeCodec::ResampleRAWData(WaveData, ImportedSoundWavePtr->GetNumOfChannels(), ImportedSoundWavePtr->GetSampleRate(), OverrideOptions.SampleRate, ResamplerOutputData))
				{
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to resample audio data to the overriden sample rate. Resampling failed"));
					ExecuteResult(false, TArray64<uint8>());
					return;
				}

				WaveData = MoveTemp(ResamplerOutputData);
				DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = OverrideOptions.SampleRate;
			}

			// Mixing the channels if needed
			if (OverrideOptions.IsNumOfChannelsOverriden() && ImportedSoundWavePtr->GetNumOfChannels() != OverrideOptions.NumOfChannels)
			{
				Audio::FAlignedFloatBuffer WaveDataTemp;
				if (!FRAW_RuntimeCodec::MixChannelsRAWData(WaveData, OverrideOptions.SampleRate, DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels, OverrideOptions.NumOfChannels, WaveDataTemp))
				{
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mix audio channels to the overriden number of channels. Mixing failed"));
					ExecuteResult(false, TArray64<uint8>());
					return;
				}
				WaveData = MoveTemp(WaveDataTemp);
				DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = OverrideOptions.NumOfChannels;
			}

			DecodedAudioInfo.PCMInfo.PCMNumOfFrames = WaveData.Num() / DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
			DecodedAudioInfo.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(WaveData);
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

void URuntimeAudioImporterLibrary::ExportSoundWaveToRAWFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResult& Result)
{
	if (!IsValid(ImportedSoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
		Result.ExecuteIfBound(false);
		return;
	}

	ExportSoundWaveToRAWFile(ImportedSoundWave, SavePath, RAWFormat, OverrideOptions, FOnAudioExportToFileResultNative::CreateLambda([Result](bool bSucceeded)
	{
		Result.ExecuteIfBound(bSucceeded);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToRAWFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResultNative& Result)
{
	ExportSoundWaveToRAWBuffer(ImportedSoundWavePtr, RAWFormat, OverrideOptions, FOnAudioExportToBufferResultNative::CreateLambda([Result, SavePath](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (!bSucceeded)
		{
			Result.ExecuteIfBound(false);
			return;
		}

		if (!FFileHelper::SaveArrayToFile(AudioData, *SavePath))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving RAW audio data to the path '%s'"), *SavePath);
			Result.ExecuteIfBound(false);
			return;
		}

		Result.ExecuteIfBound(true);
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToRAWBuffer(UImportedSoundWave* ImportedSoundWave, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResult& Result)
{
	if (!IsValid(ImportedSoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as it is invalid"));
		Result.ExecuteIfBound(false, TArray<uint8>());
		return;
	}

	ExportSoundWaveToRAWBuffer(ImportedSoundWave, RAWFormat, OverrideOptions, FOnAudioExportToBufferResultNative::CreateLambda([Result](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (AudioData.Num() > TNumericLimits<int32>::Max())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Array with int32 size (max length: %d) cannot fit int64 size data (retrieved length: %lld)\nA standard byte array can hold a maximum of 2 GB of data"), TNumericLimits<int32>::Max(), AudioData.Num());
			Result.ExecuteIfBound(false, TArray<uint8>());
			return;
		}

		Result.ExecuteIfBound(bSucceeded, TArray<uint8>(AudioData));
	}));
}

void URuntimeAudioImporterLibrary::ExportSoundWaveToRAWBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ImportedSoundWavePtr, RAWFormat, OverrideOptions, Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, TArray64<uint8> AudioData)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, AudioData = MoveTemp(AudioData)]() mutable
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
		FScopeLock Lock(&ImportedSoundWavePtr->DataGuard);

		if (!ImportedSoundWavePtr->GetPCMBuffer().IsValid())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave as the PCM data is invalid"));
			ExecuteResult(false, TArray64<uint8>());
			return;
		}

		TArray64<uint8> RAWDataFrom;

		// Check if the number of channels and the sampling rate of the sound wave and desired override options are not the same
		if (OverrideOptions.IsOverriden() && (ImportedSoundWavePtr->GetSampleRate() != OverrideOptions.SampleRate || ImportedSoundWavePtr->GetNumOfChannels() != OverrideOptions.NumOfChannels))
		{
			Audio::FAlignedFloatBuffer WaveData(ImportedSoundWavePtr->GetPCMBuffer().PCMData.GetView().GetData(), ImportedSoundWavePtr->GetPCMBuffer().PCMData.GetView().Num());

			// Resampling if needed
			if (OverrideOptions.IsSampleRateOverriden() && ImportedSoundWavePtr->GetSampleRate() != OverrideOptions.SampleRate)
			{
				Audio::FAlignedFloatBuffer ResamplerOutputData;
				if (!FRAW_RuntimeCodec::ResampleRAWData(WaveData, ImportedSoundWavePtr->GetNumOfChannels(), ImportedSoundWavePtr->GetSampleRate(), OverrideOptions.SampleRate, ResamplerOutputData))
				{
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to resample audio data to the overriden sample rate. Resampling failed"));
					ExecuteResult(false, TArray64<uint8>());
					return;
				}

				WaveData = MoveTemp(ResamplerOutputData);
			}

			// Mixing the channels if needed
			if (OverrideOptions.IsNumOfChannelsOverriden() && ImportedSoundWavePtr->GetNumOfChannels() != OverrideOptions.NumOfChannels)
			{
				Audio::FAlignedFloatBuffer WaveDataTemp;
				if (!FRAW_RuntimeCodec::MixChannelsRAWData(WaveData, OverrideOptions.SampleRate, ImportedSoundWavePtr->GetNumOfChannels(), OverrideOptions.NumOfChannels, WaveDataTemp))
				{
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mix audio channels to the overriden number of channels. Mixing failed"));
					ExecuteResult(false, TArray64<uint8>());
					return;
				}
				WaveData = MoveTemp(WaveDataTemp);
			}

			RAWDataFrom = TArray64<uint8>(reinterpret_cast<uint8*>(WaveData.GetData()), WaveData.Num() * sizeof(float));
		}
		else
		{
			RAWDataFrom = TArray64<uint8>(reinterpret_cast<uint8*>(ImportedSoundWavePtr->GetPCMBuffer().PCMData.GetView().GetData()), ImportedSoundWavePtr->GetPCMBuffer().PCMData.GetView().Num() * sizeof(float));
		}

		TranscodeRAWDataFromBuffer(MoveTemp(RAWDataFrom), ERuntimeRAWAudioFormat::Float32, RAWFormat, FOnRAWDataTranscodeFromBufferResultNative::CreateWeakLambda(ImportedSoundWavePtr.Get(), [Result, ExecuteResult](bool bSucceeded, const TArray64<uint8>& RAWData)
		{
			ExecuteResult(bSucceeded, RAWData);
		}));
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
		OnResult_Internal(nullptr, ERuntimeImportStatus::SoundWaveDeclarationError);
		return;
	}

	FGCObjectScopeGuard ImportedSoundWaveGuard(ImportedSoundWave);

	OnProgress_Internal(75);

	ImportedSoundWave->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("The audio data was successfully imported"));

	OnProgress_Internal(100);

	OnResult_Internal(ImportedSoundWave, ERuntimeImportStatus::SuccessfulImport);
}

ERuntimeAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const FString& FilePath)
{
	FRuntimeCodecFactory CodecFactory;
	TUniquePtr<FBaseRuntimeCodec> RuntimeCodec = CodecFactory.GetCodec(FilePath);

	if (!RuntimeCodec.IsValid())
	{
		return ERuntimeAudioFormat::Invalid;
	}

	return RuntimeCodec->GetAudioFormat();
}

ERuntimeAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const TArray<uint8>& AudioData)
{
	return GetAudioFormatAdvanced(FRuntimeBulkDataBuffer<uint8>(AudioData));
}

void URuntimeAudioImporterLibrary::GetAudioHeaderInfoFromFile(const FString& FilePath, const FOnGetAudioHeaderInfoResult& Result)
{
	GetAudioHeaderInfoFromFile(FilePath, FOnGetAudioHeaderInfoResultNative::CreateLambda([Result](bool bSucceeded, const FRuntimeAudioHeaderInfo& HeaderInfo)
	{
		Result.ExecuteIfBound(bSucceeded, HeaderInfo);
	}));
}

void URuntimeAudioImporterLibrary::GetAudioHeaderInfoFromFile(const FString& FilePath, const FOnGetAudioHeaderInfoResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [FilePath, Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, FRuntimeAudioHeaderInfo&& HeaderInfo)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, HeaderInfo = MoveTemp(HeaderInfo)]()
			{
				Result.ExecuteIfBound(bSucceeded, HeaderInfo);
			});
		};

		TArray64<uint8> AudioBuffer;
		if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			ExecuteResult(false, FRuntimeAudioHeaderInfo());
			return;
		}

		GetAudioHeaderInfoFromBuffer(MoveTemp(AudioBuffer), FOnGetAudioHeaderInfoResultNative::CreateLambda([Result](bool bSucceeded, const FRuntimeAudioHeaderInfo& HeaderInfo)
		{
			Result.ExecuteIfBound(bSucceeded, HeaderInfo);
		}));
	});
}

void URuntimeAudioImporterLibrary::GetAudioHeaderInfoFromBuffer(TArray<uint8> AudioData, const FOnGetAudioHeaderInfoResult& Result)
{
	GetAudioHeaderInfoFromBuffer(TArray64<uint8>(MoveTemp(AudioData)), FOnGetAudioHeaderInfoResultNative::CreateLambda([Result](bool bSucceeded, const FRuntimeAudioHeaderInfo& HeaderInfo)
	{
		Result.ExecuteIfBound(bSucceeded, HeaderInfo);
	}));
}

void URuntimeAudioImporterLibrary::GetAudioHeaderInfoFromBuffer(TArray64<uint8> AudioData, const FOnGetAudioHeaderInfoResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [AudioData = MoveTemp(AudioData), Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, FRuntimeAudioHeaderInfo&& HeaderInfo)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, HeaderInfo = MoveTemp(HeaderInfo)]()
			{
				Result.ExecuteIfBound(bSucceeded, HeaderInfo);
			});
		};

		FRuntimeBulkDataBuffer<uint8> BulkAudioData = FRuntimeBulkDataBuffer<uint8>(AudioData);

		FRuntimeCodecFactory CodecFactory;
		TUniquePtr<FBaseRuntimeCodec> RuntimeCodec = CodecFactory.GetCodec(BulkAudioData);

		if (!RuntimeCodec.IsValid())
		{
			ExecuteResult(false, FRuntimeAudioHeaderInfo());
			return;
		}

		FEncodedAudioStruct EncodedData;
		{
			EncodedData.AudioData = MoveTemp(BulkAudioData);
			EncodedData.AudioFormat = RuntimeCodec->GetAudioFormat();
		}

		FRuntimeAudioHeaderInfo HeaderInfo;
		if (!RuntimeCodec->GetHeaderInfo(MoveTemp(EncodedData), HeaderInfo))
		{
			ExecuteResult(false, FRuntimeAudioHeaderInfo());
			return;
		}

		ExecuteResult(true, MoveTemp(HeaderInfo));
	});
}

ERuntimeAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const TArray64<uint8>& AudioData)
{
	return GetAudioFormatAdvanced(FRuntimeBulkDataBuffer<uint8>(AudioData));
}

ERuntimeAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	FRuntimeCodecFactory CodecFactory;
	TUniquePtr<FBaseRuntimeCodec> RuntimeCodec = CodecFactory.GetCodec(AudioData);

	if (!RuntimeCodec.IsValid())
	{
		return ERuntimeAudioFormat::Invalid;
	}

	return RuntimeCodec->GetAudioFormat();
}

void URuntimeAudioImporterLibrary::ImportAudioFromFloat32Buffer(FRuntimeBulkDataBuffer<float>&& PCMData, int32 SampleRate, int32 NumOfChannels)
{
	FDecodedAudioStruct DecodedAudioInfo;
	{
		DecodedAudioInfo.PCMInfo.PCMData = MoveTemp(PCMData);
		DecodedAudioInfo.PCMInfo.PCMNumOfFrames = DecodedAudioInfo.PCMInfo.PCMData.GetView().Num() / NumOfChannels;

		DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
		DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = SampleRate;
		DecodedAudioInfo.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SampleRate;
	}

	OnProgress_Internal(65);

	ImportAudioFromDecodedInfo(MoveTemp(DecodedAudioInfo));
}

FString URuntimeAudioImporterLibrary::ConvertSecondsToString(int64 Seconds)
{
	FString FinalString;

	const int64 NewHours = Seconds / 3600;
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

void URuntimeAudioImporterLibrary::ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResult& Result)
{
	ScanDirectoryForAudioFiles(Directory, bRecursive, FOnScanDirectoryForAudioFilesResultNative::CreateLambda([Result](bool bSucceeded, const TArray<FString>& AudioFilePaths)
	{
		Result.ExecuteIfBound(bSucceeded, AudioFilePaths);
	}));
}

void URuntimeAudioImporterLibrary::ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [Directory, bRecursive, Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, TArray<FString>&& AudioFilePaths)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, AudioFilePaths = MoveTemp(AudioFilePaths)]()
			{
				Result.ExecuteIfBound(bSucceeded, AudioFilePaths);
			});
		};

		class FDirectoryVisitor_AudioScanner : public IPlatformFile::FDirectoryVisitor
		{
		public:
			FRuntimeCodecFactory CodecFactory;
			TArray<FString> AudioFilePaths;

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (bIsDirectory)
				{
					return true;
				}

				FString AudioFilePath = FilenameOrDirectory;
				if (CodecFactory.GetCodec(AudioFilePath).IsValid())
				{
					AudioFilePaths.Add(MoveTemp(AudioFilePath));
				}

				return true;
			}
		};

		FDirectoryVisitor_AudioScanner DirectoryVisitor_AudioScanner;
		const bool bSucceeded = [bRecursive, &Directory, &DirectoryVisitor_AudioScanner]()
		{
			if (bRecursive)
			{
				return FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*Directory, DirectoryVisitor_AudioScanner);
			}
			return FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*Directory, DirectoryVisitor_AudioScanner);
		}();

		ExecuteResult(bSucceeded, MoveTemp(DirectoryVisitor_AudioScanner.AudioFilePaths));
	});
}

bool URuntimeAudioImporterLibrary::DecodeAudioData(FEncodedAudioStruct&& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo)
{
	FRuntimeCodecFactory CodecFactory;
	TUniquePtr<FBaseRuntimeCodec> RuntimeCodec = [&EncodedAudioInfo, &CodecFactory]()
	{
		if (EncodedAudioInfo.AudioFormat == ERuntimeAudioFormat::Auto)
		{
			return CodecFactory.GetCodec(EncodedAudioInfo.AudioData);
		}
		return CodecFactory.GetCodec(EncodedAudioInfo.AudioFormat);
	}();

	if (!RuntimeCodec)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for decoding"));
		return false;
	}

	EncodedAudioInfo.AudioFormat = RuntimeCodec->GetAudioFormat();
	if (!RuntimeCodec->Decode(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding '%s' audio data"), *UEnum::GetValueAsString(EncodedAudioInfo.AudioFormat));
		return false;
	}

	return true;
}

bool URuntimeAudioImporterLibrary::EncodeAudioData(FDecodedAudioStruct&& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality)
{
	if (EncodedAudioInfo.AudioFormat == ERuntimeAudioFormat::Auto || EncodedAudioInfo.AudioFormat == ERuntimeAudioFormat::Invalid)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for encoding"));
		return false;
	}

	FRuntimeCodecFactory CodecFactory;
	TUniquePtr<FBaseRuntimeCodec> RuntimeCodec = CodecFactory.GetCodec(EncodedAudioInfo.AudioFormat);
	if (!RuntimeCodec.IsValid())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for encoding"));
		return false;
	}

	if (!RuntimeCodec->Encode(MoveTemp(DecodedAudioInfo), EncodedAudioInfo, Quality))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding '%s' audio data"), *UEnum::GetValueAsString(EncodedAudioInfo.AudioFormat));
		return false;
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

void URuntimeAudioImporterLibrary::OnResult_Internal(UImportedSoundWave* ImportedSoundWave, ERuntimeImportStatus Status)
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
