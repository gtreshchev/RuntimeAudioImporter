// Georgy Treshchev 2024.

#if PLATFORM_ANDROID

#include "Sound/Android/AudioCaptureAndroid.h"

#include "RuntimeAudioImporterDefines.h"

#include "Codecs/RAW_RuntimeCodec.h"
#include <Android/AndroidApplication.h>
#include <Android/AndroidJNI.h>

#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"

static JNIEnv* JavaEnv;
static jmethodID AndroidThunkJava_AndroidStartCapture = nullptr;
static jmethodID AndroidThunkJava_AndroidStopCapture = nullptr;
static Audio::FAudioCaptureAndroid* AudioCaptureAndroidForCallback = nullptr;

extern "C"
{
	JNIEXPORT void
#if !UE_VERSION_OLDER_THAN(5,0,0)
Java_com_epicgames_unreal_GameActivity_audioCapturePayload
#else
Java_com_epicgames_ue4_GameActivity_audioCapturePayload
#endif
	(JNIEnv* jenv, jobject thiz, jshortArray shortPCMDataArray, jint readSize)
	{
		UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Processing Android audio capture payload with %d samples"), readSize);

		if (readSize > 0)
		{
			TArray<int16> Int16PCMData;
			Int16PCMData.Reserve(readSize * 2);
			{
				jshort* shortPCMDataArrayElements = jenv->GetShortArrayElements(shortPCMDataArray, nullptr);
				for (int32 SampleIndex = 0; SampleIndex < readSize; ++SampleIndex) {
					jshort Int16PCMDataSample = shortPCMDataArrayElements[SampleIndex];
					Int16PCMData.Add(static_cast<int16>(Int16PCMDataSample));		
				}
				jenv->ReleaseShortArrayElements(shortPCMDataArray, shortPCMDataArrayElements, 0);
			}

			float* FloatPCMDataPtr = nullptr;
			FRAW_RuntimeCodec::TranscodeRAWData<int16, float>(Int16PCMData.GetData(), Int16PCMData.Num(), FloatPCMDataPtr);
			if (AudioCaptureAndroidForCallback)
			{
				AudioCaptureAndroidForCallback->OnAudioCapture(FloatPCMDataPtr, readSize, 0.0, false);
			}
			FMemory::Free(FloatPCMDataPtr);
		}
	} 
}

Audio::FAudioCaptureAndroid::FAudioCaptureAndroid()
{
	AudioCaptureAndroidForCallback = this;
}

Audio::FAudioCaptureAndroid::~FAudioCaptureAndroid()
{
	AudioCaptureAndroidForCallback = nullptr;
}

bool Audio::FAudioCaptureAndroid::GetCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo, int32 DeviceIndex)
{
	OutInfo.DeviceName = TEXT("Default Android Audio Device");
	OutInfo.InputChannels = NumChannels;
	OutInfo.PreferredSampleRate = SampleRate;

	return true;
}

TFuture<bool> RequestCapturePermissions()
{
	TArray<FString> Permissions = {"android.permission.RECORD_AUDIO"};
	if (UAndroidPermissionFunctionLibrary::CheckPermission(Permissions[0]))
	{
		return MakeFulfilledPromise<bool>(true).GetFuture();
	}

	UAndroidPermissionCallbackProxy* PermissionsGrantedCallbackProxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions);
	if (!PermissionsGrantedCallbackProxy)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capture as the Android record audio permission was not granted (PermissionsGrantedCallbackProxy is null)"));
		return MakeFulfilledPromise<bool>(false).GetFuture();
	}

	TSharedPtr<TPromise<bool>> PermissionPromise = MakeShared<TPromise<bool>>();
	TFuture<bool> PermissionFuture = PermissionPromise->GetFuture();

	FAndroidPermissionDelegate OnPermissionsGrantedDelegate;
#if UE_VERSION_NEWER_THAN(5, 0, 0)
	TSharedRef<FDelegateHandle> OnPermissionsGrantedDelegateHandle = MakeShared<FDelegateHandle>();
	*OnPermissionsGrantedDelegateHandle = OnPermissionsGrantedDelegate.AddLambda
#else
	OnPermissionsGrantedDelegate.BindLambda
#endif
	([
#if UE_VERSION_NEWER_THAN(5, 0, 0)
	OnPermissionsGrantedDelegateHandle,
#endif
	OnPermissionsGrantedDelegate, PermissionPromise, Permissions = MoveTemp(Permissions)](const TArray<FString>& GrantPermissions, const TArray<bool>& GrantResults) mutable
	{
#if UE_VERSION_NEWER_THAN(5, 0, 0)
		OnPermissionsGrantedDelegate.Remove(OnPermissionsGrantedDelegateHandle.Get());
#else
		OnPermissionsGrantedDelegate.Unbind();
#endif
		if (!GrantPermissions.Contains(Permissions[0]) || !GrantResults.Contains(true))
		{
			TArray<FString> GrantResultsString;
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capture as the Android record audio permission was not granted"));
			PermissionPromise->SetValue(false);
			return;
		}
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully granted record audio permission for Android"));
		PermissionPromise->SetValue(true);
	});
	PermissionsGrantedCallbackProxy->OnPermissionsGrantedDelegate = OnPermissionsGrantedDelegate;
	return PermissionFuture;
}

