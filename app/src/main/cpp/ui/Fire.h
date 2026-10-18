#pragma once
// ui/Fire.h — 0.12-D (D-3) · O FOGO (ativo, selecionado, a correr).
//
// A FONTE ÚNICA da matemática do fogo, em três consumidores (o padrão
// do ui/Sdf.h da D-1):
//   1. o FS do pass SDF (render/Renderer.cpp, o ramo uSdfFireOn) — a
//      RÉPLICA GLSL linha a linha;
//   2. o rasterizador honesto do stub (tests/stub/glstub_fb.h) — este
//      header É INCLUÍDO e corre as MESMAS funções por píxel;
//   3. os pins (tests/test_012d_fogo.cpp) — chamadas diretas + píxeis.
//
// A RAMPA do dono (#7A1F0A → #E8530F → #FFA21F → #FFE08A) vive no Theme
// (fire0..fire3 — extraída do ícone real docs/iconegonevv.png; o pin
// fogo_rampa aferra as 4 cores contra a PALETA REAL do PNG).
//
// A borda de fogo do elemento ATIVO: o ruído fbm ANIMADO no tempo em
// MISTURA ADITIVA (o par GL_SRC_ALPHA/GL_ONE do pass de fogo — o stub
// aprende-o); o glow por baixo dos ativos é o quad gradiente do chamador.
//
// AS BRASAS: 10-20 partículas a subir, SÓ na splash e no ecrã de projetos
// (NUNCA no editor) — paramétricas no tempo (determinísticas: o mesmo t
// dá as MESMAS brasas; zero estado, zero alocação).
#include "core/Types.h"
#include "ui/Sdf.h"   // a FONTE ÚNICA da distância (roundRectSdf da D-1)
#include <cmath>

namespace vv {
namespace fire {

// ---- o HASH e o FBM (a réplica GLSL é linha a linha) -----------------------
inline f32 hash21(f32 x, f32 y) {
    // o hash do sítio (o mesmo do glassNoise — inteiro, determinístico)
    u32 h = (u32(std::fabs(x) * 4096.0f) * 374761393u) ^
            (u32(std::fabs(y) * 4096.0f) * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return f32(h & 0xFFFFu) / 65535.0f;
}

// o value noise com interpolação suave (a base do fbm)
inline f32 valueNoise(f32 x, f32 y) {
    const f32 xi = std::floor(x);
    const f32 yi = std::floor(y);
    const f32 xf = x - xi;
    const f32 yf = y - yi;
    const f32 u = xf * xf * (3.0f - 2.0f * xf);   // smoothstep
    const f32 v = yf * yf * (3.0f - 2.0f * yf);
    const f32 a = hash21(xi, yi);
    const f32 b = hash21(xi + 1.0f, yi);
    const f32 c = hash21(xi, yi + 1.0f);
    const f32 d = hash21(xi + 1.0f, yi + 1.0f);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

// o FBM de 2 oitavas (o ruído do prompt; o tempo desloca a 2.ª oitava —
// a chama "a correr" pela borda)
inline f32 fbm(f32 x, f32 y, f32 time) {
    const f32 n1 = valueNoise(x, y + time * 0.9f);
    const f32 n2 = valueNoise(x * 2.3f + 7.7f, y * 2.3f - time * 1.7f);
    return n1 * 0.65f + n2 * 0.35f;
}

// ---- a MÁSCARA da borda de fogo --------------------------------------------
// lx/ly LOCAIS ao topo-esquerda (o contrato dos vértices do SDF). A
// máscara vive na banda da borda (edge = 1 junto à aresta, 0 a bevelW
// para dentro) modulada pelo fbm ANIMADO — a chama corre pelo perímetro.
// Devolve TUDO o que o FS precisa: outMask (0..1, o α do flame) e outN
// (o fbm cru — o mix da cor laranja→amarelo usa-o; o FS replica o MESMO
// mix). A DISTÂNCIA vem de sdf::roundRectSdf (ui/Sdf.h — a FONTE ÚNICA
// da D-1; o FS replica as duas funções linha a linha).
inline void fireBorder(f32 lx, f32 ly, f32 hw, f32 hh, f32 radius,
                       f32 bevelW, f32 time, f32& outMask, f32& outN) {
    outMask = 0.0f;
    outN = 0.0f;
    const f32 d = sdf::roundRectSdf(lx - hw, ly - hh, hw, hh, radius);
    // a banda da borda: 1 NA aresta, morre a bevelW PARA DENTRO (d<0) e a
    // 1px PARA FORA (d>0). O clamp nos DOIS lados é essencial: a 1.ª
    // versão só matava o lado de fora e para dentro o edge CRESCIA sem
    // teto — o widget inteiro ardia (o pin fogo_borda_anima apanhou-o).
    const f32 bw = bevelW > 1e-4f ? bevelW : 1.0f;
    const f32 edge = sdf::clamp01(std::min(1.0f - d, 1.0f + d / bw));
    if (edge <= 0.0f) {
        return;
    }
    // as coords do perímetro (o ângulo) alimentam o fbm — a chama corre
    const f32 px = lx - hw;
    const f32 py = ly - hh;
    const f32 ang = std::atan2(py, px);
    outN = fbm(std::cos(ang) * 3.0f + hw, std::sin(ang) * 3.0f + hh, time);
    f32 mask = edge * (0.35f + 0.65f * outN);
    if (mask > 1.0f) mask = 1.0f;
    outMask = mask;
}

// ---- AS BRASAS (paramétricas — o mesmo t dá as mesmas brasas) --------------
// i = 0..count-1; w/h = o rect da celebração (a splash ou o projetos);
// out = (x, y em px do rect, size px, rampT 0..1). SOBE com o tempo e
// renasce por baixo (o módulo do período próprio de cada brasa).
inline void ember(int i, f32 time, f32 w, f32 h, f32 out[4]) {
    const f32 fi = f32(i);
    const f32 speed = 18.0f + hash21(fi, 1.0f) * 26.0f;      // px/s
    const f32 period = h / speed;
    const f32 phase = hash21(fi, 2.0f) * period;
    const f32 t = std::fmod(time + phase, period);
    const f32 x0 = hash21(fi, 3.0f) * w;
    const f32 wob = std::sin((time + phase) * (1.1f + hash21(fi, 4.0f))) *
                    (4.0f + hash21(fi, 5.0f) * 8.0f);
    out[0] = x0 + wob;
    out[1] = h - t * speed;                        // SOBE
    out[2] = 1.5f + hash21(fi, 6.0f) * 2.5f;       // o tamanho px
    out[3] = 1.0f - t / period;                    // o rampT (nasce quente)
}

} // namespace fire
} // namespace vv
