// Respirant 2020.

#include "RuntimeAudioImporterLibrary.h"

URuntimeAudioImporterLibrary::URuntimeAudioImporterLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	URuntimeAudioImporterLibrary* Importer = NewObject<URuntimeAudioImporterLibrary>();
	Importer->AddToRoot();
	return Importer;
}

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& filePath, TEnumAsByte < AudioFormat > Format, bool DefineFormatAutomatically)
{
	AsyncTask(ENamedThreads::AnyThread, [=]() {
		GetUSoundWaveFromAudioFile_Internal(filePath, Format, DefineFormatAutomatically);
		});
	return this;
}

bool URuntimeAudioImporterLibrary::DestroySoundWave(USoundWave* ReadySoundWave) {

	if (ReadySoundWave->IsValidLowLevel()) {

		//Begin USoundWave destroy (block changing some data)
		ReadySoundWave->ConditionalBeginDestroy();
		ReadySoundWave->BeginDestroy();

		//Delete all audio data from physical memory 
		ReadySoundWave->RawData.RemoveBulkData();
		ReadySoundWave->RemoveAudioResource();
		AsyncTask(ENamedThreads::AudioThread, [=]() {
			ReadySoundWave->InvalidateCompressedData(true);
			});

		//Finish USoundWave destroy (complete object deletion as UObject) 
		ReadySoundWave->IsReadyForFinishDestroy(); //This is not needed for verification, but for the initial removal
		ReadySoundWave->ConditionalFinishDestroy();
		ReadySoundWave->FinishDestroy();
		return true;
	}
	else {
		return false;
	}
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
void* unreal_malloc(size_t sz, void* pUserData)
{
	return FMemory::Malloc(sz);
}
void* unreal_realloc(void* p, size_t sz, void* pUserData)
{
	return FMemory::Realloc(p, sz);
}
void unreal_free(void* p, void* pUserData)
{
	FMemory::Free(p);
}

bool URuntimeAudioImporterLibrary::ExportSoundWaveToFile(USoundWave* SoundWaveToExport, FString PathToExport)
{
	uint8* PCMData = SoundWaveToExport->RawPCMData;
	size_t PCMDataSize = SoundWaveToExport->RawPCMDataSize;

	uint32 SampleRate;
	uint16 ChannelCount;

	drwav_allocation_callbacks allocationCallbacksDecoding;
	allocationCallbacksDecoding.pUserData = NULL;
	allocationCallbacksDecoding.onMalloc = unreal_malloc;
	allocationCallbacksDecoding.onRealloc = unreal_realloc;
	allocationCallbacksDecoding.onFree = unreal_free;

	drwav_uint64 framesToWrite = PCMDataSize / sizeof(DR_WAVE_FORMAT_PCM) / SoundWaveToExport->NumChannels;

	ChannelCount = SoundWaveToExport->NumChannels;
	SampleRate = FGenericPlatformMath::RoundToInt((framesToWrite / (float)SoundWaveToExport->GetDuration() * ChannelCount));

	drwav wavEncode;

	drwav_data_format format;
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = ChannelCount;
	format.sampleRate = SampleRate;
	format.bitsPerSample = 16;

	if (!drwav_init_file_write(&wavEncode, TCHAR_TO_UTF8(*PathToExport), &format, &allocationCallbacksDecoding)) {
		return false;
	}

	drwav_write_pcm_frames(&wavEncode, framesToWrite, PCMData);
	drwav_uninit(&wavEncode);

	return true;
}

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::GetUSoundWaveFromAudioFile_Internal(const FString& filePath, TEnumAsByte < AudioFormat > Format, bool DefineFormatAutomatically)
{
	TEnumAsByte < TranscodingStatus > Status;
	if (FPaths::FileExists(filePath) != true) {
		Status = TranscodingStatus::AudioDoesNotExist;

		// Callback Dispatcher OnResult
		OnResult_Internal(nullptr, Status);
	}
	else {
		int16_t* pSampleData;
		uint64 framesToWrite;
		uint32 channels;
		uint32 sampleRate;

		if (DefineFormatAutomatically) {
			Format = GetAudioFormat(filePath);
		}

		if (Format == AudioFormat::INVALID) {
			Status = TranscodingStatus::InvalidAudioFormat;

			// Callback Dispatcher OnResult
			OnResult_Internal(nullptr, Status);
		}
		else {
			if (TranscodeAudioFileToPCMData(TCHAR_TO_UTF8(*filePath), Format, Status, framesToWrite, pSampleData, channels, sampleRate) == false) {
				// Callback Dispatcher OnResult
				OnResult_Internal(nullptr, Status);
			}
			else {
				void* WaveData;
				size_t WaveDataSize;

				if (TranscodePCMToWAVData(*&WaveData, *&WaveDataSize, framesToWrite, *&pSampleData, channels, sampleRate) == false) {
					FMemory::Free(pSampleData);

					// Callback Dispatcher OnResult
					OnResult_Internal(nullptr, Status);
				}
				else {
					USoundWave* ReadyUSoundWave = GetSoundWaveObject((const uint8*)WaveData, WaveDataSize, Status); // A ready USoundWave static object

					/* Free temporary transcoding data */
					FMemory::Free(WaveData);
					FMemory::Free(pSampleData);
					/* Free temporary transcoding data */

					// Callback Dispatcher OnProgress (not completely accurate implementation)
					OnProgress_Internal(100);

					// Callback Dispatcher OnResult
					OnResult_Internal(ReadyUSoundWave, Status);
				}


			}
		}
	}
	return this;
}

class USoundWave* URuntimeAudioImporterLibrary::GetSoundWaveObject(const uint8* WaveData, int32 WaveDataSize, TEnumAsByte < TranscodingStatus >& Status)
{
	USoundWave* sw = NewObject<USoundWave>(USoundWave::StaticClass());
	if (!sw) {
		Status = TranscodingStatus::USoundWaveDeclarationError;
		return nullptr;
	}
	FWaveModInfo WaveInfo;
	FString ErrorReason;

	// Callback Dispatcher OnProgress (not completely accurate implementation)
	OnProgress_Internal(65);

	if (WaveInfo.ReadWaveInfo(WaveData, WaveDataSize, &ErrorReason))
	{
		// Callback Dispatcher OnProgress (not completely accurate implementation)
		OnProgress_Internal(75);

		if (*WaveInfo.pBitsPerSample != 16) {
			Status = TranscodingStatus::UnsupportedBitDepth;
			return nullptr;
		}
		sw->InvalidateCompressedData();
		sw->RawData.Lock(LOCK_READ_WRITE);
		void* LockedData = sw->RawData.Realloc(WaveDataSize);
		FMemory::Memcpy(LockedData, WaveData, WaveDataSize);
		sw->RawData.Unlock();

		// Callback Dispatcher OnProgress (not completely accurate implementation)
		OnProgress_Internal(80);

		int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;
		if (DurationDiv)
		{
			sw->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			sw->Duration = 0.0f;
			Status = TranscodingStatus::ZeroDurationError;
			return nullptr;
		}
		sw->SetSampleRate(*WaveInfo.pSamplesPerSec);
		sw->NumChannels = *WaveInfo.pChannels;
		sw->RawPCMDataSize = WaveInfo.SampleDataSize;
		sw->SoundGroup = ESoundGroup::SOUNDGROUP_Default;

		// Callback Dispatcher OnProgress (not completely accurate implementation)
		OnProgress_Internal(85);
	}
	else {
		if (ErrorReason.Equals(TEXT("Invalid WAVE file."), ESearchCase::CaseSensitive)) {
			Status = TranscodingStatus::InvalidWAVEData;
		}
		else if (ErrorReason.Equals(TEXT("Unsupported WAVE file format: actual bit rate does not match the container size."), ESearchCase::CaseSensitive)) {
			Status = TranscodingStatus::InvalidBitrate;
		}
		else if (ErrorReason.Equals(TEXT("Unsupported WAVE file format: subformat identifier not recognized."), ESearchCase::CaseSensitive)) {
			Status = TranscodingStatus::InvalidSubformat;
		}
		else {
			Status = TranscodingStatus::UnrecognizedReadWaveError;
		}
		return nullptr;
	}

	sw->RawPCMData = (uint8*)FMemory::Malloc(sw->RawPCMDataSize);

	// Callback Dispatcher OnProgress (not completely accurate implementation)
	OnProgress_Internal(90);

	FMemory::Memcpy(sw->RawPCMData, WaveInfo.SampleDataStart, sw->RawPCMDataSize);

	// Callback Dispatcher OnProgress (not completely accurate implementation)
	OnProgress_Internal(95);

	if (!sw) {
		Status = TranscodingStatus::InvalidUSoundWave;
		return nullptr;
	}
	Status = TranscodingStatus::SuccessTranscoding;
	return sw;
}

bool URuntimeAudioImporterLibrary::TranscodeAudioFileToPCMData(const char* filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus >& Status, uint64& framesToWrite, int16_t*& pSampleData, uint32& channels, uint32& sampleRate)
{
	OnProgress_Internal(5);
	switch (Format)
	{
	case AudioFormat::MP3:
	{
		drmp3_allocation_callbacks allocationCallbacksDecoding;
		allocationCallbacksDecoding.pUserData = NULL;
		allocationCallbacksDecoding.onMalloc = unreal_malloc;
		allocationCallbacksDecoding.onRealloc = unreal_realloc;
		allocationCallbacksDecoding.onFree = unreal_free;

		drmp3 mp3;
		if (!drmp3_init_file(&mp3, filePath, &allocationCallbacksDecoding)) {
			Status = TranscodingStatus::FailedToOpenStream;
			return false;
		}
		else {
			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(10);

			pSampleData = (int16_t*)FMemory::Malloc((size_t)drmp3_get_pcm_frame_count(&mp3) * mp3.channels * sizeof(int16_t));

			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(15);

			framesToWrite = drmp3_read_pcm_frames_s16(&mp3, drmp3_get_pcm_frame_count(&mp3), pSampleData);

			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(35);

			channels = mp3.channels;
			sampleRate = mp3.sampleRate;
			drmp3_uninit(&mp3);
			return true;
		}
		break;
	}
	case AudioFormat::WAV:
	{
		drwav_allocation_callbacks allocationCallbacksDecoding;
		allocationCallbacksDecoding.pUserData = NULL;
		allocationCallbacksDecoding.onMalloc = unreal_malloc;
		allocationCallbacksDecoding.onRealloc = unreal_realloc;
		allocationCallbacksDecoding.onFree = unreal_free;

		drwav wav;
		if (!drwav_init_file(&wav, filePath, &allocationCallbacksDecoding)) {
			Status = TranscodingStatus::FailedToOpenStream;
			return false;
		}
		else {
			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(10);

			pSampleData = (int16_t*)FMemory::Malloc((size_t)wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));

			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(15);

			framesToWrite = drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pSampleData);

			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(35);

			channels = wav.channels;
			sampleRate = wav.sampleRate;
			drwav_uninit(&wav);
			return true;
		}
		break;
	}
	case AudioFormat::FLAC:
	{
		drflac_allocation_callbacks allocationCallbacksDecoding;
		allocationCallbacksDecoding.pUserData = NULL;
		allocationCallbacksDecoding.onMalloc = unreal_malloc;
		allocationCallbacksDecoding.onRealloc = unreal_realloc;
		allocationCallbacksDecoding.onFree = unreal_free;

		drflac* pFlac = drflac_open_file(filePath, &allocationCallbacksDecoding);
		if (pFlac == NULL) {
			Status = TranscodingStatus::FailedToOpenStream;
			return false;
		}
		else {
			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(10);

			pSampleData = (int16_t*)FMemory::Malloc((size_t)pFlac->totalPCMFrameCount * pFlac->channels * sizeof(int16_t));

			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(15);

			framesToWrite = drflac_read_pcm_frames_s16(pFlac, pFlac->totalPCMFrameCount, pSampleData);

			// Callback Dispatcher OnProgress (not completely accurate implementation)
			OnProgress_Internal(35);

			channels = pFlac->channels;
			sampleRate = pFlac->sampleRate;
			drflac_close(pFlac);
			return true;
		}
		break;
	}
	default:
	{
		Status = TranscodingStatus::InvalidAudioFormat;
		return false;
		break;
	}
	}
}

