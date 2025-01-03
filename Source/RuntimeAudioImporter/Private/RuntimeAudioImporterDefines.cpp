// Georgy Treshchev 2024.

#include "RuntimeAudioImporterDefines.h"

#if PLATFORM_ANDROID && WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT
#include "Async/Future.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformMisc.h"
#include "Async/TaskGraphInterfaces.h"

#if USE_ANDROID_JNI
#include "Android/AndroidJNI.h"
static jclass _PermissionHelperClass;
static jmethodID _CheckPermissionMethodId;
static jmethodID _AcquirePermissionMethodId;
#endif

#endif

namespace RuntimeAudioImporter
{
	DECLARE_MULTICAST_DELEGATE_TwoParams(FRAIAndroidPermissionDelegate, const TArray<FString>& /*Permissions*/, const TArray<bool>& /*GrantResults*/);
	
	class FRAIAndroidPermissionCallbackProxy
	{
	public:

		FRAIAndroidPermissionDelegate OnPermissionsGrantedDelegate;
	};

	TSharedPtr<FRAIAndroidPermissionCallbackProxy> AndroidPermissionCallbackProxyInstance = nullptr;
	
	TSharedPtr<FRAIAndroidPermissionCallbackProxy> AcquirePermissions(const TArray<FString>& Permissions)
	{
#if PLATFORM_ANDROID && USE_ANDROID_JNI && WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT
		JNIEnv* env = FAndroidApplication::GetJavaEnv();
		auto permissionsArray = NewScopedJavaObject(env, (jobjectArray)env->NewObjectArray(Permissions.Num(), FJavaWrapper::JavaStringClass, NULL));
		for (int i = 0; i < Permissions.Num(); i++)
		{
			auto str = FJavaHelper::ToJavaString(env, Permissions[i]);
			env->SetObjectArrayElement(*permissionsArray, i, *str);
		}
		env->CallStaticVoidMethod(_PermissionHelperClass, _AcquirePermissionMethodId, *permissionsArray);
#endif
		AndroidPermissionCallbackProxyInstance = MakeShared<FRAIAndroidPermissionCallbackProxy>();
		return AndroidPermissionCallbackProxyInstance;
	}
	
	bool CheckPermission(const FString& Permission)
	{
#if PLATFORM_ANDROID && USE_ANDROID_JNI && WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT
		JNIEnv* env = FAndroidApplication::GetJavaEnv();
		_PermissionHelperClass = FAndroidApplication::FindJavaClassGlobalRef("com/Plugins/RuntimeAudioImporter/RuntimeAudioPermissionHelper");
		_CheckPermissionMethodId = env->GetStaticMethodID(_PermissionHelperClass, "checkPermission", "(Ljava/lang/String;)Z");
		_AcquirePermissionMethodId = env->GetStaticMethodID(_PermissionHelperClass, "acquirePermissions", "([Ljava/lang/String;)V");
		
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		auto Argument = FJavaHelper::ToJavaString(Env, Permission);
		bool bResult = env->CallStaticBooleanMethod(_PermissionHelperClass, _CheckPermissionMethodId, *Argument);
		return bResult;
#else
		return true;
#endif
	}
	
	bool CheckAndRequestPermissions(TArray<FString> AllRequiredPermissions)
	{
#if PLATFORM_ANDROID && WITH_RUNTIMEAUDIOIMPORTER_FILEOPERATION_SUPPORT
		if (AllRequiredPermissions.Num() == 0)
		{
			AllRequiredPermissions = []()
			{
				TArray<FString> InternalPermissions = {TEXT("android.permission.READ_EXTERNAL_STORAGE"), TEXT("android.permission.WRITE_EXTERNAL_STORAGE")};
				if (FAndroidMisc::GetAndroidBuildVersion() >= 33)
				{
					InternalPermissions.Add(TEXT("android.permission.READ_MEDIA_AUDIO"));
				}
				return InternalPermissions;
			}();
		}

		TArray<FString> UngrantedPermissions;
		for (const FString& Permission : AllRequiredPermissions)
		{
			if (!CheckPermission(Permission))
			{
				UngrantedPermissions.Add(Permission);
				UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Permission '%s' is not granted on Android. Requesting permission..."), *Permission);
			}
			else
			{
				UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Permission '%s' is granted on Android. No need to request permission"), *Permission);
			}
		}

		if (UngrantedPermissions.Num() == 0)
		{
			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("All required permissions are granted on Android. No need to request permissions"));
			return true;
		}

