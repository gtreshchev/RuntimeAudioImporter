// Georgy Treshchev 2024.

#include "RuntimeAudioImporterDefines.h"

#if PLATFORM_ANDROID
#include "Async/Future.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "AndroidPermissionCallbackProxy.h"
#include "Android/AndroidPlatformMisc.h"
#endif

namespace RuntimeAudioImporter
{
	bool CheckAndRequestPermissions()
	{
#if PLATFORM_ANDROID
		TArray<FString> AllRequiredPermissions = []()
		{
			TArray<FString> InternalPermissions = {TEXT("android.permission.READ_EXTERNAL_STORAGE"), TEXT("android.permission.WRITE_EXTERNAL_STORAGE")};
			if (FAndroidMisc::GetAndroidBuildVersion() >= 33)
			{
				InternalPermissions.Add(TEXT("android.permission.READ_MEDIA_AUDIO"));
			}
			return InternalPermissions;
		}();

		TArray<FString> UngrantedPermissions;
		for (const FString& Permission : AllRequiredPermissions)
		{
			if (!UAndroidPermissionFunctionLibrary::CheckPermission(Permission))
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
		UAndroidPermissionCallbackProxy* PermissionsGrantedCallbackProxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(UngrantedPermissions);
		if (!PermissionsGrantedCallbackProxy)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to request permissions for reading/writing audio data on Android (PermissionsGrantedCallbackProxy is null)"));
			return false;
		}

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
		OnPermissionsGrantedDelegate, bPermissionGrantedPromise, Permissions = MoveTemp(UngrantedPermissions)](const TArray<FString>& GrantPermissions, const TArray<bool>& GrantResults) mutable
		{
#if UE_VERSION_NEWER_THAN(5, 0, 0)
			OnPermissionsGrantedDelegate.Remove(OnPermissionsGrantedDelegateHandle.Get());
#else
			OnPermissionsGrantedDelegate.Unbind();
#endif
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

		// Removing unused two uninitialized bytes
		AudioData.RemoveAt(AudioData.Num() - 2, 2);

		return true;
	}
}
