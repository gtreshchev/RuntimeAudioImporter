// Georgy Treshchev 2024.


#include "RuntimeAudioExporter.h"

#include "RuntimeAudioTranscoder.h"
#include "RuntimeAudioUtilities.h"
#include "Codecs/RAW_RuntimeCodec.h"
#include "Async/Async.h"

void URuntimeAudioExporter::ExportSoundWaveToFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResult& Result)
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

void URuntimeAudioExporter::ExportSoundWaveToFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResultNative& Result)
{
	TArray<ERuntimeAudioFormat> AudioFormats = URuntimeAudioUtilities::GetAudioFormats(SavePath);
	AudioFormat = AudioFormat == ERuntimeAudioFormat::Auto ? (AudioFormats.Num() == 0 ? ERuntimeAudioFormat::Invalid : AudioFormats[0]) : AudioFormat;
	ExportSoundWaveToBuffer(ImportedSoundWavePtr, AudioFormat, Quality, OverrideOptions, FOnAudioExportToBufferResultNative::CreateLambda([Result, SavePath](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (!bSucceeded)
		{
			Result.ExecuteIfBound(false);
			return;
		}

		if (!RuntimeAudioImporter::SaveAudioFileFromArray(AudioData, *SavePath))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving audio data to the path '%s'"), *SavePath);
			Result.ExecuteIfBound(false);
			return;
		}

		Result.ExecuteIfBound(true);
	}));
}

void URuntimeAudioExporter::ExportSoundWaveToBuffer(UImportedSoundWave* ImportedSoundWave, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResult& Result)
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
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave to buffer: array with int32 size (max length: %d) cannot fit int64 size data (retrieved length: %lld)\nA standard byte array can hold a maximum of 2 GB of data"), TNumericLimits<int32>::Max(), AudioData.Num());
			Result.ExecuteIfBound(false, TArray<uint8>());
			return;
		}

		Result.ExecuteIfBound(bSucceeded, TArray<uint8>(AudioData));
	}));
}

void URuntimeAudioExporter::ExportSoundWaveToBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, ERuntimeAudioFormat AudioFormat, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResultNative& Result)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ImportedSoundWavePtr, AudioFormat, Quality, OverrideOptions, Result]()
		{
			ExportSoundWaveToBuffer(ImportedSoundWavePtr, AudioFormat, Quality, OverrideOptions, Result);
		});
		return;
	}

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

	FDecodedAudioStruct DecodedAudioInfo;
	{
		FRAIScopeLock Lock(&*ImportedSoundWavePtr->DataGuard);

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

	if (!URuntimeAudioImporterLibrary::EncodeAudioData(MoveTemp(DecodedAudioInfo), EncodedAudioInfo, Quality))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to export sound wave '%s'"), *ImportedSoundWavePtr->GetName());
		ExecuteResult(false, TArray64<uint8>());
		return;
	}

	ExecuteResult(true, TArray64<uint8>(EncodedAudioInfo.AudioData.GetView().GetData(), EncodedAudioInfo.AudioData.GetView().Num()));
}

void URuntimeAudioExporter::ExportSoundWaveToRAWFile(UImportedSoundWave* ImportedSoundWave, const FString& SavePath, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResult& Result)
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

void URuntimeAudioExporter::ExportSoundWaveToRAWFile(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, const FString& SavePath, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToFileResultNative& Result)
{
	ExportSoundWaveToRAWBuffer(ImportedSoundWavePtr, RAWFormat, OverrideOptions, FOnAudioExportToBufferResultNative::CreateLambda([Result, SavePath](bool bSucceeded, const TArray64<uint8>& AudioData)
	{
		if (!bSucceeded)
		{
			Result.ExecuteIfBound(false);
			return;
		}

		if (!RuntimeAudioImporter::SaveAudioFileFromArray(AudioData, *SavePath))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving RAW audio data to the path '%s'"), *SavePath);
			Result.ExecuteIfBound(false);
			return;
		}

		Result.ExecuteIfBound(true);
	}));
}

void URuntimeAudioExporter::ExportSoundWaveToRAWBuffer(UImportedSoundWave* ImportedSoundWave, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResult& Result)
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

void URuntimeAudioExporter::ExportSoundWaveToRAWBuffer(TWeakObjectPtr<UImportedSoundWave> ImportedSoundWavePtr, ERuntimeRAWAudioFormat RAWFormat, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnAudioExportToBufferResultNative& Result)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ImportedSoundWavePtr, RAWFormat, OverrideOptions, Result]()
		{
			ExportSoundWaveToRAWBuffer(ImportedSoundWavePtr, RAWFormat, OverrideOptions, Result);
		});
		return;
	}

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

	FRAIScopeLock Lock(&*ImportedSoundWavePtr->DataGuard);

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

	URuntimeAudioTranscoder::TranscodeRAWDataFromBuffer(MoveTemp(RAWDataFrom), ERuntimeRAWAudioFormat::Float32, RAWFormat, FOnRAWDataTranscodeFromBufferResultNative::CreateWeakLambda(ImportedSoundWavePtr.Get(), [ExecuteResult](bool bSucceeded, const TArray64<uint8>& RAWData)
	{
		ExecuteResult(bSucceeded, RAWData);
	}));
}