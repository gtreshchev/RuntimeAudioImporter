// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Codecs/RAW_RuntimeCodec.h"
#include "RuntimeAudioImporterTypes.h"
#include "RuntimeAudioTranscoder.generated.h"

enum class ERuntimeAudioFormat : uint8;
enum class ERuntimeRAWAudioFormat : uint8;

/** Static delegate broadcasting the result of the RAW data transcoded from buffer */
DECLARE_DELEGATE_TwoParams(FOnRAWDataTranscodeFromBufferResultNative, bool, const TArray64<uint8>&);

/** Dynamic delegate broadcasting the result of the RAW data transcoded from buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnRAWDataTranscodeFromBufferResult, bool, bSucceeded, const TArray<uint8>&, RAWData);


/** Static delegate broadcasting the result of the RAW data transcoded from file */
DECLARE_DELEGATE_OneParam(FOnRAWDataTranscodeFromFileResultNative, bool);

/** Dynamic delegate broadcasting the result of the RAW data transcoded from file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRAWDataTranscodeFromFileResult, bool, bSucceeded);


/** Static delegate broadcasting the result of the encoded data transcoded from buffer */
DECLARE_DELEGATE_TwoParams(FOnEncodedDataTranscodeFromBufferResultNative, bool, const TArray64<uint8>&);

/** Dynamic delegate broadcasting the result of the encoded data transcoded from buffer */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnEncodedDataTranscodeFromBufferResult, bool, bSucceeded, const TArray<uint8>&, EncodedData);


/** Static delegate broadcasting the result of the encoded data transcoded from file */
DECLARE_DELEGATE_OneParam(FOnEncodedDataTranscodeFromFileResultNative, bool);


/** Dynamic delegate broadcasting the result of the encoded data transcoded from file */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnEncodedDataTranscodeFromFileResult, bool, bSucceeded);


/**
 * Runtime Audio Transcoder
 * Contains functions for transcoding audio data from one format to another
 */
