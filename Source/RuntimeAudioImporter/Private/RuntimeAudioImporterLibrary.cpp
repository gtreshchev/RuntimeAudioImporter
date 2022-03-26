// Georgy Treshchev 2022.

#include "RuntimeAudioImporterLibrary.h"

#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "PreImportedSoundAsset.h"

#include "AudioDevice.h"
#include "AudioCompressionSettingsUtils.h"
#include "Interfaces/IAudioFormat.h"

#include "Transcoders/MP3Transcoder.h"
#include "Transcoders/WAVTranscoder.h"
#include "Transcoders/FlacTranscoder.h"
#include "Transcoders/VorbisTranscoder.h"
#include "Transcoders/OpusTranscoder.h"
#include "Transcoders/RAWTranscoder.h"

#include "Misc/FileHelper.h"
#include "Async/Async.h"

URuntimeAudioImporterLibrary* URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter()
{
	URuntimeAudioImporterLibrary* Importer = NewObject<URuntimeAudioImporterLibrary>();
	Importer->AddToRoot();
	return Importer;
}

bool LoadAudioFileToArray(TArray<uint8>& AudioData, const FString& FilePath)
{
	/** Filling AudioBuffer with a binary file */
	if (!FFileHelper::LoadFileToArray(AudioData, *FilePath))
	{
		return false;
	}

	/** Removing unused two unitialized bytes */
	AudioData.RemoveAt(AudioData.Num() - 2, 2);

	return true;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, EAudioFormat Format)
{
	/** Checking if the file exists */
	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
		return;
	}

	/** Getting the audio format */
	Format = Format == EAudioFormat::Auto ? GetAudioFormat(FilePath) : Format;
	Format = Format == EAudioFormat::Invalid ? EAudioFormat::Auto : Format;

	TArray<uint8> AudioBuffer;

	/** Filling AudioBuffer with a binary file */
	if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
		return;
	}

	ImportAudioFromBuffer(MoveTemp(AudioBuffer), Format);
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat Format, int32 SampleRate, int32 NumOfChannels)
{
	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
		return;
	}

	OnProgress_Internal(5);

	TArray<uint8> AudioBuffer;
	if (!LoadAudioFileToArray(AudioBuffer, *FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
		return;
	}

	OnProgress_Internal(35);

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, AudioBuffer = MoveTemp(AudioBuffer), Format, SampleRate, NumOfChannels]()
	{
		ImportAudioFromRAWBuffer(AudioBuffer, Format, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format, int32 SampleRate, int32 NumOfChannels)
{
	uint8* RAWData{RAWBuffer.GetData()};
	const int32 RAWDataSize{RAWBuffer.Num()};

	float* PCMData{nullptr};
	int32 PCMDataSize{0};

	/** Transcoding RAW data to 32-bit float Data */
	{
		switch (Format)
		{
		case ERAWAudioFormat::Int16:
			{
				RAWTranscoder::TranscodeRAWData<int16, float>(reinterpret_cast<int16*>(RAWData), RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::Int32:
			{
				RAWTranscoder::TranscodeRAWData<int32, float>(reinterpret_cast<int32*>(RAWData), RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::UInt8:
			{
				RAWTranscoder::TranscodeRAWData<uint8, float>(RAWData, RAWDataSize, PCMData, PCMDataSize);
				break;
			}
		case ERAWAudioFormat::Float32:
			{
				PCMDataSize = RAWDataSize;
				PCMData = static_cast<float*>(FMemory::Malloc(PCMDataSize));
				FMemory::Memcpy(PCMData, RAWData, RAWDataSize);
				break;
			}
		}
	}

	if (!PCMData || PCMDataSize < 0)
	{
		OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
		return;
	}

	ImportAudioFromFloat32Buffer(reinterpret_cast<uint8*>(PCMData), PCMDataSize, SampleRate, NumOfChannels);
}

void URuntimeAudioImporterLibrary::CompressSoundWave(UImportedSoundWave* ImportedSoundWaveRef, FOnSoundWaveCompressedResult OnCompressedResult, uint8 Quality, bool bFillCompressedBuffer, bool bFillPCMBuffer, bool bFillRAWWaveBuffer)
{
	USoundWave* RegularSoundWaveRef = NewObject<USoundWave>(USoundWave::StaticClass());

	if (RegularSoundWaveRef == nullptr)
	{
		OnCompressedResult.Execute(false, nullptr);
		return;
	}

	/** Filling Decoded Audio Info */
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

	/** Filling the basic information of the sound wave */
	{
		RegularSoundWaveRef->Duration = DecodedAudioInfo.SoundWaveBasicInfo.Duration;
		RegularSoundWaveRef->SetSampleRate(DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
		RegularSoundWaveRef->NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
		RegularSoundWaveRef->SoundGroup = SOUNDGROUP_Default;

		if (RegularSoundWaveRef->NumChannels >= 4)
		{
			RegularSoundWaveRef->bIsAmbisonics = 1;
		}

		RegularSoundWaveRef->bProcedural = false;
		RegularSoundWaveRef->DecompressionType = EDecompressionType::DTYPE_RealTime;
	}

	RegularSoundWaveRef->AddToRoot();

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [RegularSoundWaveRef, Quality, bFillPCMBuffer, bFillRAWWaveBuffer, bFillCompressedBuffer, DecodedAudioInfo, OnCompressedResult]()
	{
		auto OnResultExecute = [OnCompressedResult](bool bSuccess, USoundWave* SoundWaveRef)
		{
			AsyncTask(ENamedThreads::GameThread, [bSuccess, OnCompressedResult, SoundWaveRef]()
			{
				OnCompressedResult.Execute(bSuccess, SoundWaveRef);

				if (SoundWaveRef != nullptr)
				{
					SoundWaveRef->RemoveFromRoot();
				}
			});
		};

		/** Fill in the standard PCM buffer if needed */
		if (bFillPCMBuffer)
		{
			int16* RawPCMData;
			int32 RawPCMDataSize;
			RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData), DecodedAudioInfo.PCMInfo.PCMDataSize, RawPCMData, RawPCMDataSize);
			RegularSoundWaveRef->RawPCMDataSize = RawPCMDataSize;
			RegularSoundWaveRef->RawPCMData = static_cast<uint8*>(FMemory::Malloc(RawPCMDataSize));
			FMemory::Memmove(RegularSoundWaveRef->RawPCMData, RawPCMData, RawPCMDataSize);

			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Filled PCM Buffer with size '%d'"), RawPCMDataSize);
		}

		/** Fill in the standard RAW Wave 16-bit buffer if needed */
		if (bFillRAWWaveBuffer)
		{
			int16* RawPCMData;
			int32 RawPCMDataSize;
			RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData), DecodedAudioInfo.PCMInfo.PCMDataSize, RawPCMData, RawPCMDataSize);

			FDecodedAudioStruct CustomDecodedAudioInfo = DecodedAudioInfo;
			{
				CustomDecodedAudioInfo.PCMInfo.PCMData = reinterpret_cast<uint8*>(RawPCMData);
				CustomDecodedAudioInfo.PCMInfo.PCMDataSize = RawPCMDataSize;
			}

			/** Encoding to WAV format */
			FEncodedAudioStruct EncodedAudioInfo;
			if (!WAVTranscoder::Encode(CustomDecodedAudioInfo, EncodedAudioInfo, FWAVEncodingFormat(EWAVEncodingFormat::FORMAT_PCM, 16)))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to encode PCM to WAV format due to transcoder error"));
				OnResultExecute(false, nullptr);
				return;
			}

			RegularSoundWaveRef->RawData.Lock(LOCK_READ_WRITE);
			FMemory::Memmove(RegularSoundWaveRef->RawData.Realloc(EncodedAudioInfo.AudioDataSize), EncodedAudioInfo.AudioData, EncodedAudioInfo.AudioDataSize);
			RegularSoundWaveRef->RawData.Unlock();

			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Filled RAW Wave Buffer with size '%d'"), EncodedAudioInfo.AudioDataSize);
		}

		if (bFillCompressedBuffer)
		{
			FAudioDeviceManager* DeviceManager{GEngine->GetAudioDeviceManager()};

			if (DeviceManager == nullptr)
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to get audio device manager"));
				OnResultExecute(false, nullptr);
				return;
			}

			/** Getting the name of the current compressed audio format */
			const FName CurrentAudioFormat{DeviceManager->GetActiveAudioDevice()->GetRuntimeFormat(RegularSoundWaveRef)};

			const FName CurrentAudioPlatformSpecificFormat{GetPlatformSpecificFormat(CurrentAudioFormat)};

			FEncodedAudioStruct EncodedAudioInfo;

			static const FName NAME_ADPCM{TEXT("ADPCM")};
			static const FName NAME_OGG{TEXT("OGG")};
			static const FName NAME_OPUS{TEXT("OPUS")};

			if (CurrentAudioFormat == NAME_ADPCM)
			{
				int16* Int16PCMData;
				int32 Int16PCMDataSize;
				RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData), DecodedAudioInfo.PCMInfo.PCMDataSize, Int16PCMData, Int16PCMDataSize);

				EncodedAudioInfo.AudioData = reinterpret_cast<uint8*>(Int16PCMData);
				EncodedAudioInfo.AudioDataSize = Int16PCMDataSize;
			}
			else if (CurrentAudioFormat == NAME_OGG)
			{
				if (!VorbisTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo, Quality))
				{
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding Vorbis audio data"));
				}
			}
			else if (CurrentAudioFormat == NAME_OPUS)
			{
				if (!OpusTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo, Quality))
				{
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while encoding Vorbis audio data"));
				}
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unknown encoding format"));
			}

			if (EncodedAudioInfo.AudioDataSize <= 0)
			{
				OnResultExecute(false, nullptr);
				return;
			}

			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::NotStarted);
			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::InProgress);

			/** Getting the compressed bulk data */
			FByteBulkData* CompressedBulkData = &RegularSoundWaveRef->CompressedFormatData.GetFormat(CurrentAudioPlatformSpecificFormat);

			/** Filling the compressed data */
			{
				CompressedBulkData->Lock(LOCK_READ_WRITE);
				FMemory::Memmove(CompressedBulkData->Realloc(EncodedAudioInfo.AudioDataSize), EncodedAudioInfo.AudioData, EncodedAudioInfo.AudioDataSize);
				CompressedBulkData->Unlock();
			}

			RegularSoundWaveRef->SetPrecacheState(ESoundWavePrecacheState::Done);

			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Filled compressed audio buffer '%s' ('%s') with size '%d'"), *CurrentAudioFormat.ToString(), *CurrentAudioPlatformSpecificFormat.ToString(), EncodedAudioInfo.AudioDataSize);
		}

		OnResultExecute(true, RegularSoundWaveRef);
	});
}

