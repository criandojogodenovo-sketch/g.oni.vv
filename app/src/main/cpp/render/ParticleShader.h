#pragma once
// render/ParticleShader.h — a FONTE ÚNICA do sistema de partículas
// (0.12-Y-F1). O padrão Pbr.h/SkyRenderer.h: as puras C++ são o contrato,
// o GLSL replica LINHA A LINHA e os pins aferem a MESMA conta (o R-062
// aferra as expressões SEM comentários — o stripCppComments).
//
// Vertex Shader Animation (a decisão do dono): o CPU NÃO simula nada —
// escreve UMA VEZ o buffer de seeds (a origem aleatória + a semente) e o
// VS calcula a posição por partícula com o uTime global. O C33 paga:
//   - VS: 1 mod + ~10 mul/add POR VÉRTICE (4 vértices por partícula)
//   - FS: 1 textura + 1 mix (sem loops, sem noise, sem raymarch)
//
// A vida (a fórmula EXATA do prompt — o pin conta-a no fonte):
//   float localTime = mod(uTime + (seed * 100.0), lifetime);
//   pos = pos_inicial + (velocity * localTime)
//                       + (0.5 * gravity * localTime * localTime)
//                       + (uWind * localTime);   ← 0.12-Y-F2: O VENTO
//   float lifeProgress = localTime / lifetime;
//
// 0.12-Z2 FASE 3 — O CLIMA VISÍVEL (os 3 achados do F0.4 fechados):
//   1. O TAMANHO MÍNIMO EM PÍXEIS: os presets do clima (0.015 m) medem
//      ~1.3 px a 720p/dist 7 — SUB-PIXEL, invisível no device. O VS ganha
//      uMinWorldPerDepth: o tamanho da vida entra num max() com o
//      tamanho-mundo que projeta EXATAMENTE kMinParticlePx píxeis à
//      profundidade do CENTRO da partícula (gl_Position.w do centro; a
//      padronização da casa: depth-independente na orto). A pura
//      fx::minWorldPerDepth é o contrato (o pin aferra a conta).
//   2. O EMISSOR SEGUE A CÂMARA: o clima vive À VOLTA do observador — o
//      draw do TIC com AmbientEnvironment ativo usa a origem
//      fx::followOrigin (eye + lift = raio·0.5, o volume do mundo) e o
//      volume de spawn escala por uSpawnScale = spawnRadius/0.15 (o
//      buffer de seeds nasce unitário·kSpawnRadius e NUNCA regenera).
//   3. O ORÇAMENTO DO FPS: fx::WeatherBudget — a EMA do frame real
//      abaixo de 50 fps degrada o COUNT dos emissores do clima (×0.85
//      por passo, piso 0.25); acima de 58 fps recupera (o histerético).
//      O mecanismo é provado por pin com dt sintético; os números do
//      Mali GE8320 entram na VERIFICAÇÃO NO DISPOSITIVO.
#include "core/Types.h"
#include <cmath>
#include <cstring>

