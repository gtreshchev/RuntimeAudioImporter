// Respirant 2020.

#include "RuntimeAudioImporterBPLibrary.h"
#include "RuntimeAudioImporter.h"

URuntimeAudioImporterBPLibrary::URuntimeAudioImporterBPLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

/**
* Transcode audio file to USoundWave static object
*
* @param filePath							Path to the audio file to import
* @param Format								Audio file format (extension)
* @param status								Final import status (TranscodingStatus)
* @param DefineFormatAutomatically			Whether to define format (extension) automatically or not
* @return Returns USoundWave Static Object.
*/
class USoundWave* URuntimeAudioImporterBPLibrary::GetSoundWaveFromAudioFile(const FString& filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus >& status, bool DefineFormatAutomatically)
{
	return GetUSoundWaveFromAudioFile_Internal(filePath, Format, status, DefineFormatAutomatically);
}

/**
* Destroy USoundWave static object
*
* @return Returns true - success, false - failure.
*/
bool URuntimeAudioImporterBPLibrary::DestroySoundWave(USoundWave* ReadySoundWave) {

	if (IsValid(ReadySoundWave) && ReadySoundWave->IsValidLowLevel()) {

		/* Free memory */
		ReadySoundWave->InvalidateCompressedData();
		FMemory::Free(ReadySoundWave->RawPCMData);
		ReadySoundWave->RawData.RemoveBulkData();
		ReadySoundWave->RemoveAudioResource();
		/* Free memory */

		/* Destroying USoundWave object as UObject */
		ReadySoundWave->ConditionalBeginDestroy();
		ReadySoundWave = NULL;
		/* Destroying USoundWave object as UObject */

		return true;
	}
	else {
		return false;
	}
}

/* Get USoundWave static object reference */
class USoundWave* URuntimeAudioImporterBPLibrary::GetSoundWaveObject(const uint8* WaveData, int32 WaveDataSize, TEnumAsByte < TranscodingStatus >& status) {
	USoundWave* sw = NewObject<USoundWave>(USoundWave::StaticClass());
	if (!sw) {
		status = TranscodingStatus::USoundWaveDeclarationError;
		return nullptr;
	}
	FWaveModInfo WaveInfo;
	FString ErrorReason;
	if (WaveInfo.ReadWaveInfo(WaveData, WaveDataSize, &ErrorReason))
	{
		if (*WaveInfo.pBitsPerSample != 16) {
			status = TranscodingStatus::UnsupportedBitDepth;
			return nullptr;
		}
		sw->InvalidateCompressedData();
		sw->RawData.Lock(LOCK_READ_WRITE);
		void* LockedData = sw->RawData.Realloc(WaveDataSize);
		FMemory::Memcpy(LockedData, WaveData, WaveDataSize);
		sw->RawData.Unlock();

		int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;
		if (DurationDiv)
		{
			sw->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			sw->Duration = 0.0f;
			status = TranscodingStatus::ZeroDurationError;
			return nullptr;
		}
		sw->SetSampleRate(*WaveInfo.pSamplesPerSec);
		sw->NumChannels = *WaveInfo.pChannels;
		sw->RawPCMDataSize = WaveInfo.SampleDataSize;
		sw->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
	}
	else {
		if (ErrorReason.Equals(TEXT("Invalid WAVE file."), ESearchCase::CaseSensitive)) {
			status = TranscodingStatus::InvalidWAVEData;
		}
		else if (ErrorReason.Equals(TEXT("Unsupported WAVE file format: actual bit rate does not match the container size."), ESearchCase::CaseSensitive)) {
			status = TranscodingStatus::InvalidBitrate;
		}
		else if (ErrorReason.Equals(TEXT("Unsupported WAVE file format: subformat identifier not recognized."), ESearchCase::CaseSensitive)) {
			status = TranscodingStatus::InvalidBitrate;
		}
		else {
			status = TranscodingStatus::UnrecognizedReadWaveError;
		}
		return nullptr;
	}

	sw->RawPCMData = (uint8*)FMemory::Malloc(sw->RawPCMDataSize);
	FMemory::Memcpy(sw->RawPCMData, WaveInfo.SampleDataStart, sw->RawPCMDataSize);

	if (!sw) {
		status = TranscodingStatus::InvalidUSoundWave;
		return nullptr;
	}
	status = TranscodingStatus::SuccessTranscoding;
	return sw;
}

