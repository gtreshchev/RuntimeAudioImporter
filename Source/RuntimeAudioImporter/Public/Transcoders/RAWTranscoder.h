// Georgy Treshchev 2021.

#pragma once

#include "CoreMinimal.h"

class RUNTIMEAUDIOIMPORTER_API RAWTranscoder
{
public:
	/**
	 * Getting the minimum and maximum values of the specified RAW format
	 *
	 * @note Key - Minimum, Value - Maximum
	 */
	template <typename IntegralType>
	static TPair<double, double> GetRawMinAndMaxValues();
	
	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From RAW data for transcoding
	 * @param RAWData_To Transcoded RAW data with the specified format
	 */
	template <typename IntegralTypeFrom, typename IntegralTypeTo>
	static void TranscodeRAWData(TArray<uint8> RAWData_From, TArray<uint8>& RAWData_To);

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From Pointer to memory location of the RAW data for transcoding
	 * @param RAWDataSize_From Memory size allocated for the RAW data
	 * @param RAWData_To Pointer to memory location of the transcoded RAW data with the specified format
	 * @param RAWDataSize_To Memory size allocated for the RAW data
	 */
	template <typename IntegralTypeFrom, typename IntegralTypeTo>
	static void TranscodeRAWData(IntegralTypeFrom* RAWData_From, uint32 RAWDataSize_From, IntegralTypeTo*& RAWData_To, uint32& RAWDataSize_To);
};
