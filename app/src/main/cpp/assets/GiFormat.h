#pragma once
// assets/GiFormat.h — CONTENTOR .gi + CODECS (0.8.11).
//
// .gi = o clip da engine: header comum de 32 B (a MESMA convenção do
// GOwnFormats: magic/version/endianMark/align/payloadSize/checksum
// FNV-1a/reserved) + payload:
//     codec u8 (0=ADPCM, 1=OGG-passthrough, 2=MP3-passthrough)
//     sampleRate u32 · channels u16 · frames u64
//     nameLen u8 + name
//     DADOS: ADPCM = estados IMA por canal + nibbles (4:1 vs PCM16);
//            OGG/MP3 = os bytes do ficheiro original (passthrough — o
//            decode corre no load: stb_vorbis/minimp3, header-only)
//
// DECISÃO (o prompt): ADPCM próprio TRIVIAL (sem dependências); OGG/MP3
// em passthrough porque re-encodar para ADPCM perderia mais qualidade do
// que ganha em tamanho (já comprimidos); decoders vendors: minimp3 (CC0),
// stb_vorbis (public domain) — NADA pesado.
//
// O CLIP DECODIFICADO (runtime): PCM f32 interleaved + picos de waveform
// pré-computados — o mixer e o workspace consomem Isto, nunca o payload.
//
// GL-free / Android-free → CI Linux.
#include <string>
#include <vector>

#include "core/Types.h"

namespace vv {

enum class GiCodec : u8 {
    Adpcm = 0,   // IMA-ADPCM 4:1 (de WAV/mic)
    Ogg = 1,     // passthrough (decode: stb_vorbis)
    Mp3 = 2,     // passthrough (decode: minimp3)
};

// ---- IMA-ADPCM (o codec próprio — codifica/decodifica EXATO 4:1) ---------
// Entrada/saída: PCM16 INTERLEAVED por canais. O payload guarda os
// estados INICIAIS (predictor+index) por canal — a 1ª AMOSTRA de cada
// canal É o predictor (vai crua, sem quantização) e os nibbles codificam
// AS RESTANTES (a 1ª versão codificava TAMBÉM as primeiras → o decode
// gerava canais amostras A MAIS e as primeiras saíam dobradas; apanhado
// pelo round-trip do CI antes do push). O decode é determinístico
// (o teste de round-trip aferia energia + erro máximo).
bool imaEncode(const i16* pcm, u64 frames, u16 channels, std::vector<u8>& out);
// expectedFrames: o nº de frames QUE O CONTENTOR anuncia (0 = desconhecido
// → decodifica todos os nibbles). O decoder para EXATAMENTE em
// expectedFrames*channels amostras — o último byte pode ter o nibble alto
// a zero (padding do encoder quando o total é ímpar).
bool imaDecode(const u8* data, size_t bytes, u16 channels, u64 expectedFrames,
               std::vector<i16>& out, u64& frames);

// ---- WAV (PCM16) → amostras interleaved -----------------------------------
// Só o subset da engine: PCM 16-bit mono/stereo. O resto → erro legível.
bool parseWavPcm16(const u8* bytes, size_t len, u32& sampleRate,
                   u16& channels, std::vector<i16>& out, std::string& err);

// ---- escrita/leitura do contentor ------------------------------------------
struct GiWriteIn {
    GiCodec codec = GiCodec::Adpcm;
    u32 sampleRate = 44100;
    u16 channels = 1;
    u64 frames = 0;                 // frames de áudio (samples POR canal)
    std::string name;               // nome curto do clip
    // ADPCM: pcm16 interleaved (frames*channels amostras); passthrough:
    // os bytes do ficheiro original
    const i16* pcm16 = nullptr;
    const u8* raw = nullptr;
    size_t rawSize = 0;
};
bool writeGi(const GiWriteIn& in, std::vector<u8>& out, std::string& err);

// o clip DECODIFICADO (o que o runtime/mixer/workspace consomem)
struct GiClip {
    std::string name;
    u32 sampleRate = 0;
    u16 channels = 0;
    u64 frames = 0;
    GiCodec codec = GiCodec::Adpcm;
    std::vector<f32> pcm;    // interleaved [-1,1] (frames*channels)
    u64 sourceBytes = 0;     // p/ o rácio do import (bytes da fonte)

    f32 duration() const {
        return sampleRate > 0 ? static_cast<f32>(
                   static_cast<f64>(frames) / sampleRate) : 0.0f;
    }
    bool ok() const { return sampleRate > 0 && channels > 0 && frames > 0; }

    // picos de waveform (N barras, 0..1) — o workspace desenha isto
    void peaks(u32 bars, std::vector<f32>& out) const;
};

// lê + VALIDA (header/checksum) + DECODIFICA (ADPCM direto; OGG/MP3 pelos
// decoders vendors) → GiClip. Erro → false + err LEGÍVEL (nunca crash).
bool readGi(const u8* bytes, size_t len, GiClip& out, std::string& err);

// ---- import: fonte (wav/ogg/mp3) → .gi -------------------------------------
struct GiImportOut {
    std::vector<u8> gi;       // o ficheiro .gi completo
    GiCodec codec = GiCodec::Adpcm;
    u32 sampleRate = 0;
    u16 channels = 0;
    u64 frames = 0;
    u64 sourceBytes = 0;      // p/ o rácio
    f32 duration = 0.0f;
};
// converte uma fonte de áUDIO (bytes + nome) para .gi. WAV → ADPCM;
// OGG/MP3 → passthrough (os bytes entram como estão). O rácio sai no log
// do chamador (audio: import <nome> codec=<adpcm|ogg|mp3> ratio=<x>).
bool importAudioToGi(const u8* bytes, size_t len, const std::string& name,
                     GiImportOut& out, std::string& err);

const char* giCodecName(GiCodec c);   // "adpcm" | "ogg" | "mp3"

} // namespace vv