bool Audio::FAudioCaptureAndroid::
#if UE_VERSION_NEWER_THAN(5, 2, 9)
	OpenAudioCaptureStream
#else
	OpenCaptureStream
#endif
	(const Audio::FAudioCaptureDeviceParams& InParams,
#if UE_VERSION_NEWER_THAN(5, 2, 9)
	FOnAudioCaptureFunction InOnCapture
#else
	FOnCaptureFunction InOnCapture
#endif
	, uint32 NumFramesDesired)
{
	if (bIsStreamOpen)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream because it is already open"));
		return true;
	}

	TSharedPtr<TPromise<bool>> PermissionCheckPromise = MakeShared<TPromise<bool>>();
	PermissionCheckFuture = PermissionCheckPromise->GetFuture();
	RequestCapturePermissions().Next([this, InOnCapture, PermissionCheckPromise](bool bPermissionsGranted) mutable
	{
		PermissionCheckPromise->SetValue(bPermissionsGranted);
	});
	
	OnCapture = MoveTemp(InOnCapture);
	bIsStreamOpen = true;
	return true;
}

bool Audio::FAudioCaptureAndroid::CloseStream()
{
	StopStream();
	bIsStreamOpen = false;
	return true;
}

bool Audio::FAudioCaptureAndroid::StartStream()
{
	if (PermissionCheckFuture.IsReady() && PermissionCheckFuture.Get())
	{
		bHasCaptureStarted = AndroidCaptureStart(SampleRate);
		return bHasCaptureStarted;
	}
	PermissionCheckFuture.Next([this](bool bPermissionsGranted)
	{
		if (bPermissionsGranted)
		{
			bHasCaptureStarted = AndroidCaptureStart(SampleRate);
		}
	});
	// TODO: Async return
	return true;
}

bool Audio::FAudioCaptureAndroid::StopStream()
{
	AndroidCaptureStop();
	bHasCaptureStarted = false;
	return true;
}

bool Audio::FAudioCaptureAndroid::AbortStream()
{
	return CloseStream();
}

bool Audio::FAudioCaptureAndroid::GetStreamTime(double& OutStreamTime)
{
	OutStreamTime = 0.0f;
	return true;
}

bool Audio::FAudioCaptureAndroid::IsStreamOpen() const
{
	return bIsStreamOpen;
}

bool Audio::FAudioCaptureAndroid::IsCapturing() const
{
	return bHasCaptureStarted;
}

void Audio::FAudioCaptureAndroid::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	OnCapture(static_cast<float*>(InBuffer), InBufferFrames, NumChannels, SampleRate, StreamTime, bOverflow);
}

bool Audio::FAudioCaptureAndroid::GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices)
{
	OutDevices.Reset();

	FCaptureDeviceInfo& DeviceInfo = OutDevices.AddDefaulted_GetRef();
	GetCaptureDeviceInfo(DeviceInfo, 0);

	return true;
}

bool Audio::FAudioCaptureAndroid::AndroidCaptureStart(int32 TargetSampleRate)
{
	JavaEnv = FAndroidApplication::GetJavaEnv();
	if (!JavaEnv)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to capture audio because JavaEnv is null"));
		return false;
	}

	if (!AndroidThunkJava_AndroidStartCapture)
	{
		AndroidThunkJava_AndroidStartCapture = FJavaWrapper::FindMethod(JavaEnv, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_AndroidStartCapture", "(I)Z", false);
	}

	if (!AndroidThunkJava_AndroidStartCapture)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to find AndroidThunkJava_AndroidStartCapture"));
		return false;
	}

	jboolean bResult = FJavaWrapper::CallBooleanMethod(JavaEnv, FJavaWrapper::GameActivityThis, AndroidThunkJava_AndroidStartCapture, TargetSampleRate);
	return bResult;
}

void Audio::FAudioCaptureAndroid::AndroidCaptureStop()
{
	JavaEnv = FAndroidApplication::GetJavaEnv();
	if (!JavaEnv)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to stop audio capture because JavaEnv is null"));
		return;
	}

	if (!AndroidThunkJava_AndroidStopCapture)
	{
		AndroidThunkJava_AndroidStopCapture = FJavaWrapper::FindMethod(JavaEnv, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_AndroidStopCapture", "()V", false);
	}

	if (!AndroidThunkJava_AndroidStopCapture || !JavaEnv)
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Unable to find AndroidThunkJava_AndroidStopCapture (%d) or JavaEnv (%d)"), AndroidThunkJava_AndroidStopCapture == NULL, JavaEnv == NULL);
		return;
	}

	FJavaWrapper::CallVoidMethod(JavaEnv, FJavaWrapper::GameActivityThis, AndroidThunkJava_AndroidStopCapture);
}
#endif