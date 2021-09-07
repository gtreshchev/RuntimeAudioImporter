// Georgy Treshchev 2021.

#include "RuntimeAudioImporterLibrary.h"

#include "PreimportedSoundAsset.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	URuntimeAudioImporterLibrary* Importer = NewObject<URuntimeAudioImporterLibrary>();
	Importer->AddToRoot();
	return Importer;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, EAudioFormat Format)
{
	// Checking if the file exists
	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
		return;
	}

	// Getting the audio format
	if (Format == EAudioFormat::Auto) Format = GetAudioFormat(FilePath);

	TArray<uint8> AudioBuffer;

	// Filling AudioBuffer with a binary file 
	if (!FFileHelper::LoadFileToArray(AudioBuffer, *FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
		return;
	}

	ImportAudioFromBuffer(AudioBuffer, Format);
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat Format,
                                                          const int32 SampleRate, const int32 NumOfChannels)
{
	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
		return;
	}

	// OnProgress Callback
	OnProgress_Internal(5);

	TArray<uint8> AudioBuffer;
	if (!FFileHelper::LoadFileToArray(AudioBuffer, *FilePath))
	{
		// OnResult Callback
		OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
		return;
	}

	// OnProgress Callback
	OnProgress_Internal(35);

	AsyncTask(ENamedThreads::AnyThread, [&, AudioBuffer, Format, SampleRate, NumOfChannels]()
	{
		ImportAudioFromRAWBuffer(AudioBuffer, Format, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format,
                                                            const int32 SampleRate, const int32 NumOfChannels)
{
	uint8* RAWData = RAWBuffer.GetData();
	const uint32 RAWDataSize = RAWBuffer.Num() - 2;

	uint8* PCMData = nullptr;
	uint32 PCMDataSize = 0;

	// Transcoding RAW data to 32-bit float Data
	{
		switch (Format)
		{
		case ERAWAudioFormat::Int16:
			{
				TranscodeRAWDataTo32FloatData<int16>(reinterpret_cast<int16*>(RAWData), RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::Int32:
			{
				TranscodeRAWDataTo32FloatData<int32>(reinterpret_cast<int32*>(RAWData), RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::UInt8:
			{
				TranscodeRAWDataTo32FloatData<uint8>(RAWData, RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::Float32:
			{
				PCMDataSize = RAWDataSize;
				PCMData = static_cast<uint8*>(FMemory::Malloc(PCMDataSize));
				FMemory::Memcpy(PCMData, RAWData, RAWDataSize);
				break;
			}
		}
	}

	if (!PCMData || PCMDataSize < 0)
	{
		// OnResult Callback
		OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
		return;
	}

	// Clearing the RAW Buffer
	RAWBuffer.Empty();

	ImportAudioFromFloat32Buffer(PCMData, PCMDataSize, SampleRate, NumOfChannels);
}

template <typename IntegralType>
void URuntimeAudioImporterLibrary::TranscodeRAWDataTo32FloatData(IntegralType* RAWData, uint32 RAWDataSize, uint8*& PCMData,
                                                             uint32& PCMDataSize)
{
	// Getting the required number of samples to transcode
	const int32& NumSamples = RAWDataSize / sizeof(IntegralType);

	// Getting the required PCM size
	PCMDataSize = NumSamples * sizeof(float);

	// Creating an empty PCM buffer
	float* TempPCMData = static_cast<float*>(FMemory::MallocZeroed(PCMDataSize));

	// Iterating through the RAW Data to transcode values using a divisor
	for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		TempPCMData[SampleIndex] = static_cast<float>(RAWData[SampleIndex]) / GetRawToPcmDivisor<IntegralType>();
	}

	// Returning the transcoded data as bytes
	PCMData = reinterpret_cast<uint8*>(TempPCMData);
}

template <typename IntegralType>
float URuntimeAudioImporterLibrary::GetRawToPcmDivisor()
{
	if (TIsSame<IntegralType, int16>::Value)
	{
		return 32767.0f;
	}

	if (TIsSame<IntegralType, int32>::Value)
	{
		return 2147483647.0f;
	}

	if (TIsSame<IntegralType, uint8>::Value)
	{
		return 255.0f;
	}

	return 1;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFloat32Buffer(uint8* PCMData, const uint32 PCMDataSize,
                                                                const int32 SampleRate, const int32 NumOfChannels)
{
	// Filling all the required information
	{
		TranscodingFillInfo.PCMInfo.PCMData = PCMData;
		TranscodingFillInfo.PCMInfo.PCMDataSize = PCMDataSize;
		TranscodingFillInfo.PCMInfo.PCMNumOfFrames = PCMDataSize / sizeof(float) / NumOfChannels;

		TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = NumOfChannels;
		TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = SampleRate;
		TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames)
			/ SampleRate;
	}

	// OnProgress Callback
	OnProgress_Internal(50);

	// Finalizing import
	CreateSoundWaveAndFinishImport();
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef)
{
	ImportAudioFromBuffer(PreImportedSoundAssetRef->AudioDataArray, EAudioFormat::Mp3);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_BP(TArray<uint8> AudioDataBuffer, EAudioFormat Format)
{
	ImportAudioFromBuffer(AudioDataBuffer, Format);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8>& AudioDataBuffer,
                                                         const EAudioFormat& Format)
{
	if (Format == EAudioFormat::Wav) if (!CheckAndFixWavDurationErrors(AudioDataBuffer)) return;

	TranscodingFillInfo = FTranscodingFillStruct();
	AsyncTask(ENamedThreads::AnyThread, [&, AudioDataBuffer, Format]()
	{
		ImportAudioFromBuffer_Internal(AudioDataBuffer, Format);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioDataBuffer,
                                                                  const EAudioFormat& Format)
{
	// OnProgress Callback
	OnProgress_Internal(5);

	if (Format == EAudioFormat::Auto || Format == EAudioFormat::Invalid)
	{
		// OnResult Callback
		OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
		return;
	}

	// Transcoding the imported Audio Data to PCM Data
	if (!TranscodeAudioDataArrayToPCMData(AudioDataBuffer.GetData(), AudioDataBuffer.Num() - 2, Format)) return;

	CreateSoundWaveAndFinishImport();
}

void URuntimeAudioImporterLibrary::CreateSoundWaveAndFinishImport()
{
	AsyncTask(ENamedThreads::GameThread, [&]()
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
				// OnProgress Callback
				OnProgress_Internal(100);

				// OnResult Callback, with the created SoundWave object
				OnResult_Internal(SoundWaveRef, ETranscodingStatus::SuccessfulImport);
			}
		});
		return nullptr;
	});
}

bool URuntimeAudioImporterLibrary::DefineSoundWave(UImportedSoundWave* SoundWaveRef)
{
	// OnProgress Callback
	OnProgress_Internal(70);

	// Fill SoundWave basic information (e.g. duration, number of channels, etc)
	FillSoundWaveBasicInfo(SoundWaveRef);

	// OnProgress Callback
	OnProgress_Internal(75);

	// Fill PCM data buffer
	FillPCMData(SoundWaveRef);

	// OnProgress Callback
	OnProgress_Internal(95);

	return true;
}

void URuntimeAudioImporterLibrary::FillSoundWaveBasicInfo(UImportedSoundWave* SoundWaveRef) const
{
	SoundWaveRef->RawPCMDataSize = TranscodingFillInfo.PCMInfo.PCMDataSize;
	SoundWaveRef->Duration = TranscodingFillInfo.SoundWaveBasicInfo.Duration;
	SoundWaveRef->SetSampleRate(TranscodingFillInfo.SoundWaveBasicInfo.SampleRate);
	SoundWaveRef->SamplingRate = TranscodingFillInfo.SoundWaveBasicInfo.SampleRate;
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
* Replacing standard CPP memory methods (malloc, realloc, free) with engine ones
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


bool URuntimeAudioImporterLibrary::CheckAndFixWavDurationErrors(TArray<uint8>& WavData)
{
	drwav_allocation_callbacks allocationCallbacksDecoding;
	allocationCallbacksDecoding.pUserData = nullptr;

	// Replacing standard methods for working with memory with engine ones
	{
		allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
		allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
		allocationCallbacksDecoding.onFree = Unreal_Free;
	}

	drwav wav;
	// Initializing transcoding of audio data in memory
	if (!drwav_init_memory(&wav, WavData.GetData(), WavData.Num() - 2, &allocationCallbacksDecoding))
	{
		// OnResult Callback
		OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
		return false;
	}

	// Check if the container is RIFF (not Wave64 or any other containers)
	if (wav.container != drwav_container_riff)
	{
		drwav_uninit(&wav);
		return true;
	}

	/*
	* Get 4-byte field at byte 4, which is the overall file size as uint32, according to RIFF specification.
	* If the field is set to nothing (hex FFFFFFFF), replace the incorrectly set field with the actual size.
	* The field should be (size of file - 8 bytes), as the chunk identifier for the whole file (4 bytes spelling out RIFF at the start of the file), and the chunk length (4 bytes that we're replacing) are excluded.
	*/
	if (BytesToHex(WavData.GetData() + 4, 4) == "FFFFFFFF")
	{
		int32 ActualFileSize = WavData.Num() - 8;
		FMemory::Memcpy(WavData.GetData() + 4, &ActualFileSize, 4);
	}

	/*
	* Search for the place in the file after the chunk id "data", which is where the data length is stored.
	* First 36 bytes are skipped, as they're always "RIFF", 4 bytes filesize, "WAVE", "fmt ", and 20 bytes of format data.
	*/
	int32 DataSizeLocation = INDEX_NONE;
	for (int32 i = 36; i < WavData.Num() - 4; ++i)
	{
		if (BytesToHex(WavData.GetData() + i, 4) == "64617461" /* hex for string "data" */)
		{
			DataSizeLocation = i + 4;
			break;
		}
	}
	if (DataSizeLocation == INDEX_NONE) // should never happen but just in case
	{
		drwav_uninit(&wav);

		// OnResult Callback
		OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);

		return false;
	}

	// Same process as replacing full file size, except DataSize counts bytes from end of DataSize int to end of file.
	if (BytesToHex(WavData.GetData() + DataSizeLocation, 4) == "FFFFFFFF")
	{
		int32 ActualDataSize = WavData.Num() - DataSizeLocation - 4 /*-4 to not include the DataSize int itself*/;
		FMemory::Memcpy(WavData.GetData() + DataSizeLocation, &ActualDataSize, 4);
	}

	drwav_uninit(&wav);

	return true;
}


bool URuntimeAudioImporterLibrary::TranscodeAudioDataArrayToPCMData(const uint8* AudioData, uint32 AudioDataSize,
                                                                    const EAudioFormat& Format)
{
	OnProgress_Internal(10);
	switch (Format)
	{
	case EAudioFormat::Mp3:
		{
			drmp3_allocation_callbacks allocationCallbacksDecoding;
			allocationCallbacksDecoding.pUserData = nullptr;

			// Replacing standard methods for working with memory with engine ones
			{
				allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
				allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
				allocationCallbacksDecoding.onFree = Unreal_Free;
			}

			drmp3 mp3;
			// Initializing transcoding of audio data in memory
			if (!drmp3_init_memory(&mp3, AudioData, AudioDataSize, &allocationCallbacksDecoding))
			{
				// OnResult Callback
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}

			// OnProgress Callback
			OnProgress_Internal(25);

			// Allocating memory for PCM data
			TranscodingFillInfo.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(
				drmp3_get_pcm_frame_count(&mp3) * mp3.channels * sizeof(float)));

			// OnProgress Callback
			OnProgress_Internal(35);

			// Filling PCM data and getting the number of frames
			TranscodingFillInfo.PCMInfo.PCMNumOfFrames = drmp3_read_pcm_frames_f32(
				&mp3, drmp3_get_pcm_frame_count(&mp3), reinterpret_cast<float*>(TranscodingFillInfo.PCMInfo.PCMData));

			// OnProgress Callback
			OnProgress_Internal(45);

			// Getting PCM data size
			TranscodingFillInfo.PCMInfo.PCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames *
				mp3.channels * sizeof(float));

			// Getting basic audio information
			{
				TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(drmp3_get_pcm_frame_count(&mp3)) /
					mp3.
					sampleRate;
				TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = mp3.channels;
				TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = mp3.sampleRate;
			}

			// Uninitializing transcoding of audio data in memory
			drmp3_uninit(&mp3);

			// OnProgress Callback
			OnProgress_Internal(55);

			return true;
		}
	case EAudioFormat::Wav:
		{
			drwav_allocation_callbacks allocationCallbacksDecoding;
			allocationCallbacksDecoding.pUserData = nullptr;

			// Replacing standard methods for working with memory with engine ones
			{
				allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
				allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
				allocationCallbacksDecoding.onFree = Unreal_Free;
			}

			drwav wav;
			// Initializing transcoding of audio data in memory
			if (!drwav_init_memory(&wav, AudioData, AudioDataSize, &allocationCallbacksDecoding))
			{
				// OnResult Callback
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}

			// OnProgress Callback
			OnProgress_Internal(25);

			// Allocating memory for PCM data
			TranscodingFillInfo.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(
				wav.totalPCMFrameCount * wav.channels * sizeof(float)));

			// OnProgress Callback
			OnProgress_Internal(35);

			// Filling PCM data and getting the number of frames
			TranscodingFillInfo.PCMInfo.PCMNumOfFrames = drwav_read_pcm_frames_f32(
				&wav, wav.totalPCMFrameCount, reinterpret_cast<float*>(TranscodingFillInfo.PCMInfo.PCMData));

			// OnProgress Callback
			OnProgress_Internal(45);

			// Getting PCM data size
			TranscodingFillInfo.PCMInfo.PCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames *
				wav.channels * sizeof(float));

			// Getting basic audio information
			{
				TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(wav.totalPCMFrameCount) / wav.
					sampleRate;
				TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = wav.channels;
				TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = wav.sampleRate;
			}

			// Uninitializing transcoding of audio data in memory
			drwav_uninit(&wav);

			// OnProgress Callback
			OnProgress_Internal(55);

			return true;
		}
	case EAudioFormat::Flac:
		{
			drflac_allocation_callbacks allocationCallbacksDecoding;
			allocationCallbacksDecoding.pUserData = nullptr;

			// Replacing standard methods for working with memory with engine ones
			{
				allocationCallbacksDecoding.onMalloc = Unreal_Malloc;
				allocationCallbacksDecoding.onRealloc = Unreal_Realloc;
				allocationCallbacksDecoding.onFree = Unreal_Free;
			}

			// Initializing transcoding of audio data in memory
			drflac* pFlac = drflac_open_memory(AudioData, AudioDataSize, &allocationCallbacksDecoding);
			if (pFlac == nullptr)
			{
				// OnResult Callback
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}

			// OnProgress Callback
			OnProgress_Internal(25);

			// Allocating memory for PCM data
			TranscodingFillInfo.PCMInfo.PCMData = static_cast<uint8*>(FMemory::Malloc(
				pFlac->totalPCMFrameCount * pFlac->channels * sizeof(float)));

			// OnProgress Callback
			OnProgress_Internal(35);

			// Filling PCM data and getting the number of frames
			TranscodingFillInfo.PCMInfo.PCMNumOfFrames = drflac_read_pcm_frames_f32(
				pFlac, pFlac->totalPCMFrameCount, reinterpret_cast<float*>(TranscodingFillInfo.PCMInfo.PCMData));

			// OnProgress Callback
			OnProgress_Internal(45);

			// Getting PCM data size
			TranscodingFillInfo.PCMInfo.PCMDataSize = static_cast<uint32>(TranscodingFillInfo.PCMInfo.PCMNumOfFrames *
				pFlac->channels * sizeof(float));

			// Getting basic audio information
			{
				TranscodingFillInfo.SoundWaveBasicInfo.Duration = static_cast<float>(pFlac->totalPCMFrameCount) / pFlac
					->
					sampleRate;
				TranscodingFillInfo.SoundWaveBasicInfo.ChannelsNum = pFlac->channels;
				TranscodingFillInfo.SoundWaveBasicInfo.SampleRate = pFlac->sampleRate;
			}

			// Uninitializing transcoding of audio data in memory
			drflac_close(pFlac);

			// OnProgress Callback
			OnProgress_Internal(55);

			return true;
		}
	default:
		{
			// OnResult Callback
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return false;
		}
	}
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const FString& FilePath)
{
	if (FPaths::GetExtension(FilePath, false).Equals(TEXT("mp3"), ESearchCase::IgnoreCase)) return EAudioFormat::Mp3;
	if (FPaths::GetExtension(FilePath, false).Equals(TEXT("wav"), ESearchCase::IgnoreCase)) return EAudioFormat::Wav;
	if (FPaths::GetExtension(FilePath, false).Equals(TEXT("flac"), ESearchCase::IgnoreCase)) return EAudioFormat::Flac;
	return EAudioFormat::Invalid;
}

void URuntimeAudioImporterLibrary::OnProgress_Internal(const int32& Percentage)
{
	AsyncTask(ENamedThreads::GameThread, [&, Percentage]()
	{
		if (OnProgress.IsBound()) OnProgress.Broadcast(Percentage);
	});
}

void URuntimeAudioImporterLibrary::OnResult_Internal(UImportedSoundWave* SoundWaveRef, const ETranscodingStatus& Status)
{
	AsyncTask(ENamedThreads::GameThread, [&, SoundWaveRef, Status]()
	{
		if (OnResult.IsBound())
		{
			OnResult.Broadcast(this, SoundWaveRef, Status);
		}
		RemoveFromRoot();
	});
}
