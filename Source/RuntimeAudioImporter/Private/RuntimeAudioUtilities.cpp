// Georgy Treshchev 2024.


#include "RuntimeAudioUtilities.h"

#include "Codecs/BaseRuntimeCodec.h"
#include "Codecs/RuntimeCodecFactory.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"

TArray<ERuntimeAudioFormat> URuntimeAudioUtilities::GetAudioFormats(const FString& FilePath)
{
	FRuntimeCodecFactory CodecFactory;
	TArray<ERuntimeAudioFormat> AudioFormats;

	for (FBaseRuntimeCodec* Codec : CodecFactory.GetCodecs(FilePath))
	{
		AudioFormats.Add(Codec->GetAudioFormat());
	}

	return AudioFormats;
}

TArray<ERuntimeAudioFormat> URuntimeAudioUtilities::GetAudioFormatsAdvanced(const TArray<uint8>& AudioData)
{
	return GetAudioFormatsAdvanced(FRuntimeBulkDataBuffer<uint8>(AudioData));
}

void URuntimeAudioUtilities::GetAudioHeaderInfoFromFile(const FString& FilePath, const FOnGetAudioHeaderInfoResult& Result)
{
	GetAudioHeaderInfoFromFile(FilePath, FOnGetAudioHeaderInfoResultNative::CreateLambda([Result](bool bSucceeded, FRuntimeAudioHeaderInfo HeaderInfo)
	{
		Result.ExecuteIfBound(bSucceeded, HeaderInfo);
	}));
}

void URuntimeAudioUtilities::GetAudioHeaderInfoFromFile(const FString& FilePath, const FOnGetAudioHeaderInfoResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [FilePath, Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, FRuntimeAudioHeaderInfo&& HeaderInfo)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, HeaderInfo = MoveTemp(HeaderInfo)]() mutable
			{
				Result.ExecuteIfBound(bSucceeded, MoveTemp(HeaderInfo));
			});
		};

		TArray64<uint8> AudioBuffer;
		if (!RuntimeAudioImporter::LoadAudioFileToArray(AudioBuffer, *FilePath))
		{
			ExecuteResult(false, FRuntimeAudioHeaderInfo());
			return;
		}

		{
			FRuntimeCodecFactory CodecFactory;
			TArray<FBaseRuntimeCodec*> RuntimeCodecs = CodecFactory.GetCodecs(FilePath);

			FRuntimeBulkDataBuffer<uint8> BulkAudioData = FRuntimeBulkDataBuffer<uint8>(AudioBuffer);

			FEncodedAudioStruct EncodedData;
			EncodedData.AudioData = MoveTemp(BulkAudioData);
			for (FBaseRuntimeCodec* RuntimeCodec : RuntimeCodecs)
			{
				EncodedData.AudioFormat = RuntimeCodec->GetAudioFormat();

				FRuntimeAudioHeaderInfo HeaderInfo;
				if (!RuntimeCodec->GetHeaderInfo(MoveTemp(EncodedData), HeaderInfo))
				{
					continue;
				}

				ExecuteResult(true, MoveTemp(HeaderInfo));
				return;
			}
		}

		GetAudioHeaderInfoFromBuffer(MoveTemp(AudioBuffer), FOnGetAudioHeaderInfoResultNative::CreateLambda([Result](bool bSucceeded, FRuntimeAudioHeaderInfo HeaderInfo) mutable
		{
			Result.ExecuteIfBound(bSucceeded, MoveTemp(HeaderInfo));
		}));
	});
}

void URuntimeAudioUtilities::GetAudioHeaderInfoFromBuffer(TArray<uint8> AudioData, const FOnGetAudioHeaderInfoResult& Result)
{
	GetAudioHeaderInfoFromBuffer(TArray64<uint8>(MoveTemp(AudioData)), FOnGetAudioHeaderInfoResultNative::CreateLambda([Result](bool bSucceeded, const FRuntimeAudioHeaderInfo& HeaderInfo)
	{
		Result.ExecuteIfBound(bSucceeded, HeaderInfo);
	}));
}

