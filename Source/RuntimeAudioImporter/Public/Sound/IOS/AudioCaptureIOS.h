// Georgy Treshchev 2024.

#pragma once

#if PLATFORM_IOS && !PLATFORM_TVOS
#include "AudioCaptureCore.h"
#include "Misc/EngineVersionComparison.h"
#include "Math/UnrealMathUtility.h"

#import <AVFoundation/AVAudioSession.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <AVFoundation/AVFoundation.h>

/**
 * This recreates the FAudioCaptureAudioUnitStream iOS-specific audio capture implementation, but with fixes to make it work correctly on iOS and avoid crashes
 */
namespace Audio
{
	class FAudioCaptureIOS : public IAudioCaptureStream
	{
	public:
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
#if UE_VERSION_NEWER_THAN(4, 25, 0)
		virtual void SetHardwareFeatureEnabled(Audio::EHardwareInputFeature FeatureType, bool bEnabled) override;
#endif
		//~ End IAudioCaptureStream Interface

		OSStatus OnCaptureCallback(AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData);

	private:
		void AllocateBuffer(int32 SizeInBytes);

		bool bIsStreamOpen = false;
		bool bHasCaptureStarted = false;
		AudioComponentInstance IOUnit;

		int32 NumChannels = 1;
		int32 SampleRate = 48000;

		TArray<uint8> CaptureBuffer;
		int32 BufferSize = 0;

#if UE_VERSION_NEWER_THAN(5, 2, 9)
		FOnAudioCaptureFunction
#else
		FOnCaptureFunction
#endif
		OnCapture;
	};
}
#endif