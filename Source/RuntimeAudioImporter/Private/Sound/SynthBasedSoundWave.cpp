// Georgy Treshchev 2024.

#include "Sound/SynthBasedSoundWave.h"

#include "AudioDevice.h"
#include "Codecs/RAW_RuntimeCodec.h"
#include "Engine/World.h"

USynthBasedSoundWave::USynthBasedSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), SynthComponent(nullptr)
{
}

USynthBasedSoundWave* USynthBasedSoundWave::CreateSynthBasedSoundWave(USynthComponent* InSynthComponent)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to create a sound wave outside of the game thread"));
		return nullptr;
	}

	USynthBasedSoundWave* SoundWave = NewObject<USynthBasedSoundWave>();
	SoundWave->SynthComponent = InSynthComponent;
	return SoundWave;
}

bool USynthBasedSoundWave::StartCapture_Implementation(int32 DeviceId)
{
	if (!SynthComponent)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capturing for sound wave '%s' as the synth component is not set"), *GetName());
		return false;
	}

	SynthComponent->Start();

	USynthSound* SynthSound = GetSynthSound();
	if (!SynthSound)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to start capturing for sound wave '%s' as the synth sound is not available"), *GetName());
		return false;
	}

#if WITH_RUNTIMEAUDIOIMPORTER_SYNTH_SOUND_GENERATOR_SUPPORT
	// TODO: Fill in AudioMixerNumOutputFrames and NumFramesPerCallback if needed (e.g. NumFramesPerCallback is used in PixelStreamingAudioComponent, in GetDesiredNumSamplesToRenderPerCallback)
	FSoundGeneratorInitParams InitParams;
	InitParams.AudioDeviceID = DeviceId;
	InitParams.bIsPreviewSound = false;
	InitParams.InstanceID = uint64(this);
#if !UE_VERSION_OLDER_THAN(5, 3, 0)
	if (UAudioComponent* AudioComponent = SynthComponent->GetAudioComponent())
	{
		InitParams.AudioComponentId = AudioComponent->GetAudioComponentID();
	}
#endif
	InitParams.NumChannels = GetNumberOfChannels();
	InitParams.SampleRate = GetSampleRate();

	// Note: The sound generator will be used if available, otherwise the synth component's synth sound will be used for capturing audio
	SoundGenerator = SynthSound->CreateSoundGenerator(InitParams);
	if (SoundGenerator)
	{
		SoundGenerator->OnBeginGenerate();
	}
#endif

	// After starting the synth component, it will automatically play the sound to audio device, so we need to stop it to accumulate audio data into this sound wave
	StopSynthSound().Next([WeakThis = MakeWeakObjectPtr(this)](bool bSuccess)
	{
		if (bSuccess && WeakThis.IsValid())
		{
			// Indicate that the sound wave should start capturing audio data
			WeakThis->bCapturing = true;
		}
	});

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully started capturing for sound wave '%s'"), *GetName());
	return true;
}

void USynthBasedSoundWave::StopCapture_Implementation()
{
	bCapturing = false;

#if WITH_RUNTIMEAUDIOIMPORTER_SYNTH_SOUND_GENERATOR_SUPPORT
	if (SoundGenerator)
	{
		SoundGenerator->OnEndGenerate();
	}
#endif

	if (SynthComponent)
	{
		SynthComponent->Stop();
	}

	UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully stopped capturing for sound wave '%s'"), *GetName());
}

bool USynthBasedSoundWave::ToggleMute_Implementation(bool bMute)
{
	if (bMute)
	{
		if (IsCapturing())
		{
			StopCapture();
			return true;
		}
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to mute the sound wave '%s' as it is not capturing"), *GetName());
		return false;
	}
	else
	{
		if (!IsCapturing())
		{
			return StartCapture(LastDeviceIndex);
		}
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to unmute the sound wave '%s' as it is already capturing"), *GetName());
		return false;
	}
}

bool USynthBasedSoundWave::IsCapturing_Implementation() const
{
	return bCapturing;
}

