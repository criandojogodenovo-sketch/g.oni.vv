#pragma once
// components/AmbientEnvironment.h — o TIC DE CLIMA (0.12-Y-F2).
//
// A DECISÃO DE ARQUITETURA DO DONO (OBRIGATÓRIA — a opção A): o
// AmbientEnvironment NÃO renderiza partículas. Ele CONFIGURA o
// ParticleEmitter do MESMO TIC quando o weatherType muda (a função
// applyWeatherPreset) e gere o FLASH da trovoada (runtime, desenhado
// pelo céu via uThunderFlash — sem geometria de raio). Se o TIC não
// tem ParticleEmitter, applyWeatherPreset não faz nada (o dono sabe —
// o log fala 1× por mudança, o spam morre).
//
//   weatherType — 0=NONE, 1=RAIN, 2=SNOW, 3=STORM (o Inspector cicla)
//   intensity   — 0..1, escala o COUNT de partículas do preset
//   thunderFlash/thunderTimer — RUNTIME (nunca serializados): o flash
//                 vive 1/3 de segundo (decay 3.0/s), o próximo raio
//                 chega em random(3,8) s.
//
// Os presets (a ordem do dono — os valores EXATOS do prompt):
//   RAIN  → gravidade (0,-12,0) · vel (0,-18,0) · vida 1.2s · tamanho
//           0.015→0.015 · cor (0.6,0.7,0.9,0.7)→(0.6,0.7,0.9,0) ·
//           máx 1500×intensity
//   SNOW  → gravidade (0,-1.2,0) · vel (0,-2,0) · vida 4s · tamanho
//           0.04→0.02 · cor (1,1,1,0.9)→(1,1,1,0) · máx 800×intensity
//   STORM → igual à chuva com máx 2000×intensity
//   NONE  → maxParticles 0 (o emissor silencia — o draw salta)
//
// GLES 3.0: NADA de compute shaders — tudo o que aqui vive é DADOS.
#include "core/Component.h"
#include "core/Types.h"
#include "components/ParticleEmitter.h"
#include <cmath>

namespace vv {

class AmbientEnvironment : public Component {
public:
    static constexpr u8  kWeatherNone  = 0;
    static constexpr u8  kWeatherRain  = 1;
    static constexpr u8  kWeatherSnow  = 2;
    static constexpr u8  kWeatherStorm = 3;
    static constexpr f32 kIntensityDefault = 0.5f;
    // os COUNTS do preset (o teto do PE é 2000 — a STORM chega com
    // intensity 1.0 exato; a clamp do PE aferra)
    static constexpr i32 kRainCount  = 1500;
    static constexpr i32 kSnowCount  = 800;
    static constexpr i32 kStormCount = 2000;
    // a trovoada: o flash morre a 3.0/s (vive ~0.33 s), o próximo raio
    // em [3,8] s
    static constexpr f32 kThunderDecay    = 3.0f;
    static constexpr f32 kThunderIntervalMin = 3.0f;
    static constexpr f32 kThunderIntervalMax = 8.0f;

    u8   weatherType = kWeatherNone;
    f32  intensity   = kIntensityDefault;
    bool active      = true;   // o 1.º ATIVO da cena manda (o padrão casa)

    // runtime — NUNCA serializados (a cena carrega SEM raio pendente)
    f32 thunderFlash = 0.0f;
    f32 thunderTimer = 4.0f;   // o meio do intervalo — o 1.º raio cedo

    void clampFields() {
        if (weatherType > kWeatherStorm) {
            weatherType = kWeatherNone;   // a corrupção cai no NONE (o seguro)
        }
        if (intensity < 0.0f) intensity = 0.0f;
        if (intensity > 1.0f) intensity = 1.0f;
        if (thunderFlash < 0.0f) thunderFlash = 0.0f;
        if (thunderFlash > 1.0f) thunderFlash = 1.0f;
        if (thunderTimer < 0.0f) thunderTimer = 0.0f;
    }