UCLASS(BlueprintType, Category = "Runtime Audio Transcoder")
class RUNTIMEAUDIOIMPORTER_API URuntimeAudioTranscoder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Transcode RAW data from one format into another from buffer
	 *
	 * @param RAWDataFrom The RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From Buffer"), Category = "Runtime Audio Transcoder")
	static void TranscodeRAWDataFromBuffer(UPARAM(DisplayName = "RAW Data From") TArray<uint8> RAWDataFrom, UPARAM(DisplayName = "RAW Format From") ERuntimeRAWAudioFormat RAWFormatFrom, UPARAM(DisplayName = "RAW Format To") ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResult& Result);

	/**
	 * Transcode RAW data from one format into another from buffer. Suitable for use with 64-bit data size
	 *
	 * @param RAWDataFrom The RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeRAWDataFromBuffer(TArray64<uint8> RAWDataFrom, ERuntimeRAWAudioFormat RAWFormatFrom, ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromBufferResultNative& Result);

	/**
	 * Transcode RAW data from one format into another from file
	 *
	 * @param FilePathFrom Path to file with the RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param FilePathTo File path for saving RAW data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transcode RAW Data From File"), Category = "Runtime Audio Transcoder")
	static void TranscodeRAWDataFromFile(const FString& FilePathFrom, UPARAM(DisplayName = "RAW Format From") ERuntimeRAWAudioFormat RAWFormatFrom, const FString& FilePathTo, UPARAM(DisplayName = "RAW Format To") ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResult& Result);

	/**
	 * Transcode RAW data from one format into another from file. Suitable for use with 64-bit data size
	 *
	 * @param FilePathFrom Path to file with the RAW audio data to transcode
	 * @param RAWFormatFrom The original format of the RAW audio data
	 * @param FilePathTo File path for saving RAW data
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeRAWDataFromFile(const FString& FilePathFrom, UPARAM(DisplayName = "RAW Format From") ERuntimeRAWAudioFormat RAWFormatFrom, const FString& FilePathTo, UPARAM(DisplayName = "RAW Format To") ERuntimeRAWAudioFormat RAWFormatTo, const FOnRAWDataTranscodeFromFileResultNative& Result);

	/**
	 * Transcode encoded data from one format into another from buffer
	 * 
	 * @param EncodedDataFrom The encoded audio data to transcode
	 * @param EncodedFormatFrom The original format of the encoded audio data
	 * @param EncodedFormatTo The desired format of the transcoded encoded audio data
	 * @param Quality The quality of the transcoded encoded audio data
	 * @param OverrideOptions The override options for the encoded audio data (fill with -1 if you don't want to override)
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Transcoder")
	static void TranscodeEncodedDataFromBuffer(TArray<uint8> EncodedDataFrom, ERuntimeAudioFormat EncodedFormatFrom, ERuntimeAudioFormat EncodedFormatTo, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnEncodedDataTranscodeFromBufferResult& Result);

	/**
	 * Transcode encoded data from one format into another from buffer. Suitable for use with 64-bit data size
	 * 
	 * @param EncodedDataFrom The encoded audio data to transcode
	 * @param EncodedFormatFrom The original format of the encoded audio data
	 * @param EncodedFormatTo The desired format of the transcoded encoded audio data
	 * @param Quality The quality of the transcoded encoded audio data
	 * @param OverrideOptions The override options for the encoded audio data (fill with -1 if you don't want to override)
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeEncodedDataFromBuffer(TArray64<uint8> EncodedDataFrom, ERuntimeAudioFormat EncodedFormatFrom, ERuntimeAudioFormat EncodedFormatTo, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnEncodedDataTranscodeFromBufferResultNative& Result);

	/**
	 * Transcode encoded data from one format into another from file
	 * 
	 * @param FilePathFrom The path to file with the encoded audio data to transcode 
	 * @param EncodedFormatFrom The original format of the encoded audio data
	 * @param FilePathTo The file path for saving encoded data
	 * @param EncodedFormatTo The desired format of the transcoded encoded audio data
	 * @param Quality The quality of the transcoded encoded audio data
	 * @param OverrideOptions The override options for the encoded audio data (fill with -1 if you don't want to override)
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Audio Transcoder")
	static void TranscodeEncodedDataFromFile(const FString& FilePathFrom, ERuntimeAudioFormat EncodedFormatFrom, const FString& FilePathTo, ERuntimeAudioFormat EncodedFormatTo, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnEncodedDataTranscodeFromFileResult& Result);

	/**
	 * Transcode encoded data from one format into another from file. Suitable for use with 64-bit data size
	 * 
	 * @param FilePathFrom The path to file with the encoded audio data to transcode
	 * @param EncodedFormatFrom The original format of the encoded audio data
	 * @param FilePathTo The file path for saving encoded data
	 * @param EncodedFormatTo The desired format of the transcoded encoded audio data
	 * @param Quality The quality of the transcoded encoded audio data
	 * @param OverrideOptions The override options for the encoded audio data (fill with -1 if you don't want to override)
	 * @param Result Delegate broadcasting the result
	 */
	static void TranscodeEncodedDataFromFile(const FString& FilePathFrom, ERuntimeAudioFormat EncodedFormatFrom, const FString& FilePathTo, ERuntimeAudioFormat EncodedFormatTo, uint8 Quality, const FRuntimeAudioExportOverrideOptions& OverrideOptions, const FOnEncodedDataTranscodeFromFileResultNative& Result);

	/**
	 * Helper function for transcoding RAW format
	 * 
	 * @param RAWFormatTo The desired format of the transcoded RAW audio data
	 * @param RAWDataFrom The RAW audio data to transcode
	 * @param RAWDataTo The RAW transcoded audio data
	 */
	template<typename FromType>
	static void TranscodeTo(ERuntimeRAWAudioFormat RAWFormatTo, TArray64<uint8>& RAWDataFrom, TArray64<uint8>& RAWDataTo)
	{
		switch (RAWFormatTo)
		{
		case ERuntimeRAWAudioFormat::Int8:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, int8>(RAWDataFrom, RAWDataTo);
			break;
		case ERuntimeRAWAudioFormat::UInt8:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, uint8>(RAWDataFrom, RAWDataTo);
			break;
		case ERuntimeRAWAudioFormat::Int16:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, int16>(RAWDataFrom, RAWDataTo);
			break;
		case ERuntimeRAWAudioFormat::UInt16:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, uint16>(RAWDataFrom, RAWDataTo);
			break;
		case ERuntimeRAWAudioFormat::Int32:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, int32>(RAWDataFrom, RAWDataTo);
			break;
		case ERuntimeRAWAudioFormat::UInt32:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, uint32>(RAWDataFrom, RAWDataTo);
			break;
		case ERuntimeRAWAudioFormat::Float32:
			FRAW_RuntimeCodec::TranscodeRAWData<FromType, float>(RAWDataFrom, RAWDataTo);
			break;
		}
	}

};