namespace vv {
namespace fx {

// o hash inteiro da casa (a família hashCellSky/hash3u do céu — SEM sin:
// determinístico em CPU e em GLSL, o mesmo valor nas duas pontas)
inline u32 hashFx(u32 i) {
    u32 h = i * 374761393u + 668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return h;
}

// o u32 → f32 em [0,1) (24 bits — o padrão do atlas)
inline f32 randFx(u32 h) {
    return static_cast<f32>((h >> 8u) & 0xFFFFFFu) / 16777216.0f;
}

// o buffer de SEEDS — o CPU gera UMA VEZ (na init do renderer); xyz = a
// origem ALEATÓRIA dentro da esfera de raio kSpawnRadius (o jorro nasce
// volumétrico, não num ponto), w = a semente [0,1) que desfaz o tempo.
// Determinístico: o MESMO índice dá a MESMA partícula (o pin aferra).
static constexpr f32 kSpawnRadius = 0.15f;

inline void seedFor(u32 i, f32 out[4]) {
    const u32 h0 = hashFx(i * 3u + 1u);
    const u32 h1 = hashFx(i * 3u + 2u);
    const u32 h2 = hashFx(i * 3u + 3u);
    // a esfera uniforme barata: 3 hashes → [-1,1] e NORMALIZA (o viés
    // do cubo é aceitável num jorro de 0.15 de raio — o C33 não paga
    // acos/rejection sampling por partícula)
    f32 x = randFx(h0) * 2.0f - 1.0f;
    f32 y = randFx(h1) * 2.0f - 1.0f;
    f32 z = randFx(h2) * 2.0f - 1.0f;
    const f32 len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-5f) {
        x /= len; y /= len; z /= len;
    }
    out[0] = x * kSpawnRadius;
    out[1] = y * kSpawnRadius;
    out[2] = z * kSpawnRadius;
    out[3] = randFx(hashFx(i * 7u + 5u));
}

// gera o buffer INTEIRO de uma vez (a init do renderer chama por maxParticles)
inline void generateSeeds(u32 count, f32* out /*count*4*/) {
    for (u32 i = 0; i < count; ++i) {
        seedFor(i, out + i * 4);
    }
}

// a réplica EXATA do GLSL mod(x, y) = x - y*floor(x/y) (o C++ fmod mantém
// o sinal de x — o GLSL mod NUNCA é negativo para y>0; a vida não pode
// nascer negativa)
inline f32 glslMod(f32 x, f32 y) {
    return x - y * std::floor(x / y);
}

// o TEMPO LOCAL da partícula — a semente desfaz o tempo (a fórmula do
// prompt; o M-Yf1 vive aqui: sem o termo do tempo as partículas morrem
// paradas no pino)
inline f32 localTime(f32 uTime, f32 seed, f32 lifetime) {
    return glslMod(uTime + seed * 100.0f, lifetime);
}

// a POSIÇÃO — origem + velocity*t + 0.5*gravity*t² (a fórmula do prompt)
inline void particlePos(const f32 origin[3], const f32 vel[3],
                        const f32 grav[3], f32 lt, f32 out[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = origin[i] + vel[i] * lt + 0.5f * grav[i] * lt * lt;
    }
}

// a POSIÇÃO COM VENTO (0.12-Y-F2 — a sobrecarga que o VS replica):
// origem + velocity*t + 0.5*gravity*t² + wind*t (o desvio acumulado;
// wind (0,0,0) devolve o caminho de sempre byte a byte — a F1 intacta)
inline void particlePos(const f32 origin[3], const f32 vel[3],
                        const f32 grav[3], const f32 wind[3], f32 lt,
                        f32 out[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = origin[i] + vel[i] * lt + 0.5f * grav[i] * lt * lt
                             + wind[i] * lt;
    }
}

// o PROGRESSO da vida [0,1) — interpola tamanho/cor e arma o fade out
inline f32 lifeProgress(f32 lt, f32 lifetime) {
    return (lifetime > 1e-6f) ? lt / lifetime : 0.0f;
}

// o TAMANHO interpolado pela vida
inline f32 particleSize(f32 startSize, f32 endSize, f32 prog) {
    return startSize + (endSize - startSize) * prog;
}

// ---- 0.12-Z2 FASE 3: O TAMANHO MÍNIMO EM PÍXEIS ---------------------------
// o piso do dono (2–3 px a 720p): 3.0 — a gota de 0.015 m a 720p/dist 7
// media ~1.3 px (o F0.4 PROVADO: invisível); 3 px é o piso visível no
// Mali GE8320 do alvo
inline constexpr f32 kMinParticlePx = 3.0f;

// o fator da casa (o MESMO literal dos deg2rad da ui/ e do anim — a
// conversão graus→rad aferrada)
inline constexpr f32 kDegToRad = 0.01745329252f;

// o tamanho-MUNDO que projeta minPx píxeis a 1 unidade de profundidade:
// px = mundo·viewH / (2·d·tanHalf) → mundo(d=1) = minPx·2·tanHalf/viewH.
// O VS multiplica pela profundidade REAL (gl_Position.w do centro) —
// max(szVida, isto·w). viewHPx ≤ 0 ou tanHalf ≤ 0 = PISO MORTO (0.0 —
// o caminho de sempre byte a byte; a orto usa minWorldOrtho).
inline f32 minWorldPerDepth(f32 minPx, f32 tanHalfFovY, f32 viewHPx) {
    if (viewHPx <= 0.0f || tanHalfFovY <= 0.0f) {
        return 0.0f;
    }
    return minPx * 2.0f * tanHalfFovY / viewHPx;
}

// a ORTO (o w do clip fica 1.0 em qualquer profundidade): o tamanho-mundo
// de minPx píxeis é CONSTANTE = minPx·2·meiaAltura/viewH (o orthoSize do
// CameraComp é a meia-altura do mundo)
inline f32 minWorldOrtho(f32 minPx, f32 orthoHalfH, f32 viewHPx) {
    if (viewHPx <= 0.0f || orthoHalfH <= 0.0f) {
        return 0.0f;
    }
    return minPx * 2.0f * orthoHalfH / viewHPx;
}

// o tanHalf do FOV EM GRAUS (o CameraComp fala graus; a orbit radianos —
// as duas pontas passam o tanHalf JÁ convertido a esta pura; esta é para
// o caminho de cena do gameProj: deg2rad da casa)
inline f32 tanHalfFovDeg(f32 fovDeg) {
    return std::tan(fovDeg * kDegToRad * 0.5f);
}

// ---- 0.12-Z2 FASE 3: O EMISSOR SEGUE A CÂMARA -----------------------------
// a origem do clima: ALVO + lift (o volume de spawn centrado ACIMA do
// ponto que a câmara OLHA — a chuva nasce a cair e atravessa a vista; o
// lift = raio·0.5 deixa metade do volume acima do alvo e a queda da vida
// inteira cobre a coluna. O ALVO é do chamador: no editor é o target da
// orbit, em Play é eye + fwd·meio-volume — o clima vive À VOLTA do que o
// observador OLHA, não do olho (a 1.ª run do --clima PROVOU que o olho
// põe o volume fora do frustum descendente))
inline void followOrigin(const f32 aim[3], f32 spawnRadius, f32 out[3]) {
    out[0] = aim[0];
    out[1] = aim[1] + spawnRadius * 0.5f;
    out[2] = aim[2];
}

// a escala do volume: o buffer de seeds nasce com a origem unitária
// ×kSpawnRadius (0.15) e NUNCA regenera — o raio do emissor escala o
// offset no VS. O raio default (0.15) devolve 1.0 = o caminho da F1
// byte a byte.
inline f32 spawnScale(f32 spawnRadius, f32 seedRadius) {
    return (seedRadius > 1e-6f) ? spawnRadius / seedRadius : 1.0f;
}

// ---- 0.12-Z2 FASE 3: O ORÇAMENTO DO FPS -----------------------------------
// o count dos emissores do CLIMA degrada quando o frame real passa do
// orçamento (a EMA abaixo de 50 fps — o piso do dono) e recupera acima
// de 58 (o histerético evita o dente-de-serra à beira do limite). Os
// emissores do UTILIZADOR (sem AmbientEnvironment) nunca são tocados.
struct WeatherBudget {
    static constexpr f32 kFpsFloor   = 50.0f;  // abaixo disto degrada
    static constexpr f32 kFpsRecover = 58.0f;  // acima disto recupera
    static constexpr f32 kScaleMin   = 0.25f;  // o piso do count (25%)
    static constexpr f32 kStep       = 0.85f;  // ×0.85 por passo (e /0.85
                                               // na recuperação)
    static constexpr f32 kEmaAlpha   = 0.1f;   // o peso do frame novo

