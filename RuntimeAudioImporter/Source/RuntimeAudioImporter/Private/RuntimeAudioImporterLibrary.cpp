// Georgy Treshchev 2021.

#include "RuntimeAudioImporterLibrary.h"

#include "AudioDevice.h"
#include "AudioCompressionSettingsUtils.h"
#include "Interfaces/IAudioFormat.h"

// Compressors for platform-specific audio formats
#include "VorbisCompressor.h"
#include "OpusCompressor.h"
#include "AdvancedPCMCompressor.h"


URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	URuntimeAudioImporterLibrary* Importer = NewObject<URuntimeAudioImporterLibrary>();
	Importer->AddToRoot();
	return Importer;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(
	const FString& FilePath, TEnumAsByte<EAudioFormat> Format, const FBuffersDetailsStruct& BuffersDetails)
{
	if (!FPaths::FileExists(FilePath))
	{
		// Trying to get a file inside packaged build
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();

		OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
		return;
	}


	if (Format == EAudioFormat::Auto)
	{
		Format = GetAudioFormat(FilePath);
	}

	TArray<uint8> AudioBuffer;
	if (!FFileHelper::LoadFileToArray(AudioBuffer, *FilePath))
	{
		// Callback Dispatcher OnResult
		OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
		return;
	}
	ImportAudioFromBuffer(AudioBuffer, Format, BuffersDetails);
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreimportedSound(UPreimportedSoundAsset* PreimportedSoundAssetRef,
                                                                   const FBuffersDetailsStruct& BuffersDetails)
{
	ImportAudioFromBuffer(PreimportedSoundAssetRef->AudioDataArray, EAudioFormat::Mp3, BuffersDetails);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(const TArray<uint8>& AudioDataArray,
                                                         const TEnumAsByte<EAudioFormat>& Format,
                                                         const FBuffersDetailsStruct& BuffersDetails)
{
	AsyncTask(ENamedThreads::AnyThread, [=]()
	{
		ImportAudioFromBuffer_Internal(AudioDataArray, Format, BuffersDetails);
	});
}

bool URuntimeAudioImporterLibrary::DestroySoundWave(USoundWave* ReadySoundWave)
{
	if (ReadySoundWave->IsValidLowLevel())
	{
		//Begin USoundWave destroy (block changing some data)
		ReadySoundWave->ConditionalBeginDestroy();
		ReadySoundWave->BeginDestroy();

		//Delete all audio data from physical memory
		ReadySoundWave->RawData.RemoveBulkData();
		ReadySoundWave->RemoveAudioResource();
		AsyncTask(ENamedThreads::AudioThread, [=]()
		{
			ReadySoundWave->InvalidateCompressedData(true);
		});

		//Finish USoundWave destroy (complete object deletion as UObject)
		ReadySoundWave->IsReadyForFinishDestroy(); //This is not needed for verification, but for the initial removal
		ReadySoundWave->ConditionalFinishDestroy();
		ReadySoundWave->FinishDestroy();
		return true;
	}
	else
	{
		return false;
	}
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_Internal(
	const TArray<uint8>& AudioDataArray, const TEnumAsByte<EAudioFormat>& Format,
	const FBuffersDetailsStruct& BuffersDetails)
{
	BuffersDetailsInfo = BuffersDetails;


	if (Format == EAudioFormat::Auto || Format == EAudioFormat::Invalid)
	{
		// Callback Dispatcher OnResult
		OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
		return;
	}

	// Transcoding importer Audio Data to PCM Data
	if (!TranscodeAudioDataArrayToPCMData(AudioDataArray.GetData(), AudioDataArray.Num() - 2, Format))
	{
		return;
	}

	// A ready USoundWave static object
	USoundWave* ReadySoundWave = DefineSoundWave();

	if (ReadySoundWave == nullptr)
	{
		return;
	}

	// Callback Dispatcher OnProgress
	OnProgress_Internal(100);

	// Callback Dispatcher OnResult, with the created SoundWave object
	OnResult_Internal(ReadySoundWave, ETranscodingStatus::SuccessImporting);
}

USoundWave* URuntimeAudioImporterLibrary::DefineSoundWave()
{
	USoundWave* SoundWaveRef = NewObject<USoundWave>(USoundWave::StaticClass());
	if (!SoundWaveRef)
	{
		OnResult_Internal(nullptr, ETranscodingStatus::SoundWaveDeclarationError);
		return nullptr;
	}

	// Callback Dispatcher OnProgress
	OnProgress_Internal(40);

	// Fill SoundWave basic information (e.g. duration, number of channels, etc)
	FillSoundWaveBasicInfo(SoundWaveRef);

	// Callback Dispatcher OnProgress
	OnProgress_Internal(45);

	// Whether to fill the compressed buffer or not. If not filled, it is necessary to fill Raw (Wave) data so that the engine will automatically generate compressed data.
	if (BuffersDetailsInfo.CompressionFormat == ECompressionFormat::RawData)
	{
		// Callback Dispatcher OnProgress
		OnProgress_Internal(60);

		// Transcode PCM to Raw (Wave) Data
		if (!TranscodePCMToWAVData())
		{
			FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);
			return nullptr;
		}

		// free unneeded PCM Data
		FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);

		// Callback Dispatcher OnProgress
		OnProgress_Internal(75);

		// Fill Raw (Wave) Data
		FillRawWaveData(SoundWaveRef);

		// Callback Dispatcher OnProgress
		OnProgress_Internal(95);

		return SoundWaveRef;
	}

	/* We don't need to fill RawPCMData buffer since it is not used at runtime during audio playback. But in case you want to fill this buffer - uncomment the code below (you should also remove some RawPCMData cleanups in code before it is executed)
	sw->RawPCMData = static_cast<uint8*>(FMemory::Malloc(sw->RawPCMDataSize));
	FMemory::Memmove(sw->RawPCMData, TranscodingFillInfo.PCMInfo.RawPCMData, sw->RawPCMDataSize);
	*/

	// Set a new sound compression quality
	SoundWaveRef->CompressionQuality = BuffersDetailsInfo.SoundCompressionQuality;

	// This variable won't be used anywhere, but it is recommended to set a decompression type to realtime since it will be decompressed at runtime
	SoundWaveRef->DecompressionType = EDecompressionType::DTYPE_RealTime;

	// Callback Dispatcher OnProgress
	OnProgress_Internal(60);

	// Transcode Raw PCM data to Compressed data
	if (!TranscodePCMToCompressedData(SoundWaveRef))
	{
		return nullptr;
	}

	// Callback Dispatcher OnProgress
	OnProgress_Internal(75);

	SoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::NotStarted);

	// Fill the compressed data
	FillCompressedData(SoundWaveRef);

	// Callback Dispatcher OnProgress
	OnProgress_Internal(95);


	return SoundWaveRef;
}

