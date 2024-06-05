// Georgy Treshchev 2024.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RuntimeVoiceActivityDetector.generated.h"

enum class ERuntimeVADMode : uint8;

namespace FVAD_RuntimeAudioImporter
{
	struct Fvad;
}

/**
 * Runtime Voice Activity Detector
 * Detects voice activity in audio data
 */
UCLASS(BlueprintType, Category = "Voice Activity Detector")
class RUNTIMEAUDIOIMPORTER_API URuntimeVoiceActivityDetector : public UObject
{
	GENERATED_BODY()

public:
	URuntimeVoiceActivityDetector();

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	/**
	 * Reinitializes a VAD (Voice Activity Detector) instance, clearing all state and resetting mode and sample rate to defaults
	 * 
	 * @return True if the VAD instance was successfully reset
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Voice Activity Detector Reset"), Category = "Voice Activity Detector")
	bool ResetVAD();

	/**
	 * Changes the operating ("aggressiveness") mode of a VAD (Voice Activity Detector) instance
	 * A more aggressive (higher mode) VAD is more restrictive in reporting speech
	 * In other words, the probability of detecting voice activity increases with a higher mode
	 * However, this also increases the rate of missed detections
	 * VeryAggressive is used by default
	 *
	 * @param Mode The VAD mode to set
	 * @return True if the VAD mode was successfully set
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Voice Activity Detector Mode"), Category = "Voice Activity Detector")
	bool SetVADMode(ERuntimeVADMode Mode);

	/**
	 * Calculates a VAD (Voice Activity Detection) decision for an audio frame
	 * 
	 * @param PCMData PCM audio data in 32-bit floating point interleaved format
	 * @param InSampleRate The sample rate of the provided PCM data
	 * @param NumOfChannels The number of channels in the provided PCM data
	 * @return True if the VAD decision was successfully calculated
	 */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Voice Activity Detector Process"), Category = "Voice Activity Detector")
	bool ProcessVAD(TArray<float> PCMData, UPARAM(DisplayName = "Sample Rate") int32 InSampleRate, int32 NumOfChannels);

protected:
	/** The sample rate at which the VAD is currently applied */
	int32 AppliedSampleRate;

#if WITH_RUNTIMEAUDIOIMPORTER_VAD_SUPPORT
	/** The VAD instance. Initialized in the constructor and destroyed in BeginDestroy */
	FVAD_RuntimeAudioImporter::Fvad* VADInstance;
#endif

	/**
	 * The accumulated PCM data used for VAD processing. Reset after each VAD decision
	 * VAD requires frames with a length of 10, 20, or 30 ms. Therefore, if the provided data does not match these lengths,
	 * we need to either accumulate data (if too short) or split data (if too long) to match the required frame length
	 */
	TArray<int16> AccumulatedPCMData;
};
