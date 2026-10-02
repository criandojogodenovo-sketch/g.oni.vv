#pragma once
// core/AudioEngine.h — MISTURADOR de clipes .gi (0.8.11).
//
// O áudio da engine é UM misturador de software puro: vozes ativas (clip
// + cursor + volume + pitch + loop) somadas por frame num buffer f32. O
// BACKEND (AAudio/AudioTrack no device; stub no CI) puxa mix() no seu
// callback — o motor nunca fala com o hardware diretamente.
//
// PITCH = resample LINEAR do cursor (1.0 = original; 0.5..2 na UI).
// POSICIONAL: cada voz tem uma posição no mundo; a atenuação pela
// DISTÂNCIA do ouvinte (1 dentro do raio interno, 0 no externo — o
// wireframe do editor desenha EXATAMENTE estes raios).
// MASTER VOLUME: um fator global (Settings) — o último ganho antes do
// clip de saída.
//
// GL-free / Android-free / thread-model: mix() corre no THREAD do
// callback de áUDIO (sem locks — as vozes mudam por flags atómicas
// simples; play/stop são chamados do thread da engine; a coerência
// eventual entre frames de áudio é aceitável para um misturador de jogo
// de telemóvel e EVITA deadlocks no callback real-time).
#include <string>
#include <vector>

#include "assets/GiFormat.h"
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class AudioEngine {
public:
    struct Voice {
        const GiClip* clip = nullptr;   // dono: o cache de clipes do main
        f64 cursor = 0.0;               // frame ATUAL dentro do clip
        bool loop = false;
        f32 volume = 1.0f;
        f32 pitch = 1.0f;
        bool playing = false;
        bool posicional = false;
        Vec3 pos{0.0f, 0.0f, 0.0f};
        f32 raioInterno = 1.0f;
        f32 raioExterno = 8.0f;
    };

    // uma voz por AudioPlayer ativo (o main pede id no play; guarda-o)
    i32 play(const GiClip* clip, bool loop, f32 volume, f32 pitch);
    void stop(i32 voiceId);
    void stopAll();
    bool playing(i32 voiceId) const;

    // atualiza os parâmetros vivos de uma voz (o Inspector/Play mexem)
    void setParams(i32 voiceId, f32 volume, f32 pitch, bool loop);

    // posição do cursor da voz 0..1 (a linha da waveform do preview); -1
    // se a voz não existe/não toca
    f32 voiceProgress(i32 voiceId) const;

    // POSICIONAL: a voz deixa de ser global e passa a atenuar pela pos
    void setPosicional(i32 voiceId, bool on, const Vec3& pos,
                       f32 raioInterno, f32 raioExterno);
    void setListener(const Vec3& pos) { listener_ = pos; }

    // mistura `frames` frames em `out` (interleaved `outChannels`,
    // clamped [-1,1]); avança os cursores. Chamado no callback do backend.
    void mix(f32* out, u32 frames, u32 outChannels, u32 outRate);

    // master volume (Settings — persistido; 0 = mudo)
    f32 master = 1.0f;

    // telemetria p/ o status/diagnóstico
    u32 activeVoices() const { return countActive(); }
    u64 totalFramesMixed() const { return mixedFrames_; }

    // O RESAMPLE de saída: se o clip tem sampleRate != do device, o mix
    // re-amostra LINEARMENTE (a qualidade chega para jogo; documentado)
    void reset() {
        voices_.clear();
        mixedFrames_ = 0;
    }

private:
    std::vector<Voice> voices_;   // id = índice (slots reutilizados)
    Vec3 listener_{0.0f, 0.0f, 0.0f};
    u64 mixedFrames_ = 0;

    u32 countActive() const;
    f32 attenuationOf(const Voice& v) const;
};

// atenuação posicional PURA (afervel no CI sem engine): 1 dentro do raio
// interno, decai LINEAR até 0 no raio externo, 0 além; global = 1
inline f32 positionalAttenuation(const Vec3& listener, const Vec3& pos,
                                 f32 raioInterno, f32 raioExterno) {
    if (raioExterno <= raioInterno) {
        raioExterno = raioInterno + 0.001f;   // defesa: nunca div por 0
    }
    const Vec3 d{pos.x - listener.x, pos.y - listener.y, pos.z - listener.z};
    const f32 dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (dist <= raioInterno) {
        return 1.0f;
    }
    if (dist >= raioExterno) {
        return 0.0f;
    }
    return 1.0f - (dist - raioInterno) / (raioExterno - raioInterno);
}

} // namespace vv
