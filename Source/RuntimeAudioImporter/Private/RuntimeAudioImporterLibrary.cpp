// Georgy Treshchev 2024.

#include "RuntimeAudioImporterLibrary.h"

#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "PreImportedSoundAsset.h"
#include "RuntimeAudioTranscoder.h"
#include "RuntimeAudioUtilities.h"

#include "Codecs/RAW_RuntimeCodec.h"

#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"
#include "Codecs/RuntimeCodecFactory.h"
#include "Engine/Engine.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Templates/SharedPointer.h"
#include "SampleBuffer.h"
#include "Misc/Paths.h"

#include "Interfaces/IAudioFormat.h"

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	return NewObject<URuntimeAudioImporterLibrary>();
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, ERuntimeAudioFormat AudioFormat)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = MakeWeakObjectPtr(this), FilePath, AudioFormat]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ImportAudioFromFile(FilePath, AudioFormat);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to import audio from file '%s' because the RuntimeAudioImporterLibrary object has been destroyed"), *FilePath);
			}
		});
		return;
	}

	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ERuntimeImportStatus::AudioDoesNotExist);
		return;
	}

	TArray<ERuntimeAudioFormat> PossibleFormats = URuntimeAudioUtilities::GetAudioFormats(FilePath);

	AudioFormat = AudioFormat == ERuntimeAudioFormat::Auto ? (PossibleFormats.Num() == 0 ? ERuntimeAudioFormat::Invalid : PossibleFormats[0]) : AudioFormat;
	AudioFormat = AudioFormat == ERuntimeAudioFormat::Invalid ? ERuntimeAudioFormat::Auto : AudioFormat;

	TArray64<uint8> AudioBuffer;
	if (!RuntimeAudioImporter::LoadAudioFileToArray(AudioBuffer, *FilePath))
	{
		OnResult_Internal(nullptr, ERuntimeImportStatus::LoadFileToArrayError);
		return;
	}

	ImportAudioFromBuffer(MoveTemp(AudioBuffer), AudioFormat);
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
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = MakeWeakObjectPtr(this), AudioData = MoveTemp(AudioData), AudioFormat]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ImportAudioFromBuffer(MoveTemp(AudioData), AudioFormat);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to import audio from buffer because the RuntimeAudioImporterLibrary object has been destroyed"));
			}
		});
		return;
	}

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
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERuntimeRAWAudioFormat RAWFormat, int32 SampleRate, int32 NumOfChannels)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis = MakeWeakObjectPtr(this), FilePath, RAWFormat, SampleRate, NumOfChannels]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ImportAudioFromRAWFile(FilePath, RAWFormat, SampleRate, NumOfChannels);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to import RAW audio from file '%s' because the RuntimeAudioImporterLibrary object has been destroyed"), *FilePath);
			}
		});
		return;
	}

	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ERuntimeImportStatus::AudioDoesNotExist);
		return;
	}

	OnProgress_Internal(5);

	TArray64<uint8> AudioBuffer;
	if (!RuntimeAudioImporter::LoadAudioFileToArray(AudioBuffer, *FilePath))
	{
		OnResult_Internal(nullptr, ERuntimeImportStatus::LoadFileToArrayError);
		return;
	}

	OnProgress_Internal(35);
	ImportAudioFromRAWBuffer(MoveTemp(AudioBuffer), RAWFormat, SampleRate, NumOfChannels);
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

void URuntimeAudioImporterLibrary::ConvertRegularToImportedSoundWave(USoundWave* SoundWave, TSubclassOf<UImportedSoundWave> ImportedSoundWaveClass, const FOnRegularToAudioImporterSoundWaveConvertResult& Result)
{
	ConvertRegularToImportedSoundWave(SoundWave, ImportedSoundWaveClass, FOnRegularToAudioImporterSoundWaveConvertResultNative::CreateLambda([Result](bool bSucceeded, UImportedSoundWave* ImportedSoundWave)
	{
		Result.ExecuteIfBound(bSucceeded, ImportedSoundWave);
	}));
}