    f32 scale = 1.0f;
    f32 emaDt = 1.0f / 60.0f;   // a EMA do dt (o frame real)

    // um passo por frame (dt do relógio real; dt ≤ 0 = congelado — o
    // harness/pausa não mexe no orçamento)
    void step(f32 dt) {
        if (dt <= 0.0f) {
            return;
        }
        emaDt = emaDt * (1.0f - kEmaAlpha) + dt * kEmaAlpha;
        const f32 fps = 1.0f / emaDt;
        if (fps < kFpsFloor) {
            const f32 s = scale * kStep;
            scale = s > kScaleMin ? s : kScaleMin;
        } else if (fps > kFpsRecover && scale < 1.0f) {
            const f32 s = scale / kStep;
            scale = s < 1.0f ? s : 1.0f;
        }
    }

    // o count do frame: o teto do componente × a escala (arredondado;
    // nunca acima do pedido, nunca negativo)
    i32 count(i32 requested) const {
        if (requested <= 0) {
            return 0;
        }
        const f32 c = static_cast<f32>(requested) * scale;
        i32 n = static_cast<i32>(c + 0.5f);
        if (n < 1) n = 1;             // a escala degrada o COUNT, não mata
        if (n > requested) n = requested;
        return n;
    }
};

// a COR interpolada pela vida (RGBA)
inline void particleColor(const f32 start[4], const f32 end[4], f32 prog,
                          f32 out[4]) {
    for (int i = 0; i < 4; ++i) {
        out[i] = start[i] + (end[i] - start[i]) * prog;
    }
}

// ---- O VERTEX SHADER (a réplica linha a linha das puras acima) -----------
// aCorner (loc 0)  — o canto do quad [-1,1]² (4 vértices, TRIANGLE_STRIP)
// aOriginSeed (loc 1, divisor 1) — xyz = a origem aleatória, w = a semente
// O billboard: quadPos = uPos + origem + física + right*cx*size +
// up*cy*size (a base da câmara vem do main — o MESMO viewBasis do céu).
inline constexpr const char* kVertexSrc =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location=0) in vec2 aCorner;\n"
    "layout(location=1) in vec4 aOriginSeed;\n"
    "uniform mat4 uVP;\n"
    "uniform vec3 uRight;\n"
    "uniform vec3 uUp;\n"
    "uniform vec3 uPos;\n"        // a origem do emissor (o TIC no mundo)
    "uniform vec3 uVel;\n"
    "uniform vec3 uGrav;\n"
    "uniform vec3 uWind;\n"       // 0.12-Y-F2: o vento global (o WindTIC)
    "uniform float uSpawnScale;\n"  // 0.12-Z2 F3: o volume do clima (o raio)
    "uniform float uMinWorldPerDepth;\n"  // 0.12-Z2 F3: o piso em píxeis
    "uniform float uTime;\n"
    "uniform float uLifetime;\n"
    "uniform float uStartSize;\n"
    "uniform float uEndSize;\n"
    "out vec2 vUv;\n"
    "out float vProgress;\n"
    "void main() {\n"
    "    float seed = aOriginSeed.w;\n"
    "    float localTime = mod(uTime + (seed * 100.0), uLifetime);\n"
    "    vec3 p = uPos + (aOriginSeed.xyz * uSpawnScale)\n"
    "              + (uVel * localTime)\n"
    "              + (0.5 * uGrav * localTime * localTime)\n"
    "              + (uWind * localTime);\n"
    "    float lifeProgress = localTime / uLifetime;\n"
    "    float sz = uStartSize + (uEndSize - uStartSize) * lifeProgress;\n"
    "    vec4 center = uVP * vec4(p, 1.0);\n"
    "    if (center.w > 0.0) sz = max(sz, uMinWorldPerDepth * center.w);\n"
    "    vec3 quad = p + uRight * (aCorner.x * sz)\n"
    "                   + uUp * (aCorner.y * sz);\n"
    "    gl_Position = uVP * vec4(quad, 1.0);\n"
    "    vUv = aCorner * 0.5 + 0.5;\n"
    "    vProgress = lifeProgress;\n"
    "}\n";

