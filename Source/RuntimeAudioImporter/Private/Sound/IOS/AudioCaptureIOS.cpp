// Georgy Treshchev 2024.

#if PLATFORM_IOS && !PLATFORM_TVOS
#include "Sound/IOS/AudioCaptureIOS.h"
#include "Async/Future.h"
#include "RuntimeAudioImporterDefines.h"

constexpr int32 kInputBus = 1;
constexpr int32 kOutputBus = 0;

static OSStatus RecordingCallback(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData)
{
	Audio::FAudioCaptureIOS* AudioCapture = (Audio::FAudioCaptureIOS*)inRefCon;
	return AudioCapture->OnCaptureCallback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

bool Audio::FAudioCaptureIOS::GetCaptureDeviceInfo(Audio::FCaptureDeviceInfo& OutInfo, int32 DeviceIndex)
{
	OutInfo.DeviceName = TEXT("Default iOS Audio Device");
	OutInfo.InputChannels = NumChannels;
	OutInfo.PreferredSampleRate = SampleRate;

	return true;
}


bool Audio::FAudioCaptureIOS::
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

	auto CheckPermissionGranted = [this]() -> bool {
		const bool bPermissionGranted = 
#if (defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0)
		[[AVAudioApplication sharedInstance] recordPermission] == AVAudioApplicationRecordPermissionGranted;
#else
		[[AVAudioSession sharedInstance] recordPermission] == AVAudioSessionRecordPermissionGranted;
#endif
		return bPermissionGranted;
	};

	if (!CheckPermissionGranted())
	{
		UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Permission to record audio on iOS is not granted. Requesting permission..."));
		
		TSharedPtr<TPromise<bool>> PermissionPromise = MakeShared<TPromise<bool>>();

#if (defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0)
		[AVAudioApplication requestRecordPermissionWithCompletionHandler:^(BOOL granted) {
			dispatch_async(dispatch_get_main_queue(), ^{
				PermissionPromise->SetValue(granted);
			});
		}];
#else
		[[AVAudioSession sharedInstance] requestRecordPermission:^(BOOL granted) {
			dispatch_async(dispatch_get_main_queue(), ^{
				PermissionPromise->SetValue(granted);
			});
		}];
#endif

		TFuture<bool> PermissionFuture = PermissionPromise->GetFuture();

		// This will automatically block until the future is set
		bool bPermissionGranted = PermissionFuture.Get();

		if (!bPermissionGranted)
		{
			UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream because permission to record audio was not granted"));
			return false;
		}
		else
		{
			UE_LOG(LogRuntimeAudioImporter, Warning, TEXT("Permission to record audio on iOS was granted. Opening capture stream..."));
		}
	}

	OnCapture = MoveTemp(InOnCapture);

	OSStatus Status = noErr;

	NSError *setCategoryError = nil;
	[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayAndRecord withOptions : (AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetooth) error : &setCategoryError];
	if (setCategoryError != nil)
	{		
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when setting audio session category"), static_cast<int32>(Status));
		return false;
	}

	[[AVAudioSession sharedInstance] setActive:YES error : &setCategoryError];
	if (setCategoryError != nil)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when setting audio session active"), static_cast<int32>(Status));
		return false;
	}

	// Source of info "Technical Note TN2091 - Device input using the HAL Output Audio Unit"

	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	// We use processing element always for enable runtime changing HW AEC and AGC settings.
	// When it's disable the unit work as RemoteIO
	desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	AudioComponent InputComponent = AudioComponentFindNext(NULL, &desc);

	Status = AudioComponentInstanceNew(InputComponent, &IOUnit);
	if (Status != noErr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when creating audio component instance"), static_cast<int32>(Status));
		return false;
	}

	// Enable recording
	uint32 EnableIO = 1;
	Status = AudioUnitSetProperty(IOUnit,
		kAudioOutputUnitProperty_EnableIO,
		kAudioUnitScope_Input,
		kInputBus,
		&EnableIO,
		sizeof(EnableIO));

	if (Status != noErr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when enabling input"), static_cast<int32>(Status));
		return false;
	}

	// Disable output part
	EnableIO = 0;
	Status = AudioUnitSetProperty(IOUnit,
		kAudioOutputUnitProperty_EnableIO,
		kAudioUnitScope_Output,
		kOutputBus,
		&EnableIO,
		sizeof(EnableIO));

	if (Status != noErr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when disabling output"), static_cast<int32>(Status));
		return false;
	}

	AudioStreamBasicDescription StreamDescription = { 0 };
	const UInt32 BytesPerSample = sizeof(Float32);

	StreamDescription.mSampleRate = SampleRate;
	StreamDescription.mFormatID = kAudioFormatLinearPCM;
	StreamDescription.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
	StreamDescription.mChannelsPerFrame = NumChannels;
	StreamDescription.mBitsPerChannel = 8 * BytesPerSample;
	StreamDescription.mBytesPerFrame = BytesPerSample * StreamDescription.mChannelsPerFrame;
	StreamDescription.mFramesPerPacket = 1;
	StreamDescription.mBytesPerPacket = StreamDescription.mFramesPerPacket * StreamDescription.mBytesPerFrame;

	// Configure output format
	Status = AudioUnitSetProperty(IOUnit,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Output,
		kInputBus,
		&StreamDescription,
		sizeof(StreamDescription));

	if (Status != noErr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when setting output format"), static_cast<int32>(Status));
		return false;
	}

	// Setup capture callback
	AURenderCallbackStruct CallbackInfo;
	CallbackInfo.inputProc = RecordingCallback;
	CallbackInfo.inputProcRefCon = this;
	Status = AudioUnitSetProperty(IOUnit,
		kAudioOutputUnitProperty_SetInputCallback,
		kAudioUnitScope_Global,
		kInputBus,
		&CallbackInfo,
		sizeof(CallbackInfo));
	if (Status != noErr) {		
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when setting callback"), static_cast<int32>(Status));
		return false;
	}

	// Initialize audio unit
	Status = AudioUnitInitialize(IOUnit);
	if (Status != noErr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to open capture stream due to error %d when initializing audio unit"), static_cast<int32>(Status));
		return false;
	}

	// Configure unit processing
