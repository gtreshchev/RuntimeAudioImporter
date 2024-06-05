// Georgy Treshchev 2024.

#pragma once

#if WITH_RUNTIMEAUDIOIMPORTER_VAD_SUPPORT

/**
 * Replacing C dynamic memory management functions
 * (calloc, malloc, free, realloc, memset, memcpy) with FMemory ones
 */
#undef calloc
#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy

#define calloc(Count, Size) [&]() { void* MemPtr = FMemory::Malloc(Count * Size); if (MemPtr) { FMemory::Memset(MemPtr, 0, Count * Size); } return MemPtr; }()
#define malloc(Count) FMemory::Malloc(Count)
#define free(Original) FMemory::Free(Original)
#define realloc(Original, Count) FMemory::Realloc(Original, Count)
#define memset(Dest, Char, Count) FMemory::Memset(Dest, Char, Count)
#define memcpy(Dest, Src, Count) FMemory::Memcpy(Dest, Src, Count)

THIRD_PARTY_INCLUDES_START

namespace FVAD_RuntimeAudioImporter
{
#include "fvad.h"
#include "fvad.c"
#include "vad/vad_core.c"
#include "vad/vad_sp.c"
#include "vad/vad_gmm.c"
#include "vad/vad_filterbank.c"
#include "signal_processing/division_operations.c"
#include "signal_processing/energy.c"
#include "signal_processing/resample_48khz.c"
#include "signal_processing/spl_inl.c"
#include "signal_processing/get_scaling_square.c"
#include "signal_processing/resample_fractional.c"
#include "signal_processing/resample_by_2_internal.c"
}

THIRD_PARTY_INCLUDES_END

#undef calloc
#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy

#endif