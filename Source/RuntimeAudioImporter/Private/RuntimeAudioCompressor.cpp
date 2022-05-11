// Georgy Treshchev 2022.

#include "RuntimeAudioCompressor.h"

#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"

#include "Transcoders/RAWTranscoder.h"
#include "Transcoders/VorbisTranscoder.h"
#include "Transcoders/WAVTranscoder.h"

#include "Async/Async.h"

#if ENGINE_MAJOR_VERSION < 5
#include "AudioDevice.h"
#include "AudioCompressionSettingsUtils.h"
#endif

namespace
{
#if ENGINE_MAJOR_VERSION < 5
	FName GetPlatformSpecificFormat(const FName& Format)
	{
		const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides();

		// Platforms that require compression overrides get concatenated formats

#if WITH_EDITOR
		FName PlatformSpecificFormat;
		if (CompressionOverrides)
		{
			FString HashedString = *Format.ToString();
			FPlatformAudioCookOverrides::GetHashSuffix(CompressionOverrides, HashedString);
			PlatformSpecificFormat = *HashedString;
		}
		else
		{
			PlatformSpecificFormat = Format;
		}
#else

		// Cache the concatenated hash
		static FName PlatformSpecificFormat;
		static FName CachedFormat;
		if (!Format.IsEqual(CachedFormat))
		{
			if (CompressionOverrides)
			{
				FString HashedString = *Format.ToString();
				FPlatformAudioCookOverrides::GetHashSuffix(CompressionOverrides, HashedString);
				PlatformSpecificFormat = *HashedString;
			}
			else
			{
				PlatformSpecificFormat = Format;
			}

			CachedFormat = Format;
		}
#endif

		return PlatformSpecificFormat;
	}
#endif
}

URuntimeAudioCompressor* URuntimeAudioCompressor::CreateRuntimeAudioCompressor()
{
	return NewObject<URuntimeAudioCompressor>();
}

