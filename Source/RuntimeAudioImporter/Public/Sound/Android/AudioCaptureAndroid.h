// Georgy Treshchev 2024.

#pragma once

#if PLATFORM_ANDROID

#include "AudioCaptureCore.h"
#include "Misc/EngineVersionComparison.h"
#include "Math/UnrealMathUtility.h"
#include "Async/Future.h"

/**
 * This recreates the FAudioCaptureAndroidStream Android-specific audio capture implementation, but with fixes to make it work correctly on Android
 */
namespace Audio
{
	class FAudioCaptureAndroid : public IAudioCaptureStream
	{
	public:
		FAudioCaptureAndroid();
		virtual ~FAudioCaptureAndroid() override;

		//~ Begin IAudioCaptureStream Interface
		virtual bool GetCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo, int32 DeviceIndex) override;
		virtual bool 
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
		, uint32 NumFramesDesired) override;
		virtual bool CloseStream() override;
		virtual bool StartStream() override;
		virtual bool StopStream() override;
		virtual bool AbortStream() override;
		virtual bool GetStreamTime(double& OutStreamTime) override;
		virtual int32 GetSampleRate() const override { return SampleRate; }
		virtual bool IsStreamOpen() const override;
		virtual bool IsCapturing() const override;
		virtual void OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow) override;
		virtual bool GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices) override;
		//~ End IAudioCaptureStream Interface

	protected:
		/**
		 * Starts the Android microphone capture
		 * @param TargetSampleRate The sample rate to capture at
		 * @return true if the capture was started successfully
		 */
		bool AndroidCaptureStart(int32 TargetSampleRate);

		/**
		 * Stops the Android microphone capture
		 */
		void AndroidCaptureStop();

	private:
		TFuture<bool> PermissionCheckFuture;
		bool bIsStreamOpen = false;
		bool bHasCaptureStarted = false;

		int32 NumChannels = 1;
		int32 SampleRate = 48000;

#if UE_VERSION_NEWER_THAN(5, 2, 9)
		FOnAudioCaptureFunction
#else
		FOnCaptureFunction
#endif
		OnCapture;
	};
}
#endif