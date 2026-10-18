#pragma once
// ui/Sdf.h — 0.12-D (D-1) · O SHADER SDF ÚNICO DE WIDGET.
//
// A matemática PURA (GL-free, host-testável) que desenha QUALQUER widget
// (retângulo arredondado) com: cantos arredondados PERFEITOS (SDF analítico,
// anti-aliased por cobertura), BISEL 3D (a normal derivada do gradiente da
// distância), REALCE ESPECULAR no topo, BRILHO INTERNO nas bordas e SOMBRA
// PROJETADA por baixo — tudo numa ÚNICA passagem.
//
// A FONTE ÚNICA em três consumidores:
//   1. o FS do pass SDF (render/Renderer.cpp, kSdfFsSrc) — a RÉPLICA GLSL
//      linha a linha (GLSL não inclui C++); a alterar, alterar EM AMBOS;
//   2. o rasterizador do stub (tests/stub/glstub_fb.h) — este header É
//      INCLUÍDO e a função shade() corre por píxel (os pins do C33
//      aferem os MESMOS pixéis que o device desenha);
//   3. os pins (tests/test_012d_sdf.cpp) — chamadas diretas + píxeis.
//
// O TOGGLE da cláusula anti-overengineering: "Efeitos: Alto / Médio /
// Baixo" — g_level (o default de fábrica é ALTO; o Settings da D-7 escreve
// aqui). Em BAIXO: só bisel + sombra simples (a ordem expressa).
//
// A cláusula anti-superficialidade: um botão ATIVO mostra bisel + brilho +
// fogo (o gradiente accent→accent2 da Y-1) + sombra SIMULTANEOUSLY — o
// shade() compõe TUDO numa passagem (nada é fachada).
#include "core/Types.h"
#include <cmath>

namespace vv {
namespace sdf {

// ---- o TOGGLE (Efeitos: Alto / Médio / Baixo) -------------------------------
enum : u8 { LEVEL_BAIXO = 0, LEVEL_MEDIO = 1, LEVEL_ALTO = 2 };
inline u8 g_level = LEVEL_ALTO;   // o default de fábrica; Settings escreve

// os PARÂMETROS POR NÍVEL — `s` = theme::dp(1.0f) (a densidade entra UMA
// vez, aqui; tudo o resto é px). Replicado no END FRAME do Renderer (os
// uniforms uSdf*) — a alterar, alterar EM AMBOS.
struct LevelParams {
    bool bevel;                            // os 3 níveis têm bisel
    bool spec;                             // realce especular no topo
    bool glow;                             // brilho interno nas bordas
    bool shadow;                           // sombra projetada por baixo
    f32 bevelW, specDepth, glowW;          // larguras em px
    f32 shadowDy, shadowSoft, shadowA;     // offset/soft/α da sombra
    f32 specAdd, glowAdd;                  // intensidades
};

inline LevelParams paramsFor(u8 level, f32 s) {
    LevelParams p{};
    p.bevel  = true;
    p.shadow = true;
    p.bevelW   = 3.0f * s;
    p.specDepth = 5.0f * s;
    p.glowW    = 2.5f * s;
    if (level == LEVEL_BAIXO) {
        // A ORDEM EXPRESSA: em Baixo SÓ bisel + sombra simples
        p.spec = false;
        p.glow = false;
        p.shadowDy   = 1.5f * s;
        p.shadowSoft = 1.5f * s;   // simples: sem penumbra larga
        p.shadowA    = 0.30f;
        p.specAdd = 0.0f;
        p.glowAdd = 0.0f;
        return p;
    }
    p.shadowDy   = 2.5f * s;
    p.shadowSoft = 3.5f * s;
    p.shadowA    = 0.35f;
    p.spec  = true;
    p.specAdd = (level == LEVEL_MEDIO) ? 0.22f : 0.35f;
    p.glow  = (level == LEVEL_ALTO);
    p.glowAdd = p.glow ? 0.15f : 0.0f;
    return p;
}

inline f32 clamp01(f32 x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// ---- o SDF do retângulo arredondado (a fórmula analítica clássica) ---------
// p = posição LOCAL ao CENTRO do widget (px); b = meias-extensões; r = raio
// (clampado a min(b) — o MESMO clamp do panelRounded). <0 dentro, 0 na
// aresta, >0 fora — em PÍXEIS (a AA usa esta unidade).
inline f32 roundRectSdf(f32 px, f32 py, f32 hw, f32 hh, f32 r) {
    const f32 m = hw < hh ? hw : hh;
    const f32 rr = r < m ? (r > 0.0f ? r : 0.0f) : m;
    if (rr <= 0.0f) {   // retângulo cru
        const f32 dx = std::fabs(px) - hw;
        const f32 dy = std::fabs(py) - hh;
        const f32 ox = dx > 0.0f ? dx : 0.0f;
        const f32 oy = dy > 0.0f ? dy : 0.0f;
        return (ox > 0.0f || oy > 0.0f) ? std::sqrt(ox * ox + oy * oy)
                                        : (dx > dy ? dx : dy);
    }
    const f32 qx = std::fabs(px) - hw + rr;
    const f32 qy = std::fabs(py) - hh + rr;
    const f32 ax = qx > 0.0f ? qx : 0.0f;
    const f32 ay = qy > 0.0f ? qy : 0.0f;
    return std::sqrt(ax * ax + ay * ay) +
           ((qx > qy ? qx : qy) < 0.0f ? (qx > qy ? qx : qy) : 0.0f) - rr;
}

// A NORMAL analítica (o gradiente da distância, unitário; coords y-down) —
// o bisel 3D deriva DELA (a ordem do prompt: «normal derivada do gradiente
// da distância»). Dentro do corpo é axial; no canto aponta para fora do
// arco; na aresta é perpendicular.
inline void roundRectNormal(f32 px, f32 py, f32 hw, f32 hh, f32 r,
                            f32 out[2]) {
    const f32 m = hw < hh ? hw : hh;
    const f32 rr = r < m ? (r > 0.0f ? r : 0.0f) : m;
    const f32 qx = std::fabs(px) - hw + rr;
    const f32 qy = std::fabs(py) - hh + rr;
    if (qx > 0.0f || qy > 0.0f) {   // região exterior/canto
        const f32 ax = qx > 0.0f ? qx : 0.0f;
        const f32 ay = qy > 0.0f ? qy : 0.0f;
        const f32 l = std::sqrt(ax * ax + ay * ay);
        f32 nx = l > 1e-6f ? ax / l : 1.0f;
        f32 ny = l > 1e-6f ? ay / l : 0.0f;
        if (px < 0.0f) nx = -nx;
        if (py < 0.0f) ny = -ny;
        out[0] = nx;
        out[1] = ny;
        return;
    }
    if (qx > qy) {   // aresta esquerda/direita
        out[0] = px >= 0.0f ? 1.0f : -1.0f;
        out[1] = 0.0f;
        return;
    }
    out[0] = 0.0f;   // aresta topo/fundo
    out[1] = py >= 0.0f ? 1.0f : -1.0f;
}

// ---- O SHADE — A FUNÇÃO ÚNICA (réplica GLSL em kSdfFsSrc) ------------------
struct Shade {
    f32 hw, hh, radius;      // o widget em px (locais ao CENTRO)
    f32 top[4];              // cor no TOPO (rgba — α é o vidro)
    f32 bot[4];              // cor no FUNDO (gradiente vertical)
    LevelParams p;
};

// luz do TOPO-ESQUERDA (coords y-down): L = normalize(-0.55, -0.83)
constexpr f32 kLightX = -0.5534f;
constexpr f32 kLightY = -0.8329f;

// out = rgba STRAIGHT (o par de blend do pass UI: SRC_ALPHA/ONE_MINUS).
// lx/ly = LOCAL ao TOPO-ESQUERDA do widget (o mesmo contrato dos vértices
// aLocal — a sombra vive em lx/ly fora do widget). aa = largura da
// suavização de aresta em px (1.0 no device e no stub — a distância É em
// px; os DOIS lados produzem a MESMA cobertura).
inline void shade(const Shade& s, f32 lx, f32 ly, f32 aa, f32 out[4]) {
    const f32 aaw = aa > 1e-4f ? aa : 1.0f;
    // o SDF é CENTER-RELATIVE: converte o local do topo-esquerda
    const f32 px = lx - s.hw;
    const f32 py = ly - s.hh;
    const f32 d = roundRectSdf(px, py, s.hw, s.hh, s.radius);
    // cobertura do widget (cantos PERFEITOS: a AA é a própria distância)
    const f32 aW = clamp01(0.5f - d / aaw);
    // a SOMBRA: o SDF do widget DESLOCADO para baixo, só FORA do widget —
    // o contato (logo à aresta) é forte, a penumbra fade para fora
    f32 aS = 0.0f;
    if (s.p.shadow) {
        const f32 dS = roundRectSdf(px, py - s.p.shadowDy, s.hw, s.hh,
                                    s.radius);
        const f32 soft = s.p.shadowSoft > 1e-4f ? s.p.shadowSoft : aaw;
        aS = s.p.shadowA * clamp01(0.5f - dS / soft) * (1.0f - aW);
    }
    const f32 aOut = clamp01(aW + aS);
    if (aOut <= 0.002f) {   // fora de tudo — transparente
        out[0] = out[1] = out[2] = out[3] = 0.0f;
        return;
    }
    // a cor base: o gradiente vertical amostrado em ly (o MESMO contrato
    // do emitGradientV da Y-1 — topo/fundo nas extremidades do widget)
    const f32 t = s.hh > 1e-4f ? clamp01((py + s.hh) / (2.0f * s.hh)) : 0.0f;
    f32 r = s.top[0] + (s.bot[0] - s.top[0]) * t;
    f32 g = s.top[1] + (s.bot[1] - s.top[1]) * t;
    f32 b = s.top[2] + (s.bot[2] - s.top[2]) * t;
    // a normal (uma vez; bisel e especular partilham)
    f32 n[2] = {0.0f, -1.0f};
    if (s.p.bevel || s.p.spec) {
        roundRectNormal(px, py, s.hw, s.hh, s.radius, n);
    }
    // o BISEL 3D: a banda junto à aresta acende segundo n·L (a normal da
    // aresta de TOPO aponta PARA a luz → lam 0.83 → mais CLARA; a de fundo
    // lam 0 → mais ESCURA; o volume do bisel)
    if (s.p.bevel && aW > 0.0f) {
        const f32 lam = clamp01(kLightX * n[0] + kLightY * n[1]);
        const f32 bw = s.p.bevelW > 1e-4f ? s.p.bevelW : 1.0f;
        const f32 edge = clamp01(1.0f + d / bw);   // 1 na aresta → 0 dentro
        const f32 mul = 1.0f + (lam - 0.5f) * 0.9f * edge;   // topo +30%,
                                                             // fundo −45%
        r *= mul;
        g *= mul;
        b *= mul;
    }
    // o REALCE ESPECULAR no topo: banda estreita junto à aresta superior
    if (s.p.spec) {
        const f32 up = clamp01(-n[1]);
        const f32 sd = s.p.specDepth > 1e-4f ? s.p.specDepth : 1.0f;
        const f32 band = clamp01(1.0f + d / sd);
        const f32 sp = s.p.specAdd * band * band * up * up;
        r += sp;
        g += sp;
        b += sp;
    }
    // o BRILHO INTERNO nas bordas: anel logo a seguir ao bisel (decay exp)
    if (s.p.glow) {
        const f32 depth = -d - s.p.bevelW;
        const f32 gw = s.p.glowW > 1e-4f ? s.p.glowW : 1.0f;
        const f32 ring = std::exp(-(depth > 0.0f ? depth : 0.0f) / gw);
        const f32 ga = s.p.glowAdd * ring;
        r += (1.0f - r) * 0.6f * ga;   // mistura para branco quente
        g += (1.0f - g) * 0.6f * ga;
        b += (1.0f - b) * 0.6f * ga;
    }
    // composição STRAIGHT: widget sobre sombra preta sobre o destino
    out[0] = (r * aW) / aOut;   // a sombra é PRETA — puxa por baixo
    out[1] = (g * aW) / aOut;
    out[2] = (b * aW) / aOut;
    out[3] = aOut;
}

// o quanto o QUAD da emissão cresce para CONTER a sombra + a AA (o pad por
// lado: topo/esq/dir só AA; fundo AA + offset + penumbra)
inline f32 shadowPad(const LevelParams& p) {
    return p.shadow ? (p.shadowDy + p.shadowSoft) : 0.0f;
}

} // namespace sdf
} // namespace vv
