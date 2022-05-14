// Georgy Treshchev 2022.

#pragma once

#include "ImportedSoundWave.h"
#include "RuntimeAudioImporterTypes.h"
#include "RuntimeAudioImporterLibrary.generated.h"

/** Static delegate broadcast to get the audio importer progress */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgressNative, const int32 Percentage);

/** Dynamic delegate broadcast to get the audio importer progress */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioImporterProgress, const int32, Percentage);


/** Static delegate broadcast to get the audio importer result */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResultNative, class URuntimeAudioImporterLibrary* RuntimeAudioImporterObjectRef, UImportedSoundWave* SoundWaveRef, ETranscodingStatus Status);

/** Dynamic delegate broadcast to get the audio importer result */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAudioImporterResult, class URuntimeAudioImporterLibrary*, RuntimeAudioImporterObjectRef, UImportedSoundWave*, SoundWaveRef, ETranscodingStatus, Status);

/** Forward declaration of the UPreImportedSoundAsset class */
class UPreImportedSoundAsset;

/**
 * Runtime Audio Importer library
 * Various functions related to transcoding audio data, such as importing audio files, manually encoding / decoding audio data and more
 */
UCLASS(BlueprintType, Category = "Runtime Audio Importer")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioImporterLibrary : public UObject
{
	GENERATED_BODY()

public:
	/** Bind to know when audio import is on progress. Recommended for C++ only */
	FOnAudioImporterProgressNative OnProgressNative;
	
	/** Bind to know when audio import is on progress. Recommended for Blueprints only */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer|Delegates")
	FOnAudioImporterProgress OnProgress;

	/** Bind to know when audio import is complete (even if it fails). Recommended for C++ only */
	FOnAudioImporterResultNative OnResultNative;
	
	/** Bind to know when audio import is complete (even if it fails). Recommended for Blueprints only */
	UPROPERTY(BlueprintAssignable, Category = "Runtime Audio Importer|Delegates")
	FOnAudioImporterResult OnResult;

	/**
	 * Instantiates a RuntimeAudioImporter object
	 *
	 * @return The RuntimeAudioImporter object. Bind to it's OnProgress and OnResult delegates
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Create, Audio, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer")
	static URuntimeAudioImporterLibrary* CreateRuntimeAudioImporter();

	/**
	 * Import audio from file
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format Audio format
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromFile(const FString& FilePath, EAudioFormat Format);

	/**
	 * Import audio file from the pre-imported sound asset
	 *
	 * @param PreImportedSoundAssetRef PreImportedSoundAsset object reference
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef);

	/**
	 * Import audio from buffer
	 *
	 * @param AudioData Audio data array
	 * @param Format Audio format
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Importer, Transcoder, Converter, Runtime, MP3, FLAC, WAV, OGG, Vorbis"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromBuffer(TArray<uint8> AudioData, EAudioFormat Format);

	/**
	 * Import audio from RAW file. Audio data must not have headers and must be uncompressed
	 *
	 * @param FilePath Path to the audio file to import
	 * @param Format RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW File"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat Format, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Import audio from RAW buffer. Audio data must not have headers and must be uncompressed
	 *
	 * @param RAWBuffer RAW audio buffer
	 * @param Format RAW audio format
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Import Audio From RAW Buffer"), Category = "Runtime Audio Importer|Import")
	void ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format, int32 SampleRate = 44100, int32 NumOfChannels = 1);

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From RAW data for transcoding
	 * @param FormatFrom Original format
	 * @param RAWData_To Transcoded RAW data with the specified format
	 * @param FormatTo Required format
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From Buffer"), Category = "Runtime Audio Importer|Transcode")
	static void TranscodeRAWDataFromBuffer(TArray<uint8> RAWData_From, ERAWAudioFormat FormatFrom, TArray<uint8>& RAWData_To, ERAWAudioFormat FormatTo);

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param FilePathFrom Path to file with RAW data for transcoding
	 * @param FormatFrom Original format
	 * @param FilePathTo File path for saving RAW data
	 * @param FormatTo Required format
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From File"), Category = "Runtime Audio Importer|Transcode")
	static bool TranscodeRAWDataFromFile(const FString& FilePathFrom, ERAWAudioFormat FormatFrom, const FString& FilePathTo, ERAWAudioFormat FormatTo);

	/**
	 * Export the imported sound wave to file
	 *
	 * @param ImporterSoundWave Reference to the imported sound wave
	 * @param AudioFormat Required format to export Please note that some formats are not supported
	 * @param SavePath Path to save the file
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Export")
	static bool ExportSoundWaveToFile(UImportedSoundWave* ImporterSoundWave, const FString& SavePath, EAudioFormat AudioFormat, uint8 Quality);

	/**
	 * Export the imported sound wave to buffer
	 *
	 * @param ImporterSoundWave Reference to the imported sound wave
	 * @param AudioFormat Required format to export Please note that some formats are not supported
	 * @param AudioData The exported (compressed) audio data
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Export")
	static bool ExportSoundWaveToBuffer(UImportedSoundWave* ImporterSoundWave, TArray<uint8>& AudioData, EAudioFormat AudioFormat, uint8 Quality);

	/**
	 * Get audio format by extension
	 *
	 * @param FilePath File path where to find the format (by extension)
	 * @return The found audio format (e.g. mp3. flac, etc)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static EAudioFormat GetAudioFormat(const FString& FilePath);

	/**
	 * Determine audio format based on audio data. A more advanced way to get the format
	 *
	 * @param AudioData Audio data array
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static EAudioFormat GetAudioFormatAdvanced(const TArray<uint8>& AudioData);

	/**
	 * Convert seconds to string (hh:mm:ss or mm:ss depending on the number of seconds)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Importer|Utilities")
	static FString ConvertSecondsToString(int32 Seconds);

	/**
	 * Decode compressed audio data to uncompressed
	 *
	 * @param EncodedAudioInfo Encoded audio data
	 * @param DecodedAudioInfo Decoded audio data
	 * @return Whether the decoding was successful or not
	 */
	static bool DecodeAudioData(FEncodedAudioStruct& EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo);

	/**
	 * Encode uncompressed audio data to compressed
	 *
	 * @param DecodedAudioInfo Decoded audio data
	 * @param EncodedAudioInfo Encoded audio data
	 * @param Quality The quality of the encoded audio data. From 0 to 100
	 * @return Whether the encoding was successful or not
	 */
	static bool EncodeAudioData(const FDecodedAudioStruct& DecodedAudioInfo, FEncodedAudioStruct& EncodedAudioInfo, uint8 Quality);

	/**
	 * Determine audio format based on audio data
	 *
	 * @param AudioData Pointer to in-memory audio data
	 * @param AudioDataSize Size of in-memory audio data
	 */
	static EAudioFormat GetAudioFormat(const uint8* AudioData, int32 AudioDataSize);

	/**
	 * Import audio from 32-bit float PCM data
	 *
	 * @param PCMData Pointer to memory location of the PCM data
	 * @param PCMDataSize Memory size allocated for the PCM data
	 * @param SampleRate The number of samples per second
	 * @param NumOfChannels The number of channels (1 for mono, 2 for stereo, etc)
	 */
	void ImportAudioFromFloat32Buffer(uint8* PCMData, const int32 PCMDataSize, const int32 SampleRate = 44100, const int32 NumOfChannels = 1);

	/**
	 * Create Imported Sound Wave and finish importing.
	 *
	 * @param DecodedAudioInfo Decoded audio data
	 */
	void ImportAudioFromDecodedInfo(const FDecodedAudioStruct& DecodedAudioInfo);

	/**
	 * Define SoundWave object reference
	 *
	 * @param SoundWaveRef Reference to the imported sound wave
	 * @param DecodedAudioInfo Decoded audio data
	 * @return Whether the defining was successful or not
	 */
	virtual void DefineSoundWave(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo);

	/**
	 * Fill SoundWave basic information (e.g. duration, number of channels, etc)
	 *
	 * @param SoundWaveRef Reference to the imported sound wave
	 * @param DecodedAudioInfo Decoded audio data
	 */
	static void FillSoundWaveBasicInfo(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo);

	/**
	 * Fill SoundWave PCM data buffer
	 *
	 * @param SoundWaveRef Reference to the imported sound wave
	 * @param DecodedAudioInfo Decoded audio data
	 */
	static void FillPCMData(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo);

protected:
	/** Creates a new instance of the ImportedSoundWave class to use */
	virtual UImportedSoundWave* CreateImportedSoundWave() const;

	/**
	 * Audio transcoding progress callback
	 * 
	 * @param Percentage Percentage of importing completion (0-100%)
	 */
	void OnProgress_Internal(int32 Percentage);

	/**
	 * Audio importing finished callback
	 * 
	 * @param SoundWaveRef Reference to the imported sound wave
	 * @param Status Importing status
	 */
	void OnResult_Internal(UImportedSoundWave* SoundWaveRef, ETranscodingStatus Status);
};
