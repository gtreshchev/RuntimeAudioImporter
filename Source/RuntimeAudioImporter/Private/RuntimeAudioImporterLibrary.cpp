// Georgy Treshchev 2022.

#include "RuntimeAudioImporterLibrary.h"

#include "RuntimeAudioImporterDefines.h"
#include "RuntimeAudioImporterTypes.h"
#include "PreImportedSoundAsset.h"

#include "Transcoders/FLACTranscoder.h"
#include "Transcoders/MP3Transcoder.h"
#include "Transcoders/VorbisTranscoder.h"
#include "Transcoders/RAWTranscoder.h"
#include "Transcoders/WAVTranscoder.h"

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

	ImportAudioFromBuffer(AudioBuffer, Format);
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWFile(const FString& FilePath, ERAWAudioFormat Format, const int32 SampleRate, const int32 NumOfChannels)
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

	AsyncTask(ENamedThreads::AnyThread, [this, AudioBuffer, Format, SampleRate, NumOfChannels]()
	{
		ImportAudioFromRAWBuffer(AudioBuffer, Format, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format, const int32 SampleRate, const int32 NumOfChannels)
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

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef)
{
	ImportAudioFromBuffer(PreImportedSoundAssetRef->AudioDataArray, EAudioFormat::Mp3);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8>& AudioData, EAudioFormat AudioFormat)
{
	if (AudioFormat == EAudioFormat::Wav && !WAVTranscoder::CheckAndFixWavDurationErrors(AudioData)) return;

	AsyncTask(ENamedThreads::AnyThread, [&, AudioData, AudioFormat]()
	{
		ImportAudioFromBuffer_Internal(AudioData, AudioFormat);
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
	if (!FFileHelper::SaveArrayToFile(RAWBufferTo, *FilePathTo))
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
			SoundWaveBasicInfo.ChannelsNum = SoundWaveRef->NumChannels;
			SoundWaveBasicInfo.SampleRate = SoundWaveRef->SamplingRate;
			SoundWaveBasicInfo.Duration = SoundWaveRef->Duration;
		}
		DecodedAudioInfo.SoundWaveBasicInfo = SoundWaveBasicInfo;
	}

	/** Encoding to WAV format */
	FEncodedAudioStruct EncodedAudioInfo;
	if (!WAVTranscoder::Encode(DecodedAudioInfo, EncodedAudioInfo))
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

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioData, EAudioFormat Format)
{
	OnProgress_Internal(5);

	if (Format == EAudioFormat::Invalid)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Undefined audio data format for import"));
		OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
		return;
	}

	FEncodedAudioStruct EncodedAudioInfo(const_cast<uint8*>(AudioData.GetData()), AudioData.Num(), Format);

	FDecodedAudioStruct DecodedAudioInfo;
	if (!DecodeAudioData(MoveTemp(EncodedAudioInfo), DecodedAudioInfo))
	{
		return;
	}

	CreateSoundWaveAndFinishImport(DecodedAudioInfo);
}

void URuntimeAudioImporterLibrary::CreateSoundWaveAndFinishImport(const FDecodedAudioStruct& DecodedAudioInfo)
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
	SoundWaveRef->NumChannels = DecodedAudioInfo.SoundWaveBasicInfo.ChannelsNum;
	SoundWaveRef->SoundGroup = SOUNDGROUP_Default;
	if (SoundWaveRef->NumChannels >= 4)
	{
		SoundWaveRef->bIsAmbisonics = 1;
	}
	SoundWaveRef->bProcedural = true;
	SoundWaveRef->bLooping = false;
}

void URuntimeAudioImporterLibrary::FillPCMData(UImportedSoundWave* SoundWaveRef, const FDecodedAudioStruct& DecodedAudioInfo)
{
	/*int16* RawPCMData;
	int32 RawPCMDataSize;
	RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData), DecodedAudioInfo.PCMInfo.PCMDataSize, RawPCMData, RawPCMDataSize);
	SoundWaveRef->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWaveRef->RawPCMDataSize));
	FMemory::Memmove(SoundWaveRef->RawPCMData, RawPCMData, RawPCMDataSize);*/
	// We do not need to fill a standard PCM buffer since we have a custom sound wave with custom buffer. But if you want to fill the standard PCM buffer, just uncomment the code above

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
			if (!FLACTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
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

	if (FLACTranscoder::CheckAudioFormat(AudioData, AudioDataSize))
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

		DecodedAudioInfo.SoundWaveBasicInfo.ChannelsNum = NumOfChannels;
		DecodedAudioInfo.SoundWaveBasicInfo.SampleRate = SampleRate;
		DecodedAudioInfo.SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SampleRate;
	}

	OnProgress_Internal(50);

	/** Finalizing import */
	CreateSoundWaveAndFinishImport(DecodedAudioInfo);
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
