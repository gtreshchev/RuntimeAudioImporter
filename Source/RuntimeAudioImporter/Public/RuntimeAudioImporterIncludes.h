// Georgy Treshchev 2022.

/**
* Replacing standard CPP memory methods (malloc, free, realloc, memset) with engine ones
*/
#undef malloc
#undef free
#undef realloc
#undef memset

#define malloc(Count)				FMemory::Malloc(Count)
#define free(Original)				FMemory::Free(Original)
#define realloc(Original, Count)	FMemory::Realloc(Original, Count)
#define memset(Dest, Char, Count)	FMemory::Memset(Dest, Char, Count)

#ifdef INCLUDE_MP3
#include "ThirdParty/dr_mp3.h"
#endif

#ifdef INCLUDE_WAV
#include "ThirdParty/dr_wav.h"
#endif

#ifdef INCLUDE_FLAC
#include "ThirdParty/dr_flac.h"
#endif

#ifdef INCLUDE_VORBIS
#include "ThirdParty/stb_vorbis.c"
#endif

#undef malloc
#undef free
#undef realloc
#undef memset