void URuntimeAudioImporterLibrary::FillSoundWaveBasicInfo(USoundWave* SoundWaveRef) const
{
	SoundWaveRef->RawPCMDataSize = TranscodingFillInfo.PCMInfo.RawPCMDataSize;
	SoundWaveRef->Duration = TranscodingFillInfo.SoundWaveBasicInfo.Duration;
	SoundWaveRef->SetSampleRate(TranscodingFillInfo.SoundWaveBasicInfo.SampleRate);
	SoundWaveRef->NumChannels = TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum;
	SoundWaveRef->SoundGroup = ESoundGroup::SOUNDGROUP_Default;

	if (SoundWaveRef->NumChannels == 4)
	{
		SoundWaveRef->bIsAmbisonics = 1;
	}
}

void URuntimeAudioImporterLibrary::FillRawWaveData(USoundWave* SoundWaveRef) const
{
	SoundWaveRef->RawData.Lock(LOCK_READ_WRITE);
	FMemory::Memmove(SoundWaveRef->RawData.Realloc(TranscodingFillInfo.WAVInfo.WaveDataSize),
	                 TranscodingFillInfo.WAVInfo.WaveData, TranscodingFillInfo.WAVInfo.WaveDataSize);
	SoundWaveRef->RawData.Unlock();
}

void URuntimeAudioImporterLibrary::FillCompressedData(USoundWave* SoundWaveRef)
{
	// Get the name of the runtime format that will be used during audio playback
	const FName CurrentAudioFormat = GEngine->GetAudioDeviceManager()->GetActiveAudioDevice()->
	                                          GetRuntimeFormat(SoundWaveRef);

	SoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::InProgress);

	// Fill the compressed data
	FByteBulkData* CompressedBulkData = &SoundWaveRef->CompressedFormatData.GetFormat(
		GetPlatformSpecificFormat(CurrentAudioFormat));
	CompressedBulkData->Lock(LOCK_READ_WRITE);
	FMemory::Memmove(CompressedBulkData->Realloc(TranscodingFillInfo.CompressedInfo.CompressedFormatData.Num()),
	                 TranscodingFillInfo.CompressedInfo.CompressedFormatData.GetData(),
	                 TranscodingFillInfo.CompressedInfo.CompressedFormatData.Num());
	CompressedBulkData->Unlock();

	SoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::Done);
}

