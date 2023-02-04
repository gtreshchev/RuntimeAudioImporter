// Georgy Treshchev 2023.

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "RuntimeAudioImporterDefines.h"

class RUNTIMEAUDIOIMPORTER_API FRAW_RuntimeCodec
{
public:
	/**
	 * Getting the minimum and maximum values of the specified RAW format
	 *
	 * @note Key - Minimum, Value - Maximum
	 */
	template <typename IntegralType>
	static TTuple<double, double> GetRawMinAndMaxValues()
	{
		/** Signed 16-bit PCM */
		if (TIsSame<IntegralType, int16>::Value)
		{
			return TTuple<double, double>(-32767.f, 32768.f);
		}

		/** Signed 32-bit PCM */
		if (TIsSame<IntegralType, int32>::Value)
		{
			return TTuple<double, double>(-2147483648.f, 2147483647.f);
		}

		/** Unsigned 8-bit PCM */
		if (TIsSame<IntegralType, uint8>::Value)
		{
			return TTuple<double, double>(0.f, 255.f);
		}

		/** 32-bit float */
		if (TIsSame<IntegralType, float>::Value)
		{
			return TTuple<double, double>(-1.f, 1.f);
		}

		return TTuple<double, double>(-1.f, 1.f);
	}

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWData_From RAW data for transcoding
	 * @param RAWData_To Transcoded RAW data with the specified format
	 */
	template <typename IntegralTypeFrom, typename IntegralTypeTo>
	static void TranscodeRAWData(const TArray64<uint8>& RAWData_From, TArray64<uint8>& RAWData_To)
	{
		const IntegralTypeFrom* DataFrom = reinterpret_cast<const IntegralTypeFrom*>(RAWData_From.GetData());
		const int64 DataFrom_Size = RAWData_From.Num();

		IntegralTypeTo* DataTo = nullptr;
		int64 DataTo_Size = 0;

		TranscodeRAWData<IntegralTypeFrom, IntegralTypeTo>(DataFrom, DataFrom_Size, DataTo, DataTo_Size);

		RAWData_To = TArray64<uint8>(reinterpret_cast<uint8*>(DataTo), DataTo_Size);

		FMemory::Free(DataTo);
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
	static void TranscodeRAWData(const IntegralTypeFrom* RAWData_From, int64 RAWDataSize_From, IntegralTypeTo*& RAWData_To, int64& RAWDataSize_To)
	{
		/** Getting the required number of samples to transcode */
		const uint64 NumSamples = RAWDataSize_From / sizeof(IntegralTypeFrom);

		/** Getting the required PCM size */
		RAWDataSize_To = NumSamples * sizeof(IntegralTypeTo);

		/** Creating an empty PCM buffer */
		IntegralTypeTo* TempPCMData = static_cast<IntegralTypeTo*>(FMemory::Malloc(RAWDataSize_To));

		const TTuple<double, double> MinAndMaxValuesFrom{GetRawMinAndMaxValues<IntegralTypeFrom>()};
		const TTuple<double, double> MinAndMaxValuesTo{GetRawMinAndMaxValues<IntegralTypeTo>()};

		/** Iterating through the RAW Data to transcode values using a divisor */
		for (uint64 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			TempPCMData[SampleIndex] = static_cast<IntegralTypeTo>(FMath::GetMappedRangeValueClamped(FVector2D(MinAndMaxValuesFrom.Key, MinAndMaxValuesFrom.Value), FVector2D(MinAndMaxValuesTo.Key, MinAndMaxValuesTo.Value), RAWData_From[SampleIndex]));
		}

		/** Returning the transcoded data as bytes */
		RAWData_To = reinterpret_cast<IntegralTypeTo*>(TempPCMData);

		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Transcoding RAW data of size '%llu' (min: %f, max: %f) to size '%llu' (min: %f, max: %f)"),
		       static_cast<uint64>(sizeof(IntegralTypeFrom)), MinAndMaxValuesFrom.Key, MinAndMaxValuesFrom.Value, static_cast<uint64>(sizeof(IntegralTypeTo)), MinAndMaxValuesTo.Key, MinAndMaxValuesTo.Value);
	}
};
