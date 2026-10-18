#pragma once
// vendor/stb_vorbis/stb_vorbis.h — declaração p/ inclusão única do .c
// (stb_vorbis.c é a implementação completa; o TU que a inclui define
// STB_VORBIS_IMPLEMENTATION — o padrão da casa é UM TU por vendor).
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