    // o PRESET (a FONTE ÚNICA da fábrica do clima): escreve o
    // ParticleEmitter do MESMO TIC. Devolve false quando pe == nullptr
    // (o chamador decide o aviso — a decisão do dono: sem emissor não
    // há clima aplicado). O NONE silencia o emissor com maxParticles 0 —
    // A clamp do PE levantaria o 0 para 1, por isso a clamp só corre nos
    // tipos com partículas.
    bool applyWeatherPreset(ParticleEmitter* pe) const {
        if (!pe) {
            return false;
        }
        switch (weatherType) {
            case kWeatherRain:
                pe->gravity[0]  = 0.0f;  pe->gravity[1]  = -12.0f;
                pe->gravity[2]  = 0.0f;
                pe->velocity[0] = 0.0f;  pe->velocity[1] = -18.0f;
                pe->velocity[2] = 0.0f;
                pe->lifetime    = 1.2f;
                pe->startSize   = 0.015f;
                pe->endSize     = 0.015f;
                pe->startColor[0] = 0.6f;  pe->startColor[1] = 0.7f;
                pe->startColor[2] = 0.9f;  pe->startColor[3] = 0.7f;
                pe->endColor[0]   = 0.6f;  pe->endColor[1]   = 0.7f;
                pe->endColor[2]   = 0.9f;  pe->endColor[3]   = 0.0f;
                pe->maxParticles  = static_cast<i32>(
                    static_cast<f32>(kRainCount) * intensity);
                pe->clampFields();
                break;
            case kWeatherSnow:
                pe->gravity[0]  = 0.0f;  pe->gravity[1]  = -1.2f;
                pe->gravity[2]  = 0.0f;
                pe->velocity[0] = 0.0f;  pe->velocity[1] = -2.0f;
                pe->velocity[2] = 0.0f;
                pe->lifetime    = 4.0f;
                pe->startSize   = 0.04f;
                pe->endSize     = 0.02f;
                pe->startColor[0] = 1.0f;  pe->startColor[1] = 1.0f;
                pe->startColor[2] = 1.0f;  pe->startColor[3] = 0.9f;
                pe->endColor[0]   = 1.0f;  pe->endColor[1]   = 1.0f;
                pe->endColor[2]   = 1.0f;  pe->endColor[3]   = 0.0f;
                pe->maxParticles  = static_cast<i32>(
                    static_cast<f32>(kSnowCount) * intensity);
                pe->clampFields();
                break;
            case kWeatherStorm:
                pe->gravity[0]  = 0.0f;  pe->gravity[1]  = -12.0f;
                pe->gravity[2]  = 0.0f;
                pe->velocity[0] = 0.0f;  pe->velocity[1] = -18.0f;
                pe->velocity[2] = 0.0f;
                pe->lifetime    = 1.2f;
                pe->startSize   = 0.015f;
                pe->endSize     = 0.015f;
                pe->startColor[0] = 0.6f;  pe->startColor[1] = 0.7f;
                pe->startColor[2] = 0.9f;  pe->startColor[3] = 0.7f;
                pe->endColor[0]   = 0.6f;  pe->endColor[1]   = 0.7f;
                pe->endColor[2]   = 0.9f;  pe->endColor[3]   = 0.0f;
                pe->maxParticles  = static_cast<i32>(
                    static_cast<f32>(kStormCount) * intensity);
                pe->clampFields();
                break;
            default:   // NONE — o emissor silencia (o 0 é o contrato)
                pe->maxParticles = 0;
                break;
        }
        return true;
    }

    // o UPDATE da TROVOADA (o pseudocódigo EXATO do prompt — só corre em
    // STORM; o rnd01 ∈ [0,1) vem do chamador — o main alimenta com o seu
    // hash do relógio, os testes com valor fixo: determinismo aferrado)
    void updateStorm(f32 dt, f32 rnd01) {
        if (weatherType != kWeatherStorm || !active) {
            return;
        }
        thunderTimer -= dt;
        if (thunderTimer <= 0.0f) {
            thunderFlash = 1.0f;
            thunderTimer = kThunderIntervalMin +
                rnd01 * (kThunderIntervalMax - kThunderIntervalMin);
        }
        thunderFlash -= dt * kThunderDecay;
        if (thunderFlash < 0.0f) {
            thunderFlash = 0.0f;
        }
    }
};

} // namespace vv
