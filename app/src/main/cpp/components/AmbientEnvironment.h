#pragma once
// components/AmbientEnvironment.h — o TIC DE CLIMA (0.12-Y-F2 · 0.12-W-1).
//
// A DECISÃO DE ARQUITETURA DO DONO (OBRIGATÓRIA — a Y-F2, REFINADA pela
// W-1): o AmbientEnvironment NÃO renderiza partículas. É um CONTROLADOR:
// configura UM ParticleEmitter e gere o FLASH da trovoada (runtime, no céu
// via uThunderFlash — sem geometria de raio). A W-1 adiciona o ALVO POR
// NOME — o fluxo do dono com DOIS TICs separados:
//   TIC "RainEmitter"     — ParticleEmitter + Transform (o emissor)
//   TIC "WeatherController" — AmbientEnvironment (targetEmitter="RainEmitter")
// A RESOLUÇÃO (a função findWeatherTarget do core/Presets — uma fonte):
//   1.º o ParticleEmitter do MESMO TIC (o contrato da Y-F2 byte a byte —
//       os presets Rain/Snow/Storm continuam a funcionar IGUAL);
//   2.º SEM PE no TIC → o TIC ATIVO com o NOME targetEmitter que tenha um
//       ParticleEmitter (o fluxo W-1 dos dois TICs);
//   NENHUM dos dois → o LOG WARN 1× (edge-triggered — o flag
//   targetWarned desarma o spam; o pin ambient_env_logs_warning aferre) e
//   NADA acontece (o dono sabe — sem emissor não há clima).
//
//   weatherType — 0=NONE, 1=RAIN, 2=SNOW, 3=STORM (o Inspector cicla)
//   intensity   — 0..1, escala o COUNT de partículas do preset
//   targetEmitter — o NOME do TIC do emissor (default "RainEmitter")
//   thunderFlash/thunderTimer — RUNTIME (nunca serializados): o flash
//                 vive 1/3 de segundo (decay 3.0/s), o próximo raio
//                 chega em random(3,8) s.
//
// Os presets (a ordem do dono — os valores EXATOS do prompt):
//   RAIN  → gravidade (0,-12,0) · vel (0,-18,0) · vida 1.2s · tamanho
//           0.015→0.015 · cor (0.6,0.7,0.9,0.7)→(0.6,0.7,0.9,0) ·
//           máx 1500×intensity · raio 14 (0.12-Z2 F3 — o volume do clima)
//   SNOW  → gravidade (0,-1.2,0) · vel (0,-2,0) · vida 4s · tamanho
//           0.04→0.02 · cor (1,1,1,0.9)→(1,1,1,0) · máx 800×intensity ·
//           raio 12 (0.12-Z2 F3)
//   STORM → igual à chuva com máx 2000×intensity · raio 14 (0.12-Z2 F3)
//   NONE  → maxParticles 0 + o raio de volta ao jorro (o emissor
//           silencia — o draw salta; o emissor fica COMO ANTES do clima)
//
// GLES 3.0: NADA de compute shaders — tudo o que aqui vive é DADOS.
#include "core/Component.h"
#include "core/Types.h"
#include "components/ParticleEmitter.h"
#include <cmath>
#include <string>

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
    // os RAIOS do volume de spawn do clima (0.12-Z2 F3 — o emissor vive
    // num volume À VOLTA do que a câmara OLHA; o raio manda na DENSIDADE
    // do count do dono: 1500 partículas num raio 6 formam o cortinado que
    // o frustum cruza — um raio 14 espalhava o mesmo count num volume 10×
    // maior e o frustum ficava vazio, PROVADO pela 1.ª run do --clima)
    static constexpr f32 kRainRadius = 6.0f;
    static constexpr f32 kSnowRadius = 5.0f;
    // a trovoada: o flash morre a 3.0/s (vive ~0.33 s), o próximo raio
    // em [3,8] s
    static constexpr f32 kThunderDecay    = 3.0f;
    static constexpr f32 kThunderIntervalMin = 3.0f;
    static constexpr f32 kThunderIntervalMax = 8.0f;

    u8   weatherType = kWeatherNone;
    f32  intensity   = kIntensityDefault;
    bool active      = true;   // o 1.º ATIVO da cena manda (o padrão casa)

    // 0.12-W-1 — O ALVO POR NOME (o fluxo do dono: DOIS TICs separados).
    // O default é o nome do prompt; o serializer só escreve ≠ default.
    static constexpr const char* kTargetDefault = "RainEmitter";
    std::string targetEmitter = kTargetDefault;

    // runtime — NUNCA serializados (a cena carrega SEM raio pendente)
    f32 thunderFlash = 0.0f;
    f32 thunderTimer = 4.0f;   // o meio do intervalo — o 1.º raio cedo
    // 0.12-W-1 — o warn do alvo É EDGE-TRIGGERED (1× por ausência; o alvo
    // a aparecer o flag desarma — a próxima ausência volta a falar)
    bool targetWarned = false;

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
                pe->spawnRadius  = kRainRadius;   // 0.12-Z2 F3: o volume
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
                pe->spawnRadius  = kSnowRadius;   // 0.12-Z2 F3: o volume
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
                pe->spawnRadius  = kRainRadius;   // 0.12-Z2 F3: o volume
                pe->clampFields();
                break;
            default:   // NONE — o emissor silencia (o 0 é o contrato) e
                       // volta ao JORRO (o raio de fábrica — o emissor
                       // fica como antes do clima)
                pe->maxParticles = 0;
                pe->spawnRadius = ParticleEmitter::kSpawnRadiusDefault;
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
