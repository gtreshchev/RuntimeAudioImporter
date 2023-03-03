// Georgy Treshchev 2023.

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "RuntimeAudioImporterDefines.h"
#include <type_traits>
#include <limits>

class RUNTIMEAUDIOIMPORTER_API FRAW_RuntimeCodec
{
public:
	/**
	 * Getting the minimum and maximum values of the specified RAW format
	 *
	 * @note Key - Minimum, Value - Maximum
	 */
	template <typename IntegralType>
	static TTuple<long long, long long> GetRawMinAndMaxValues()
	{
		// Signed 8-bit integer
		if (std::is_same<IntegralType, int8>::value)
		{
			return TTuple<long long, long long>(std::numeric_limits<int8>::min(), std::numeric_limits<int8>::max());
		}

		// Unsigned 8-bit integer
		if (std::is_same<IntegralType, uint8>::value)
		{
			return TTuple<long long, long long>(std::numeric_limits<uint8>::min(), std::numeric_limits<uint8>::max());
		}

		// Signed 16-bit integer
		if (std::is_same<IntegralType, int16>::value)
		{
			return TTuple<long long, long long>(std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());
		}

		// Unsigned 16-bit integer
		if (std::is_same<IntegralType, uint16>::value)
		{
			return TTuple<long long, long long>(std::numeric_limits<uint16>::min(), std::numeric_limits<uint16>::max());
		}

		// Signed 32-bit integer
		if (std::is_same<IntegralType, int32>::value)
		{
			return TTuple<long long, long long>(std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max());
		}

		// Unsigned 32-bit integer
		if (std::is_same<IntegralType, uint32>::value)
		{
			return TTuple<long long, long long>(std::numeric_limits<uint32>::min(), std::numeric_limits<uint32>::max());
		}

		// Floating point 32-bit
		if (std::is_same<IntegralType, float>::value)
		{
			return TTuple<long long, long long>(-1, 1);
		}

		ensureMsgf(false, TEXT("Unsupported RAW format"));
		return TTuple<long long, long long>(0, 0);
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
		const int64 RawDataSize = RAWData_From.Num() / sizeof(IntegralTypeFrom);

		IntegralTypeTo* DataTo = nullptr;
		TranscodeRAWData<IntegralTypeFrom, IntegralTypeTo>(DataFrom, RawDataSize, DataTo);

		RAWData_To = TArray64<uint8>(reinterpret_cast<uint8*>(DataTo), RawDataSize * sizeof(IntegralTypeTo));
		FMemory::Free(DataTo);
	}

	/**
	 * Transcoding one RAW Data format to another
	 *
	 * @param RAWDataFrom Pointer to memory location of the RAW data for transcoding
	 * @param NumOfSamples Number of samples in the RAW data
	 * @param RAWDataTo Pointer to memory location of the transcoded RAW data with the specified format. The number of samples is RAWDataSize
	 */
	template <typename IntegralTypeFrom, typename IntegralTypeTo>
	static void TranscodeRAWData(const IntegralTypeFrom* RAWDataFrom, int64 NumOfSamples, IntegralTypeTo*& RAWDataTo)
	{
		/** Creating an empty PCM buffer */
		RAWDataTo = static_cast<IntegralTypeTo*>(FMemory::Malloc(NumOfSamples * sizeof(IntegralTypeTo)));

		const TTuple<long long, long long> MinAndMaxValuesFrom{GetRawMinAndMaxValues<IntegralTypeFrom>()};
		const TTuple<long long, long long> MinAndMaxValuesTo{GetRawMinAndMaxValues<IntegralTypeTo>()};

		/** Iterating through the RAW Data to transcode values using a divisor */
		for (int64 SampleIndex = 0; SampleIndex < NumOfSamples; ++SampleIndex)
		{
			RAWDataTo[SampleIndex] = static_cast<IntegralTypeTo>(FMath::GetMappedRangeValueClamped(FVector2D(MinAndMaxValuesFrom.Key, MinAndMaxValuesFrom.Value), FVector2D(MinAndMaxValuesTo.Key, MinAndMaxValuesTo.Value), RAWDataFrom[SampleIndex]));
		}

		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Transcoding RAW data of size '%llu' (min: %lld, max: %lld) to size '%llu' (min: %lld, max: %lld)"),
		       static_cast<uint64>(sizeof(IntegralTypeFrom)), MinAndMaxValuesFrom.Key, MinAndMaxValuesFrom.Value, static_cast<uint64>(sizeof(IntegralTypeTo)), MinAndMaxValuesTo.Key, MinAndMaxValuesTo.Value);
	}
};