bool URuntimeAudioImporterLibrary::TranscodePCMToCompressedData(USoundWave* SoundWaveToGetData)
{
	// Raw PCM data Array
	TArray<uint8> RawPCMArray = TArray<uint8>(TranscodingFillInfo.PCMInfo.RawPCMData,
	                                          TranscodingFillInfo.PCMInfo.RawPCMDataSize);

	// Sound quality information for compression purposes
	FSoundQualityInfo SoundQualityInfo = GenerateSoundQualityInfo(SoundWaveToGetData);

	switch (BuffersDetailsInfo.CompressionFormat)
	{
	case OggVorbis:
		{
			if (!FVorbisCompressor::GenerateCompressedData(RawPCMArray, SoundQualityInfo,
			                                               TranscodingFillInfo.CompressedInfo.CompressedFormatData))
			{
				// Clearing all data in case of error for performance reasons
				TranscodingFillInfo.CompressedInfo.CompressedFormatData.Empty();
				RawPCMArray.Empty();
				FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);
				OnResult_Internal(nullptr, ETranscodingStatus::GenerateCompressedDataError);
				return false;
			}
			break;
		}

	case OggOpus:
		{
			if (!FOpusCompressor::GenerateCompressedData(RawPCMArray, SoundQualityInfo,
			                                             TranscodingFillInfo.CompressedInfo.CompressedFormatData))
			{
				// Clearing all data in case of error for performance reasons
				TranscodingFillInfo.CompressedInfo.CompressedFormatData.Empty();
				RawPCMArray.Empty();
				FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);
				OnResult_Internal(nullptr, ETranscodingStatus::GenerateCompressedDataError);
				return false;
			}
			break;
		}
	case ADPCMLPCM:
		{
			if (!FAdvancedPCMCompressor::GenerateCompressedData(RawPCMArray, SoundQualityInfo,
			                                                    TranscodingFillInfo.CompressedInfo.CompressedFormatData)
			)
			{
				// Clearing all data in case of error for performance reasons
				TranscodingFillInfo.CompressedInfo.CompressedFormatData.Empty();
				RawPCMArray.Empty();
				FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);
				OnResult_Internal(nullptr, ETranscodingStatus::GenerateCompressedDataError);
				return false;
			}
			break;
		}
	default:
		{
			// Clearing all data in case of error for performance reasons
			TranscodingFillInfo.CompressedInfo.CompressedFormatData.Empty();
			RawPCMArray.Empty();
			FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidCompressedFormat);
			return false;
		}
	}

	// When the compressed data is generated, RawPCMData is no longer needed.
	RawPCMArray.Empty();
	FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);

	// Callback Dispatcher OnProgress
	OnProgress_Internal(85);

	return true;
}


/**
* Including dr_wav, dr_mp3 and dr_flac libraries
*/
#include "ThirdParty/dr_wav.h"
#include "ThirdParty/dr_mp3.h"
#include "ThirdParty/dr_flac.h"

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

