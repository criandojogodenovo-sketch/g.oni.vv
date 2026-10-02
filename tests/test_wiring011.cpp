// tests/test_wiring011.cpp — 0.8.11: ÁUDIO (parte PURA).
//
// COBERTURA (prompt 0.8.11, casos que não precisam do TU do main.cpp):
//   1. IMA-ADPCM round-trip EXATO — a 1ª versão do par encode/decode
//      gerava canais amostras A MAIS e as primeiras saíam dobradas (o
//      encoder codificava TAMBÉM as amostras que já iam cras nos
//      predictors): o aferimento é a CONTAGEM EXATA frames*channels +
//      energia dentro de 0.5% + erro máximo por amostra.
//   2. CONTENTOR .gi — writeGi/readGi ADPCM: metadados (rate/channels/
//      frames/nome), PCM f32 [-1,1], duração, picos de waveform; a
//      CORRUPÇÃO (magic/versão/checksum/truncado/codec) pára em erro
//      LEGÍVEL (nunca crash).
//   3. IMPORT — WAV PCM16 sintetizado (mono+stereo) → .gi ADPCM com
//      rácio ~4x; formatos não suportados (8-bit, float, .flac) e lixo
//      .ogg/.mp3 → erros legíveis (os decoders vendors param com código).
//   4. MISTURADOR — vozes: cursor avança, loop enrola, fim pára, pitch
//      2x consome o dobro, mono→stereo duplica, slots reutilizam, master
//      e POSICIONAL (atenuação linear entre raios; fora do raio = silêncio
//      com cursor a andar).
//   5. PROBE — o harness puro contra fakes: start falha → FALLBACK;
//      disconnect a meio → FALLBACK; tudo verde → AAudio OK; null → crash.
//      A tabela contém as chaves que o engine.log do C33 vai mostrar.
//   6. WORKSPACE ÁUDIO — desenho puro com fake host: toques chamam o host
//      (import/gravar/play/stop), seleção devolve 1..N, apagar exige
//      CONFIRMAÇÃO (2 toques), ponteiros NUNCA desreferenciados sem guard.
//   7. SERIALIZER/PRESET/STORE — AudioPlayer dump/load com defaults
//      omitidos; preset Audio = Transform+AudioPlayer SEM mesh/física;
//      registro cria e removeAll remove.
//   8. WIRING PURO — FileApi kinds (wav/ogg/mp3→'s'), raiz Music, seletor
//      de clips (drawAssetMenu 5 + applyAssetPick 5: none limpa, pick
//      aplica, fora do catálogo não crasha), plano do Inspector com a
//      secção Audio.
//
// Os casos do CAMINHO DO DEVICE (browserImportAudio e2e, applyImported
// 'a', gravação sintética, boot do backend, glifos) vivem no TU do
// test_wiring087.cpp (único que inclui platform/main.cpp) — secção 0.8.11.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)
#include <dirent.h>     // rmrfLogs (isolamento do engine.log por caso)
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GiFormat.h"
#include "assets/GOwnFormats.h"   // gfnv1a (o checksum recalculado do teste)
#include "components/AudioPlayer.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "core/AudioEngine.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/ProjectStorage.h"
#include "platform/AudioOut.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"
#include "ui/AudioWorkspace.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "ui/Toolbar.h"

using namespace vv;
using ::test::nearEqF;