FName URuntimeAudioImporterLibrary::GetPlatformSpecificFormat(const FName& Format)
{
	const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides();

	/** Platforms that require compression overrides get concatenated formats */

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

	/** Cache the concatenated hash */
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

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef)
{
	ImportAudioFromBuffer(MoveTemp(PreImportedSoundAssetRef->AudioDataArray), EAudioFormat::Mp3);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8> AudioData, EAudioFormat AudioFormat)
{
	if (AudioFormat == EAudioFormat::Wav && !WAVTranscoder::CheckAndFixWavDurationErrors(AudioData)) return;

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, AudioData = MoveTemp(AudioData), AudioFormat]()
	{
		OnProgress_Internal(5);

		if (AudioFormat == EAudioFormat::Invalid)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for import"));
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return;
		}

		const FEncodedAudioStruct EncodedAudioInfo(const_cast<uint8*>(AudioData.GetData()), AudioData.Num(), AudioFormat);

		FDecodedAudioStruct DecodedAudioInfo;
		if (!DecodeAudioData(EncodedAudioInfo, DecodedAudioInfo))
		{
			return;
		}

		ImportAudioFromDecodedInfo(DecodedAudioInfo);
	});
}

void URuntimeAudioImporterLibrary::TranscodeRAWDataFromBuffer(TArray<uint8> RAWData_From, ERAWAudioFormat RAWFrom, TArray<uint8>& RAWData_To, ERAWAudioFormat RAWTo)
{
	TArray<uint8> IntermediateRAWBuffer;

	/** Transcoding of all formats to unsigned 8-bit PCM format (intermediate) */
	switch (RAWFrom)
	{
	case ERAWAudioFormat::Int16:
		{
			RAWTranscoder::TranscodeRAWData<int16, uint8>(RAWData_From, IntermediateRAWBuffer);
			break;
		}
	case ERAWAudioFormat::Int32:
		{
			RAWTranscoder::TranscodeRAWData<int32, uint8>(RAWData_From, IntermediateRAWBuffer);
			break;
		}
	case ERAWAudioFormat::UInt8:
		{
			IntermediateRAWBuffer = RAWData_From;
			break;
		}
	case ERAWAudioFormat::Float32:
		{
			RAWTranscoder::TranscodeRAWData<float, uint8>(RAWData_From, IntermediateRAWBuffer);
			break;
		}
	}

	RAWData_From.Empty();

	/** Transcoding unsigned 8-bit PCM to the specified format */
	switch (RAWTo)
	{
	case ERAWAudioFormat::Int16:
		{
			RAWTranscoder::TranscodeRAWData<uint8, int16>(IntermediateRAWBuffer, RAWData_To);
			break;
		}
	case ERAWAudioFormat::Int32:
		{
			RAWTranscoder::TranscodeRAWData<uint8, int32>(IntermediateRAWBuffer, RAWData_To);
			break;
		}
	case ERAWAudioFormat::UInt8:
		{
			RAWData_To = IntermediateRAWBuffer;
			break;
		}
	case ERAWAudioFormat::Float32:
		{
			RAWTranscoder::TranscodeRAWData<uint8, float>(IntermediateRAWBuffer, RAWData_To);
			break;
		}
	}
}

