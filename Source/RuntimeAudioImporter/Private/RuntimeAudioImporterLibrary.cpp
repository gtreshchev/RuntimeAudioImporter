// Georgy Treshchev 2022.

#include "RuntimeAudioImporterLibrary.h"

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

void URuntimeAudioImporterLibrary::ImportAudioFromFile(const FString& FilePath, EAudioFormat Format)
{
	/** Checking if the file exists */
	if (!FPaths::FileExists(FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::AudioDoesNotExist);
		return;
	}

	/** Getting the audio format */
	Format = Format != EAudioFormat::Auto ? Format : GetAudioFormat(FilePath);

	TArray<uint8> AudioBuffer;

	/** Filling AudioBuffer with a binary file */
	if (!FFileHelper::LoadFileToArray(AudioBuffer, *FilePath))
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
	if (!FFileHelper::LoadFileToArray(AudioBuffer, *FilePath))
	{
		OnResult_Internal(nullptr, ETranscodingStatus::LoadFileToArrayError);
		return;
	}
	
	OnProgress_Internal(35);

	AsyncTask(ENamedThreads::AnyThread, [&, AudioBuffer, Format, SampleRate, NumOfChannels]()
	{
		ImportAudioFromRAWBuffer(AudioBuffer, Format, SampleRate, NumOfChannels);
	});
}

