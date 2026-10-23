#pragma once
// components/ParticleEmitter.h — o TIC DE PARTÍCULAS (0.12-Y-F1).
//
// "DECISÃO DE ARQUITETURA" (o prompt): GLES 3.0 — NÃO HÁ COMPUTE SHADERS.
// Vertex Shader Animation: o CPU gera UMA VEZ o buffer de SEEDS (a origem
// aleatória + a semente [0,1) por partícula — a FONTE ÚNICA vive no
// render/ParticleShader.h, a função fx::generateSeeds) e o Vertex Shader
// recebe o uTime (o tempo global do motor) e calcula a posição em tempo
// real. ZERO física no CPU. Um único draw call
// (glDrawArraysInstanced — o GLES 3.0 core, o GE8320 do alvo suporta).
//
// Campos (o Inspector edita; o serializer grava):
//   maxParticles — o teto do draw (o buffer nasce com ESTE tamanho;
//                  mudar o valor regenera o buffer no frame seguinte)
//   lifetime     — segundos de vida de cada partícula
//   startColor/endColor — RGBA 0..1 (interpola pela vida)
//   startSize/endSize   — o tamanho do billboard (unidades do mundo;
//                  interpola pela vida)
//   velocity     — a direção inicial (unidades/s)
//   gravity      — a aceleração (unidades/s²)
//   texturePath  — a imagem da partícula ("" = o sprite procedural —
//                  o disco suave do FS; fogo/fumaça MVP sem asset)
//
// A matemática da vida (o contrato do VS, replicado nas puras):
//   localTime    = mod(uTime + seed*100, lifetime)   — a semente DESFAZ o
//                  tempo: as partículas não nascem todas ao mesmo momento
//   pos          = origem + velocity*localTime + 0.5*gravity*localTime²
//   lifeProgress = localTime / lifetime              — interpola tamanho/
//                  cor e faz o FADE OUT no fim da vida
#include "core/Component.h"
#include "core/Types.h"
#include <string>

namespace vv {

class ParticleEmitter : public Component {
public:
    // os tetos da casa (o padrão Environment::k*Max — o slider usa-os)
    static constexpr i32 kMaxParticlesDefault = 500;
    static constexpr i32 kMaxParticlesMin     = 1;
    static constexpr i32 kMaxParticlesMax     = 2000;  // o orçamento mobile
    static constexpr f32 kLifetimeDefault     = 2.0f;
    static constexpr f32 kLifetimeMin         = 0.1f;
    static constexpr f32 kLifetimeMax         = 20.0f;
    static constexpr f32 kSizeMin             = 0.0f;
    static constexpr f32 kSizeMax             = 4.0f;
    static constexpr f32 kSizeStartDefault    = 0.25f;
    static constexpr f32 kSizeEndDefault      = 0.05f;
    static constexpr f32 kVelMax              = 20.0f;   // |eixo| do slider
    static constexpr f32 kGravMax             = 20.0f;
    static constexpr f32 kVelYDefault         = 3.0f;    // a fonte (jorro)
    static constexpr f32 kGravYDefault        = -4.0f;   // cai — a fonte

    i32 maxParticles = kMaxParticlesDefault;
    f32 lifetime     = kLifetimeDefault;
    // as cores 0..1 RGBA (o âmbar→vermelho: o fogo MVP de fábrica)
    f32 startColor[4] = {1.0f, 0.72f, 0.25f, 1.0f};
    f32 endColor[4]   = {0.90f, 0.15f, 0.05f, 1.0f};
    f32 startSize = kSizeStartDefault;
    f32 endSize   = kSizeEndDefault;
    f32 velocity[3] = {0.0f, kVelYDefault, 0.0f};
    f32 gravity[3]  = {0.0f, kGravYDefault, 0.0f};
    std::string texturePath;   // "" = o sprite procedural do FS

    bool active = true;       // o invariante da casa (o TIC ativo manda —
                              // o MESMO padrão do Environment::active)

    // runtime — nunca serializado
    bool texFailed = false;    // o load falhou (o FS cai no sprite; o LOG
                               // fala 1× — o spam morre no flag)

    // clamps de defesa (o padrão AudioPlayer::clampFields / Environment)
    void clampFields() {
        if (maxParticles < kMaxParticlesMin) maxParticles = kMaxParticlesMin;
        if (maxParticles > kMaxParticlesMax) maxParticles = kMaxParticlesMax;
        if (lifetime < kLifetimeMin) lifetime = kLifetimeMin;
        if (lifetime > kLifetimeMax) lifetime = kLifetimeMax;
        if (startSize < kSizeMin) startSize = kSizeMin;
        if (startSize > kSizeMax) startSize = kSizeMax;
        if (endSize < kSizeMin) endSize = kSizeMin;
        if (endSize > kSizeMax) endSize = kSizeMax;
        for (int i = 0; i < 3; ++i) {
            if (velocity[i] < -kVelMax) velocity[i] = -kVelMax;
            if (velocity[i] > kVelMax) velocity[i] = kVelMax;
            if (gravity[i] < -kGravMax) gravity[i] = -kGravMax;
            if (gravity[i] > kGravMax) gravity[i] = kGravMax;
        }
        for (int i = 0; i < 4; ++i) {
            if (startColor[i] < 0.0f) startColor[i] = 0.0f;
            if (startColor[i] > 1.0f) startColor[i] = 1.0f;
            if (endColor[i] < 0.0f) endColor[i] = 0.0f;
            if (endColor[i] > 1.0f) endColor[i] = 1.0f;
        }
    }
};

} // namespace vv
