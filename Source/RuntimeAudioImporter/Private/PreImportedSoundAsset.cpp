// Georgy Treshchev 2024.

#include "PreImportedSoundAsset.h"


UPreImportedSoundAsset::UPreImportedSoundAsset()
	: AudioFormat(ERuntimeAudioFormat::Mp3)
#if WITH_EDITORONLY_DATA
	  , NumberOfChannels(0), SampleRate(0)
#endif
{
}
