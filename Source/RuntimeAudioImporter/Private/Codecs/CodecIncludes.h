// Georgy Treshchev 2023.

/**
* Replacing C dynamic memory allocation functions (calloc, malloc, free, realloc, memset, memcpy) with FMemory ones
*/
#undef calloc
#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy

#define calloc(Count, Size)			[](){ void* MemPtr = FMemory::Malloc(Count * Size); FMemory::Memset(MemPtr, 0, Count * Size); return MemPtr; }()
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
#pragma pack(push, 8)
#include "vorbis/vorbisenc.h"
#pragma pack(pop)
#endif
#endif

#undef calloc
#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy

#ifdef INCLUDE_VORBIS
#include "VorbisAudioInfo.h"
#include "Interfaces/IAudioFormat.h"
#ifndef WITH_OGGVORBIS
#define WITH_OGGVORBIS 0
#endif
#endif