// ---- O FRAGMENT SHADER — lê a textura (ou o sprite procedural) e aplica
// a cor + o alpha (o FADE OUT no fim da vida). Sem loops, sem noise.
inline constexpr const char* kFragmentSrc =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision mediump int;\n"
    "in vec2 vUv;\n"
    "in float vProgress;\n"
    "uniform sampler2D uTex;\n"
    "uniform float uHasTex;\n"
    "uniform vec4 uStartCol;\n"
    "uniform vec4 uEndCol;\n"
    "out vec4 fragColor;\n"
    "float sstep(float e0, float e1, float x) {\n"
    "    float d = e1 - e0;\n"
    "    float t = d != 0.0 ? (x - e0) / d : 0.0;\n"
    "    t = clamp(t, 0.0, 1.0);\n"
    "    return t * t * (3.0 - 2.0 * t);\n"
    "}\n"
    "void main() {\n"
    "    vec4 col = vec4(1.0);\n"
    "    if (uHasTex > 0.5) {\n"
    "        col = texture(uTex, vUv);\n"
    "    } else {\n"
    "        float d = length(vUv - 0.5) * 2.0;\n"
    "        col.a = 1.0 - sstep(0.55, 1.0, d);\n"   // o disco suave
    "    }\n"
    "    vec4 tint = mix(uStartCol, uEndCol, vProgress);\n"
    "    float fade = 1.0 - vProgress;\n"            // o fade out no fim
    "    fragColor = col * tint * vec4(1.0, 1.0, 1.0, fade);\n"
    "    if (fragColor.a < 0.004) discard;\n"
    "}\n";

} // namespace fx
} // namespace vv
