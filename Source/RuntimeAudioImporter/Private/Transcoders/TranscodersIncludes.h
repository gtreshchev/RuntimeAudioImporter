// Georgy Treshchev 2022.

/**
* Replacing the C dynamic memory allocation functions (malloc, free, realloc, memset, memcpy) with FMemory ones
*/
#undef malloc
#undef free
#undef realloc
#undef memset

#define malloc(Count)				FMemory::Malloc(Count)
#define free(Original)				FMemory::Free(Original)
#define realloc(Original, Count)	FMemory::Realloc(Original, Count)
#define memset(Dest, Char, Count)	FMemory::Memset(Dest, Char, Count)
#define memcpy(Dest, Src, Count)	FMemory::Memcpy(Dest, Src, Count)

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
#if PLATFORM_SUPPORTS_VORBIS_CODEC
#include "vorbis/vorbisenc.h"
#endif
#include "ThirdParty/stb_vorbis.c"
#endif

#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy
