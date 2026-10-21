#pragma once
// render/SkyRenderer.h — 0.12-Y (3.Y-D) — O CÉU PROCEDURAL COM NUVENS
// ANIMADAS (a ordem do dono: «Adicionar Céu Procedural com Nuvens
// Animadas ao WorldEnvironment»; a cláusula calma: performance mobile é
// prioridade, NADA de raymarching volumétrico 3D).
//
// O desenho: UM quad fullscreen de 4 vértices em NDC desenhado PRIMEIRO
// no frame (depth test/write OFF — é o fundo; tudo o que vem depois
// pinta por cima; z=1.0 — o longe de sempre). O FS reconstrói a direção
// da vista por pixel a partir da BASE da câmara (uRight/uUp/uFwd — a
// extração pura viewBasis abaixo, válida para a orbit E para a câmara
// de jogo, as duas passam por lookAt) e pinta:
//   · o GRADIENTE: horizonte → zénite por dir.y (clamp 0..1);
//   · o SOL: o halo (pow) ANTES das nuvens + o disco DEPOIS (morre
//     atrás da nuvem densa);
//   · as NUVENS: FBM 2D de value noise POR HASH INTEIRO (a família do
//     ui/Noise2D.h — determinístico, SEM sin(), o domínio é Z² INTEIRO:
//     o vento anda para sempre sem emenda nem tile), projeção planar
//     dir.xz/dir.y, o UV anda com o TEMPO × a velocidade do vento;
//   · a COBERTURA: 0 = céu limpo, 1 = nublado (smoothstep pela fonte).
//
// O padrão da casa (render/Pbr.h): as FUNÇÕES PURAS aqui em baixo são a
// FONTE ÚNICA — o FS do SkyRenderer.cpp replica LINHA A LINHA e os pins
// aferem a MESMA conta (a mutação M-Yd1 mata o tempo e os pins vêem).
//
// GL-free nas puras (o CI Linux corre sem GLES).
#include "core/Types.h"
#include "math/Math.h"
#include "render/DrawStats.h"
#include <cmath>

namespace vv {
namespace sky {

// ---- as constantes do céu (as MESMAS no FS — a fonte) ----------------------
constexpr f32 kCloudScale = 3.0f;     // o zoom do padrão de nuvens (UV/mundo)
constexpr f32 kCloudSoft  = 0.2f;     // a suavidade da borda da nuvem
constexpr f32 kSunCosDisk = 0.9993f;  // ~2.1° — o limiar do disco
constexpr f32 kSunCosSoft = 0.0004f;  // a penumbra do disco
constexpr f32 kSunGlowPow = 24.0f;    // o expoente do halo
constexpr f32 kSunGlowGain = 0.35f;   // o ganho do halo
constexpr f32 kHorizonFadeY = 0.15f;  // as nuvens morrem neste dir.y
constexpr f32 kPlaneEps = 0.05f;      // o piso da projeção planar (o
                                      // horizonte não divide por ~0)
constexpr f32 kWindDirX = 1.0f;       // a direção do vento (não unitária —
constexpr f32 kWindDirY = 0.35f;      // a magnitude dobra na velocidade)
constexpr f32 kCloudShadow = 0.75f;   // o piso da sombra da nuvem (o
constexpr f32 kCloudLight  = 0.25f;   // 0.75+0.25·n — o ventre escuro)
constexpr f32 kCloudSunMix = 0.35f;   // o tint do sol na nuvem

// smoothstep de GLSL (a réplica da casa — o FS chama o nativo)
inline f32 sstep(f32 e0, f32 e1, f32 x) {
    const f32 d = e1 - e0;
    f32 t = d != 0.0f ? (x - e0) / d : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// o hash INTEIRO da célula (a família do ui/Noise2D.h hashCell — SEM sin:
// determinístico em CPU e em GPU, o domínio é Z² inteiro — o vento anda
// sem emenda; a semântica uint é a mesma no GLSL ES 3.0)
inline u32 hashCellSky(i32 x, i32 y) {
    u32 h = static_cast<u32>(x) * 374761393u +
            static_cast<u32>(y) * 668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return h;
}

// o ruído da CÉLULA [0,1] (16 bits — 256 valores bandeiam o FBM)
inline f32 hash21(i32 x, i32 y) {
    return static_cast<f32>(hashCellSky(x, y) & 0xFFFFu) / 65535.0f;
}

// o VALUE NOISE suavizado [0,1] (bilinear + smoothstep — o mesmo esqueleto
// do noise2d::noisePixel, agora em float livre, sem tile de 64)
inline f32 valueNoise2(f32 u, f32 v) {
    const f32 gu = std::floor(u);
    const f32 gv = std::floor(v);
    const i32 x0 = static_cast<i32>(gu);
    const i32 y0 = static_cast<i32>(gv);
    f32 tx = u - gu;
    f32 ty = v - gv;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    const f32 a = hash21(x0, y0);
    const f32 b = hash21(x0 + 1, y0);
    const f32 c = hash21(x0, y0 + 1);
    const f32 d = hash21(x0 + 1, y0 + 1);
    return (a * (1.0f - tx) + b * tx) * (1.0f - ty) +
           (c * (1.0f - tx) + d * tx) * ty;
}

// o FBM de DUAS oitavas (mobile: 2, não 5 — o custo por pixel manda) —
// a 2.ª oitava desloca-se e escala (a descorrelação, sem matriz)
inline f32 fbm2(f32 u, f32 v) {
    return 0.65f * valueNoise2(u, v) +
           0.35f * valueNoise2(u * 2.13f + 17.7f, v * 2.13f + 9.2f);
}

// o OFFSET do VENTO no UV (o TEMPO é a alma — a mutação M-Yd1 mata o tempo
// e os pins vêem: o mesmo t = o mesmo offset; speed 0 = parado de sempre)
inline void windOffset(f32 time, f32 speed, f32 out[2]) {
    out[0] = time * speed * kWindDirX;
    out[1] = time * speed * kWindDirY;
}

// a COBERTURA: 0 = céu limpo (o limiar fica acima de TODO o FBM), 1 =
// nublado (o limiar a 0 — quase tudo é nuvem). O FBM vive em [0,1].
inline f32 cloudAlpha(f32 n, f32 coverage) {
    const f32 t = 1.0f - coverage;
    return sstep(t, t + kCloudSoft, n);
}

// o GRADIENTE: horizonte → zénite por dir.y (o clamp 0..1 — abaixo do
// horizonte fica a cor do horizonte, o chão do quad é coberto pelo grid)
inline void skyGradient(f32 dirY, const f32 hor[3], const f32 zen[3],
                        f32 out[3]) {
    f32 t = dirY;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    out[0] = hor[0] + (zen[0] - hor[0]) * t;
    out[1] = hor[1] + (zen[1] - hor[1]) * t;
    out[2] = hor[2] + (zen[2] - hor[2]) * t;
}

// o DISCO do sol (a penumbra kSunCosSoft — uma aresta dura no sol é feia)
inline f32 sunDisk(f32 cosA) {
    return sstep(kSunCosDisk, kSunCosDisk + kSunCosSoft, cosA);
}

// o HALO (o glow — o pow cresce para o disco; o max 0 evita o verso)
inline f32 sunGlow(f32 cosA) {
    const f32 d = cosA > 0.0f ? cosA : 0.0f;
    return std::pow(d, kSunGlowPow) * kSunGlowGain;
}

// o fade das nuvens no horizonte (a projeção planar explode a dir.y~0 —
// o alpha morre ANTES de a distorção aparecer no ecrã)
inline f32 horizonFade(f32 dirY) {
    return sstep(kPlaneEps, kHorizonFadeY, dirY);
}

// ---- a BASE da câmara a partir da VISTA (a extração pura) ------------------
// O lookAt da casa (math/Math.h) guarda as LINHAS right/up/−fwd na rotação
// column-major: right=(m0,m4,m8) · up=(m1,m5,m9) · fwd=−(m2,m6,m10). Vale
// para a orbit do editor E para a câmara de jogo (as duas passam por
// lookAt). false = vista degenerada (fwd nulo — não devia acontecer).
inline bool viewBasis(const Mat4& view, Vec3& right, Vec3& up, Vec3& fwd) {
    right = Vec3{view.m[0], view.m[4], view.m[8]};
    up = Vec3{view.m[1], view.m[5], view.m[9]};
    fwd = Vec3{-view.m[2], -view.m[6], -view.m[10]};
    const f32 len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y +
                              fwd.z * fwd.z);
    return len > 1e-6f;
}

// o tan(fovY/2) e o ASPECT a partir da PROJEÇÃO (perspective da casa:
// m[5]=1/tan(fovY/2), m[0]=f/aspect → aspect=m[5]/m[0]). Guards: m[0]==0
// (a janela degenerada) devolve os neutros (o pin aferre).
inline f32 tanFovYFromProj(const Mat4& proj) {
    return proj.m[5] != 0.0f ? 1.0f / proj.m[5] : 0.5774f;
}
inline f32 aspectFromProj(const Mat4& proj) {
    return proj.m[0] != 0.0f ? proj.m[5] / proj.m[0] : 1.0f;
}

// ---- o RECT do FS (a fórmula ÚNICA da direção — o FS replica) --------------
// dir = normalize(fwd + ndc.x·aspect·tanF·right + ndc.y·tanF·up)
inline void viewDir(f32 ndcX, f32 ndcY, const Vec3& right, const Vec3& up,
                    const Vec3& fwd, f32 aspect, f32 tanF, Vec3& out) {
    out = Vec3{fwd.x + ndcX * aspect * tanF * right.x +
                   ndcY * tanF * up.x,
               fwd.y + ndcX * aspect * tanF * right.y +
                   ndcY * tanF * up.y,
               fwd.z + ndcX * aspect * tanF * right.z +
                   ndcY * tanF * up.z};
}

} // namespace sky

// ---- o PROGRAMA GL (o irmão do Grid: quad 4 vértices + shader) -------------
class SkyRenderer {
public:
    // os parâmetros de UM frame (o main constrói; o draw só consome)
    struct Params {
        Vec3 right{1.0f, 0.0f, 0.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};
        Vec3 fwd{0.0f, 0.0f, -1.0f};
        f32  aspect = 1.0f;
        f32  tanHalfFovY = 0.5774f;   // tan(30°) — o fovY de sempre
        f32  time = 0.0f;             // o relógio das nuvens (g_skyTime)
        f32  horizon[3] = {0.2902f, 0.5647f, 0.8863f};   // #4A90E2
        f32  zenith[3]  = {0.1176f, 0.2275f, 0.5412f};   // #1E3A8A
        f32  sunCol[3]  = {1.0f, 0.8667f, 0.6667f};      // #FFDDAA
        f32  sunDir[3]  = {0.5571f, 0.7428f, -0.3714f};  // a tarde
        f32  coverage = 0.4f;         // 0 = limpo, 1 = nublado
        f32  speed = 0.02f;           // o vento (UV/s)
    };

    bool init();      // compila/linka o programa + o quad (o padrão Grid)
    void destroy();
    bool ok() const { return vao_ != 0; }

    // glUseProgram do programa do céu (o padrão LitMaterial::use — o pin
    // consulta os uniforms PELO NOME com o programa corrente; o corpo vive
    // no .cpp — o header é GL-free)
    void use() const;

    // desenha o quad fullscreen (depth test/write OFF, blend OFF — é o
    // fundo). Devolve as métricas (4 vértices, 1 draw).
    DrawStats draw(const Params& p) const;

    u32 vertexCount() const { return vertexCount_; }

private:
    u32 prog_ = 0;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    i32 locRight_ = -1;
    i32 locUp_ = -1;
    i32 locFwd_ = -1;
    i32 locAspect_ = -1;
    i32 locTanF_ = -1;
    i32 locTime_ = -1;     // uTime — a M-Yd1 vive AQUI (o último 1f do draw)
    i32 locSpeed_ = -1;
    i32 locCoverage_ = -1;
    i32 locHor_ = -1;
    i32 locZen_ = -1;
    i32 locSunCol_ = -1;
    i32 locSunDir_ = -1;
    u32 vertexCount_ = 0;
};

} // namespace vv
