// Georgy Treshchev 2023.

#include "Sound/CapturableSoundWave.h"

#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT

// Untested thus disabled for now
#if false && PLATFORM_ANDROID
#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#endif

#endif
#include "RuntimeAudioImporterDefines.h"
#include "AudioThread.h"
#include "Async/Async.h"
#include "UObject/WeakObjectPtrTemplates.h"

UCapturableSoundWave::UCapturableSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCapturableSoundWave::BeginDestroy()
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#if PLATFORM_IOS && !PLATFORM_TVOS
	AudioCaptureIOS.AbortStream();
	AudioCaptureIOS.CloseStream();
#else
	AudioCapture.AbortStream();
	AudioCapture.CloseStream();
#endif
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
		FAudioThread::RunCommandOnGameThread([Result, AvailableDevices]()
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
// Untested thus disabled for now
#if false && PLATFORM_ANDROID
	TArray<FString> Permissions = {"android.permission.RECORD_AUDIO"};
	if (!UAndroidPermissionFunctionLibrary::CheckPermission(Permissions[0]))
	{
		TPromise<bool> bPermissionGrantedPromise;
		TFuture<bool> bPermissionGrantedFuture = bPermissionGrantedPromise.GetFuture();
		UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions)->OnPermissionsGrantedDelegate.AddWeakLambda(this, [this, bPermissionGrantedPromise = MoveTemp(bPermissionGrantedPromise), Permissions = MoveTemp(Permissions)](const TArray<FString>& GrantPermissions, const TArray<bool>& GrantResults) mutable
		{
			if (GrantPermissions.Find(Permissions[0]) == INDEX_NONE || GrantResults.Find(true) != INDEX_NONE)
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capture as the permission was not granted"));
				bPermissionGrantedPromise.SetValue(false);
				return;
			}
			bPermissionGrantedPromise.SetValue(true);
		});
		bPermissionGrantedFuture.Wait();
		if (!bPermissionGrantedFuture.Get())
		{
			return false;
		}
	}
#endif
	
	Audio::FAudioCaptureDeviceParams Params = Audio::FAudioCaptureDeviceParams();
	Params.DeviceIndex = DeviceId;

#if UE_VERSION_NEWER_THAN(5, 2, 9)
	Audio::FOnAudioCaptureFunction
#else
	Audio::FOnCaptureFunction
#endif
	OnCapture = [WeakThis = TWeakObjectPtr<UCapturableSoundWave>(this)](const void* PCMData, int32 NumFrames, int32 NumOfChannels,
#if UE_VERSION_NEWER_THAN(4, 25, 0)
	                   int32 InSampleRate,
#endif
	                   double StreamTime, bool bOverFlow)
	{
		if (!WeakThis.IsValid())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to capture audio using a capturable sound wave as it has been destroyed"));
			return;
		}

#if PLATFORM_IOS && !PLATFORM_TVOS
		if (WeakThis->AudioCaptureIOS.IsCapturing())
#else
		if (WeakThis->AudioCapture.IsCapturing())
#endif
		{
			const int64 PCMDataSize = NumOfChannels * NumFrames;
			int64 PCMDataSizeInBytes = PCMDataSize * sizeof(float);

			if (PCMDataSizeInBytes > TNumericLimits<int32>::Max())
			{
				UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to append audio data as the size of the data exceeds the maximum size of int32 (PCMDataSizeInBytes: %lld, Max: %d)"), PCMDataSizeInBytes, TNumericLimits<int32>::Max());
				PCMDataSizeInBytes = TNumericLimits<int32>::Max();
			}

			WeakThis->AppendAudioDataFromRAW(TArray<uint8>(reinterpret_cast<const uint8*>(PCMData), static_cast<int32>(PCMDataSizeInBytes)), ERuntimeRAWAudioFormat::Float32,
#if UE_VERSION_NEWER_THAN(4, 25, 0)
									InSampleRate
#else
#if PLATFORM_IOS && !PLATFORM_TVOS
			                       WeakThis->AudioCaptureIOS.GetSampleRate()
#else
			                       WeakThis->AudioCapture.GetSampleRate()
#endif
#endif
			                     , NumOfChannels);
		}
	};

#if PLATFORM_IOS && !PLATFORM_TVOS
#if (defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0)
	return [[AVAudioApplication sharedInstance] recordPermission] == AVAudioApplicationRecordPermissionGranted;
#else
	return [[AVAudioSession sharedInstance] recordPermission] == AVAudioSessionRecordPermissionGranted;
#endif
#else
	if (AudioCapture.IsStreamOpen())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capture as the stream is already open"));
		return false;
	}

	if (!AudioCapture.
#if UE_VERSION_NEWER_THAN(5, 2, 9)
		OpenAudioCaptureStream
#else
		OpenCaptureStream
#endif
		(Params, MoveTemp(OnCapture), 1024))
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capturing stream for sound wave %s"), *GetName());
		return false;
	}

	if (!AudioCapture.StartStream())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capturing for sound wave %s"), *GetName());
		return false;
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully started capturing for sound wave %s"), *GetName());
	return true;
#endif
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capturing as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
	return false;
#endif
}

void UCapturableSoundWave::StopCapture()
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#if PLATFORM_IOS && !PLATFORM_TVOS
	if (AudioCaptureIOS.IsStreamOpen())
	{
		AudioCaptureIOS.CloseStream();
	}
#else
	if (AudioCapture.IsStreamOpen())
	{
		AudioCapture.CloseStream();
	}
#endif
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to stop capturing as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
#endif
}

bool UCapturableSoundWave::ToggleMute(bool bMute)
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#if PLATFORM_IOS && !PLATFORM_TVOS
	if (!AudioCaptureIOS.IsStreamOpen())
#else
	if (!AudioCapture.IsStreamOpen())
#endif
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to toggle mute for %s as the stream is not open"), *GetName());
		return false;
	}
	if (bMute)
	{
#if PLATFORM_IOS && !PLATFORM_TVOS
		if (!AudioCaptureIOS.IsCapturing())
#else
		if (!AudioCapture.IsCapturing())
#endif
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mute as the stream for %s is already closed"), *GetName());
			return false;
		}
#if PLATFORM_IOS && !PLATFORM_TVOS
		if (!AudioCaptureIOS.StopStream())
#else
		if (!AudioCapture.StopStream())
#endif
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mute the stream for sound wave %s"), *GetName());
			return false;
		}
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully muted the stream for sound wave %s"), *GetName());
		return true;
	}
	else
	{
#if PLATFORM_IOS && !PLATFORM_TVOS
		if (AudioCaptureIOS.IsCapturing())
#else
		if (AudioCapture.IsCapturing())
#endif
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to unmute as the stream for %s is already open"), *GetName());
			return false;
		}
#if PLATFORM_IOS && !PLATFORM_TVOS
		if (!AudioCaptureIOS.StartStream())
#else
		if (!AudioCapture.StartStream())
#endif
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to unmute the stream for sound wave %s"), *GetName());
			return false;
		}
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully unmuted the stream for sound wave %s"), *GetName());
		return true;
	}
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to toggle mute as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
	return false;
#endif
}

bool UCapturableSoundWave::IsCapturing() const
{
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#if PLATFORM_IOS && !PLATFORM_TVOS
	return AudioCaptureIOS.IsCapturing();
#else
	return AudioCapture.IsCapturing();
#endif
#else
	UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to get capturing state as its support is disabled (please enable in RuntimeAudioImporter.Build.cs)"));
	return false;
#endif
}