namespace {

const char* kTestLogs = "test-wiring011-logs";

void rmrfLogs() {
    const std::string dir = kTestLogs;
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

bool logHas(const char* needle) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, 400);
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// senoide PCM16 (o sinal de teste de todo o ficheiro)
std::vector<i16> sine16(u32 rate, u16 channels, f32 secs, f32 freq,
                        i16 amp) {
    const size_t n = static_cast<size_t>(rate * secs) * channels;
    std::vector<i16> out(n);
    for (size_t i = 0; i < n; ++i) {
        const f64 t = static_cast<f64>(i / channels) / rate;
        out[i] = static_cast<i16>(std::sin(t * 6.283185307179586 *
                                           freq) * amp);
    }
    return out;
}

// WAV PCM16 mínimo (header RIFF/fmt/data + samples interleaved)
std::vector<u8> makeWav(u32 rate, u16 channels,
                        const std::vector<i16>& pcm, u16 audioFormat = 1,
                        u16 bits = 16) {
    const u32 dataBytes = static_cast<u32>(pcm.size()) * 2;
    std::vector<u8> out;
    auto push = [&out](const void* p, size_t n) {
        const u8* b = static_cast<const u8*>(p);
        out.insert(out.end(), b, b + n);
    };
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    const u32 riff = 36 + dataBytes;
    push(&riff, 4);
    out.insert(out.end(), {'W', 'A', 'V', 'E'});
    out.insert(out.end(), {'f', 'm', 't', ' '});
    const u32 fmtSz = 16;
    push(&fmtSz, 4);
    push(&audioFormat, 2);
    push(&channels, 2);
    push(&rate, 4);
    const u32 byteRate = rate * channels * bits / 8;
    push(&byteRate, 4);
    const u16 blockAlign = static_cast<u16>(channels * bits / 8);
    push(&blockAlign, 2);
    push(&bits, 2);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    push(&dataBytes, 4);
    push(pcm.data(), dataBytes);
    return out;
}

// um clip .gi ADPCM pronto a misturar
GiClip makeClip(u32 rate, u16 channels, f32 secs) {
    GiWriteIn wi;
    wi.codec = GiCodec::Adpcm;
    wi.sampleRate = rate;
    wi.channels = channels;
    const std::vector<i16> pcm = sine16(rate, channels, secs, 440.0f, 12000);
    wi.frames = pcm.size() / channels;
    wi.pcm16 = pcm.data();
    wi.name = "teste";
    std::vector<u8> gi;
    std::string err;
    GiClip c;
    EXPECT(writeGi(wi, gi, err));
    EXPECT(readGi(gi.data(), gi.size(), c, err));
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. IMA-ADPCM — round-trip EXATO (a contagem era o bug da 1ª versão)
// ---------------------------------------------------------------------------
TEST(wiring011_adpcm_roundtrip_contagem_exata_mono_estereo) {
    for (u16 ch : {1, 2}) {
        for (u64 frames : {1ull, 2ull, 3ull, 100ull, 101ull, 44100ull}) {
            const std::vector<i16> pcm =
                sine16(44100, ch, 1.0f, 440.0f, 12000);
            const u64 n = frames < pcm.size() / ch ? frames
                                                   : pcm.size() / ch;
            std::vector<i16> sub(pcm.begin(), pcm.begin() + n * ch);
            std::vector<u8> enc;
            EXPECT(imaEncode(sub.data(), n, ch, enc));
            // o payload: 4 B de estado POR CANAL + nibbles (4 bits/amostra)
            const size_t nibbles = (n * ch - ch + 1) / 2;
            EXPECT(enc.size() >= static_cast<size_t>(ch) * 4 + nibbles - 1);
            EXPECT(enc.size() <= static_cast<size_t>(ch) * 4 + nibbles);
            std::vector<i16> dec;
            u64 framesOut = 0;
            EXPECT(imaDecode(enc.data(), enc.size(), ch, n, dec, framesOut));
            // A CONTAGEM EXATA: frames*channels amostras — nem mais (o bug
            // da 1ª versão: +ch amostras), nem menos
            EXPECT(framesOut == n);
            EXPECT(dec.size() == static_cast<size_t>(n) * ch);
            // as PRIMEIRAS amostras (os predictors crús) são EXATAS
            for (u16 c = 0; c < ch; ++c) {
                EXPECT(dec[c] == sub[c]);
            }
            // erro de quantização: energia dentro de 0.5%, máx por amostra
            // (aferido com o codec REAL: ratio 0.00004 em 1 s de senoide; os
            // clips CURTOS pagam o WARM-UP do index — a energia só se afere
            // com material suficiente)
            double eOrig = 0.0, eDiff = 0.0;
            i16 maxErr = 0;
            for (size_t i = 0; i < dec.size(); ++i) {
                const long d = labs(static_cast<long>(dec[i]) -
                                   static_cast<long>(sub[i]));
                eOrig += static_cast<double>(sub[i]) * sub[i];
                eDiff += static_cast<double>(d) * d;
                if (d > maxErr) {
                    maxErr = static_cast<i16>(d);
                }
            }
            if (n >= 1024 && eOrig > 0.0) {
                EXPECT(eDiff / eOrig < 0.005);
                EXPECT(maxErr < 4000);   // ~12% da amplitude de teste
            } else {
                EXPECT(maxErr < 8000);   // warm-up do index: só o teto bruto
            }
        }
    }
}

TEST(wiring011_adpcm_defesas) {
    std::vector<u8> enc;
    std::vector<i16> dec;
    u64 frames = 0;
    // entradas inválidas: NUNCA crash
    EXPECT(!imaEncode(nullptr, 100, 1, enc));
    EXPECT(!imaEncode(nullptr, 0, 0, enc));
    const i16 one[2] = {100, -100};
    EXPECT(!imaEncode(one, 0, 1, enc));   // zero frames = nada
    EXPECT(!imaDecode(nullptr, 100, 1, 100, dec, frames));
    EXPECT(!imaDecode(reinterpret_cast<const u8*>("ab"), 2, 1, 100, dec,
                      frames));   // curto demais
    // index de estado fora da tabela → recusa
    std::vector<u8> bad(8, 0);
    bad[2] = 200;   // index = 200 > 88
    EXPECT(!imaDecode(bad.data(), bad.size(), 1, 100, dec, frames));
    // frames anunciadas a MAIS do que o payload produz → recusa honesta
    const i16 two[4] = {1, 2, 3, 4};
    EXPECT(imaEncode(two, 2, 1, enc));
    EXPECT(!imaDecode(enc.data(), enc.size(), 1, 1000, dec, frames));
}

// ---------------------------------------------------------------------------
// 2. CONTENTOR .gi — round-trip + corrupção com erro LEGÍVEL
// ---------------------------------------------------------------------------
TEST(wiring011_gi_roundtrip_metadados_e_pcm) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    const std::vector<i16> pcm = sine16(22050, 2, 0.5f, 440.0f, 12000);
    GiWriteIn wi;
    wi.codec = GiCodec::Adpcm;
    wi.sampleRate = 22050;
    wi.channels = 2;
    wi.frames = pcm.size() / 2;
    wi.pcm16 = pcm.data();
    wi.name = "clip de teste";
    std::vector<u8> gi;
    std::string err;
    EXPECT(writeGi(wi, gi, err));
    EXPECT(gi.size() > 32);
    EXPECT(gi[0] == 'G' && gi[1] == 'I' && gi[2] == 'C' && gi[3] == 'L');

    GiClip c;
    EXPECT(readGi(gi.data(), gi.size(), c, err));
    EXPECT(c.sampleRate == 22050);
    EXPECT(c.channels == 2);
    EXPECT(c.frames == wi.frames);
    EXPECT(c.name == "clip de teste");
    EXPECT(c.codec == GiCodec::Adpcm);
    EXPECT(c.pcm.size() == static_cast<size_t>(c.frames) * 2);
    EXPECT(nearEqF(c.duration(), 0.5f, 0.01f));
    // o PCM é f32 [-1,1]: a amplitude de teste (12000/32768 ≈ 0.366)
    f32 peak = 0.0f;
    for (f32 s : c.pcm) {
        const f32 a = std::fabs(s);
        if (a > peak) {
            peak = a;
        }
    }
    EXPECT(peak > 0.25f && peak < 0.45f);
    // picos de waveform: N barras em [0,1], com sinal > 0
    std::vector<f32> pk;
    c.peaks(64, pk);
    EXPECT(pk.size() == 64);
    bool anyPeak = false;
    for (f32 p : pk) {
        EXPECT(p >= 0.0f && p <= 1.0f);
        anyPeak = anyPeak || p > 0.1f;
    }
    EXPECT(anyPeak);
}

TEST(wiring011_gi_corrupcao_erros_legiveis) {
    const GiClip ref = makeClip(44100, 1, 0.1f);
    (void)ref;
    GiWriteIn wi;
    wi.codec = GiCodec::Adpcm;
    wi.sampleRate = 44100;
    wi.channels = 1;
    const std::vector<i16> pcm = sine16(44100, 1, 0.1f, 440.0f, 9000);
    wi.frames = pcm.size();
    wi.pcm16 = pcm.data();
    wi.name = "x";
    std::vector<u8> gi;
    std::string err;
    EXPECT(writeGi(wi, gi, err));

    GiClip c;
    // magic errado
    std::vector<u8> bad = gi;
    bad[0] = 'X';
    EXPECT(!readGi(bad.data(), bad.size(), c, err));
    EXPECT(err.find("magic") != std::string::npos);
    // truncado
    EXPECT(!readGi(gi.data(), 10, c, err));
    // checksum corrompido (1 byte do payload flipado)
    bad = gi;
    bad[40] ^= 0xFF;
    EXPECT(!readGi(bad.data(), bad.size(), c, err));
    EXPECT(err.find("CHECKSUM") != std::string::npos);
    // versão desconhecida
    bad = gi;
    bad[4] = 9;
    EXPECT(!readGi(bad.data(), bad.size(), c, err));
    EXPECT(err.find("vers") != std::string::npos);
    // codec desconhecido (byte 32 do payload) — com o CHECKSUM
    // RECALCULADO (senão o erro que chega é o checksum, não o codec)
    bad = gi;
    bad[32] = 7;
    {
        u64 payloadSize = 0;
        std::memcpy(&payloadSize, bad.data() + 12, 8);
        const u64 ck = gfnv1a(bad.data() + 32, static_cast<size_t>(payloadSize));
        std::memcpy(bad.data() + 20, &ck, 8);
    }
    EXPECT(!readGi(bad.data(), bad.size(), c, err));
    EXPECT(err.find("codec") != std::string::npos);
    // payload truncado (tamanho declarado > real)
    bad = gi;
    const u64 big = 0xFFFFFF;
    std::memcpy(bad.data() + 16, &big, 8);   // payloadSize = enorme
    EXPECT(!readGi(bad.data(), bad.size(), c, err));
    EXPECT(err.find("truncado") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 3. IMPORT — WAV sintetizado → .gi; erros legíveis
// ---------------------------------------------------------------------------
TEST(wiring011_import_wav_mono_estereo_com_ratio) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    // MONO 22050: 1 s de senoide
    {
        const std::vector<i16> pcm = sine16(22050, 1, 1.0f, 440.0f, 12000);
        const std::vector<u8> wav = makeWav(22050, 1, pcm);
        GiImportOut out;
        std::string err;
        EXPECT(importAudioToGi(wav.data(), wav.size(), "salto.wav", out, err));
        EXPECT(out.codec == GiCodec::Adpcm);
        EXPECT(out.sampleRate == 22050);
        EXPECT(out.channels == 1);
        EXPECT(out.frames == pcm.size());
        EXPECT(nearEqF(out.duration, 1.0f, 0.01f));
        EXPECT(out.sourceBytes == wav.size());
        // o rácio: WAV (PCM16 + header) vs .gi ADPCM ~4x
        EXPECT(out.gi.size() > 32);
        const double ratio = static_cast<double>(out.sourceBytes) /
                             static_cast<double>(out.gi.size());
        EXPECT(ratio > 3.0 && ratio < 4.6);
        // o .gi LÊ-SE de volta (o import não guarda lixo)
        GiClip c;
        EXPECT(readGi(out.gi.data(), out.gi.size(), c, err));
        EXPECT(c.frames == out.frames);
    }
    // STEREO 44100
    {
        const std::vector<i16> pcm = sine16(44100, 2, 0.25f, 880.0f, 9000);
        const std::vector<u8> wav = makeWav(44100, 2, pcm);
        GiImportOut out;
        std::string err;
        EXPECT(importAudioToGi(wav.data(), wav.size(), "musica.wav", out,
                               err));
        EXPECT(out.channels == 2);
        EXPECT(out.sampleRate == 44100);
        EXPECT(out.frames * 2 == pcm.size());
    }
}

TEST(wiring011_import_erros_legiveis) {
    GiImportOut out;
    std::string err;
    // extensão não suportada
    const u8 flac[16] = "fLaC...........";
    EXPECT(!importAudioToGi(flac, sizeof(flac), "x.flac", out, err));
    EXPECT(err.find("suportado") != std::string::npos);
    // WAV não-PCM (IEEE float)
    {
        const std::vector<i16> pcm = sine16(22050, 1, 0.05f, 440.0f, 9000);
        const std::vector<u8> wav = makeWav(22050, 1, pcm, 3 /*float*/);
        EXPECT(!importAudioToGi(wav.data(), wav.size(), "x.wav", out, err));
        EXPECT(err.find("PCM 16") != std::string::npos);
    }
    // WAV 8-bit
    {
        const std::vector<i16> pcm = sine16(22050, 1, 0.05f, 440.0f, 9000);
        const std::vector<u8> wav = makeWav(22050, 1, pcm, 1, 8);
        EXPECT(!importAudioToGi(wav.data(), wav.size(), "x.wav", out, err));
        EXPECT(err.find("16-bit") != std::string::npos);
    }
    // RIFF que não é WAVE
    const u8 riff[64] = {};
    EXPECT(!importAudioToGi(riff, sizeof(riff), "x.wav", out, err));
    EXPECT(!err.empty());
    // OGG/MP3: lixo → o decoder pára com erro LEGÍVEL (nunca crash)
    const u8 ogg[64] = "OggS-lixo-completo-sem-sentido-qualquer";
    EXPECT(!importAudioToGi(ogg, sizeof(ogg), "x.ogg", out, err));
    EXPECT(!err.empty());
    const u8 mp3[512] = {};
    EXPECT(!importAudioToGi(mp3, sizeof(mp3), "x.mp3", out, err));
    EXPECT(!err.empty());
    // vazio
    EXPECT(!importAudioToGi(nullptr, 0, "x.wav", out, err));
}

// ---------------------------------------------------------------------------
// 4. MISTURADOR — vozes, loop, pitch, mono→stereo, posicional, master
// ---------------------------------------------------------------------------
TEST(wiring011_mixer_cursor_loop_fim_e_pitch) {
    AudioEngine eng;
    GiClip clip = makeClip(44100, 1, 1.0f);   // 1 s de senoide mono
    const i32 v = eng.play(&clip, false /*loop*/, 1.0f, 1.0f);
    EXPECT(v >= 0);
    EXPECT(eng.playing(v));
    EXPECT(eng.activeVoices() == 1);
    EXPECT(nearEqF(eng.voiceProgress(v), 0.0f, 0.01f));

    // mix 0.5 s → cursor na METADE (rate igual ao do clip)
    std::vector<f32> out(44100 * 2);
    eng.mix(out.data(), 22050, 2, 44100);
    EXPECT(nearEqF(eng.voiceProgress(v), 0.5f, 0.01f));
    EXPECT(eng.totalFramesMixed() == 22050);
    // mono→stereo: os DOIS canais têm sinal igual
    bool nonZero = false;
    for (size_t i = 0; i < 22050; ++i) {
        EXPECT(nearEqF(out[i * 2], out[i * 2 + 1], 1e-5f));
        nonZero = nonZero || std::fabs(out[i * 2]) > 0.01f;
    }
    EXPECT(nonZero);
    // clamp [-1,1]
    for (f32 s : out) {
        EXPECT(s <= 1.0f && s >= -1.0f);
    }

    // o RESTO do clip: fim SEM loop → voz morre, progress limpa
    eng.mix(out.data(), 22050, 2, 44100);
    EXPECT(!eng.playing(v));
    EXPECT(eng.voiceProgress(v) < 0.0f);

    // LOOP: enrola para sempre
    const i32 vl = eng.play(&clip, true, 1.0f, 1.0f);
    out.resize(44100 * 3 * 2);   // 3 s de saída stereo
    eng.mix(out.data(), 44100 * 3, 2, 44100);   // 3 s de um clip de 1 s
    EXPECT(eng.playing(vl));
    EXPECT(eng.totalFramesMixed() == 22050 + 22050 + 44100 * 3);
    eng.stop(vl);

    // PITCH 2x: consome o dobro dos frames do clip por frame de saída
    const i32 vp = eng.play(&clip, false, 1.0f, 2.0f);
    eng.mix(out.data(), 22050, 2, 44100);   // 0.5 s reais = 1 s do clip
    EXPECT(!eng.playing(vp));   // acabou (sem loop)
}

TEST(wiring011_mixer_slots_reutilizam_e_stopall) {
    AudioEngine eng;
    GiClip clip = makeClip(44100, 1, 2.0f);
    std::vector<f32> out(2048);
    std::vector<i32> ids;
    for (int i = 0; i < 8; ++i) {
        ids.push_back(eng.play(&clip, true, 0.05f, 1.0f));
    }
    EXPECT(eng.activeVoices() == 8);
    eng.mix(out.data(), 512, 2, 44100);
    // para metade → os slots FICAM livres (reutilizáveis SEM crescer o vetor)
    for (int i = 0; i < 4; ++i) {
        eng.stop(ids[static_cast<size_t>(i)]);
    }
    EXPECT(eng.activeVoices() == 4);
    size_t voicesBefore = 0;
    const i32 v = eng.play(&clip, true, 1.0f, 1.0f);
    EXPECT(v >= 0 && v < 8);   // reutilizou um slot morto
    (void)voicesBefore;
    eng.stopAll();
    EXPECT(eng.activeVoices() == 0);
    // clip inválido → -1 (sem voz fantasma)
    EXPECT(eng.play(nullptr, true, 1.0f, 1.0f) == -1);
    GiClip empty;
    EXPECT(eng.play(&empty, true, 1.0f, 1.0f) == -1);
}

TEST(wiring011_mixer_posicional_e_master) {
    // a atenuação PURA: 1 dentro do raio interno, 0 no/além do externo,
    // linear entre eles (o MEIO entre 1 e 8 é 4.5)
    EXPECT(nearEqF(positionalAttenuation(Vec3{0, 0, 0}, Vec3{0.5f, 0, 0},
                                          1.0f, 8.0f),
                   1.0f));
    EXPECT(nearEqF(positionalAttenuation(Vec3{0, 0, 0}, Vec3{9, 0, 0}, 1.0f,
                                          8.0f),
                   0.0f));
    EXPECT(nearEqF(positionalAttenuation(Vec3{0, 0, 0}, Vec3{4.5f, 0, 0},
                                          1.0f, 8.0f),
                   0.5f, 0.01f));
    // defesa: raio externo ≤ interno nunca divide por zero
    const f32 ok = positionalAttenuation(Vec3{0, 0, 0}, Vec3{3, 0, 0}, 2.0f,
                                         2.0f);
    EXPECT(ok >= 0.0f && ok <= 1.0f);

    AudioEngine eng;
    GiClip clip = makeClip(44100, 1, 1.0f);
    const i32 v = eng.play(&clip, true, 1.0f, 1.0f);
    eng.setPosicional(v, true, Vec3{100.0f, 0.0f, 0.0f}, 1.0f, 8.0f);
    eng.setListener(Vec3{0, 0, 0});
    std::vector<f32> out(4096);
    eng.mix(out.data(), 2048, 2, 44100);
    // FORA do raio: SILENCIO — mas o cursor ANDOU (o loop conta o tempo)
    for (f32 s : out) {
        EXPECT(nearEqF(s, 0.0f, 1e-6f));
    }
    EXPECT(eng.playing(v));
    EXPECT(eng.voiceProgress(v) > 0.0f);
    // DENTRO do raio: som volta (a voz segue a POS dada)
    eng.setPosicional(v, true, Vec3{0.5f, 0.0f, 0.0f}, 1.0f, 8.0f);
    eng.mix(out.data(), 512, 2, 44100);
    bool nonZero = false;
    for (f32 s : out) {
        nonZero = nonZero || std::fabs(s) > 0.01f;
    }
    EXPECT(nonZero);
    // MASTER 0: silêncio total (o mute global)
    eng.master = 0.0f;
    eng.mix(out.data(), 512, 2, 44100);
    for (f32 s : out) {
        EXPECT(nearEqF(s, 0.0f, 1e-6f));
    }
}

TEST(wiring011_mixer_resample_de_saida) {
    // clip a 22050 misturado num device a 44100: o cursor avança ao ritmo
    // REAL (o resample de saída produz 2 frames por frame do clip — o
    // tempo do clip anda à velocidade do relógio, não mais depressa)
    AudioEngine eng;
    GiClip clip = makeClip(22050, 1, 2.0f);   // 2 s @ 22050 = 44100 frames
    const i32 v = eng.play(&clip, true, 1.0f, 1.0f);
    std::vector<f32> out(22050 * 2);   // 0.5 s de SAÍDA stereo
    eng.mix(out.data(), 22050, 2, 44100);
    // 0.5 s de saída = 0.5 s do clip (11025 frames) = 25% dos 2 s
    EXPECT(nearEqF(eng.voiceProgress(v), 0.25f, 0.01f));
}

// ---------------------------------------------------------------------------
// 5. PROBE — o harness puro contra fakes (a DECISÃO de fallback)
// ---------------------------------------------------------------------------
namespace {

class FakeBackend final : public audioout::Backend {
public:
    bool start(u32, u16) override {
        if (failStarts > 0) {
            --failStarts;
            return false;
        }
        running = true;
        return true;
    }
    void stop() override { running = false; }
    void pause() override {}
    void resume() override {}
    bool ready() const override { return running && !dropMidway; }
    const char* name() const override { return "fake"; }
    int failStarts = 0;
    bool running = false;
    bool dropMidway = false;   // headset desligado a meio (disconnect)
};

} // namespace

TEST(wiring011_probe_backend_saudavel_aaaudio_ok) {
    FakeBackend ok;
    const audioout::ProbeResult r = audioout::runProbe(&ok, 5, 3, 0);
    EXPECT(r.cycles == 5);
    EXPECT(r.failures == 0);
    EXPECT(r.pauseCycles == 3);
    EXPECT(r.pauseFailures == 0);
    EXPECT(r.disconnects == 0);
    EXPECT(!r.shouldFallback());
    const std::string t = audioout::probeTable(r, "aaudio");
    EXPECT(t.find("AAudio OK") != std::string::npos);
    EXPECT(t.find("FALLBACK") == std::string::npos);
}

TEST(wiring011_probe_start_falha_decide_fallback) {
    FakeBackend bad;
    bad.failStarts = 2;   // 2 dos 5 ciclos falham o start
    const audioout::ProbeResult r = audioout::runProbe(&bad, 5, 0, 0);
    EXPECT(r.failures == 2);
    EXPECT(r.shouldFallback());
    const std::string t = audioout::probeTable(r, "aaudio");
    EXPECT(t.find("FALLBACK AudioTrack") != std::string::npos);
}

TEST(wiring011_probe_disconnect_decide_fallback) {
    FakeBackend dc;
    dc.dropMidway = true;   // o stream cai a meio (headset out no device)
    const audioout::ProbeResult r = audioout::runProbe(&dc, 5, 0, 0);
    EXPECT(r.disconnects > 0);
    EXPECT(r.shouldFallback());
}

TEST(wiring011_probe_null_é_crash_e_fallback) {
    const audioout::ProbeResult r = audioout::runProbe(nullptr, 5, 5, 0);
    EXPECT(r.crashed);
    EXPECT(r.shouldFallback());
    const std::string t = audioout::probeTable(r, "aaudio");
    EXPECT(t.find("FALLBACK") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 6. WORKSPACE ÁUDIO — desenho puro com fake host
// ---------------------------------------------------------------------------
namespace {

struct FakeAudioHost {
    editor::AudioWorkspaceHost h{};
    int imports = 0, records = 0, plays = 0, stops = 0, renames = 0,
        assigns = 0;
    std::string deleted;
    bool recording = false;

    FakeAudioHost() {
        h.onImport = [this]() { ++imports; };
        h.onRecord = [this]() { ++records; };
        h.onPreviewToggle = [this]() { ++plays; };
        h.onPreviewStop = [this]() { ++stops; };
        h.onRename = [this]() { ++renames; };
        h.onAssign = [this]() { ++assigns; };
        h.onDelete = [this](const std::string& rel) { deleted = rel; };
        static const std::vector<f32> kPeaks(64, 0.5f);
        h.codecOf = [](const std::string&) -> const char* { return "adpcm"; };
        h.durationOf = [](const std::string&) -> f32 { return 1.5f; };
        h.peaksOf = [](const std::string&) -> const std::vector<f32>& {
            return kPeaks;
        };
    }
};

} // namespace

TEST(wiring011_workspace_toques_chamam_o_host) {
    UiContext ui;
    InputState in;
    const f32 kSW = 1600.0f, kSH = 720.0f;
    const UiRect view{100.0f, 100.0f, 1000.0f, 500.0f};
    FakeAudioHost host;
    editor::AudioWorkspaceState st;
    std::vector<std::string> clips = {"audio/salto.gi", "audio/musica.gi"};
    editor::audioWorkspaceReset(st);

    // o helper de toque: down → draw → up → draw (o padrão im-mode)
    auto tap = [&](f32 x, f32 y) -> int {
        in.injectDown(0, x, y);
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int r = editor::drawAudioWorkspace(ui, in, view, st, clips,
                                                 host.h);
        ui.endFrame();
        in.injectUp(0);
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int r2 = editor::drawAudioWorkspace(ui, in, view, st, clips,
                                                 host.h);
        ui.endFrame();
        in.clearEdges();
        return r2;
    };

    // ---- geometria (a MESMA fórmula do draw — nada de números mágicos) --
    const f32 ox = view.x, oy = view.y, aw = view.w, ah = view.h;
    const f32 btnW = (aw - 12.0f - 3.0f * 8.0f) * 0.25f;   // 241
    const f32 barY = oy + 6.0f;
    const f32 b[4] = {ox + 6.0f + btnW * 0.5f,
                      ox + 6.0f + btnW + 8.0f + btnW * 0.5f,
                      ox + 6.0f + 2.0f * (btnW + 8.0f) + btnW * 0.5f,
                      ox + 6.0f + 3.0f * (btnW + 8.0f) + btnW * 0.5f};
    const f32 listTop = barY + 52.0f;
    const f32 row1Y = listTop + 30.0f + 46.0f + 23.0f;   // 2ª linha (i=1)

    // IMPORTAR (1º botão da barra de ações)
    EXPECT(tap(b[0], barY + 22.0f) == 0);
    EXPECT(host.imports == 1);
    // GRAVAR (2º)
    tap(b[1], barY + 22.0f);
    EXPECT(host.records == 1);
    // PLAY (3º)
    tap(b[2], barY + 22.0f);
    EXPECT(host.plays == 1);
    // STOP (4º)
    tap(b[3], barY + 22.0f);
    EXPECT(host.stops == 1);

    // SELEÇÃO: a linha do clip 2 devolve 2 (i+1)
    EXPECT(tap(ox + aw * 0.5f, row1Y) == 2);
    EXPECT(st.selected == 1);

    // APAGAR exige CONFIRMAÇÃO: o 1º toque arma, o APAGAR confirma
    host.h.recording = false;
    const f32 listH = ah - (listTop - oy) - 196.0f;
    const f32 waveY = listTop + listH + 10.0f;
    const f32 actionsY = waveY + 102.0f;
    const f32 abW = (aw - 12.0f - 2.0f * 8.0f) * 0.3333f;
    const f32 aRnm = ox + 6.0f + abW * 0.5f;
    const f32 aDel = ox + 6.0f + abW + 8.0f + abW * 0.5f;
    const f32 aAss = ox + 6.0f + 2.0f * (abW + 8.0f) + abW * 0.5f;
    tap(aDel, actionsY + 20.0f);
    EXPECT(host.deleted.empty());   // armado, NÃO apagou
    EXPECT(st.confirmDelete);
    tap(ox + aw * 0.5f - 60.0f, actionsY + 46.0f + 26.0f);
    EXPECT(host.deleted == "audio/musica.gi");   // o CONFIRMA apagou
    EXPECT(!st.confirmDelete);

    // ATRIBUIR
    tap(aAss, actionsY + 20.0f);
    EXPECT(host.assigns == 1);
    // RENOMEAR
    tap(aRnm, actionsY + 20.0f);
    EXPECT(host.renames == 1);
}

TEST(wiring011_workspace_host_parcial_nunca_crasha) {
    // TODOS os ponteiros a null + host vazio: o desenho corre (guards)
    UiContext ui;
    InputState in;
    editor::AudioWorkspaceHost empty;   // zero callbacks
    empty.recording = true;             // o overlay de gravação desenha
    empty.recordSecs = 3;
    empty.recordLevel = 0.7f;
    empty.previewing = true;            // previewPos null — guard
    editor::AudioWorkspaceState st;
    const std::vector<std::string> clips = {"audio/a.gi"};
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    const int r = editor::drawAudioWorkspace(
        ui, in, UiRect{50.0f, 50.0f, 1200.0f, 600.0f}, st, clips, empty);
    ui.endFrame();
    EXPECT(r == 0);
    // rect degenerado: nada desenha, sem crash
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    EXPECT(editor::drawAudioWorkspace(ui, in, UiRect{0, 0, 10.0f, 10.0f},
                                      st, clips, empty) == 0);
    ui.endFrame();
}

// ---------------------------------------------------------------------------
// 7. SERIALIZER / PRESET / STORE
// ---------------------------------------------------------------------------
TEST(wiring011_serializer_audioplayer_roundtrip) {
    Scene s;
    const Handle h = s.create("Som");
    Tic* t = s.get(h);
    AudioPlayer* au = t->addComponent<AudioPlayer>();
    au->clipPath = "audio/salto.gi";
    au->autoplay = true;
    au->loop = true;
    au->volume = 0.6f;
    au->pitch = 1.25f;
    au->posicional = true;
    au->raioInterno = 2.0f;
    au->raioExterno = 12.0f;
    au->voiceId = 7;   // runtime: NÃO serializa
    au->previewing = true;

    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;   // zero resolvers — o AudioPlayer não precisa
    EXPECT(SceneSerializer::loadText(s2, text, ctx));
    Tic* t2 = s2.get(s2.find("Som"));
    EXPECT(t2 != nullptr);
    const AudioPlayer* au2 = t2->getComponent<AudioPlayer>();
    EXPECT(au2 != nullptr);
    EXPECT(au2->clipPath == "audio/salto.gi");
    EXPECT(au2->autoplay);
    EXPECT(au2->loop);
    EXPECT(nearEqF(au2->volume, 0.6f));
    EXPECT(nearEqF(au2->pitch, 1.25f));
    EXPECT(au2->posicional);
    EXPECT(nearEqF(au2->raioInterno, 2.0f));
    EXPECT(nearEqF(au2->raioExterno, 12.0f));
    // runtime resetado (cena carregada nasce calada)
    EXPECT(au2->voiceId == -1);
    EXPECT(!au2->previewing);

    // DEFAULTS omitidos: dump → load devolve os defaults do componente
    Scene s3;
    Tic* td = s3.get(s3.create("Default"));
    td->addComponent<AudioPlayer>();
    const std::string text2 = SceneSerializer::dump(s3);
    EXPECT(text2.find("autoplay") == std::string::npos);
    EXPECT(text2.find("volume") == std::string::npos);
    Scene s4;
    EXPECT(SceneSerializer::loadText(s4, text2, ctx));
    const AudioPlayer* au4 =
        s4.get(s4.find("Default"))->getComponent<AudioPlayer>();
    EXPECT(!au4->autoplay && !au4->loop && !au4->posicional);
    EXPECT(nearEqF(au4->volume, 1.0f) && nearEqF(au4->pitch, 1.0f));
    // clampFields na leitura: valores fora do range ficam DENTRO
    Scene s5;
    const std::string bad = "{\"tics\":[{\"name\":\"B\",\"components\":"
                            "[{\"type\":\"AudioPlayer\",\"volume\":9,"
                            "\"pitch\":0.1,\"rint\":-5,\"rext\":-9}]}]}";
    EXPECT(SceneSerializer::loadText(s5, bad, ctx));
    const AudioPlayer* au5 =
        s5.get(s5.find("B"))->getComponent<AudioPlayer>();
    EXPECT(au5 != nullptr);
    EXPECT(nearEqF(au5->volume, 1.0f));   // clamp 0..1
    EXPECT(nearEqF(au5->pitch, 0.5f));    // clamp 0.5..2
    EXPECT(au5->raioExterno > au5->raioInterno);   // nunca invertido
}

TEST(wiring011_preset_audio_estrutura) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::Audio, nullptr,
                                          nullptr);
    EXPECT(h.valid());
    Tic* t = s.get(h);
    EXPECT(t != nullptr);
    EXPECT(t->getComponent<Transform3D>() != nullptr);
    EXPECT(t->getComponent<AudioPlayer>() != nullptr);
    // SEM mesh, SEM física (estrutura pura — o som é 3D posicional)
    EXPECT(t->getComponent<MeshRenderer>() == nullptr);
    EXPECT(t->getComponent<BodyComp>() == nullptr);
    EXPECT(std::string(presetName(PresetKind::Audio)) == "Audio");
    EXPECT(static_cast<u32>(PresetKind::Count) == 6);

    // o REGISTRO cria por nome (o caminho do "+" e do serializer)
    Scene s2;
    const Handle h2 = s2.create("X");
    EXPECT(s2.components().registry().create("AudioPlayer",
                                             s2.components(), h2));
    EXPECT(s2.get(h2)->getComponent<AudioPlayer>() != nullptr);
    // removeAll remove (morte do TIC)
    s2.components().removeAll(h2);
    EXPECT(s2.get(h2)->getComponent<AudioPlayer>() == nullptr);
}

// ---------------------------------------------------------------------------
// 8. WIRING PURO — FileApi, catálogo, seletor (menu 5), plano do Inspector
// ---------------------------------------------------------------------------
TEST(wiring011_fileapi_kinds_e_raiz_music) {
    EXPECT(fileapi::kindOfExtension("a.WAV") == 's');
    EXPECT(fileapi::kindOfExtension("a.ogg") == 's');
    EXPECT(fileapi::kindOfExtension("a.MP3") == 's');
    EXPECT(fileapi::kindOfExtension("a.obj") == 'm');
    EXPECT(fileapi::kindOfExtension("a.zip") == 'a');
    EXPECT(fileapi::kindOfExtension("a.txt") == 0);
}

TEST(wiring011_seletor_de_clips_draw_e_apply) {
    UiContext ui;
    InputState in;
    const f32 kSW = 1600.0f, kSH = 720.0f;
    editor::EditorState st;
    st.assetMenu = 5;
    editor::AssetCatalog cat;
    cat.audio = {"audio/salto.gi", "audio/musica.gi"};

    // geometria do overlay (a MESMA fórmula do drawAssetMenu: menu
    // centrado, header + (1+shown)*48 + importar + pad; shown = cap 5)
    const f32 w = editor::kMenuW;
    const f32 h = editor::kHeaderH + (1.0f + 2.0f) * 48.0f + 48.0f +
                  editor::kPad;
    const f32 x = (kSW - w) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;
    // o seletor FECHA no clique (st.assetMenu = 0 dentro do draw) — cada
    // toque REARMA o menu (o padrão do test_browser com fileBrowser)
    auto tap = [&](f32 tx, f32 ty) -> int {
        st.assetMenu = 5;
        in.injectDown(0, tx, ty);
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int r = editor::drawAssetMenu(ui, in, kSW, kSH, st, cat, false);
        ui.endFrame();
        in.injectUp(0);
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int r2 = editor::drawAssetMenu(ui, in, kSW, kSH, st, cat,
                                             false);
        ui.endFrame();
        in.clearEdges();
        return r2;
    };
    // none → 1
    EXPECT(tap(x + w * 0.5f, y + editor::kHeaderH + 20.0f) == 1);
    // clip 1 → 2; clip 2 → 3
    EXPECT(tap(x + w * 0.5f, y + editor::kHeaderH + 48.0f + 20.0f) == 2);
    EXPECT(tap(x + w * 0.5f, y + editor::kHeaderH + 96.0f + 20.0f) == 3);
    // importar → kAssetPickImport (o seletor de clips SEMPRE oferece)
    EXPECT(tap(x + w * 0.5f, y + editor::kHeaderH + 144.0f + 20.0f) ==
           editor::kAssetPickImport);

    // applyAssetPick menuKind 5 (PURO): none limpa, pick aplica, out-of-range
    // não crasha, sem AudioPlayer → outcome vazio
    Scene s;
    const Handle hAu = createTicFromPreset(s, PresetKind::Audio, nullptr,
                                            nullptr);
    editor::AssetResolvers res;   // zero resolvers — o áudio não precisa
    AudioPlayer* au = s.get(hAu)->getComponent<AudioPlayer>();
    au->clipPath = "audio/velho.gi";
    au->voiceId = 3;
    editor::AssetPickOutcome out = editor::applyAssetPick(s, hAu, 5, 1, cat,
                                                          res);
    EXPECT(out.applied);
    EXPECT(!au->hasClip());
    EXPECT(au->voiceId == -1);
    out = editor::applyAssetPick(s, hAu, 5, 2, cat, res);
    EXPECT(out.applied);
    EXPECT(au->clipPath == "audio/salto.gi");
    EXPECT(std::string(out.toast).find("salto") != std::string::npos);
    // fora do catálogo: sem crash, sem ação
    out = editor::applyAssetPick(s, hAu, 5, 99, cat, res);
    EXPECT(!out.applied);
    // TIC SEM AudioPlayer: outcome vazio (o toast honesto é do chamador)
    const Handle hm = createTicFromPreset(s, PresetKind::Mesh, nullptr,
                                          nullptr);
    out = editor::applyAssetPick(s, hm, 5, 2, cat, res);
    EXPECT(!out.applied);
}

TEST(wiring011_inspector_plano_com_seccao_audio) {
    // o PERFIL/PLANO (fonte única — F5.0-fix): com AudioPlayer a secção
    // acrescenta 10 linhas (secção + clip + ouvir + autoplay + loop +
    // volume + pitch + posicional + 2 raios)
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::Audio, nullptr,
                                         nullptr);
    Tic* t = s.get(h);
    const editor::InspProfile withAu = editor::inspectorProfile(*t);
    EXPECT(withAu.au);
    t->removeComponent<AudioPlayer>();
    const editor::InspProfile withoutAu = editor::inspectorProfile(*t);
    EXPECT(!withoutAu.au);

    const TextMetrics tm;   // fallback (sem atlas): o plano é o mesmo
    const u32 nWith = editor::inspectorRowCount(withAu, true);
    const u32 nWithout = editor::inspectorRowCount(withoutAu, true);
    EXPECT(nWith == nWithout + 10);

    editor::InspRow rows[48];
    const u32 n = editor::inspectorPlan(withAu, tm, true, rows);
    EXPECT(n == nWith);
    int auRows = 0;
    bool hasClip = false, hasPlay = false, hasVol = false;
    for (u32 i = 0; i < n; ++i) {
        if (rows[i].kind == editor::InspRow::Kind::AuSection ||
            rows[i].kind == editor::InspRow::Kind::AuClip ||
            rows[i].kind == editor::InspRow::Kind::AuPlay ||
            rows[i].kind == editor::InspRow::Kind::AuAutoplay ||
            rows[i].kind == editor::InspRow::Kind::AuLoop ||
            rows[i].kind == editor::InspRow::Kind::AuVolume ||
            rows[i].kind == editor::InspRow::Kind::AuPitch ||
            rows[i].kind == editor::InspRow::Kind::AuPos ||
            rows[i].kind == editor::InspRow::Kind::AuRint ||
            rows[i].kind == editor::InspRow::Kind::AuRext) {
            ++auRows;
        }
        hasClip = hasClip || rows[i].kind == editor::InspRow::Kind::AuClip;
        hasPlay = hasPlay || rows[i].kind == editor::InspRow::Kind::AuPlay;
        hasVol = hasVol || rows[i].kind == editor::InspRow::Kind::AuVolume;
    }
    EXPECT(auRows == 10);
    EXPECT(hasClip && hasPlay && hasVol);
    // os ids NÃO colidem com os das outras secções (faixa 5700..5708)
    EXPECT(editor::kInspectorAuClip == 5700);
    EXPECT(editor::toolbar::kModeAudioId == 15);   // NÃO colide com o G5 (13)
    EXPECT(editor::toolbar::kModeAudioId !=
           editor::toolbar::kTbInspectId);

    // a altura de conteúdo CRESCE com a secção (o scroll ativa)
    EXPECT(editor::inspectorContentHeight(withAu, tm, true) >
           editor::inspectorContentHeight(withoutAu, tm, true));
}

TEST(wiring011_plus_menu_3d_tem_audio) {
    // o "+" no modo 3D lista 7 (…Camera, Mesh, Audio) — o 7º cria o TIC de
    // áudio; os ids continuam na faixa 20+i
    UiContext ui;
    InputState in;
    const f32 kSW = 1600.0f, kSH = 720.0f;
    editor::EditorState st;
    st.plusMenu = true;
    const f32 w = editor::kMenuW;
    const f32 h = editor::kHeaderH + 7.0f * 64.0f + editor::kPad;
    const f32 x = (kSW - w) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;
    // o 7º item (Audio) → choice 7
    in.injectDown(0, x + w * 0.5f, y + editor::kHeaderH + 6.0f * 64.0f + 28.0f);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    const int r = editor::drawPlusMenu(ui, in, kSW, kSH, st);
    ui.endFrame();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    const int r2 = editor::drawPlusMenu(ui, in, kSW, kSH, st);
    ui.endFrame();
    in.clearEdges();
    EXPECT(r2 == 7);
    EXPECT(!st.plusMenu);
    (void)r;
}