void USynthBasedSoundWave::Tick(float DeltaTime)
{
	if (SynthComponent->IsPlaying())
	{
		StopSynthSound();
	}
	if (IsCapturing())
	{
		AudioTaskPipe->Launch(AudioTaskPipe->GetDebugName(), [WeakThis = MakeWeakObjectPtr(this)]() mutable
		{
			if (!WeakThis.IsValid())
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to populate audio data into the synth-based sound wave as it is not available"));
				return;
			}
			
			USynthSound* SynthSound = WeakThis->GetSynthSound();
			if (!SynthSound)
			{
				UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to populate audio data into the synth-based sound wave as the synth sound is not available"));
				return;
			}
			
			TArray<float> PCMData;

#if !UE_VERSION_OLDER_THAN(5, 0, 0)
			// Use the sound generator if available, otherwise use the synth component (same to FAsyncDecodeWorker behavior)
			if (WeakThis->SoundGenerator)
			{
				PCMData.SetNumUninitialized(WeakThis->NumSamplesToGeneratePerCallback);
				PCMData.SetNum(WeakThis->SoundGenerator->OnGenerateAudio(PCMData.GetData(), PCMData.Num()));
			}
			else
#endif
			{
				TArray<uint8> AudioData;
				SynthSound->OnGeneratePCMAudio(AudioData, WeakThis->NumSamplesToGeneratePerCallback);

				const Audio::EAudioMixerStreamDataFormat::Type GeneratedPCMDataFormat = SynthSound->GetGeneratedPCMDataFormat();

				if (GeneratedPCMDataFormat == Audio::EAudioMixerStreamDataFormat::Float)
				{
					PCMData = TArray<float>(reinterpret_cast<float*>(AudioData.GetData()), AudioData.Num() / sizeof(float));
				}
				else if (GeneratedPCMDataFormat == Audio::EAudioMixerStreamDataFormat::Int16)
				{
					// TODO: Confirm that it works correctly (only float format was tested)
					float* FloatPCMDataPtr = nullptr;
					const int64 NumOfSamples = AudioData.Num() / sizeof(int16);
					FRAW_RuntimeCodec::TranscodeRAWData(reinterpret_cast<int16*>(AudioData.GetData()), NumOfSamples, FloatPCMDataPtr);
					PCMData = TArray<float>(FloatPCMDataPtr, NumOfSamples);
					FMemory::Free(FloatPCMDataPtr);
				}
			}

			if (PCMData.Num() > 0)
			{
				// If all the values in the buffer are zero, it generally means that the generator doesn't have any audio to generate at the moment, e.g. in pixel streaming when the player is not talking, so we can skip the rest of the processing
				{
					const bool bCheckIfAllBufferZero = FRAIMemory::MemIsZero(PCMData.GetData(), PCMData.Num() * sizeof(float));
					if (bCheckIfAllBufferZero)
					{
						UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("All values in the buffer provided by the sound '%s' are zero, skipping the rest of the processing"), *WeakThis->GetName());
						return;
					}
				}

				UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Audio data has been generated by the sound '%s'"), *WeakThis->GetName());
				
				FDecodedAudioStruct DecodedAudioInfo;
				{
					FPCMStruct PCMInfo;
					{
						PCMInfo.PCMData = FRuntimeBulkDataBuffer<float>(PCMData);
						PCMInfo.PCMNumOfFrames = PCMData.Num() / SynthSound->NumChannels;
					}
					DecodedAudioInfo.PCMInfo = MoveTemp(PCMInfo);

					FSoundWaveBasicStruct SoundWaveBasicInfo;
					{
						SoundWaveBasicInfo.NumOfChannels = SynthSound->NumChannels;
						SoundWaveBasicInfo.SampleRate = SynthSound->GetSampleRateForCurrentPlatform();
						SoundWaveBasicInfo.Duration = static_cast<float>(DecodedAudioInfo.PCMInfo.PCMNumOfFrames) / SynthSound->GetSampleRateForCurrentPlatform();
					}
					DecodedAudioInfo.SoundWaveBasicInfo = MoveTemp(SoundWaveBasicInfo);
				}
				WeakThis->PopulateAudioDataFromDecodedInfo(MoveTemp(DecodedAudioInfo));
				UE_LOG(LogRuntimeAudioImporter, Verbose, TEXT("Audio data has been appended to the sound wave '%s'"), *WeakThis->GetName());
			}
		});
	}
}

ETickableTickType USynthBasedSoundWave::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

bool USynthBasedSoundWave::IsTickable() const
{
	return SynthComponent != nullptr;
}

bool USynthBasedSoundWave::IsTickableWhenPaused() const
{
	return true;
}

#if WITH_EDITOR
bool USynthBasedSoundWave::IsTickableInEditor() const
{
	return true;
}
#endif

USynthSound* USynthBasedSoundWave::GetSynthSound() const
{
	if (!SynthComponent)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to get the synth sound as the synth component is not set"));
		return nullptr;
	}

	UAudioComponent* AudioComponent = SynthComponent->GetAudioComponent();
	if (!AudioComponent)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to get the synth sound as the audio component is not available"));
		return nullptr;
	}

	USynthSound* SynthSound = Cast<USynthSound>(AudioComponent->Sound);
	if (!SynthSound)
	{
		UE_LOG(LogRuntimeAudioImporter, Error, TEXT("Unable to get the synth sound as the sound is not available"));
		return nullptr;
	}
	
	return SynthSound;
}

FAudioDevice* USynthBasedSoundWave::GetAudioDevice() const
{
	if (!SynthComponent)
	{
		return nullptr;
	}

	// If the synth component has a world, that means it was already registered with that world
	if (UWorld* World = SynthComponent->GetWorld())
	{
#if UE_VERSION_OLDER_THAN(4, 25, 0)
		return World->GetAudioDevice();
#else
		if (FAudioDeviceHandle AudioDeviceHandle = World->GetAudioDevice())
		{
			return AudioDeviceHandle.GetAudioDevice();
		}
#endif
	}

	// Otherwise, retrieve the audio component's audio device (probably from its owner)
	if (UAudioComponent* AudioComponent = SynthComponent->GetAudioComponent())
	{
		return AudioComponent->GetAudioDevice();
	}

	// No audio device
	return nullptr;
}

TFuture<bool> USynthBasedSoundWave::StopSynthSound()
{
	if (!IsInAudioThread())
	{
		TSharedRef<TPromise<bool>> Promise = MakeShareable(new TPromise<bool>());
		FAudioThread::RunCommandOnAudioThread([WeakThis = MakeWeakObjectPtr(this), Promise]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->StopSynthSound().Next([Promise](bool bSuccess)
				{
					Promise->SetValue(bSuccess);
				});
				return;
			}
			Promise->SetValue(false);
		});
		return Promise->GetFuture();
	}
	
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		const TArray<FActiveSound*>& ActiveSounds = AudioDevice->GetActiveSounds();
		for (FActiveSound* ActiveSoundPtr : ActiveSounds)
		{
			if (USynthSound* SynthSound = GetSynthSound())
			{
				if (ActiveSoundPtr->GetSound() == SynthSound)
				{
					AudioDevice->StopActiveSound(ActiveSoundPtr);
					break;
				}
			}
		}
	}

	return MakeFulfilledPromise<bool>(true).GetFuture();
}