#if UE_VERSION_NEWER_THAN(4, 25, 0)
    SetHardwareFeatureEnabled(Audio::EHardwareInputFeature::EchoCancellation, InParams.bUseHardwareAEC);
    SetHardwareFeatureEnabled(Audio::EHardwareInputFeature::AutomaticGainControl, InParams.bUseHardwareAEC);
#endif

	bIsStreamOpen = (Status == noErr);
	return bIsStreamOpen;
}

bool Audio::FAudioCaptureIOS::CloseStream()
{
	StopStream();
	AudioComponentInstanceDispose(IOUnit);
	bIsStreamOpen = false;
	return true;
}

bool Audio::FAudioCaptureIOS::StartStream()
{
	if (!IsStreamOpen() || IsCapturing())
	{
		return false;
	}
	AudioUnitReset(IOUnit, kAudioUnitScope_Global, 0);
	bHasCaptureStarted = (AudioOutputUnitStart(IOUnit) == noErr);
	return bHasCaptureStarted;
}

bool Audio::FAudioCaptureIOS::StopStream()
{
	if (!IsStreamOpen() || !IsCapturing())
	{
		return false;
	}
	const bool bStopped = (AudioOutputUnitStop(IOUnit) == noErr);
	bHasCaptureStarted = !bStopped;
	return bStopped;
}

bool Audio::FAudioCaptureIOS::AbortStream()
{
	StopStream();
	CloseStream();
	return true;
}

bool Audio::FAudioCaptureIOS::GetStreamTime(double& OutStreamTime)
{
	OutStreamTime = 0.0f;
	return true;
}

bool Audio::FAudioCaptureIOS::IsStreamOpen() const
{
	return bIsStreamOpen;
}