bool URuntimeAudioImporterLibrary::TranscodeAudioDataArrayToPCMData(const uint8* AudioData, uint32 AudioDataSize,
                                                                    TEnumAsByte<EAudioFormat> Format)
{
	OnProgress_Internal(5);
	switch (Format)
	{
	case EAudioFormat::Mp3:
		{
			drmp3_allocation_callbacks allocationCallbacksDecoding;
			allocationCallbacksDecoding.pUserData = nullptr;
			allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
			allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
			allocationCallbacksDecoding.onFree = Unreal_Free;

			drmp3 mp3;

			if (!drmp3_init_memory(&mp3, AudioData, AudioDataSize, &allocationCallbacksDecoding))
			{
				// Callback Dispatcher OnResult
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}

			// Callback Dispatcher OnProgress
			OnProgress_Internal(10);

			TranscodingFillInfo.PCMInfo.RawPCMData = static_cast<uint8*>(FMemory::Malloc(
				static_cast<size_t>(drmp3_get_pcm_frame_count(&mp3)) * mp3.channels * sizeof(int16_t)));


			// Callback Dispatcher OnProgress
			OnProgress_Internal(15);

			TranscodingFillInfo.PCMInfo.RawPCMNumOfFrames = drmp3_read_pcm_frames_s16(
				&mp3, drmp3_get_pcm_frame_count(&mp3),
				reinterpret_cast<int16_t*>(TranscodingFillInfo.PCMInfo.RawPCMData));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(35);


			TranscodingFillInfo.PCMInfo.RawPCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.
				RawPCMNumOfFrames * mp3.channels * sizeof(int16_t));

			TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(drmp3_get_pcm_frame_count(&mp3)) / mp3.
				sampleRate;
			TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = mp3.channels;
			TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = mp3.sampleRate;


			drmp3_uninit(&mp3);
			return true;
		}
	case EAudioFormat::Wav:
		{
			drwav_allocation_callbacks allocationCallbacksDecoding;
			allocationCallbacksDecoding.pUserData = nullptr;
			allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
			allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
			allocationCallbacksDecoding.onFree = Unreal_Free;

			drwav wav;

			if (!drwav_init_memory(&wav, AudioData, AudioDataSize, &allocationCallbacksDecoding))

			{
				// Callback Dispatcher OnResult
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}

			// Callback Dispatcher OnProgress
			OnProgress_Internal(10);


			TranscodingFillInfo.PCMInfo.RawPCMData = static_cast<uint8*>(FMemory::Malloc(
				static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels * sizeof(int16_t)));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(15);

			TranscodingFillInfo.PCMInfo.RawPCMNumOfFrames = drwav_read_pcm_frames_s16(
				&wav, wav.totalPCMFrameCount, reinterpret_cast<int16_t*>(TranscodingFillInfo.PCMInfo.RawPCMData));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(35);


			TranscodingFillInfo.PCMInfo.RawPCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.
				RawPCMNumOfFrames * wav.channels * sizeof(int16_t));

			TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(wav.totalPCMFrameCount) / wav.
				sampleRate;
			TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = wav.channels;
			TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = wav.sampleRate;


			drwav_uninit(&wav);
			return true;
		}
	case EAudioFormat::Flac:
		{
			drflac_allocation_callbacks allocationCallbacksDecoding;
			allocationCallbacksDecoding.pUserData = nullptr;
			allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
			allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
			allocationCallbacksDecoding.onFree = Unreal_Free;


			drflac* pFlac = drflac_open_memory(AudioData, AudioDataSize, &allocationCallbacksDecoding);

			if (pFlac == nullptr)
			{
				// Callback Dispatcher OnResult
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}

			// Callback Dispatcher OnProgress
			OnProgress_Internal(10);


			TranscodingFillInfo.PCMInfo.RawPCMData = static_cast<uint8*>(FMemory::Malloc(
				static_cast<size_t>(pFlac->totalPCMFrameCount) * pFlac->channels * sizeof(int16_t)));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(15);


			TranscodingFillInfo.PCMInfo.RawPCMNumOfFrames = drflac_read_pcm_frames_s16(
				pFlac, pFlac->totalPCMFrameCount, reinterpret_cast<int16_t*>(TranscodingFillInfo.PCMInfo.RawPCMData));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(35);


			TranscodingFillInfo.PCMInfo.RawPCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.
				RawPCMNumOfFrames * pFlac->channels * sizeof(int16_t));


			TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(pFlac->totalPCMFrameCount) / pFlac->
				sampleRate;
			TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = pFlac->channels;
			TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = pFlac->sampleRate;
			drflac_close(pFlac);
			return true;
		}
	default:
		{
			// Callback Dispatcher OnResult
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return false;
		}
	}
}

