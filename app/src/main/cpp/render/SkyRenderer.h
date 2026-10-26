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
#include "components/Environment.h"   // 0.12-W-1: o mapeamento ÚNICO WE→Params
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

// o GRADIENTE: horizonte → zénite por dir.y (o clamp 0..1). 0.12-Z2 F2
// (o ACHADO da FASE 0 no GLES REAL — a captura B5_we_meianoite.png): o
// abaixo-do-horizonte ficava na COR DO HORIZONTE — o meio-plano inferior
// era uma ZONA AZUL/ESCURA FIXA que «cortava» a grelha (o sintoma do
// dono). AGORA: abaixo do horizonte o céu escurece para o TOM DE CHÃO
// (o horizonte × kGroundDim) com a transição suave de kGroundFadeY — a
// linha do horizonte FICA VISÍVEL e a grelha nunca «afunda» num azul fixo.
inline constexpr f32 kGroundDim   = 0.45f;  // o tom de chão (o horizonte ×)
inline constexpr f32 kGroundFadeY = 0.18f;  // a meia-largura da transição
inline void skyGradient(f32 dirY, const f32 hor[3], const f32 zen[3],
                        f32 out[3]) {
    f32 t = dirY;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    f32 sky[3] = {hor[0] + (zen[0] - hor[0]) * t,
                  hor[1] + (zen[1] - hor[1]) * t,
                  hor[2] + (zen[2] - hor[2]) * t};
    // o peso do CHÃO: 0 acima do horizonte → 1 a dir.y = -kGroundFadeY
    f32 g = sstep(0.0f, -kGroundFadeY, dirY);
    out[0] = sky[0] + (hor[0] * kGroundDim - sky[0]) * g;
    out[1] = sky[1] + (hor[1] * kGroundDim - sky[1]) * g;
    out[2] = sky[2] + (hor[2] * kGroundDim - sky[2]) * g;
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

// ---- 0.12-Y-E: O CICLO DIA/NOITE + AS ESTRELAS + A ALTURA DAS NUVENS ----
// A otimização mobile (o relatório 0.12-Y-E-SKY): a PALETA do dia/noite e o
// ÂNGULO do sol correm UMA VEZ POR FRAME no CPU (main.cpp consome as puras
// abaixo e manda as cores JÁ graduadas pelos uniforms que o céu JÁ TEM) —
// o FS não paga NENHUMA cadeia nova por pixel além do campo de estrelas,
// que fica ATRÁS de um branch uniforme (uStars — o dia salta-o inteiro).

constexpr f32 kStarDensity = 60.0f;   // as células do campo na cúpula
constexpr f32 kStarRadius  = 0.22f;   // o raio do ponto (em células)
constexpr f32 kStarThresh  = 184.0f;  // o hash <isto = célula VAZIA (~72%)
constexpr f32 kStarHorizonY = 0.08f;  // as estrelas morrem no horizonte
constexpr f32 kSunHourRad  = 3.14159265f / 12.0f;  // 1h = 15° de arco

// o hash 3D (a família do hashCellSky — SEM sin, determinístico em CPU e
// em GLSL; o domínio é Z³ inteiro)
inline u32 hash3u(i32 x, i32 y, i32 z) {
    u32 h = static_cast<u32>(x) * 374761393u +
            static_cast<u32>(y) * 668265263u +
            static_cast<u32>(z) * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return h;
}

// o campo de ESTRELAS [0,1]: pontos brancos estocásticos — a célula 3D da
// direção tem uma estrela só acima do THRESHOLD do hash (o céu não vira
// poeira); o ponto vive num offset 0.2..0.8 da célula (nunca corta)
// 0.12-W-1: o LIMAR deixa de ser constante — nasce do starDensity do
// WorldEnvironment (starThreshFromDensity abaixo). A sobrecarga de 3 args
// mantém o campo DE SEMPRE (limiar 184) — os pins da Y-E ficam intactos.
inline f32 starFieldThresh(f32 dx, f32 dy, f32 dz, f32 thresh) {
    const f32 sx = dx * kStarDensity;
    const f32 sy = dy * kStarDensity;
    const f32 sz = dz * kStarDensity;
    const i32 cx = static_cast<i32>(std::floor(sx));
    const i32 cy = static_cast<i32>(std::floor(sy));
    const i32 cz = static_cast<i32>(std::floor(sz));
    const u32 h = hash3u(cx, cy, cz);
    if (static_cast<f32>(h & 0xFFu) < thresh) {
        return 0.0f;
    }
    const f32 ox = 0.2f + 0.6f * static_cast<f32>((h >> 8u) & 0xFFu) / 255.0f;
    const f32 oy = 0.2f + 0.6f * static_cast<f32>((h >> 16u) & 0xFFu) / 255.0f;
    const f32 oz = 0.2f + 0.6f * static_cast<f32>((h >> 24u) & 0xFFu) / 255.0f;
    const f32 ax = sx - static_cast<f32>(cx) - ox;
    const f32 ay = sy - static_cast<f32>(cy) - oy;
    const f32 az = sz - static_cast<f32>(cz) - oz;
    const f32 d = std::sqrt(ax * ax + ay * ay + az * az);
    return 1.0f - sstep(0.0f, kStarRadius, d);
}
inline f32 starField(f32 dx, f32 dy, f32 dz) {
    return starFieldThresh(dx, dy, dz, kStarThresh);
}

// 0.12-W-1 — A DENSIDADE DAS ESTRELAS DO UTILIZADOR: o limiar do campo nasce
// do starDensity (0..1). A CONTA preserva o olhar da Y-E: density 0.5 = o
// limiar 184 de SEMPRE (byte a byte — 28,1% das células têm estrela);
// density 0 = 256 (nenhuma passa — o campo morto); density 1 = 112 (43,7%
// — denso sem virar poeira). preenchido(d) = 0.5625·d → limiar = 256−144·d.
inline f32 starThreshFromDensity(f32 density) {
    if (!(density > 0.0f)) {
        return 256.0f;   // 0 (ou lixo saneado) = SEM estrelas
    }
    if (density > 1.0f) {
        density = 1.0f;
    }
    return 256.0f - 144.0f * density;
}

// o ÂNGULO do sol pelo relógio: 06h nasce a LESTE (+x, no horizonte), 12h
// no ZÉNITE, 18h se põe a OESTE (−x), 00h sob o mundo (a noite). O eixo z
// leva o tilte fixo da tarde da D (o arco inclina — não é um círculo seco).
inline void sunDirFromTime(f32 tod, f32 out[3]) {
    const f32 th = (tod - 6.0f) * kSunHourRad;
    out[0] = std::cos(th);
    out[1] = std::sin(th);
    out[2] = -0.25f;
    const f32 len = std::sqrt(out[0] * out[0] + out[1] * out[1] +
                              out[2] * out[2]);
    if (len > 1e-6f) {
        out[0] /= len;
        out[1] /= len;
        out[2] /= len;
    }
}

// ---- 0.12-W-1: A LUA — o disco, as FASES, o glow (a ordem do dono) --------
// A lua vive OPSTA ao sol (a posição oposta: lua = −sol — à noite o sol está
// sob o mundo e a lua nas ALTURAS); o DISCO é branco-amarelado com o glow
// subtil à volta; a fase controla a iluminação do disco (o terminador).
constexpr f32 kMoonCosDisk = 0.99945f;  // ~1.9° — um tico menor que o sol
constexpr f32 kMoonCosSoft = 0.0004f;   // a penumbra da borda do disco
constexpr f32 kMoonGlowPow = 16.0f;     // o halo (mais largo que o do sol)
constexpr f32 kMoonGlowGain = 0.18f;    // o ganho (SUBTIL — a ordem do dono)
constexpr f32 kMoonTermSoft = 0.08f;    // a penumbra do terminador
constexpr f32 kMoonCol[3] = {0.98f, 0.95f, 0.86f};   // branco-amarelado

// a posição da LUA pelo relógio (oposta ao sol — 22h: sol embaixo, lua alta)
inline void moonDirFromTime(f32 tod, f32 out[3]) {
    sunDirFromTime(tod, out);
    out[0] = -out[0];
    out[1] = -out[1];
    out[2] = -out[2];
}

// o DISCO da lua (a penumbra da casa — o MESMO sstep do sol)
inline f32 moonDisk(f32 cosA) {
    return sstep(kMoonCosDisk, kMoonCosDisk + kMoonCosSoft, cosA);
}

// o HALO da lua (o glow subtil — pow largo, ganho pequeno)
inline f32 moonGlow(f32 cosA) {
    const f32 d = cosA > 0.0f ? cosA : 0.0f;
    return std::pow(d, kMoonGlowPow) * kMoonGlowGain;
}

// A FASE — a iluminação do disco (u = a coordenada horizontal do ponto no
// disco, −1..1; o terminador LINEAR: a fração iluminada é exata nas 4
// fases-chave e o lado que encolhe troca no 0.5):
//   frac = (1−cos(2π·fase))/2   → 0=nova · 0.5=quarto · 1=cheia
//   b    = 2·frac−1             → a borda da sombra no eixo do disco
//   uEff = fase<0.5 ? u : −u    → crescente encolhe dum lado, minguante do outro
//   lit  = 1−sstep(b−ε, b+ε, uEff)
inline f32 moonPhaseLit(f32 u, f32 phase) {
    if (!(phase >= 0.0f)) {
        phase = 0.0f;   // NaN/−: a lua NOVA (nada iluminado — o seguro)
    }
    if (phase > 1.0f) {
        phase = 1.0f;
    }
    const f32 frac = 0.5f * (1.0f - std::cos(phase * 6.28318530718f));
    const f32 b = 2.0f * frac - 1.0f;
    const f32 uEff = phase < 0.5f ? u : -u;
    return 1.0f - sstep(b - kMoonTermSoft, b + kMoonTermSoft, uEff);
}

// o u DO DISCO: a coordenada horizontal do ponto no disco (−1..1 dentro do
// disco; o tangent t = cross(lua, up) — o eixo horizontal do céu). Fora do
// disco o valor extrapola (o chamador só o lê com cosA > kMoonCosDisk).
inline f32 moonDiscU(f32 dx, f32 dy, f32 dz, const f32 moonDir[3]) {
    // t = normalize(cross(m, +Y)) — a lua nunca está no polo (|m.y| < 0.97)
    f32 tx = moonDir[2];
    f32 ty = 0.0f;
    f32 tz = -moonDir[0];
    const f32 tl = std::sqrt(tx * tx + tz * tz);
    if (tl > 1e-6f) {
        tx /= tl;
        tz /= tl;
    } else {
        tx = 1.0f;   // o polo degenerado: o eixo x (nunca acontece com o arco)
        tz = 0.0f;
    }
    (void)ty;
    const f32 sinDisk = std::sqrt(1.0f - kMoonCosDisk * kMoonCosDisk);
    return (dx * tx + dz * tz) / sinDisk;
}

// a GRADE do dia/noite — os 4 estados chave (a ordem do dono) interpolados
// por sstep nos segmentos [00,06,12,18,24] (a noite fecha o anel):
//   00:00 noite — azul-marinho escuro, estrelas, LUAR fraco (a luz azulada)
//   06:00 amanhecer — laranja/rosa no horizonte
//   12:00 meio-dia — a IDENTIDADE (os cores do utilizador byte a byte)
//   18:00 pôr do sol — vermelho/laranja intenso
// O horizonte/zénite/sol saem MISTURADOS com os do utilizador (o w do
// segmento manda: 0 no meio-dia = o céu da D; 1 na noite = a paleta-chave).
struct SkyGrade {
    f32 hor[3];    // o horizonte graduado (o uHorizon de sempre)
    f32 zen[3];    // o zénite graduado (o uZenith de sempre)
    f32 sun[3];    // a cor do sol graduada (disco/halo)
    f32 light[3];  // a cor da DIRECTIONAL (o lit consome — o luar à noite)
    f32 stars;     // o gate das estrelas (a luminância do céu manda)
};

inline void gradeLerp(const f32 a[3], const f32 b[3], f32 t, f32 out[3]) {
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

// as paletas-chave (constantes da fonte — o FS NÃO as replica: elas chegam
// pelos uniforms que o céu já tem, a otimização é não pagá-las por pixel)
constexpr f32 kNightHor[3] = {0.020f, 0.030f, 0.075f};
constexpr f32 kNightZen[3] = {0.008f, 0.014f, 0.055f};
constexpr f32 kNightSun[3] = {0.060f, 0.080f, 0.150f};
constexpr f32 kNightLight[3] = {0.16f, 0.22f, 0.36f};  // o LUAR fraco
constexpr f32 kDawnHor[3]  = {0.98f, 0.55f, 0.38f};
constexpr f32 kDawnZen[3]  = {0.35f, 0.38f, 0.60f};
constexpr f32 kDawnSun[3]  = {1.00f, 0.72f, 0.45f};
constexpr f32 kDawnLight[3] = {0.85f, 0.62f, 0.45f};
constexpr f32 kDuskHor[3]  = {0.95f, 0.38f, 0.20f};
constexpr f32 kDuskZen[3]  = {0.25f, 0.20f, 0.45f};
constexpr f32 kDuskSun[3]  = {1.00f, 0.48f, 0.26f};
constexpr f32 kDuskLight[3] = {0.80f, 0.52f, 0.38f};
constexpr f32 kDawnStars = 0.10f;
constexpr f32 kDuskStars = 0.15f;
constexpr f32 kNightStars = 1.0f;

inline void dayNightGrade(f32 tod, const f32 userHor[3], const f32 userZen[3],
                          const f32 userSun[3], SkyGrade& g) {
    // o relógio entra SEMPRE em [0,24] (o clampFields do Environment)
    if (tod < 0.0f) tod = 0.0f;
    if (tod > 24.0f) tod = 24.0f;
    // A FONTE ÚNICA: os 4 estados chave com O MEIO-DIA SENDO AS CORES DO
    // UTILIZADOR (a identidade da D — 12h devolve byte a byte) e a ÚNICA
    // interpolação por segmento (sstep — suave, a ordem do dono):
    //   [00..04[ noite (o plateau do luar)  → [04..06] pre-amanhecer
    //   [06..12] amanhecer → meio-dia       → [12..18] meio-dia → pôr do sol
    //   [18..21] crepúsculo (a noite cai em 3h) → [21..24] noite (plateau)
    f32 u;
    const f32* hor0; const f32* zen0; const f32* sun0; const f32* lig0;
    f32 stars0; f32 stars1;
    const f32* hor1; const f32* zen1; const f32* sun1; const f32* lig1;
    if (tod < 6.0f) {          // noite → amanhecer (o plateau até às 4)
        u = sstep(4.0f, 6.0f, tod);
        hor0 = kNightHor; zen0 = kNightZen; sun0 = kNightSun;
        lig0 = kNightLight; stars0 = kNightStars;
        hor1 = kDawnHor; zen1 = kDawnZen; sun1 = kDawnSun;
        lig1 = kDawnLight; stars1 = kDawnStars;
    } else if (tod < 12.0f) {  // amanhecer → meio-dia (as cores do dono)
        u = sstep(6.0f, 12.0f, tod);
        hor0 = kDawnHor; zen0 = kDawnZen; sun0 = kDawnSun;
        lig0 = kDawnLight; stars0 = kDawnStars;
        hor1 = userHor; zen1 = userZen; sun1 = userSun;
        lig1 = userSun; stars1 = 0.0f;
    } else if (tod < 18.0f) {  // meio-dia (o dono) → pôr do sol
        u = sstep(12.0f, 18.0f, tod);
        hor0 = userHor; zen0 = userZen; sun0 = userSun;
        lig0 = userSun; stars0 = 0.0f;
        hor1 = kDuskHor; zen1 = kDuskZen; sun1 = kDuskSun;
        lig1 = kDuskLight; stars1 = kDuskStars;
    } else {                   // pôr do sol → noite (o crepúsculo em 3h)
        u = sstep(18.0f, 21.0f, tod);
        hor0 = kDuskHor; zen0 = kDuskZen; sun0 = kDuskSun;
        lig0 = kDuskLight; stars0 = kDuskStars;
        hor1 = kNightHor; zen1 = kNightZen; sun1 = kNightSun;
        lig1 = kNightLight; stars1 = kNightStars;
    }
    // o MIX ÚNICO (as cores do utilizador já são o estado do meio-dia —
    // nenhuma segunda mistura dilui a noite)
    gradeLerp(hor0, hor1, u, g.hor);
    gradeLerp(zen0, zen1, u, g.zen);
    gradeLerp(sun0, sun1, u, g.sun);
    gradeLerp(lig0, lig1, u, g.light);
    g.stars = stars0 + (stars1 - stars0) * u;
    // o clamp honesto (as paletas-chave vivem em [0,1]; o mix nunca sai)
    for (int i = 0; i < 3; ++i) {
        if (g.hor[i] < 0.0f) g.hor[i] = 0.0f;
        if (g.hor[i] > 1.0f) g.hor[i] = 1.0f;
        if (g.zen[i] < 0.0f) g.zen[i] = 0.0f;
        if (g.zen[i] > 1.0f) g.zen[i] = 1.0f;
        if (g.sun[i] < 0.0f) g.sun[i] = 0.0f;
        if (g.sun[i] > 1.0f) g.sun[i] = 1.0f;
        if (g.light[i] < 0.0f) g.light[i] = 0.0f;
        if (g.light[i] > 1.0f) g.light[i] = 1.0f;
    }
}

// o UV das NUVENS com a ALTURA (a réplica do FS linha a linha): o plano
// da projeção sobe/desce — 1 = o céu da D byte a byte
inline void cloudUv(f32 dx, f32 dy, f32 dz, f32 height, f32 time, f32 speed,
                    f32 out[2]) {
    const f32 inv = 1.0f / (dy > kPlaneEps ? dy : kPlaneEps);
    out[0] = dx * inv * kCloudScale * height + time * speed * kWindDirX;
    out[1] = dz * inv * kCloudScale * height + time * speed * kWindDirY;
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

// 0.12-W-1 — A INVERSA (para as PROVAS de píxel): a direção NORMALIZADA →
// o NDC onde ela cai. A MATEMÁTICA: viewDir constrói v = fwd + a·right +
// b·up (a = ndc.x·aspect·tanF, b = ndc.y·tanF) e o raster NORMALIZA dir =
// v/|v| — o dot(dir, fwd) = 1/|v| (base ortonormal) é a divisão perspética:
// (dir−fwd)·right = a/|v| → dividir por dot(dir,fwd) recupera a exato.
// Direções fora do frustum EXTRAPOLAM; atrás da câmara devolve os neutros
// (a sonda da lua do raster vive AQUI).
inline void dirToNdc(const Vec3& dir, const Vec3& right, const Vec3& up,
                     const Vec3& fwd, f32 aspect, f32 tanF, f32 outNdc[2]) {
    const f32 w = dir.x * fwd.x + dir.y * fwd.y + dir.z * fwd.z;
    if (w <= 1e-6f || aspect <= 1e-6f || tanF <= 1e-6f) {
        outNdc[0] = 0.0f;
        outNdc[1] = 0.0f;
        return;
    }
    const Vec3 d{dir.x - fwd.x, dir.y - fwd.y, dir.z - fwd.z};
    const f32 rx = (d.x * right.x + d.y * right.y + d.z * right.z) / w;
    const f32 ux = (d.x * up.x + d.y * up.y + d.z * up.z) / w;
    outNdc[0] = rx / (aspect * tanF);
    outNdc[1] = ux / tanF;
}

// ---- 0.12-W-1: OS DEFAULTS SEM WorldEnvironment (a decisão do dono — o céu
// procedural EXISTE por defeito; o WE é um OVERRIDE opcional) ---------------
constexpr f32 kNoEnvTod      = 12.0f;   // o meio-dia (a identidade da Y-E)
constexpr f32 kNoEnvCoverage = 0.3f;    // o valor do prompt do dono (W-1)

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
        f32  height = 1.0f;           // 0.12-Y-E: a escala vertical das nuvens
        f32  stars = 0.0f;            // 0.12-Y-E: o gate das estrelas (0..1)
        f32  thunder = 0.0f;          // 0.12-Y-F2: o FLASH da trovoada
                                      // (0..1 — o AmbientEnvironment STORM;
                                      // 0 = o céu de sempre byte a byte)
        // ---- 0.12-W-1: as estrelas do utilizador + a LUA -----------------
        f32  starThresh = 184.0f;     // o limiar do campo (184 = a Y-E;
                                      // nasce do starDensity via
                                      // sky::starThreshFromDensity)
        f32  starBright = 0.8f;       // o ganho dos pontos (0..1)
        f32  moonDir[3] = {0.0f, -1.0f, 0.0f};  // oposta ao sol (o nadir
                                                // de fábrica = invisível)
        f32  moonPhase = 0.5f;        // 0=nova · 0.5=cheia · 1=nova
        f32  moonBright = 0.6f;       // 0 = sem lua (o branch morre)

        // 0.12-W-1 — A DECISÃO DE ARQUITETURA DO DONO (a FONTE ÚNICA do
        // mapeamento WorldEnvironment→Params — o main E os pins partilham):
        //   • env == nullptr → OS DEFAULTS DA CASA (o céu procedural EXISTE
        //     por defeito): timeOfDay 12.0, cloudCoverage 0.3 (kNoEnv*), as
        //     cores da identidade da Y-E, estrelas/lua nos defaults do dono;
        //   • env != nullptr → O OVERRIDE pelas propriedades do WE.
        // A CÂMARA (right/up/fwd/aspect/tanHalfFovY) fica a cargo do
        // chamador (é do frame, não do WE). As cores chegam JÁ graduadas
        // pela dayNightGrade (o contrato da Y-E: a grade corre no CPU 1×
        // por frame; aqui só entram os escalares + sol/lua/estrelas).
        void fromEnvironment(const Environment* env,
                             const sky::SkyGrade& grade,
                             f32 thunderNow, f32 skyTime) {
            time = skyTime;
            thunder = thunderNow;
            for (int i = 0; i < 3; ++i) {
                horizon[i] = grade.hor[i];
                zenith[i] = grade.zen[i];
                sunCol[i] = grade.sun[i];
            }
            if (env != nullptr) {
                // O OVERRIDE: o WE manda em TUDO (a cobertura dele, o
                // relógio dele, as estrelas/lua dele)
                coverage   = env->cloudCoverage;
                speed      = env->cloudSpeed;
                height     = env->cloudHeight;
                stars      = grade.stars;
                starThresh = sky::starThreshFromDensity(env->starDensity);
                starBright = env->starBrightness;
                sky::moonDirFromTime(env->timeOfDay, moonDir);
                sky::sunDirFromTime(env->timeOfDay, sunDir);
                moonPhase  = env->moonPhase;
                moonBright = env->moonBrightness;
            } else {
                // OS DEFAULTS SEM WorldEnvironment (o céu de fábrica — o
                // meio-dia da identidade Y-E + a cobertura do dono W-1)
                coverage   = sky::kNoEnvCoverage;
                speed      = Environment::kSpeedDefault;
                height     = Environment::kHeightDefault;
                stars      = grade.stars;   // a grade do tod default (12) → 0
                starThresh = sky::starThreshFromDensity(
                    Environment::kStarDensityDefault);
                starBright = Environment::kStarBrightDefault;
                sky::sunDirFromTime(sky::kNoEnvTod, sunDir);
                sky::moonDirFromTime(sky::kNoEnvTod, moonDir);
                moonPhase  = Environment::kMoonPhaseDefault;
                moonBright = Environment::kMoonBrightDefault;
            }
        }
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
    i32 locHeight_ = -1;   // 0.12-Y-E: a escala vertical das nuvens
    i32 locStars_ = -1;    // 0.12-Y-E: o gate das estrelas
    i32 locThunder_ = -1;  // 0.12-Y-F2: o flash da trovoada (entra ANTES do
                           // uTime — o ÚLTIMO 1f do draw continua a ser o
                           // relógio das nuvens, o contrato do pin da D)
    i32 locStarThresh_ = -1;  // 0.12-W-1: o limiar do campo (antes do uTime)
    i32 locStarBright_ = -1;  // 0.12-W-1: o ganho dos pontos (antes do uTime)
    i32 locMoonPhase_ = -1;   // 0.12-W-1: a fase da lua (antes do uTime)
    i32 locMoonBright_ = -1;  // 0.12-W-1: o brilho da lua (antes do uTime)
    i32 locMoonDir_ = -1;     // 0.12-W-1: a direção da lua (3f)
    i32 locHor_ = -1;
    i32 locZen_ = -1;
    i32 locSunCol_ = -1;
    i32 locSunDir_ = -1;
    u32 vertexCount_ = 0;
};

} // namespace vv