#include "ThirdParty/dr_wav.h"

#include "ThirdParty/dr_mp3.h"

#include "ThirdParty/dr_flac.h"

/* Main internal get USoundWave from audio file */
class USoundWave* URuntimeAudioImporterBPLibrary::GetUSoundWaveFromAudioFile_Internal(const FString& filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus >& status, bool DefineFormatAutomatically)
{
	if (FPaths::FileExists(filePath) != true) {
		status = TranscodingStatus::AudioDoesNotExist;
		return nullptr;
	}

	int16_t* pSampleData;
	uint64 framesToWrite;
	uint32 channels;
	uint32 sampleRate;

	if (DefineFormatAutomatically) {
		Format = GetAudioFormat(filePath);
		if (Format == AudioFormat::INVALID) {
			status = TranscodingStatus::InvalidAudioFormat;
			return nullptr;
		}
	}
	if (TranscodeAudioToWaveData(TCHAR_TO_ANSI(*filePath), Format, status, framesToWrite, pSampleData, channels, sampleRate) == false) {
		return nullptr;
	}

	drwav wavEncode;
	drwav_data_format format;
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = channels;
	format.sampleRate = sampleRate;
	format.bitsPerSample = 16; // Unreal Engine 4 supports only 16 bit rate

	void* pWavFileData;
	size_t wavFileSize;
	drwav_init_memory_write(&wavEncode, &pWavFileData, &wavFileSize, &format, NULL);
	drwav_write_pcm_frames(&wavEncode, framesToWrite, pSampleData);
	drwav_uninit(&wavEncode);

	USoundWave* ReadyUSoundWave = GetSoundWaveObject((const uint8*)pWavFileData, wavFileSize, status); // A ready USoundWave static object

	drwav_free(pSampleData, NULL);
	drwav_free(pWavFileData, NULL);

	return ReadyUSoundWave;
}

/* Automatically get audio format. Internal use only recommended */
TEnumAsByte<AudioFormat> URuntimeAudioImporterBPLibrary::GetAudioFormat(const FString& filePath) {
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

/* Transcode Audio to Wave Data */
bool URuntimeAudioImporterBPLibrary::TranscodeAudioToWaveData(const char* filePath, TEnumAsByte<AudioFormat> Format, TEnumAsByte<TranscodingStatus>& status, uint64& framesToWrite, int16_t*& pSampleData, uint32& channels, uint32& sampleRate) {
	switch (Format)
	{
	case AudioFormat::MP3:
	{
		drmp3 mp3;
		if (!drmp3_init_file(&mp3, filePath, NULL)) {
			status = TranscodingStatus::FailedToOpenStream;
			return false;
		}
		else {
			pSampleData = (int16_t*)malloc((size_t)drmp3_get_pcm_frame_count(&mp3) * mp3.channels * sizeof(int16_t));
			framesToWrite = drmp3_read_pcm_frames_s16(&mp3, drmp3_get_pcm_frame_count(&mp3), pSampleData);
			channels = mp3.channels;
			sampleRate = mp3.sampleRate;
			drmp3_uninit(&mp3);
			return true;
		}
		break;
	}
	case AudioFormat::WAV:
	{
		drwav wav;
		if (!drwav_init_file(&wav, filePath, NULL)) {
			status = TranscodingStatus::FailedToOpenStream;
			return false;
		}
		else {
			pSampleData = (int16_t*)malloc((size_t)wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
			framesToWrite = drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pSampleData);
			channels = wav.channels;
			sampleRate = wav.sampleRate;
			drwav_uninit(&wav);
			return true;
		}
		break;
	}
	case AudioFormat::FLAC:
	{
		drflac* pFlac = drflac_open_file(filePath, NULL);
		if (pFlac == NULL) {
			status = TranscodingStatus::FailedToOpenStream;
			return false;
		}
		else {
			pSampleData = (int16_t*)malloc((size_t)pFlac->totalPCMFrameCount * pFlac->channels * sizeof(int16_t));
			framesToWrite = drflac_read_pcm_frames_s16(pFlac, pFlac->totalPCMFrameCount, pSampleData);
			channels = pFlac->channels;
			sampleRate = pFlac->sampleRate;
			drflac_close(pFlac);
			return true;
		}
		break;
	}
	default:
	{
		status = TranscodingStatus::InvalidAudioFormat;
		return false;
		break;
	}
	}
}
