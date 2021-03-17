// Georgy Treshchev 2021.

#include "RuntimeAudioImporterLibrary.h"

#include "PreimportedSoundAsset.h"
#include "Misc/FileHelper.h"

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	URuntimeAudioImporterLibrary* Importer = NewObject<URuntimeAudioImporterLibrary>();
	return Importer;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, TEnumAsByte<EAudioFormat> Format)
{
	if (!FPaths::FileExists(FilePath))
	{
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
	ImportAudioFromBuffer(AudioBuffer, Format);
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreimportedSound(UPreimportedSoundAsset* PreimportedSoundAssetRef)
{
	ImportAudioFromBuffer(PreimportedSoundAssetRef->AudioDataArray, EAudioFormat::Mp3);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(const TArray<uint8>& AudioDataArray,
                                                         const TEnumAsByte<EAudioFormat>& Format)
{
	AsyncTask(ENamedThreads::AnyThread, [=]()
	{
		ImportAudioFromBuffer_Internal(AudioDataArray, Format);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioDataArray,
                                                                  const TEnumAsByte<EAudioFormat>& Format)
{
	// Callback Dispatcher OnProgress
	OnProgress_Internal(5);
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

	// Callback Dispatcher OnProgress
	OnProgress_Internal(65);
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		UImportedSoundWave* SoundWaveRef = NewObject<UImportedSoundWave>(UImportedSoundWave::StaticClass());
		if (!SoundWaveRef)
		{
			OnResult_Internal(nullptr, ETranscodingStatus::SoundWaveDeclarationError);
			return nullptr;
		}
		AsyncTask(ENamedThreads::AnyThread, [=]()
		{
			if (DefineSoundWave(SoundWaveRef))
			{
				// Callback Dispatcher OnProgress
				OnProgress_Internal(100);

				// Callback Dispatcher OnResult, with the created SoundWave object
				OnResult_Internal(SoundWaveRef, ETranscodingStatus::SuccessImporting);
			}
		});
		return nullptr;
	});
}

bool URuntimeAudioImporterLibrary::DefineSoundWave(UImportedSoundWave* SoundWaveRef)
{
	// Callback Dispatcher OnProgress
	OnProgress_Internal(70);

	// Fill SoundWave basic information (e.g. duration, number of channels, etc)
	FillSoundWaveBasicInfo(SoundWaveRef);

	// Callback Dispatcher OnProgress
	OnProgress_Internal(75);

	// Fill PCM data buffer
	FillPCMData(SoundWaveRef);

	// Callback Dispatcher OnProgress
	OnProgress_Internal(95);
	
	return true;
}

void URuntimeAudioImporterLibrary::FillSoundWaveBasicInfo(UImportedSoundWave* SoundWaveRef) const
{
	SoundWaveRef->RawPCMDataSize = TranscodingFillInfo.PCMInfo.PCMDataSize;
	SoundWaveRef->Duration = TranscodingFillInfo.SoundWaveBasicInfo.Duration;
	SoundWaveRef->SetSampleRate(TranscodingFillInfo.SoundWaveBasicInfo.SampleRate);
	SoundWaveRef->NumChannels = TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum;
	SoundWaveRef->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
	if (SoundWaveRef->NumChannels >= 4)
	{
		SoundWaveRef->bIsAmbisonics = 1;
	}
	SoundWaveRef->bProcedural = true;
	SoundWaveRef->bLooping = false;
}

void URuntimeAudioImporterLibrary::FillPCMData(UImportedSoundWave* SoundWaveRef) const
{
	/*SoundWaveRef->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWaveRef->RawPCMDataSize));
	FMemory::Memmove(SoundWaveRef->RawPCMData, TranscodingFillInfo.PCMInfo.RawPCMData,
	                 SoundWaveRef->RawPCMDataSize);*/
	// We do not need to fill a standard PCM buffer since we have a custom sound wave with custom buffer. But if you want to fill the standard PCM buffer, just uncomment the code above.

	SoundWaveRef->PCMBufferInfo = TranscodingFillInfo.PCMInfo;
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
	OnProgress_Internal(10);
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
			OnProgress_Internal(25);
			
			TranscodingFillInfo.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(
				static_cast<size_t>(drmp3_get_pcm_frame_count(&mp3)) * mp3.channels * sizeof(float)));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(35);
			
			TranscodingFillInfo.PCMInfo.PCMNumOfFrames = drmp3_read_pcm_frames_f32(
				&mp3, drmp3_get_pcm_frame_count(&mp3), reinterpret_cast<float*>(TranscodingFillInfo.PCMInfo.PCMData));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(45);
			
			TranscodingFillInfo.PCMInfo.PCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames *
				mp3.channels * sizeof(float));
			TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(drmp3_get_pcm_frame_count(&mp3)) / mp3.
				sampleRate;
			TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = mp3.channels;
			TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = mp3.sampleRate;
			drmp3_uninit(&mp3);

			// Callback Dispatcher OnProgress
			OnProgress_Internal(55);
			
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
			OnProgress_Internal(25);
			TranscodingFillInfo.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(
				static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels * sizeof(float)));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(35);
			
			TranscodingFillInfo.PCMInfo.PCMNumOfFrames = drwav_read_pcm_frames_f32(
				&wav, wav.totalPCMFrameCount, reinterpret_cast<float*>(TranscodingFillInfo.PCMInfo.PCMData));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(45);
			
			TranscodingFillInfo.PCMInfo.PCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames *
				wav.channels * sizeof(float));
			TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(wav.totalPCMFrameCount) / wav.
				sampleRate;
			TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = wav.channels;
			TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = wav.sampleRate;
			drwav_uninit(&wav);
			
			// Callback Dispatcher OnProgress
			OnProgress_Internal(55);
			
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
			OnProgress_Internal(25);
			TranscodingFillInfo.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(
				static_cast<size_t>(pFlac->totalPCMFrameCount) * pFlac->channels * sizeof(float)));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(35);
			TranscodingFillInfo.PCMInfo.PCMNumOfFrames = drflac_read_pcm_frames_f32(
				pFlac, pFlac->totalPCMFrameCount, reinterpret_cast<float*>(TranscodingFillInfo.PCMInfo.PCMData));

			// Callback Dispatcher OnProgress
			OnProgress_Internal(45);
			
			TranscodingFillInfo.PCMInfo.PCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames *
				pFlac->channels * sizeof(float));
			TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(pFlac->totalPCMFrameCount) / pFlac->
				sampleRate;
			TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = pFlac->channels;
			TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = pFlac->sampleRate;
			drflac_close(pFlac);

			// Callback Dispatcher OnProgress
			OnProgress_Internal(55);
			
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

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		OnProgress.Broadcast(Percentage);
	});
}

void URuntimeAudioImporterLibrary::OnResult_Internal(UImportedSoundWave* SoundWaveRef,
                                                     const TEnumAsByte<ETranscodingStatus>& Status)
{
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		RemoveFromRoot();
		OnResult.Broadcast(this, SoundWaveRef, Status);
	});
}
