// Georgy Treshchev 2022.

/**
* Replacing standard CPP memory methods (malloc, free, realloc, memset, memcpy) with engine ones
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
#if PLATFORM_SUPPORTS_VORBIS_CODEC
#include "vorbis/vorbisenc.h"
#endif
#include "ThirdParty/stb_vorbis.c"
#endif

#ifdef INCLUDE_OPUS

#if !defined(WITH_OPUS)
	#define WITH_OPUS (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_UNIX)
#endif

#if WITH_OPUS
THIRD_PARTY_INCLUDES_START
#include "opus_multistream.h"
THIRD_PARTY_INCLUDES_END
#endif

#endif

#undef malloc
#undef free
#undef realloc
#undef memset