bool URuntimeAudioImporterLibrary::TryToRetrieveSoundWaveData(USoundWave* SoundWave, FDecodedAudioStruct& OutDecodedAudioInfo)
{
	auto TryRetrieveFromCompressedData = [&OutDecodedAudioInfo](FRuntimeBulkDataBuffer<uint8> BulkAudioData) -> bool
	{
		FDecodedAudioStruct DecodedAudioInfo;
		FEncodedAudioStruct EncodedAudioInfo(MoveTemp(BulkAudioData), ERuntimeAudioFormat::Auto);
		if (!DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
		{
			return false;
		}

		OutDecodedAudioInfo = MoveTemp(DecodedAudioInfo);
		return true;
	};

	if (SoundWave->RawPCMData && SoundWave->RawPCMDataSize > 0)
	{
		float* TempFloatBuffer;
		const int64 NumOfSamples = SoundWave->RawPCMDataSize / sizeof(int16);
		FRAW_RuntimeCodec::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(SoundWave->RawPCMData), SoundWave->RawPCMDataSize, TempFloatBuffer);

		if (NumOfSamples > 0)
		{
			OutDecodedAudioInfo.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(TempFloatBuffer, NumOfSamples);
			OutDecodedAudioInfo.PCMInfo.PCMNumOfFrames = NumOfSamples / SoundWave->NumChannels;
			OutDecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = SoundWave->NumChannels;
			OutDecodedAudioInfo.SoundWaveBasicInfo.SampleRate = SoundWave->GetSampleRateForCurrentPlatform();
			OutDecodedAudioInfo.SoundWaveBasicInfo.Duration = static_cast<float>(OutDecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SoundWave->GetSampleRateForCurrentPlatform();

			return true;
		}
	}

	if (SoundWave->CookedPlatformData.Num() > 0)
	{
		const TSortedMap<FString, FStreamedAudioPlatformData*> CookedPlatformData_Local = SoundWave->CookedPlatformData;

		for (const auto& PlatformData : CookedPlatformData_Local)
		{
			if (PlatformData.Value)
			{
#if UE_VERSION_NEWER_THAN(5, 1, 0)
				TIndirectArray<struct FStreamedAudioChunk>& Chunks = PlatformData.Value->GetChunks();
#else
				TIndirectArray<struct FStreamedAudioChunk> Chunks = PlatformData.Value->Chunks;
#endif
				if (Chunks.Num() <= 0)
				{
					continue;
				}

				const int64 TotalDataSize = [&Chunks]()
				{
					int64 InternalTotalDataSize = 0;
					for (const FStreamedAudioChunk& Chunk : Chunks)
					{
						InternalTotalDataSize += Chunk.DataSize;
					}
					return InternalTotalDataSize;
				}();

				if (TotalDataSize <= 0)
				{
					continue;
				}

				uint8* BufferPtr = static_cast<uint8*>(FMemory::Malloc(TotalDataSize));

				int64 Offset = 0;
				for (FStreamedAudioChunk& Chunk : Chunks)
				{
					Chunk.BulkData.GetCopy(reinterpret_cast<void**>(&BufferPtr + Offset), false);
					Offset += Chunk.DataSize;
				}

				if (TryRetrieveFromCompressedData(FRuntimeBulkDataBuffer<uint8>(BufferPtr, TotalDataSize)))
				{
					return true;
				}
			}
		}
	}

	auto AudioDevice = GEngine->GetMainAudioDevice();
	if (!AudioDevice)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to get AudioDevice for converting SoundWave to ImportedSoundWave"));
		return false;
	}

	const FName RuntimeFormat =
#if UE_VERSION_NEWER_THAN(5, 2, 0)
	SoundWave->GetRuntimeFormat();
#else
	AudioDevice->GetRuntimeFormat(SoundWave);
#endif

	FByteBulkData* Bulk = SoundWave->GetCompressedData(RuntimeFormat, SoundWave->GetPlatformCompressionOverridesForCurrentPlatform());
	if (Bulk)
	{
		FRuntimeBulkDataBuffer<uint8> ResourceBuffer = [&Bulk]()
		{
			uint8* BufferPtr = nullptr;
			Bulk->GetCopy(reinterpret_cast<void**>(&BufferPtr), true);
			return MoveTempIfPossible(FRuntimeBulkDataBuffer<uint8>(BufferPtr, Bulk->GetElementCount()));
		}();

		if (TryRetrieveFromCompressedData(ResourceBuffer))
		{
			return true;
		}
	}

	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to convert SoundWave to ImportedSoundWave because SoundWave has no valid audio data"));
	return false;
}

void URuntimeAudioImporterLibrary::ConvertRegularToImportedSoundWave(USoundWave* SoundWave, TSubclassOf<UImportedSoundWave> ImportedSoundWaveClass, const FOnRegularToAudioImporterSoundWaveConvertResultNative& Result)
{
	if (IsInGameThread())
	{
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [SoundWave, ImportedSoundWaveClass, Result]()
		{
			ConvertRegularToImportedSoundWave(SoundWave, ImportedSoundWaveClass, Result);
		});
		return;
	}

	auto ExecuteResult = [Result](bool bSucceeded, UImportedSoundWave* ImportedSoundWave)
	{
		AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, ImportedSoundWave]() mutable
		{
			Result.ExecuteIfBound(bSucceeded, ImportedSoundWave);
		});
	};

	if (!IsValid(SoundWave))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to convert SoundWave to ImportedSoundWave because SoundWave is nullptr"));
		ExecuteResult(false, nullptr);
		return;
	}

	if (!ImportedSoundWaveClass)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to convert SoundWave to ImportedSoundWave because ImportedSoundWaveClass is nullptr"));
		ExecuteResult(false, nullptr);
		return;
	}

	FDecodedAudioStruct DecodedAudioInfo;
	if (!TryToRetrieveSoundWaveData(SoundWave, DecodedAudioInfo))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to convert SoundWave to ImportedSoundWave because failed to retrieve SoundWave data"));
		ExecuteResult(false, nullptr);
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [ExecuteResult, ImportedSoundWaveClass, DecodedAudioInfo = MoveTemp(DecodedAudioInfo)]() mutable
	{
		UImportedSoundWave* ImportedSoundWave = NewObject<UImportedSoundWave>((UObject*)GetTransientPackage(), ImportedSoundWaveClass);
		if (!ImportedSoundWave)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to create an imported sound wave from %s class for converting SoundWave to ImportedSoundWave"), *ImportedSoundWaveClass->GetName());
			ExecuteResult(false, nullptr);
			return;
		}

		ImportedSoundWave->AddToRoot();
		TSharedPtr<FDelegateHandle> Handle = MakeShareable<FDelegateHandle>(new FDelegateHandle());
		*Handle = ImportedSoundWave->OnPopulateAudioStateNative.AddLambda([Handle, ExecuteResult, ImportedSoundWave]()
		{
			ImportedSoundWave->RemoveFromRoot();
			ExecuteResult(true, ImportedSoundWave);
			ImportedSoundWave->OnPopulateAudioStateNative.Remove(*Handle);
		});

		ImportedSoundWave->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromDecodedInfo(FDecodedAudioStruct&& DecodedAudioInfo)
{
	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this), DecodedAudioInfo = MoveTemp(DecodedAudioInfo)]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ImportAudioFromDecodedInfo(MoveTemp(DecodedAudioInfo));
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to import audio from decoded info '%s' because the RuntimeAudioImporterLibrary object has been destroyed"), *DecodedAudioInfo.ToString());
			}
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

	ImportedSoundWave->AddToRoot();

	OnProgress_Internal(75);

	ImportedSoundWave->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("The audio data was successfully imported"));

	OnProgress_Internal(100);

	OnResult_Internal(ImportedSoundWave, ERuntimeImportStatus::SuccessfulImport);

	ImportedSoundWave->RemoveFromRoot();
}