		TSharedRef<TPromise<bool>> bPermissionGrantedPromise = MakeShared<TPromise<bool>>();
		TFuture<bool> bPermissionGrantedFuture = bPermissionGrantedPromise->GetFuture();
		TSharedPtr<FRAIAndroidPermissionCallbackProxy> PermissionsGrantedCallbackProxy = AcquirePermissions(UngrantedPermissions);
		if (!PermissionsGrantedCallbackProxy)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to request permissions for reading/writing audio data on Android (PermissionsGrantedCallbackProxy is null)"));
			return false;
		}

		FRAIAndroidPermissionDelegate OnPermissionsGrantedDelegate;
		TSharedRef<FDelegateHandle> OnPermissionsGrantedDelegateHandle = MakeShared<FDelegateHandle>();
		*OnPermissionsGrantedDelegateHandle = OnPermissionsGrantedDelegate.AddLambda([OnPermissionsGrantedDelegateHandle,
		OnPermissionsGrantedDelegate, bPermissionGrantedPromise, Permissions = MoveTemp(UngrantedPermissions)](const TArray<FString>& GrantPermissions, const TArray<bool>& GrantResults) mutable
		{
			OnPermissionsGrantedDelegate.Remove(OnPermissionsGrantedDelegateHandle.Get());
			for (const FString& Permission : Permissions)
			{
				if (!GrantPermissions.Contains(Permission) || !GrantResults.IsValidIndex(GrantPermissions.Find(Permission)) || !GrantResults[GrantPermissions.Find(Permission)])
				{
					TArray<FString> GrantResultsString;
					UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to request permission '%s' for reading/writing audio data on Android"), *Permission);
					bPermissionGrantedPromise->SetValue(false);
					return;
				}
			}
			UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully granted permission for reading/writing audio data on Android"));
			bPermissionGrantedPromise->SetValue(true);
		});
		PermissionsGrantedCallbackProxy->OnPermissionsGrantedDelegate = OnPermissionsGrantedDelegate;
		bPermissionGrantedFuture.Wait();
		if (!bPermissionGrantedFuture.Get())
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Failed to request permission for reading/writing audio data on Android"));
			return false;
		}
		UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully requested permission for reading/writing audio data on Android"));
		return true;
#else
		return true;
#endif
	}

	bool LoadAudioFileToArray(TArray64<uint8>& AudioData, const FString& FilePath)
	{
		CheckAndRequestPermissions();

#if UE_VERSION_OLDER_THAN(4, 26, 0)
		TArray<uint8> AudioData32;
		if (!FFileHelper::LoadFileToArray(AudioData32, *FilePath))
		{
			return false;
		}
		AudioData = AudioData32;
#else
		if (!FFileHelper::LoadFileToArray(AudioData, *FilePath))
		{
			return false;
		}
#endif

		return true;
	}
}

#if PLATFORM_ANDROID && USE_ANDROID_JNI
JNI_METHOD void Java_com_Plugins_RuntimeAudioImporter_RuntimeAudioPermissionHelper_onAcquirePermissions(JNIEnv *env, jclass clazz, jobjectArray permissions, jintArray grantResults) 
{
	if (!RuntimeAudioImporter::AndroidPermissionCallbackProxyInstance) return;

	TArray<FString> arrPermissions;
	TArray<bool> arrGranted;
	int num = env->GetArrayLength(permissions);
	jint* jarrGranted = env->GetIntArrayElements(grantResults, 0);
	for (int i = 0; i < num; i++)
	{
		arrPermissions.Add(FJavaHelper::FStringFromLocalRef(env, (jstring)env->GetObjectArrayElement(permissions, i)));
		arrGranted.Add(jarrGranted[i] == 0 ? true : false); // 0: permission granted, -1: permission denied
	}
	env->ReleaseIntArrayElements(grantResults, jarrGranted, 0);

	RuntimeAudioImporter::AndroidPermissionCallbackProxyInstance->OnPermissionsGrantedDelegate.Broadcast(arrPermissions, arrGranted);
}
#endif
