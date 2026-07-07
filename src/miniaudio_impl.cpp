// RaythmDemo - miniaudio Implementation
// Compiles miniaudio and its optional Ogg Vorbis backend in exactly one translation unit.
// Author: RatherHard
// Date: 2026-07-03

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "stb_vorbis.c"