void URuntimeAudioImporterLibrary::ImportAudioFromRAWBuffer(TArray<uint8> RAWBuffer, ERAWAudioFormat Format, const int32 SampleRate, const int32 NumOfChannels)
{
	uint8* RAWData{RAWBuffer.GetData()};
	const uint32 RAWDataSize{static_cast<const uint32>(RAWBuffer.Num() - 2)};

	float* PCMData{nullptr};
	uint32 PCMDataSize{0};

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

void URuntimeAudioImporterLibrary::ImportAudioFromFloat32Buffer(uint8* PCMData, const uint32 PCMDataSize, const int32 SampleRate, const int32 NumOfChannels)
{
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
	CreateSoundWaveAndFinishImport();
}

void URuntimeAudioImporterLibrary::ImportAudioFromPreImportedSound(UPreImportedSoundAsset* PreImportedSoundAssetRef)
{
	ImportAudioFromBuffer(PreImportedSoundAssetRef->AudioDataArray, EAudioFormat::Mp3);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_BP(TArray<uint8> AudioDataBuffer, EAudioFormat Format)
{
	ImportAudioFromBuffer(AudioDataBuffer, Format);
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer(TArray<uint8>& AudioDataBuffer, EAudioFormat Format)
{
	if (Format == EAudioFormat::Wav) if (!WAVTranscoder::CheckAndFixWavDurationErrors(AudioDataBuffer)) return;

	DecodedAudioInfo = FDecodedAudioStruct();
	AsyncTask(ENamedThreads::AnyThread, [&, AudioDataBuffer, Format]()
	{
		ImportAudioFromBuffer_Internal(AudioDataBuffer, Format);
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
	/** Loading a file into a byte array */
	TArray<uint8> RAWBufferFrom;
	if (!FFileHelper::LoadFileToArray(RAWBufferFrom, *FilePathFrom))
	{
		return false;
	}

	TArray<uint8> RAWBufferTo;
	TranscodeRAWDataFromBuffer(RAWBufferFrom, FormatFrom, RAWBufferTo, FormatTo);
	
	RAWBufferFrom.Empty();

	/** Writing a file to a specified location */
	if (!FFileHelper::SaveArrayToFile(RAWBufferTo, *FilePathTo))
	{
		RAWBufferTo.Empty();
		return false;
	}
	
	RAWBufferTo.Empty();

	return true;
}

void URuntimeAudioImporterLibrary::ImportAudioFromBuffer_Internal(const TArray<uint8>& AudioDataBuffer, const EAudioFormat& Format)
{
	OnProgress_Internal(5);

	if (Format == EAudioFormat::Auto || Format == EAudioFormat::Invalid)
	{
		OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
		return;
	}

	const FEncodedAudioStruct& EncodedAudioInfo{FEncodedAudioStruct(const_cast<uint8*>(AudioDataBuffer.GetData()), AudioDataBuffer.Num() - 2, Format)};

	/** Transcoding the imported Audio Data to PCM Data */
	if (!DecodeAudioData(EncodedAudioInfo)) return;

	CreateSoundWaveAndFinishImport();
}

void URuntimeAudioImporterLibrary::CreateSoundWaveAndFinishImport()
{
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UImportedSoundWave* SoundWaveRef = NewObject<UImportedSoundWave>(UImportedSoundWave::StaticClass());
		if (!SoundWaveRef)
		{
			OnResult_Internal(nullptr, ETranscodingStatus::SoundWaveDeclarationError);
			return nullptr;
		}
		AsyncTask(ENamedThreads::AnyThread, [=]()
		{
			if (DefineSoundWave(SoundWaveRef))
			{
				OnProgress_Internal(100);
				
				OnResult_Internal(SoundWaveRef, ETranscodingStatus::SuccessfulImport);
			}
		});
		return nullptr;
	});
}

bool URuntimeAudioImporterLibrary::DefineSoundWave(UImportedSoundWave* SoundWaveRef)
{
	OnProgress_Internal(70);

	/** Fill SoundWave basic information (e.g. duration, number of channels, etc) */
	FillSoundWaveBasicInfo(SoundWaveRef);
	
	OnProgress_Internal(75);

	/** Fill PCM data buffer */
	FillPCMData(SoundWaveRef);
	
	OnProgress_Internal(95);

	return true;
}

void URuntimeAudioImporterLibrary::FillSoundWaveBasicInfo(UImportedSoundWave* SoundWaveRef) const
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

void URuntimeAudioImporterLibrary::FillPCMData(UImportedSoundWave* SoundWaveRef) const
{
	/*int16* RawPCMData;
	uint32 RawPCMDataSize;
	RAWTranscoder::TranscodeRAWData<float, int16>(reinterpret_cast<float*>(DecodedAudioInfo.PCMInfo.PCMData), DecodedAudioInfo.PCMInfo.PCMDataSize, RawPCMData, RawPCMDataSize);
	SoundWaveRef->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWaveRef->RawPCMDataSize));
	FMemory::Memmove(SoundWaveRef->RawPCMData, RawPCMData, RawPCMDataSize);*/
	// We do not need to fill a standard PCM buffer since we have a custom sound wave with custom buffer. But if you want to fill the standard PCM buffer, just uncomment the code above

	SoundWaveRef->RawPCMDataSize = DecodedAudioInfo.PCMInfo.PCMDataSize;
	SoundWaveRef->PCMBufferInfo = DecodedAudioInfo.PCMInfo;
}


bool URuntimeAudioImporterLibrary::DecodeAudioData(const FEncodedAudioStruct& EncodedAudioInfo)
{
	OnProgress_Internal(10);
	switch (EncodedAudioInfo.AudioFormat)
	{
	case EAudioFormat::Mp3:
		{
			if (!MP3Transcoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			return true;
		}
	case EAudioFormat::Wav:
		{
			if (!WAVTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			return true;
		}
	case EAudioFormat::Flac:
		{
			if (!FLACTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			return true;
		}
	case EAudioFormat::OggVorbis:
		{
			if (!VorbisTranscoder::Decode(EncodedAudioInfo, DecodedAudioInfo))
			{
				OnResult_Internal(nullptr, ETranscodingStatus::FailedToReadAudioDataArray);
				return false;
			}
			return true;
		}
	default:
		{
			OnResult_Internal(nullptr, ETranscodingStatus::InvalidAudioFormat);
			return false;
		}
	}
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

	return EAudioFormat::Invalid;
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
		RemoveFromRoot();
	});
}