void URuntimeAudioUtilities::GetAudioHeaderInfoFromBuffer(TArray64<uint8> AudioData, const FOnGetAudioHeaderInfoResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [AudioData = MoveTemp(AudioData), Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, FRuntimeAudioHeaderInfo&& HeaderInfo)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, HeaderInfo = MoveTemp(HeaderInfo)]() mutable
			{
				Result.ExecuteIfBound(bSucceeded, MoveTemp(HeaderInfo));
			});
		};

		FRuntimeBulkDataBuffer<uint8> BulkAudioData = FRuntimeBulkDataBuffer<uint8>(AudioData);

		FRuntimeCodecFactory CodecFactory;
		TArray<FBaseRuntimeCodec*> RuntimeCodecs = CodecFactory.GetCodecs(BulkAudioData);

		FEncodedAudioStruct EncodedData;
		EncodedData.AudioData = MoveTemp(BulkAudioData);
		for (FBaseRuntimeCodec* RuntimeCodec : RuntimeCodecs)
		{
			EncodedData.AudioFormat = RuntimeCodec->GetAudioFormat();

			FRuntimeAudioHeaderInfo HeaderInfo;
			if (!RuntimeCodec->GetHeaderInfo(MoveTemp(EncodedData), HeaderInfo))
			{
				continue;
			}

			ExecuteResult(true, MoveTemp(HeaderInfo));
			return;
		}

		ExecuteResult(false, FRuntimeAudioHeaderInfo());
	});
}

TArray<ERuntimeAudioFormat> URuntimeAudioUtilities::GetAudioFormatsAdvanced(const TArray64<uint8>& AudioData)
{
	return GetAudioFormatsAdvanced(FRuntimeBulkDataBuffer<uint8>(AudioData));
}

TArray<ERuntimeAudioFormat> URuntimeAudioUtilities::GetAudioFormatsAdvanced(const FRuntimeBulkDataBuffer<uint8>& AudioData)
{
	FRuntimeCodecFactory CodecFactory;
	TArray<FBaseRuntimeCodec*> RuntimeCodecs = CodecFactory.GetCodecs(AudioData);
	TArray<ERuntimeAudioFormat> AudioFormats;

	for (FBaseRuntimeCodec* RuntimeCodec : RuntimeCodecs)
	{
		AudioFormats.Add(RuntimeCodec->GetAudioFormat());
	}

	return AudioFormats;
}

FString URuntimeAudioUtilities::ConvertSecondsToString(int64 Seconds)
{
	FString FinalString;

	const int64 NewHours = Seconds / 3600;
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

void URuntimeAudioUtilities::ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResult& Result)
{
	ScanDirectoryForAudioFiles(Directory, bRecursive, FOnScanDirectoryForAudioFilesResultNative::CreateLambda([Result](bool bSucceeded, const TArray<FString>& AudioFilePaths)
	{
		Result.ExecuteIfBound(bSucceeded, AudioFilePaths);
	}));
}

void URuntimeAudioUtilities::ScanDirectoryForAudioFiles(const FString& Directory, bool bRecursive, const FOnScanDirectoryForAudioFilesResultNative& Result)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [Directory, bRecursive, Result]
	{
		auto ExecuteResult = [Result](bool bSucceeded, TArray<FString>&& AudioFilePaths)
		{
			AsyncTask(ENamedThreads::GameThread, [Result, bSucceeded, AudioFilePaths = MoveTemp(AudioFilePaths)]()
			{
				Result.ExecuteIfBound(bSucceeded, AudioFilePaths);
			});
		};

		class FDirectoryVisitor_AudioScanner : public IPlatformFile::FDirectoryVisitor
		{
		public:
			FRuntimeCodecFactory CodecFactory;
			TArray<FString> AudioFilePaths;

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (bIsDirectory)
				{
					return true;
				}

				FString AudioFilePath = FilenameOrDirectory;
				if (CodecFactory.GetCodecs(AudioFilePath).Num() > 0)
				{
					AudioFilePaths.Add(MoveTemp(AudioFilePath));
				}

				return true;
			}
		};

		FDirectoryVisitor_AudioScanner DirectoryVisitor_AudioScanner;
		const bool bSucceeded = [bRecursive, &Directory, &DirectoryVisitor_AudioScanner]()
		{
			if (bRecursive)
			{
				return FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*Directory, DirectoryVisitor_AudioScanner);
			}
			return FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*Directory, DirectoryVisitor_AudioScanner);
		}();

		ExecuteResult(bSucceeded, MoveTemp(DirectoryVisitor_AudioScanner.AudioFilePaths));
	});
}
