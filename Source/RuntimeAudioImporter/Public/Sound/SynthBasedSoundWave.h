// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "CapturableSoundWave.h"
#include "Delegates/Delegate.h"
#include "Containers/Queue.h"
#include "Components/SynthComponent.h"
#include "Tickable.h"
#include <atomic>

#include "SynthBasedSoundWave.generated.h"

// Sound generator support is only available in UE 5.0 and later
#define WITH_RUNTIMEAUDIOIMPORTER_SYNTH_SOUND_GENERATOR_SUPPORT !UE_VERSION_OLDER_THAN(5, 0, 0)


/**
 * Sound wave that captures audio data using a synth component.
 * This class is capable of working with any derived class of USynthComponent, but it has been thoroughly tested with UPixelStreamingAudioComponent.
 */
UCLASS(BlueprintType, Category = "Synth Based Sound Wave")
class RUNTIMEAUDIOIMPORTER_API USynthBasedSoundWave : public UCapturableSoundWave, public FTickableGameObject
{
	GENERATED_BODY()

public:
	USynthBasedSoundWave(const FObjectInitializer& ObjectInitializer);

	/**
	 * Create a new instance of the synth-based sound wave.
	 *
	 * @param InSynthComponent The synth component used to capture audio.
	 * @return A new instance of USynthBasedSoundWave.
	 */
	UFUNCTION(BlueprintCallable, Category = "Synth Based Sound Wave|Main")
	static USynthBasedSoundWave* CreateSynthBasedSoundWave(USynthComponent* InSynthComponent);

protected:
	//~ Begin UCapturableSoundWave Interface
	virtual bool StartCapture_Implementation(int32 DeviceId) override;
	virtual void StopCapture_Implementation() override;
	virtual bool ToggleMute_Implementation(bool bMute) override;
	virtual bool IsCapturing_Implementation() const override;
	//~ End UCapturableSoundWave Interface

protected:
	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override;
#if WITH_EDITOR
	virtual bool IsTickableInEditor() const override;
#endif
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USynthBasedSoundWave, STATGROUP_Tickables); }
	//~ End FTickableGameObject Interface

protected:
	/** Synth component used to capture audio. This will be used if the sound generator is not available (same to FAsyncDecodeWorker behavior). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Synth Based Sound Wave")
	USynthComponent* SynthComponent;

#if WITH_RUNTIMEAUDIOIMPORTER_SYNTH_SOUND_GENERATOR_SUPPORT
	/** Sound generator used to generate audio data. This will be used over the synth component's synth sound for capturing audio (same to FAsyncDecodeWorker behavior). */
	ISoundGeneratorPtr SoundGenerator;
#endif

	/**
	 * Get the synth sound from the synth component
	 *
	 * @return The synth sound associated with the synth component.
	 */
	USynthSound* GetSynthSound() const;

	/**
	 * Get the audio device from the synth component
	 *
	 * @return The audio device associated with the synth component.
	 */
	FAudioDevice* GetAudioDevice() const;

	/**
	 * Stop the synth sound
	 *
	 * @return Future that resolves to true if the synth sound was stopped successfully.
	 */
	TFuture<bool> StopSynthSound();

	/** Whether the sound wave is currently capturing audio */
	std::atomic<bool> bCapturing{ false };
};