void URuntimeAudioCompressor::CompressSoundWave(UImportedSoundWave* ImportedSoundWaveRef, FCompressedSoundWaveInfo CompressedSoundWaveInfo, uint8 Quality, bool bFillCompressedBuffer, bool bFillPCMBuffer, bool bFillRAWWaveBuffer)
{
	USoundWave* RegularSoundWaveRef = NewObject<USoundWave>(USoundWave::StaticClass());

	if (!RegularSoundWaveRef)
	{
		BroadcastResult(nullptr);
		return;
	}

	RegularSoundWaveRef->AddToRoot();

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this,ImportedSoundWaveRef, RegularSoundWaveRef, Quality, bFillPCMBuffer, bFillRAWWaveBuffer, bFillCompressedBuffer, CompressedSoundWaveInfo]()
	{
		// Filling in decoded audio info
		FDecodedAudioStruct DecodedAudioInfo;
		{
			DecodedAudioInfo.PCMInfo = ImportedSoundWaveRef->PCMBufferInfo;
			FSoundWaveBasicStruct SoundWaveBasicInfo;
			{
				SoundWaveBasicInfo.NumOfChannels = ImportedSoundWaveRef->NumChannels;
				SoundWaveBasicInfo.SampleRate = ImportedSoundWaveRef->SamplingRate;
				SoundWaveBasicInfo.Duration = ImportedSoundWaveRef->Duration;
			}
			DecodedAudioInfo.SoundWaveBasicInfo = SoundWaveBasicInfo;
		}

		// Filling in the basic information of the sound wave
		{
			RegularSoundWaveRef->Duration = DecodedAudioInfo.SoundWaveBasicInfo.Duration;
			RegularSoundWaveRef->SetSampleRate(DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
			RegularSoundWaveRef->NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
			RegularSoundWaveRef->SoundGroup = SOUNDGROUP_Default;

			if (RegularSoundWaveRef->NumChannels == 4)
			{
				RegularSoundWaveRef->bIsAmbisonics = 1;
			}

			RegularSoundWaveRef->bProcedural = false;
			RegularSoundWaveRef->DecompressionType = EDecompressionType::DTYPE_RealTime;

			// Filling in the compressed sound wave info
			{
				RegularSoundWaveRef->SoundGroup = CompressedSoundWaveInfo.SoundGroup;
				RegularSoundWaveRef->bLooping = CompressedSoundWaveInfo.bLooping;
				RegularSoundWaveRef->Volume = CompressedSoundWaveInfo.Volume;
				RegularSoundWaveRef->Pitch = CompressedSoundWaveInfo.Pitch;
			}
		}

		// Filling in the standard PCM buffer if needed
		if (bFillPCMBuffer)
		{
			int16* RawPCMData;
			int32 RawPCMDataSize;
			RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData()), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num(), RawPCMData, RawPCMDataSize);
			RegularSoundWaveRef->RawPCMDataSize = RawPCMDataSize;
			RegularSoundWaveRef->RawPCMData = static_cast<uint8*>(FMemory::Malloc(RawPCMDataSize));
			FMemory::Memcpy(RegularSoundWaveRef->RawPCMData, RawPCMData, RawPCMDataSize);

			FMemory::Free(RawPCMData);

			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::Done);

			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Filled PCM Buffer with size '%d'"), RawPCMDataSize);
		}

		// Filling in the standard RAW Wave 16-bit buffer if needed
		if (bFillRAWWaveBuffer)
		{
			int16* RawPCMData;
			int32 RawPCMDataSize;
			RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData.GetView().GetData()), DecodedAudioInfo.PCMInfo.PCMData.GetView().Num(), RawPCMData, RawPCMDataSize);

			FDecodedAudioStruct CustomDecodedAudioInfo;
			{
				CustomDecodedAudioInfo.SoundWaveBasicInfo = DecodedAudioInfo.SoundWaveBasicInfo;
				CustomDecodedAudioInfo.PCMInfo.PCMData = FBulkDataBuffer<uint8>(reinterpret_cast<uint8*>(RawPCMData), RawPCMDataSize);
				CustomDecodedAudioInfo.PCMInfo.PCMNumOfFrames = DecodedAudioInfo.PCMInfo.PCMNumOfFrames;
			}

			// Encoding to WAV format
			FEncodedAudioStruct EncodedAudioInfo;
			if (!WAVTranscoder::Encode(CustomDecodedAudioInfo, EncodedAudioInfo, FWAVEncodingFormat(EWAVEncodingFormat::FORMAT_PCM, 16)))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to encode PCM to WAV format due to transcoder error"));
				BroadcastResult(nullptr);
				return;
			}

			RegularSoundWaveRef->RawData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(RegularSoundWaveRef->RawData.Realloc(EncodedAudioInfo.AudioData.GetView().Num()), EncodedAudioInfo.AudioData.GetView().GetData(), EncodedAudioInfo.AudioData.GetView().Num());
			RegularSoundWaveRef->RawData.Unlock();

			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::Done);

			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Filled RAW Wave Buffer with size '%d'"), EncodedAudioInfo.AudioData.GetView().Num());
		}

		// Filling in the compressed OGG buffer
		if (bFillCompressedBuffer)
		{
			FEncodedAudioStruct EncodedAudioInfo;
			if (!VorbisTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo, Quality))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding Vorbis audio data"));
			}

			if (EncodedAudioInfo.AudioData.GetView().Num() <= 0)
			{
				BroadcastResult(nullptr);
				return;
			}

			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::NotStarted);
			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::InProgress);

			{
#if ENGINE_MAJOR_VERSION < 5

				static const FName NAME_OGG{TEXT("OGG")};

				// Getting the name of the current compressed platform-specific audio format
				const FName CurrentAudioPlatformSpecificFormat{GetPlatformSpecificFormat(NAME_OGG)};

				// Getting the compressed bulk data
				FByteBulkData* CompressedBulkDataPtr = &RegularSoundWaveRef->CompressedFormatData.GetFormat(CurrentAudioPlatformSpecificFormat);

#else
				
				FByteBulkData CompressedBulkData;
				FByteBulkData* CompressedBulkDataPtr = &CompressedBulkData;

#endif


				// Filling in the compressed data
				{
					CompressedBulkDataPtr->Lock(LOCK_READ_WRITE);
					FMemory::Memcpy(CompressedBulkDataPtr->Realloc(EncodedAudioInfo.AudioData.GetView().Num()), EncodedAudioInfo.AudioData.GetView().GetData(), EncodedAudioInfo.AudioData.GetView().Num());
					CompressedBulkDataPtr->Unlock();
				}

#if ENGINE_MAJOR_VERSION >= 5
				RegularSoundWaveRef->InitAudioResource(*CompressedBulkDataPtr);
#endif
			}

			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::Done);

			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Filled in the compressed audio buffer 'OGG' with size '%d'"), EncodedAudioInfo.AudioData.GetView().Num());
		}

		BroadcastResult(RegularSoundWaveRef);
	});
}

void URuntimeAudioCompressor::BroadcastResult(USoundWave* SoundWaveRef)
{
	AsyncTask(ENamedThreads::GameThread, [this, SoundWaveRef]()
	{
		bool bBroadcasted{false};
		const bool bSuccess{SoundWaveRef != nullptr};

		if (OnResultNative.IsBound())
		{
			bBroadcasted = true;
			OnResultNative.Broadcast(bSuccess, SoundWaveRef);
		}

		if (OnResult.IsBound())
		{
			bBroadcasted = true;
			OnResult.Broadcast(bSuccess, SoundWaveRef);
		}

		if (SoundWaveRef != nullptr)
		{
			SoundWaveRef->RemoveFromRoot();
		}

		if (!bBroadcasted)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("You did not bind to the delegate to get the result of the compress"));
		}
	});
}
