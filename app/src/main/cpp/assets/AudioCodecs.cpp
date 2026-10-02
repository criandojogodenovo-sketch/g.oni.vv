// assets/AudioCodecs.cpp — decoders OGG/MP3 (vendors; 0.8.11).
//
// stb_vorbis (public domain) e minimp3 (CC0): header-only, incluídos aqui
// num ÚNICO TU (o padrão da casa — um TU por vendor). O decode devolve PCM
// f32 interleaved + sampleRate/canais/frames. Toda a falha sai em err
// LEGÍVEL (nunca crash — bytes maliciosos param nos decoders com código
// de erro, não com exceções).
#define STB_VORBIS_IMPLEMENTATION
#include "vendor/stb_vorbis/stb_vorbis.c"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT   // f32 direto (sem conversão à mão)
#include "vendor/minimp3/minimp3.h"

#include "assets/GiFormat.h"

#include <cstring>

namespace vv {

bool decodeOgg(const u8* data, size_t len, u32& sampleRate, u16& channels,
               std::vector<f32>& pcm, u64& frames, std::string& err) {
    pcm.clear();
    frames = 0;
    sampleRate = 0;
    channels = 0;
    if (!data || len == 0) {
        err = "ogg vazio";
        return false;
    }
    // stb_vorbis: precisa de buffer MUTÁVEL (faz o seu próprio parsing)
    std::vector<u8> mut(data, data + len);
    int error = 0;
    stb_vorbis* st = stb_vorbis_open_memory(mut.data(), static_cast<int>(len),
                                            &error, nullptr);
    if (!st) {
        err = "ogg invalido (stb_vorbis erro " + std::to_string(error) + ")";
        return false;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(st);
    if (info.channels < 1 || info.channels > 2 ||
        info.sample_rate < 4000 || info.sample_rate > 192000) {
        stb_vorbis_close(st);
        err = "ogg com canais/rate fora do suporte";
        return false;
    }
    sampleRate = static_cast<u32>(info.sample_rate);
    channels = static_cast<u16>(info.channels);
    // get_frame_float devolve PONTEIROS POR CANAL (planar) — interleava
    {
        // reinicia e decodifica por frames (a API devolve planar)
        stb_vorbis_seek_start(st);
        u64 totalFrames = 0;
        std::vector<std::vector<f32>> planar(info.channels);
        while (true) {
            float** outputs = nullptr;
            const int n = stb_vorbis_get_frame_float(st, nullptr, &outputs);
            if (n <= 0) {
                break;
            }
            for (int c = 0; c < info.channels; ++c) {
                planar[static_cast<size_t>(c)].insert(
                    planar[static_cast<size_t>(c)].end(), outputs[c],
                    outputs[c] + n);
            }
            totalFrames += static_cast<u64>(n);
            if (totalFrames > (1ull << 30)) {   // guarda: >1 G frames = lixo
                stb_vorbis_close(st);
                err = "ogg demasiado longo";
                return false;
            }
        }
        stb_vorbis_close(st);
        frames = totalFrames;
        pcm.resize(static_cast<size_t>(totalFrames) * info.channels);
        for (u64 f = 0; f < totalFrames; ++f) {
            for (int c = 0; c < info.channels; ++c) {
                pcm[static_cast<size_t>(f) * info.channels +
                    static_cast<size_t>(c)] =
                    planar[static_cast<size_t>(c)][static_cast<size_t>(f)];
            }
        }
    }
    if (frames == 0) {
        err = "ogg sem frames decodificaveis";
        return false;
    }
    return true;
}

bool decodeMp3(const u8* data, size_t len, u32& sampleRate, u16& channels,
               std::vector<f32>& pcm, u64& frames, std::string& err) {
    pcm.clear();
    frames = 0;
    sampleRate = 0;
    channels = 0;
    if (!data || len == 0) {
        err = "mp3 vazio";
        return false;
    }
    static mp3dec_t dec;
    mp3dec_init(&dec);
    // minimp3: consome o buffer por FRAMES mp3 (até 1152 samples por
    // frame); o buffer tem de ser mutável e com PADDING no fim (o decoder
    // lê além do último frame p/ o header — o padrão do exemplo oficial).
    // A 1ª versão cortava em off+100<len — perdia o ÚLTIMO frame de um
    // ficheiro que termina exatamente no fim (a nota final do clip sumia);
    // agora o loop vai até len e o padding alimenta o parse sem produzir
    // frames fantasma (zeros não sincronizam).
    std::vector<u8> mut(len + 4096, 0);
    std::memcpy(mut.data(), data, len);
    size_t off = 0;
    u64 totalFrames = 0;
    mp3d_sample_t samp[MINIMP3_MAX_SAMPLES_PER_FRAME];
    while (off < len) {
        mp3dec_frame_info_t fi{};
        const int n = mp3dec_decode_frame(&dec, mut.data() + off,
                                          static_cast<int>(mut.size() - off),
                                          samp, &fi);
        if (fi.frame_bytes > 0) {
            off += static_cast<size_t>(fi.frame_bytes);
        } else {
            // sync: procura o próximo header
            ++off;
            continue;
        }
        if (n > 0) {
            if (channels == 0) {
                channels = static_cast<u16>(fi.channels);
                sampleRate = static_cast<u32>(fi.hz);
                if (channels < 1 || channels > 2 || sampleRate < 4000 ||
                    sampleRate > 192000) {
                    err = "mp3 com canais/rate fora do suporte";
                    return false;
                }
            }
            for (int i = 0; i < n * fi.channels; ++i) {
                pcm.push_back(samp[i]);
            }
            totalFrames += static_cast<u64>(n);
            if (totalFrames > (1ull << 30)) {
                err = "mp3 demasiado longo";
                return false;
            }
        }
    }
    frames = totalFrames;
    if (frames == 0 || channels == 0) {
        err = "mp3 sem frames decodificaveis";
        return false;
    }
    return true;
}

} // namespace vv
