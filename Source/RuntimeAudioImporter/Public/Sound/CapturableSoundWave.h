// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#include "AudioCaptureCore.h"
#if PLATFORM_IOS && !PLATFORM_TVOS
#include "IOS/AudioCaptureIOS.h"
#elif PLATFORM_ANDROID
#include "Android/AudioCaptureAndroid.h"
#endif
#endif
#include "StreamingSoundWave.h"
#include "CapturableSoundWave.generated.h"

/** Static delegate broadcasting available audio input devices */
DECLARE_DELEGATE_OneParam(FOnGetAvailableAudioInputDevicesResultNative, const TArray<FRuntimeAudioInputDeviceInfo>&);

/** Dynamic delegate broadcasting available audio input devices */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGetAvailableAudioInputDevicesResult, const TArray<FRuntimeAudioInputDeviceInfo>&, AvailableDevices);

/**
 * Sound wave that can capture audio data from input devices (eg. microphone)
 */
UCLASS(BlueprintType, Category = "Capturable Sound Wave")
class RUNTIMEAUDIOIMPORTER_API UCapturableSoundWave : public UStreamingSoundWave
{
	GENERATED_BODY()

public:
	UCapturableSoundWave(const FObjectInitializer& ObjectInitializer);

	//~ Begin UImportedSoundWave Interface
	virtual void BeginDestroy() override;
	//~ End UImportedSoundWave Interface

	/**
	 * Create a new instance of the capturable sound wave
	 *
	 * @return Created capturable sound wave
	 */
	UFUNCTION(BlueprintCallable, Category = "Capturable Sound Wave|Main")
	static UCapturableSoundWave* CreateCapturableSoundWave();

	/**
	 * Get information about all available audio input devices
	 * 
	 * @param Result Delegate broadcasting the result
	 */
	UFUNCTION(BlueprintCallable, Category = "Capturable Sound Wave|Info")
	static void GetAvailableAudioInputDevices(const FOnGetAvailableAudioInputDevicesResult& Result);

	/**
	 * Gets information about all audio output devices available in the system. Suitable for use in C++
	 * 
	 * @param Result Delegate broadcasting the result
	 */
	static void GetAvailableAudioInputDevices(const FOnGetAvailableAudioInputDevicesResultNative& Result);

	/**
	 * Start the capture process
	 *
	 * @param DeviceId Required device index (order as from GetAvailableAudioInputDevices)
	 * @return Whether the capture was started or not
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Capturable Sound Wave|Capture")
	bool StartCapture(int32 DeviceId);

protected:
	virtual bool StartCapture_Implementation(int32 DeviceId);
public:

	/**
	 * Stop the capture process
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Capturable Sound Wave|Capture")
	void StopCapture();

protected:
	virtual void StopCapture_Implementation();

public:
	/**
	 * Toggles the mute state of audio capture, pausing the accumulation of audio data without closing the stream
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Capturable Sound Wave|Capture")
	bool ToggleMute(bool bMute);

protected:
	virtual bool ToggleMute_Implementation(bool bMute);

public:
	/**
	 * Get whether the capture is processing or not
	 *
	 * @return Whether the capture is processing or not
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Capturable Sound Wave|Info")
	bool IsCapturing() const;

protected:
	virtual bool IsCapturing_Implementation() const;

private:
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#if PLATFORM_IOS && !PLATFORM_TVOS
	/** Audio capture instance specific to iOS. Implemented manually due to the engine not properly supporting iOS audio capture at the moment */
	Audio::FAudioCaptureIOS AudioCapture;
#elif PLATFORM_ANDROID
	/** Audio capture instance specific to Android. Implemented manually due to the engine not properly supporting Android audio capture at the moment */
	Audio::FAudioCaptureAndroid AudioCapture;
#else
	/** Audio capture instance */
	Audio::FAudioCapture AudioCapture;
#endif
protected:
	/** The last device index used for capture */
	int32 LastDeviceIndex = -1;
#endif
};
