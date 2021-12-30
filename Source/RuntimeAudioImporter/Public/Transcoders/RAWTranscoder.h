// Georgy Treshchev 2022.

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
	static TPair<double, double> GetRawMinAndMaxValues()
	{
		/** Signed 16-bit PCM */
		if (TIsSame<IntegralType, int16>::Value)
		{
			return TPair<double, double>(-32767, 32768);
		}

		/** Signed 32-bit PCM */
		if (TIsSame<IntegralType, int32>::Value)
		{
			return TPair<double, double>(-2147483648.0, 2147483647.0);
		}

		/** Unsigned 8-bit PCM */
		if (TIsSame<IntegralType, uint8>::Value)
		{
			return TPair<double, double>(-128.0, 127.0);
		}

		/** 32-bit float */
		if (TIsSame<IntegralType, float>::Value)
		{
			return TPair<double, double>(-1.0, 1.0);
		}

		return TPair<double, double>(-1.0, 1.0);
	}

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From RAW data for transcoding
	 * @param RAWData_To Transcoded RAW data with the specified format
	 */
	template <typename IntegralTypeFrom, typename IntegralTypeTo>
	static void TranscodeRAWData(TArray<uint8> RAWData_From, TArray<uint8>& RAWData_To)
	{
		IntegralTypeFrom* DataFrom = reinterpret_cast<IntegralTypeFrom*>(RAWData_From.GetData());
		const uint32 DataFrom_Size = RAWData_From.Num() - 2;

		IntegralTypeTo* DataTo = nullptr;
		uint32 DataTo_Size = 0;

		TranscodeRAWData<IntegralTypeFrom, IntegralTypeTo>(DataFrom, DataFrom_Size, DataTo, DataTo_Size);

		RAWData_To = TArray<uint8>(reinterpret_cast<uint8*>(DataTo), DataTo_Size);
	}

	static float NormalizeToRange(float Value, float RangeMin, float RangeMax)
	{
		if (RangeMin == RangeMax)
		{
			if (Value < RangeMin)
			{
				return 0.f;
			}
			return 1.f;
		}

		if (RangeMin > RangeMax)
		{
			Swap(RangeMin, RangeMax);
		}
		return (Value - RangeMin) / (RangeMax - RangeMin);
	}

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From Pointer to memory location of the RAW data for transcoding
	 * @param RAWDataSize_From Memory size allocated for the RAW data
	 * @param RAWData_To Pointer to memory location of the transcoded RAW data with the specified format
	 * @param RAWDataSize_To Memory size allocated for the RAW data
	 */
	template <typename IntegralTypeFrom, typename IntegralTypeTo>
	static void TranscodeRAWData(IntegralTypeFrom* RAWData_From, uint32 RAWDataSize_From, IntegralTypeTo*& RAWData_To, uint32& RAWDataSize_To)
	{
		/** Getting the required number of samples to transcode */
		const int32 NumSamples = RAWDataSize_From / sizeof(IntegralTypeFrom);

		/** Getting the required PCM size */
		RAWDataSize_To = NumSamples * sizeof(IntegralTypeTo);

		/** Creating an empty PCM buffer */
		IntegralTypeTo* TempPCMData = static_cast<IntegralTypeTo*>(FMemory::MallocZeroed(RAWDataSize_To));

		const TPair<double, double> MinAndMaxValuesFrom{GetRawMinAndMaxValues<IntegralTypeFrom>()};
		const TPair<double, double> MinAndMaxValuesTo{GetRawMinAndMaxValues<IntegralTypeTo>()};

		/** Iterating through the RAW Data to transcode values using a divisor */
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			const float NormalizedValue = NormalizeToRange(RAWData_From[SampleIndex], MinAndMaxValuesFrom.Key, MinAndMaxValuesFrom.Value);

			TempPCMData[SampleIndex] = static_cast<IntegralTypeTo>(NormalizedValue * MinAndMaxValuesTo.Value);
		}

		/** Returning the transcoded data as bytes */
		RAWData_To = reinterpret_cast<IntegralTypeTo*>(TempPCMData);
	}
};
