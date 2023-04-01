// Georgy Treshchev 2023.

#pragma once

#include "CoreMinimal.h"
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
#include "AudioCaptureCore.h"
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
	UFUNCTION(BlueprintCallable, Category = "Capturable Sound Wave|Capture")
	bool StartCapture(int32 DeviceId);

	/**
	 * Stop the capture process
	 */
	UFUNCTION(BlueprintCallable, Category = "Capturable Sound Wave|Capture")
	void StopCapture();

	/**
	 * Toggles the mute state of audio capture, pausing the accumulation of audio data without closing the stream
	 */
	UFUNCTION(BlueprintCallable, Category = "Capturable Sound Wave|Capture")
	bool ToggleMute(bool bMute);

	/**
	 * Get whether the capture is processing or not
	 *
	 * @return Whether the capture is processing or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Capturable Sound Wave|Info")
	bool IsCapturing() const;

private:
#if WITH_RUNTIMEAUDIOIMPORTER_CAPTURE_SUPPORT
	/** Audio capture instance */
	Audio::FAudioCapture AudioCapture;
#endif
};
