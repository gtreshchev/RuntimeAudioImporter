// Georgy Treshchev 2023.

#include "Sound/CapturableSoundWave.h"
#include "RuntimeAudioImporterDefines.h"
#include "AudioThread.h"
#include "Async/Async.h"

UCapturableSoundWave::UCapturableSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCapturableSoundWave::BeginDestroy()
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	AudioCapture.AbortStream();
	AudioCapture.CloseStream();
#endif

	Super::BeginDestroy();
}

UCapturableSoundWave* UCapturableSoundWave::CreateCapturableSoundWave()
{
	if (!IsInGameThread())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to create a sound wave outside of the game thread"));
		return nullptr;
	}

	return NewObject<UCapturableSoundWave>();
}

void UCapturableSoundWave::GetAvailableAudioInputDevices(const FOnGetAvailableAudioInputDevicesResult& Result)
{
	GetAvailableAudioInputDevices(FOnGetAvailableAudioInputDevicesResultNative::CreateLambda([Result](const TArray<FRuntimeAudioInputDeviceInfo>& AvailableDevices)
	{
		Result.ExecuteIfBound(AvailableDevices);
	}));
}

void UCapturableSoundWave::GetAvailableAudioInputDevices(const FOnGetAvailableAudioInputDevicesResultNative& Result)
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	if (!IsInAudioThread())
	{
		FAudioThread::RunCommandOnAudioThread([Result]()
		{
			GetAvailableAudioInputDevices(Result);
		});

		return;
	}

	auto ExecuteResult = [Result](const TArray<FRuntimeAudioInputDeviceInfo>& AvailableDevices)
	{
		AsyncTask(ENamedThreads::GameThread, [Result, AvailableDevices]()
		{
			Result.ExecuteIfBound(AvailableDevices);
		});
	};

	TArray<FRuntimeAudioInputDeviceInfo> AvailableDevices;

	Audio::FAudioCapture AudioCapture;
	TArray<Audio::FCaptureDeviceInfo> InputDevices;

	AudioCapture.GetCaptureDevicesAvailable(InputDevices);

	for (const Audio::FCaptureDeviceInfo& CaptureDeviceInfo : InputDevices)
	{
		AvailableDevices.Add(FRuntimeAudioInputDeviceInfo(CaptureDeviceInfo));
	}

	ExecuteResult(AvailableDevices);
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to get available audio input devices as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
	Result.ExecuteIfBound(TArray<FRuntimeAudioInputDeviceInfo>());
#endif
}

bool UCapturableSoundWave::StartCapture(int32 DeviceId)
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	if (AudioCapture.IsStreamOpen())
	{
		return false;
	}


	Audio::FOnCaptureFunction OnCapture = [this](const float* PCMData, int32 NumFrames, int32 NumOfChannels,
#if (ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION > 24) || ENGINE_MAJOR_VERSION >= 5
		int32 InSampleRate,
#endif
	                                             double StreamTime, bool bOverFlow)
	{
		const int32 PCMDataSize = NumOfChannels * NumFrames;

		if (AudioCapture.IsCapturing())
		{
			AppendAudioDataFromRAW(TArray<uint8>(reinterpret_cast<const uint8*>(PCMData), PCMDataSize * sizeof(float)), ERuntimeRAWAudioFormat::Float32,
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION > 24
									InSampleRate
#else
			                       AudioCapture.GetSampleRate()
#endif
			                     , NumOfChannels);
		}
	};

	Audio::FAudioCaptureDeviceParams Params = Audio::FAudioCaptureDeviceParams();
	Params.DeviceIndex = DeviceId;

	if (!AudioCapture.OpenCaptureStream(Params, MoveTemp(OnCapture), 1024))
	{
		return false;
	}

	return AudioCapture.StartStream();
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capturing as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
	return false;
#endif
}

void UCapturableSoundWave::StopCapture()
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	if (AudioCapture.IsStreamOpen())
	{
		AudioCapture.StopStream();
	}
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to stop capturing as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
#endif
}