bool URuntimeAudioImporterLibrary::ResampleAndMixChannelsInDecodedInfo(FDecodedAudioStruct& DecodedAudioInfo, uint32 NewSampleRate, uint32 NewNumOfChannels)
{
	if (DecodedAudioInfo.SoundWaveBasicInfo.SampleRate <= 0 || NewSampleRate <= 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to resample audio data because the sample rate is invalid (Current: %d, New: %d)"), DecodedAudioInfo.SoundWaveBasicInfo.SampleRate, NewSampleRate);
		return false;
	}

	if (DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels <= 0 || NewNumOfChannels <= 0)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mix audio data because the number of channels is invalid (Current: %d, New: %d)"), DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels, NewNumOfChannels);
		return false;
	}
	
	if (NewSampleRate == DecodedAudioInfo.SoundWaveBasicInfo.SampleRate && NewNumOfChannels == DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels)
	{
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("No need to resample or mix audio data"));
		return true;
	}
	
	Audio::FAlignedFloatBuffer WaveData(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData(), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num());

	// Resampling if needed
	if (NewSampleRate != DecodedAudioInfo.SoundWaveBasicInfo.SampleRate)
	{
		Audio::FAlignedFloatBuffer ResamplerOutputData;
		if (!FRAW_RuntimeCodec::ResampleRAWData(WaveData, DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels, DecodedAudioInfo.SoundWaveBasicInfo.SampleRate, NewSampleRate, ResamplerOutputData))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to resample audio data to the sound wave's sample rate. Resampling failed"));
			return false;
		}
		WaveData = MoveTemp(ResamplerOutputData);
		DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = NewSampleRate;
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Audio data has been resampled to the desired sample rate '%d'"), NewSampleRate);
	}

	// Mixing the channels if needed
	if (NewNumOfChannels != DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels)
	{
		Audio::FAlignedFloatBuffer WaveDataTemp;
		if (!FRAW_RuntimeCodec::MixChannelsRAWData(WaveData, NewSampleRate, DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels, NewNumOfChannels, WaveDataTemp))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mix audio data to the sound wave's number of channels. Mixing failed"));
			return false;
		}
		WaveData = MoveTemp(WaveDataTemp);
		DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = NewNumOfChannels;
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Audio data has been mixed to the desired number of channels '%d'"), NewNumOfChannels);
	}

	DecodedAudioInfo.PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(WaveData);
	return true;
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

bool URuntimeAudioImporterLibrary::DecodeAudioData(FEncodedAudioStruct&& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo)
{
	FRuntimeCodecFactory CodecFactory;
	TArray<FBaseRuntimeCodec*> RuntimeCodecs = [&EncodedAudioInfo, &CodecFactory]()
	{
		if (EncodedAudioInfo.AudioFormat == ERuntimeAudioFormat::Auto)
		{
			return CodecFactory.GetCodecs(EncodedAudioInfo.AudioData);
		}
		return CodecFactory.GetCodecs(EncodedAudioInfo.AudioFormat);
	}();

	for (FBaseRuntimeCodec* RuntimeCodec : RuntimeCodecs)
	{
		EncodedAudioInfo.AudioFormat = RuntimeCodec->GetAudioFormat();
		if (!RuntimeCodec->Decode(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding '%s' audio data"), *UEnum::GetValueAsString(EncodedAudioInfo.AudioFormat));
			continue;
		}
		return true;
	}

	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to decode the audio data because the codec for the format '%s' was not found"), *UEnum::GetValueAsString(EncodedAudioInfo.AudioFormat));
	return false;
}

bool URuntimeAudioImporterLibrary::EncodeAudioData(FDecodedAudioStruct&& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality)
{
	if (EncodedAudioInfo.AudioFormat == ERuntimeAudioFormat::Auto || EncodedAudioInfo.AudioFormat == ERuntimeAudioFormat::Invalid)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for encoding"));
		return false;
	}

	FRuntimeCodecFactory CodecFactory;
	TArray<FBaseRuntimeCodec*> RuntimeCodecs = CodecFactory.GetCodecs(EncodedAudioInfo.AudioFormat);
	for (FBaseRuntimeCodec* RuntimeCodec : RuntimeCodecs)
	{
		if (!RuntimeCodec->Encode(MoveTemp(DecodedAudioInfo), EncodedAudioInfo, Quality))
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding '%s' audio data"), *UEnum::GetValueAsString(EncodedAudioInfo.AudioFormat));
			continue;
		}
		return true;
	}
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to encode the audio data because the codec for the format '%s' was not found"), *UEnum::GetValueAsString(EncodedAudioInfo.AudioFormat));
	return false;
}

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	// Making sure we are in the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this), Percentage]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnProgress_Internal(Percentage);
			}
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
		AsyncTask(ENamedThreads::GameThread, [WeakThis = MakeWeakObjectPtr(this), ImportedSoundWave, Status]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnResult_Internal(ImportedSoundWave, Status);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to broadcast the result of the import because the RuntimeAudioImporterLibrary object has been destroyed"));
			}
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