bool URuntimeAudioImporterLibrary::TranscodeRAWDataFromFile(const FString& FilePathFrom, ERAWAudioFormat FormatFrom, const FString& FilePathTo, ERAWAudioFormat FormatTo)
{
	TArray<uint8> RAWBufferFrom;

	/** Loading a file into a byte array */
	if (!LoadAudioFileToArray(RAWBufferFrom, *FilePathFrom))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when reading RAW data on the path '%s'"), *FilePathFrom);
		return false;
	}

	TArray<uint8> RAWBufferTo;
	TranscodeRAWDataFromBuffer(RAWBufferFrom, FormatFrom, RAWBufferTo, FormatTo);

	/** Writing a file to a specified location */
	if (!FFileHelper::SaveArrayToFile(MoveTemp(RAWBufferTo), *FilePathTo))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving RAW data to the path '%s'"), *FilePathTo);
		return false;
	}

	return true;
}

bool URuntimeAudioImporterLibrary::ExportSoundWaveToWAV(UImportedSoundWave* SoundWaveRef, const FString& SavePath)
{
	/** Filling Decoded Audio Info */
	FDecodedAudioStruct DecodedAudioInfo;
	{
		DecodedAudioInfo.PCMInfo = SoundWaveRef->PCMBufferInfo;
		FSoundWaveBasicStruct SoundWaveBasicInfo;
		{
			SoundWaveBasicInfo.NumOfChannels = SoundWaveRef->NumChannels;
			SoundWaveBasicInfo.SampleRate = SoundWaveRef->SamplingRate;
			SoundWaveBasicInfo.Duration = SoundWaveRef->Duration;
		}
		DecodedAudioInfo.SoundWaveBasicInfo = SoundWaveBasicInfo;
	}

	/** Encoding to WAV format */
	FEncodedAudioStruct EncodedAudioInfo;
	if (!WAVTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo, FWAVEncodingFormat(EWAVEncodingFormat::FORMAT_IEEE_FLOAT, 32)))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to encode PCM to WAV format due to transcoder error"));
		return false;
	}

	TArray<uint8> EncodedArray(EncodedAudioInfo.AudioData, EncodedAudioInfo.AudioDataSize);

	/** Writing a file to a specified location */
	if (!FFileHelper::SaveArrayToFile(MoveTemp(EncodedArray), *SavePath))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong when saving WAV data to the path '%s'"), *SavePath);
		return false;
	}

	return true;
}

