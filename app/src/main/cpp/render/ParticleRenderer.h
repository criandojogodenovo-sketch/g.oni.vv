#pragma once
// render/ParticleRenderer.h — o GPU particle system (0.12-Y-F1).
//
// O CPU gera UMA VEZ o VBO de seeds (fx::generateSeeds — a origem
// aleatória + a semente por partícula) com o TETO kSeedCapacity e cada
// frame é UM draw call:
// glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count) — o GLES 3.0
// core. O VS faz TODA a física (render/ParticleShader.h: a fórmula do
// tempo local + a balística + o billboard pela base da câmara); o FS
// aplica a textura/cor/alpha. Zero física no CPU, zero buffers por frame.
//
// O uTime é o ÚLTIMO 1f do draw (o contrato da casa — o MESMO padrão do
// uTime do céu: o pin aferra a ordem).
#include "core/Types.h"
#include "render/DrawStats.h"
#include <string>

namespace vv {

class Texture;      // a textura da partícula (lazy — setTexture)
class ProjectStorage;

class ParticleRenderer {
public:
    bool init();      // o programa + o quad das CANTOS + o VAO dos seeds
    void destroy();

    // a textura LAZY: path vazio = o sprite procedural (uHasTex 0); path
    // novo = lê do storage (readBytes → loadPng → createFromRGBA) e faz
    // cache pelo path. Falha = LOG 1× (o flag texFailed) + sprite.
    void setTexture(ProjectStorage* storage, const std::string& path);

    struct Params {
        const f32* vp = nullptr;      // mat4 column-major (o VP de sempre)
        f32  right[3] = {1, 0, 0};    // a base da câmara (sky::viewBasis)
        f32  up[3]    = {0, 1, 0};
        f32  pos[3]   = {0, 0, 0};    // a origem do emissor (o TIC)
        f32  time     = 0.0f;         // o tempo global do motor (g_fxTime)
        f32  lifetime = 2.0f;
        f32  vel[3]   = {0, 3, 0};
        f32  grav[3]  = {0, -4, 0};
        f32  wind[3]  = {0, 0, 0};    // 0.12-Y-F2: o vento global (o 1.º
                                      // WindTIC ativo; zero = a F1 byte a byte)
        f32  startSize = 0.25f;
        f32  endSize   = 0.05f;
        f32  startCol[4] = {1, 0.72f, 0.25f, 1};
        f32  endCol[4]   = {0.9f, 0.15f, 0.05f, 1};
        i32  maxParticles = 500;      // o COUNT do draw (o teto do buffer)
        bool hasTex   = false;
        // 0.12-Z2 F3 — o CLIMA VISÍVEL: a escala do volume de spawn
        // (spawnRadius/kSpawnRadius; 1.0 = o jorro da F1 byte a byte) e o
        // piso em píxeis (fx::minWorldPerDepth; 0.0 = o piso MORTO — o
        // tamanho da vida puro)
        f32  spawnScale = 1.0f;
        f32  minWorldPerDepth = 0.0f;
    };

    DrawStats draw(const Params& p) const;
    bool ok() const { return prog_ != 0; }
    u32 bufferParticles() const { return bufferCount_; }  // o buffer vivo

private:
    u32 prog_ = 0;
    u32 vao_ = 0;
    u32 vboCorner_ = 0;   // os 4 cantos do quad (divisor 0)
    u32 vboSeeds_ = 0;    // os seeds (divisor 1 — UMA VEZ pelo CPU)
    u32 bufferCount_ = 0; // o buffer nasce COM O TETO kSeedCapacity (a
                          // regeneração nunca acontece — o draw usa o
                          // count do componente, clampado ao buffer)

    i32 locVP_ = -1;
    i32 locRight_ = -1;
    i32 locUp_ = -1;
    i32 locPos_ = -1;
    i32 locVel_ = -1;
    i32 locGrav_ = -1;
    i32 locWind_ = -1;   // 0.12-Y-F2: uWind (3f — o uTime continua o
                         // ÚLTIMO 1f do draw: o contrato do pin intacto)
    i32 locSpawnScale_ = -1;   // 0.12-Z2 F3: o volume (1f, antes do uTime)
    i32 locMinW_ = -1;         // 0.12-Z2 F3: o piso em píxeis (1f, idem)
    i32 locLifetime_ = -1;     // 0.12-Z2 F3: O ACHADO P0 — o uLifetime
                               // NUNCA era carregado (o uniform ficava 0.0
                               // → mod(x,0)=NaN → TODOS os vértices morrem
                               // no clip — o sistema não desenhava NENHUM
                               // píxel no driver real desde a F1; o stub
                               // do CI não apanha — conta draws, não
                               // fragmentos)
    i32 locStartSize_ = -1;
    i32 locEndSize_ = -1;
    i32 locStartCol_ = -1;
    i32 locEndCol_ = -1;
    i32 locHasTex_ = -1;
    i32 locTex_ = -1;
    i32 locTime_ = -1;    // o ÚLTIMO 1f do draw (o contrato do pin)

    Texture* tex_ = nullptr;        // o dono (lazy — setTexture)
    std::string texPath_;           // o path carregado (o cache por path)
    bool texFailed_ = false;        // o LOG 1× (o spam morre)
};

} // namespace vv