bool Audio::FAudioCaptureIOS::IsCapturing() const
{
	return bHasCaptureStarted;
}

void Audio::FAudioCaptureIOS::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	OnCapture(static_cast<float*>(InBuffer), InBufferFrames, NumChannels, SampleRate, StreamTime, bOverflow);
}

bool Audio::FAudioCaptureIOS::GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices)
{
	OutDevices.Reset();

	FCaptureDeviceInfo& DeviceInfo = OutDevices.AddDefaulted_GetRef();
	GetCaptureDeviceInfo(DeviceInfo, 0);

	return true;
}

#if UE_VERSION_NEWER_THAN(4, 25, 0)
void Audio::FAudioCaptureIOS::SetHardwareFeatureEnabled(Audio::EHardwareInputFeature FeatureType, bool bEnabled)
{
	if (IOUnit == nil)
	{
		return;
	}

	// we ignore result those functions because sometime we can't set this parameters
	OSStatus status = noErr;

	switch (FeatureType)
	{
	case Audio::EHardwareInputFeature::EchoCancellation:
	{
		const UInt32 EnableParam = (bEnabled ? 0 : 1);
		status = AudioUnitSetProperty(IOUnit,
			kAUVoiceIOProperty_BypassVoiceProcessing,
			kAudioUnitScope_Global,
			kInputBus,
			&EnableParam,
			sizeof(EnableParam)
		);
	}
	break;
	case Audio::EHardwareInputFeature::AutomaticGainControl:
	{
		const UInt32 EnableParam = (bEnabled ? 1 : 0);
		status = AudioUnitSetProperty(IOUnit,
			kAUVoiceIOProperty_VoiceProcessingEnableAGC,
			kAudioUnitScope_Global,
			kInputBus,
			&EnableParam,
			sizeof(EnableParam)
		);
	}
	break;
	}
}
#endif

OSStatus Audio::FAudioCaptureIOS::OnCaptureCallback(AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
	OSStatus status = noErr;

	const int32 NeededBufferSize = inNumberFrames * NumChannels * sizeof(float);
	if (CaptureBuffer.Num() == 0 || BufferSize < NeededBufferSize)
	{
		BufferSize = NeededBufferSize;
		AllocateBuffer(BufferSize);
	}

	AudioBufferList* BufferList = (AudioBufferList*)CaptureBuffer.GetData();
	for (int i = 0; i < BufferList->mNumberBuffers; ++i)
	{
		BufferList->mBuffers[i].mDataByteSize = (UInt32)BufferSize;
	}

	status = AudioUnitRender(IOUnit,
		ioActionFlags,
		inTimeStamp,
		inBusNumber,
		inNumberFrames,
		BufferList);

	if (status != noErr)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to process audio unit render callback for capture device: %d"), static_cast<int32>(status));
		return 0;
	}

	void* InBuffer = (void*)BufferList->mBuffers[0].mData; // only first channel ?!
	OnAudioCapture(InBuffer, inNumberFrames, 0.0, false); // need calculate timestamp

	return noErr;
}

void Audio::FAudioCaptureIOS::AllocateBuffer(int32 SizeInBytes)
{
	const size_t CaptureBufferBytes = sizeof(AudioBufferList) + NumChannels * (sizeof(AudioBuffer) + SizeInBytes);
	CaptureBuffer.SetNum(CaptureBufferBytes);

	AudioBufferList* list = (AudioBufferList*)CaptureBuffer.GetData();
	uint8* CaptureBufferData = CaptureBuffer.GetData() + sizeof(AudioBufferList) + sizeof(AudioBuffer);

	list->mNumberBuffers = NumChannels;
	for (int32 Index = 0; Index < NumChannels; ++Index)
	{
		list->mBuffers[Index].mNumberChannels = 1;
		list->mBuffers[Index].mDataByteSize = (UInt32)SizeInBytes;
		list->mBuffers[Index].mData = CaptureBufferData;

		CaptureBufferData += (SizeInBytes + sizeof(AudioBuffer));
	}
}
#endif
