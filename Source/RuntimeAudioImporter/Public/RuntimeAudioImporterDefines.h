/**
* Replacing standard CPP memory methods (malloc, realloc, free) with engine ones
*/
#undef malloc
#undef free
#undef realloc
//#undef memset

#define malloc(Count)   FMemory::Malloc(Count)
#define free(Original)      FMemory::Free(Original)
#define realloc(Original, Count) FMemory::Realloc(Original, Count)
//#define memset(Dest, Char, Count) FMemory::Realloc(Dest, Char, Count)

#ifdef INCLUDE_DR_MP3
#include "ThirdParty/dr_mp3.h"
#endif

#ifdef INCLUDE_DR_WAV
#include "ThirdParty/dr_wav.h"
#endif

#ifdef INCLUDE_DR_FLAC
#include "ThirdParty/dr_flac.h"
#endif

#ifdef INCLUDE_STB_VORBIS
#include "ThirdParty/stb_vorbis.c"
#endif

#undef malloc
#undef free
#undef realloc
//#undef memset