void URuntimeAudioImporterLibrary::ImportAudioFromDecodedInfo(const FDecodedAudioStruct& DecodedAudioInfo)
{
	AsyncTask(ENamedThreads::GameThread, [this, DecodedAudioInfo]()
	{
		UImportedSoundWave* SoundWaveRef = NewObject<UImportedSoundWave>(UImportedSoundWave::StaticClass());
		if (SoundWaveRef == nullptr)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while creating the imported sound wave"));
			OnResult_Internal(nullptr, ETranscodingStatus::SoundWaveDeclarationError);
			return nullptr;
		}

		if (DefineSoundWave(SoundWaveRef, DecodedAudioInfo))
		{
			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("The audio data was successfully imported. Information about imported data:\n%s"), *DecodedAudioInfo.ToString());
			OnProgress_Internal(100);
			OnResult_Internal(SoundWaveRef, ETranscodingStatus::SuccessfulImport);
		}

		return nullptr;
	});
}

bool URuntimeAudioImporterLibrary::DefineSoundWave(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo)
{
	OnProgress_Internal(70);

	/** Fill SoundWave basic information (e.g. duration, number of channels, etc) */
	FillSoundWaveBasicInfo(SoundWaveRef, DecodedAudioInfo);

	OnProgress_Internal(75);

	/** Fill PCM data buffer */
	FillPCMData(SoundWaveRef, DecodedAudioInfo);

	OnProgress_Internal(95);

	return true;
}

void URuntimeAudioImporterLibrary::FillSoundWaveBasicInfo(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo)
{
	SoundWaveRef->Duration = DecodedAudioInfo.SoundWaveBasicInfo.Duration;
	SoundWaveRef->SetSampleRate(DecodedAudioInfo.SoundWaveBasicInfo.SampleRate);
	SoundWaveRef->SamplingRate = DecodedAudioInfo.SoundWaveBasicInfo.SampleRate;
	SoundWaveRef->NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels;
	SoundWaveRef->SoundGroup = SOUNDGROUP_Default;

	if (SoundWaveRef->NumChannels >= 4)
	{
		SoundWaveRef->bIsAmbisonics = 1;
	}

	SoundWaveRef->bProcedural = true;
	SoundWaveRef->DecompressionType = EDecompressionType::DTYPE_Procedural;
}

void URuntimeAudioImporterLibrary::FillPCMData(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo)
{
	SoundWaveRef->RawPCMDataSize = DecodedAudioInfo.PCMInfo.PCMDataSize;
	SoundWaveRef->PCMBufferInfo = DecodedAudioInfo.PCMInfo;
}

bool URuntimeAudioImporterLibrary::DecodeAudioData(FEncodedAudioStruct EncodedAudioInfo, FDecodedAudioStruct& DecodedAudioInfo)
{
	OnProgress_Internal(10);

	if (EncodedAudioInfo.AudioFormat == EAudioFormat::Auto)
	{
		EncodedAudioInfo.AudioFormat = GetAudioFormat(EncodedAudioInfo.AudioData, EncodedAudioInfo.AudioDataSize);
	}

	switch (EncodedAudioInfo.AudioFormat)
	{
	case EAudioFormat::Mp3:
		{
			if (!MP3Transcoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Mp3 audio data"));
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			break;
		}
	case EAudioFormat::Wav:
		{
			if (!WAVTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Wav audio data"));
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			break;
		}
	case EAudioFormat::Flac:
		{
			if (!FlacTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Flac audio data"));
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			break;
		}
	case EAudioFormat::OggVorbis:
		{
			if (!VorbisTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Something went wrong while decoding Vorbis audio data"));
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			break;
		}
	default:
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for decode"));
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return false;
		}
	}

	OnProgress_Internal(65);

	return true;
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const FString& FilePath)
{
	const FString& Extension{FPaths::GetExtension(FilePath, false).ToLower()};

	if (Extension == TEXT("mp3"))
	{
		return EAudioFormat::Mp3;
	}

	if (Extension == TEXT("wav") || Extension == TEXT("wave"))
	{
		return EAudioFormat::Wav;
	}

	if (Extension == TEXT("flac"))
	{
		return EAudioFormat::Flac;
	}

	if (Extension == TEXT("ogg") || Extension == TEXT("oga") || Extension == TEXT("sb0"))
	{
		return EAudioFormat::OggVorbis;
	}

	UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to determine audio file format with path '%s' by name"), *FilePath);

	return EAudioFormat::Invalid;
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormatAdvanced(const TArray<uint8>& AudioData)
{
	return GetAudioFormat(AudioData.GetData(), AudioData.Num());
}

EAudioFormat URuntimeAudioImporterLibrary::GetAudioFormat(const uint8* AudioData, int32 AudioDataSize)
{
	if (MP3Transcoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::Mp3;
	}

	if (WAVTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::Wav;
	}

	if (FlacTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::Flac;
	}

	if (VorbisTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
	{
		return EAudioFormat::OggVorbis;
	}

	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to determine audio data format"));

	return EAudioFormat::Invalid;
}

void URuntimeAudioImporterLibrary::ImportAudioFromFloat32Buffer(uint8* PCMData, const int32 PCMDataSize, const int32 SampleRate, const int32 NumOfChannels)
{
	FDecodedAudioStruct DecodedAudioInfo;

	/** Filling all the required information */
	{
		DecodedAudioInfo.PCMInfo.PCMData = PCMData;
		DecodedAudioInfo.PCMInfo.PCMDataSize = PCMDataSize;
		DecodedAudioInfo.PCMInfo.PCMNumOfFrames = PCMDataSize / sizeof(float) / NumOfChannels;

		DecodedAudioInfo.SoundWaveBasicInfo.NumOfChannels = NumOfChannels;
		DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = SampleRate;
		DecodedAudioInfo.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SampleRate;
	}

	OnProgress_Internal(50);

	/** Finalizing import */
	ImportAudioFromDecodedInfo(DecodedAudioInfo);
}

FString URuntimeAudioImporterLibrary::ConvertSecondsToString(int32 Seconds)
{
	FString FinalString;

	const int32 NewHours = Seconds / 3600;
	if (NewHours > 0)
	{
		FinalString += ((NewHours < 10) ? TEXT("0") + FString::FromInt(NewHours) : FString::FromInt(NewHours)) + TEXT(":");
	}

	Seconds = Seconds % 3600;

	const int32 NewMinutes = Seconds / 60;
	FinalString += ((NewMinutes < 10) ? TEXT("0") + FString::FromInt(NewMinutes) : FString::FromInt(NewMinutes)) + TEXT(":");

	const int32 NewSeconds = Seconds % 60;
	FinalString += (NewSeconds < 10) ? TEXT("0") + FString::FromInt(NewSeconds) : FString::FromInt(NewSeconds);

	return FinalString;
}

void URuntimeAudioImporterLibrary::OnProgress_Internal(int32 Percentage)
{
	AsyncTask(ENamedThreads::GameThread, [this, Percentage]()
	{
		if (OnProgress.IsBound())
		{
			OnProgress.Broadcast(Percentage);
		}
	});
}

void URuntimeAudioImporterLibrary::OnResult_Internal(UImportedSoundWave* SoundWaveRef, ETranscodingStatus Status)
{
	AsyncTask(ENamedThreads::GameThread, [this, SoundWaveRef, Status]()
	{
		if (OnResult.IsBound())
		{
			OnResult.Broadcast(this, SoundWaveRef, Status);
		}
		else
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("You did not bind to the delegate to get the result of the import"));
		}
		RemoveFromRoot();
	});
}
