// Respirant 2020.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "Sound/SoundWave.h"

#include "Developer/TargetPlatform/Public/Interfaces/IAudioFormat.h"

#include "RuntimeAudioImporterBPLibrary.generated.h"


UENUM(BlueprintType)
enum TranscodingStatus
{
	SuccessTranscoding			UMETA(DisplayName = "Success"),
	FailedToOpenStream			UMETA(DisplayName = "Failed to open stream"),
	UnsupportedBitDepth			UMETA(DisplayName = "Unsupported bit depth (Unreal Engine only supports 16 bit)"),
	InvalidWAVEData				UMETA(DisplayName = "Invalid WAVE data"),
	InvalidBitrate				UMETA(DisplayName = "Invalid bitrate (Actual bit rate does not match the container size)"),
	InvalidSubformat			UMETA(DisplayName = "InvalidSubformat (Subformat identifier not recognized)"),
	UnrecognizedReadWaveError   UMETA(DisplayName = "Unrecognized ReadWave error"),
	USoundWaveDeclarationError	UMETA(DisplayName = "USoundWave declaration error"),
	InvalidUSoundWave			UMETA(DisplayName = "Invalid USoundWave (The USoundWave object contains incorrect data)"),
	InvalidAudioFormat			UMETA(DisplayName = "Invalid audio format (Can't determine the format of the audio file)"),
	AudioDoesNotExist			UMETA(DisplayName = "The audio file does not exist"),
	ZeroDurationError			UMETA(DisplayName = "Transcoded audio has zero length")
};

UENUM(BlueprintType)
enum AudioFormat
{
	MP3							UMETA(DisplayName = "mp3"),
	WAV							UMETA(DisplayName = "wav"),
	FLAC						UMETA(DisplayName = "flac"),
	INVALID						UMETA(DisplayName = "invalid (not defined format, internal use only)", Hidden)
};

UCLASS()
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioImporterBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
public:

	/**
	* Transcode audio file to USoundWave static object 
	*
	* @param filePath							Path to the audio file to import
	* @param Format								Audio file format (extension)
	* @param status								Final import status (TranscodingStatus)
	* @param DefineFormatAutomatically			Whether to define format (extension) automatically or not
	* @return Returns USoundWave Static Object.
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, MP3, FLAC, WAV", ToolTip = "Transcode audio file to USoundWave static object"), Category = "RuntimeAudioImporter")
		static class USoundWave* GetSoundWaveFromAudioFile(const FString & filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus > & status, bool DefineFormatAutomatically);

	/**
	* Destroy USoundWave static object
	*
	* @return Returns true - success, false - failure.
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, MP3, FLAC, WAV, Destroy", ToolTip = "After the Soundwave is no longer needed, you need to call this function to free memory"), Category = "RuntimeAudioImporter")
		static bool DestroySoundWave(USoundWave * ReadySoundWave);

private:

	/* Main internal get USoundWave from audio file */
	static class USoundWave* GetUSoundWaveFromAudioFile_Internal(const FString & filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus > & status, bool DefineFormatAutomatically);

	/* Get USoundWave static object reference */
	static class USoundWave* GetSoundWaveObject(const uint8 * WaveData, int32 WaveDataSize, TEnumAsByte < TranscodingStatus > & status);

	/* Transcode Audio to Wave Data */
	static bool TranscodeAudioToWaveData(const char* filePath, TEnumAsByte < AudioFormat > Format, TEnumAsByte < TranscodingStatus > & status, uint64 & framesToWrite, int16_t * &pSampleData, uint32 & channels, uint32 & sampleRate);

	/* Automatically get audio format. Internal use only recommended */
	static TEnumAsByte<AudioFormat> GetAudioFormat(const FString & filePath);
};