bool URuntimeAudioImporterLibrary::TranscodePCMToWAVData(void*& WaveData, size_t& WaveDataSize, uint64 framesToWrite, int16_t*& pSampleData, uint32 channels, uint32 sampleRate) {
	drwav wavEncode;
	drwav_data_format format;
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = channels;
	format.sampleRate = sampleRate;
	format.bitsPerSample = 16; // Unreal Engine supports only 16-bit rate

	drwav_allocation_callbacks allocationCallbacks;
	allocationCallbacks.pUserData = NULL;
	allocationCallbacks.onMalloc = unreal_malloc;
	allocationCallbacks.onRealloc = unreal_realloc;
	allocationCallbacks.onFree = unreal_free;
	if (!drwav_init_memory_write(&wavEncode, &WaveData, &WaveDataSize, &format, &allocationCallbacks)) {
		return false; //error
	}

	// Callback Dispatcher OnProgress (not completely accurate implementation)
	OnProgress_Internal(40);

	drwav_write_pcm_frames(&wavEncode, framesToWrite, pSampleData);

	// Callback Dispatcher OnProgress (not completely accurate implementation)
	OnProgress_Internal(60);

	drwav_uninit(&wavEncode);
	return true;
}

TEnumAsByte<AudioFormat> URuntimeAudioImporterLibrary::GetAudioFormat(const FString& filePath)
{
	if (FPaths::GetExtension(filePath, false).Equals(TEXT("mp3"), ESearchCase::IgnoreCase)) {
		return AudioFormat::MP3;
	}
	else if (FPaths::GetExtension(filePath, false).Equals(TEXT("wav"), ESearchCase::IgnoreCase)) {
		return AudioFormat::WAV;
	}
	else if (FPaths::GetExtension(filePath, false).Equals(TEXT("flac"), ESearchCase::IgnoreCase)) {
		return AudioFormat::FLAC;
	}
	else {
		return AudioFormat::INVALID;
	}
}

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [=]() {
		OnProgress.Broadcast(Percentage);
		});
}

void URuntimeAudioImporterLibrary::OnResult_Internal(USoundWave* ReadySoundWave, TEnumAsByte < TranscodingStatus > Status)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [=]() {
		RemoveFromRoot();
		OnResult.Broadcast(ReadySoundWave, Status);
		});
}