bool URuntimeAudioImporterLibrary::TranscodePCMToWAVData()
{
	drwav wavEncode;
	drwav_data_format format;
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum;
	format.sampleRate = TranscodingFillInfo.SoundWaveBasicInfo.SampleRate;
	format.bitsPerSample = 16; // Unreal Engine supports only 16-bit rate

	drwav_allocation_callbacks allocationCallbacks;
	allocationCallbacks.pUserData = nullptr;
	allocationCallbacks.onMalloc = Unreal_Malloc;
	allocationCallbacks.onRealloc = Unreal_Realloc;
	allocationCallbacks.onFree = Unreal_Free;

	// Initializing PCM data writing to memory
	if (!drwav_init_memory_write(&wavEncode, reinterpret_cast<void**>(&TranscodingFillInfo.WAVInfo.WaveData),
	                             reinterpret_cast<size_t*>(&TranscodingFillInfo.WAVInfo.WaveDataSize), &format,
	                             &allocationCallbacks))
	{
		FMemory::Free(TranscodingFillInfo.PCMInfo.RawPCMData);
		OnResult_Internal(nullptr, ETranscodingStatus::PCMToWavDataError);
		return false;
	}


	// Callback Dispatcher OnProgress
	OnProgress_Internal(40);

	// Write Raw PCM Data
	drwav_write_pcm_frames(&wavEncode, TranscodingFillInfo.PCMInfo.RawPCMNumOfFrames,
	                       reinterpret_cast<int16_t*>(TranscodingFillInfo.PCMInfo.RawPCMData));

	// Callback Dispatcher OnProgress
	OnProgress_Internal(60);


	drwav_uninit(&wavEncode);

	return true;
}

TEnumAsByte<EAudioFormat> URuntimeAudioImporterLibrary::GetAudioFormat(const FString& FilePath)
{
	if (FPaths::GetExtension(FilePath, false).Equals(TEXT("mp3"), ESearchCase::IgnoreCase))
	{
		return EAudioFormat::Mp3;
	}
	if (FPaths::GetExtension(FilePath, false).Equals(TEXT("wav"), ESearchCase::IgnoreCase))
	{
		return EAudioFormat::Wav;
	}
	if (FPaths::GetExtension(FilePath, false).Equals(TEXT("flac"), ESearchCase::IgnoreCase))
	{
		return EAudioFormat::Flac;
	}
	return EAudioFormat::Invalid;
}

FSoundQualityInfo URuntimeAudioImporterLibrary::GenerateSoundQualityInfo(USoundWave* SoundWaveToGetData) const
{
	FSoundQualityInfo SoundQualityInfo;
	SoundQualityInfo.Duration = SoundWaveToGetData->Duration;
	SoundQualityInfo.Quality = SoundWaveToGetData->CompressionQuality;
	SoundQualityInfo.NumChannels = SoundWaveToGetData->NumChannels;

	// since we cannot get the Sample Rate from the SoundWave, we will get it from the data of the initial PCM transcoding
	SoundQualityInfo.SampleRate = TranscodingFillInfo.SoundWaveBasicInfo.SampleRate;

	SoundQualityInfo.SampleDataSize = SoundWaveToGetData->RawPCMDataSize;
	SoundQualityInfo.bStreaming = SoundWaveToGetData->IsStreaming(*FPlatformCompressionUtilities::GetCookOverrides());
	SoundQualityInfo.DebugName = SoundWaveToGetData->GetFullName();
	return SoundQualityInfo;
}


FName URuntimeAudioImporterLibrary::GetPlatformSpecificFormat(const FName Format)
{
	const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides();

	// Platforms that require compression overrides get concatenated formats.
#if WITH_EDITOR
	FName PlatformSpecificFormat;
	if (CompressionOverrides)
	{
		FString HashedString = *Format.ToString();
		FPlatformAudioCookOverrides::GetHashSuffix(CompressionOverrides, HashedString);
		PlatformSpecificFormat = *HashedString;
	}
	else
	{
		PlatformSpecificFormat = Format;
	}

#else
	// Cache the concatenated hash:
	static FName PlatformSpecificFormat;
	static FName CachedFormat;
	if (!Format.IsEqual(CachedFormat))
	{
		if (CompressionOverrides)
		{
			FString HashedString = *Format.ToString();
			FPlatformAudioCookOverrides::GetHashSuffix(CompressionOverrides, HashedString);
			PlatformSpecificFormat = *HashedString;
		}
		else
		{
			PlatformSpecificFormat = Format;
		}

		CachedFormat = Format;
	}

#endif // WITH_EDITOR

	return PlatformSpecificFormat;
}

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnProgress.Broadcast(Percentage);
	});
}

void URuntimeAudioImporterLibrary::OnResult_Internal(USoundWave* ReadySoundWave,
                                                     const TEnumAsByte<ETranscodingStatus>& Status)
{
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		RemoveFromRoot();
		OnResult.Broadcast(this, ReadySoundWave, Status);
	});
}
