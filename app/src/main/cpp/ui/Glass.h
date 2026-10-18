#pragma once
// ui/Glass.h — 0.12-D (D-2) · O VIDRO (REAL e FALSO).
//
// VIDRO REAL (os painéis FLUTUANTES — os menus ancorados sobre o viewport):
// a cena captura-se para textura a 1/4 de resolução, o BLUR DUAL KAWASE
// corre EM CIMA (2 passagens do padrão de 8 offsets — a função PURA
// kawasePass abaixo, host-testável), e o painel é o WIDGET SDF a AMOSTRAR
// essa textura com a TINTA ESCURA por cima (o ramo uSdfTexOn do mesmo
// shader único da D-1 — os cantos, o bisel e a sombra do widget continuam).
//
// VIDRO FALSO (os painéis FIXOS — a hierarquia e o inspector): o gradiente
// semi-opaco (surface→surface2), a BORDA CLARA 1px (o glassEdge — a
// FRESNEL), o REFLEXO DIAGONAL (os veios brancos fracos) e o RUÍDO FRACO
// (o hash glassNoise — células esparsas a α mínimo).
//
// A CACHE (a cláusula D-7): o blur só recalcula quando O QUE ESTÁ ATRÁS
// MUDA — glassCacheDirty() compara o estado da câmara/da cena/do rect com
// o capturado (a função PURA abaixo; o Renderer guarda o carimbo).
#include "core/Types.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace vv {
namespace glass {

// ---- O BLUR DUAL KAWASE (puro — o device e o stub partilham) ---------------
// Uma passagem: 8 amostras do padrão Kawase ((±k,±k) e (±2k,0)/(0,±2k))
// centradas no pixel; k = offsetScale (a 1.ª passagem k=1, a 2.ª k=2 — o
// "dual" do prompt; o raio efetivo cresce ~2× por passagem).
inline void kawasePass(const u8* src, int w, int h, int offsetScale,
                       u8* dst) {
    const int k = offsetScale > 0 ? offsetScale : 1;
    static const int kOff[8][2] = {{k, k},  {-k, k},  {k, -k},  {-k, -k},
                                   {2 * k, 0}, {-2 * k, 0}, {0, 2 * k},
                                   {0, -2 * k}};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sr = 0, sg = 0, sb = 0, sa = 0;
            for (int i = 0; i < 8; ++i) {
                int sx = x + kOff[i][0];
                int sy = y + kOff[i][1];
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;
                if (sx >= w) sx = w - 1;
                if (sy >= h) sy = h - 1;
                const size_t pi =
                    (size_t(sy) * size_t(w) + size_t(sx)) * 4u;
                sr += src[pi];
                sg += src[pi + 1];
                sb += src[pi + 2];
                sa += src[pi + 3];
            }
            const size_t po = (size_t(y) * size_t(w) + size_t(x)) * 4u;
            dst[po] = static_cast<u8>(sr / 8);
            dst[po + 1] = static_cast<u8>(sg / 8);
            dst[po + 2] = static_cast<u8>(sb / 8);
            dst[po + 3] = static_cast<u8>(sa / 8);
        }
    }
}

// o BLUR DUAL completo (2 passagens com ping-pong; src e dst NÃO se
// sobrepõem; o buffer tmp tem de ter w*h*4 bytes)
inline void kawaseBlurDual(const u8* src, int w, int h, u8* tmp, u8* dst) {
    kawasePass(src, w, h, 1, tmp);    // passagem 1 (k=1)
    kawasePass(tmp, w, h, 2, dst);    // passagem 2 (k=2 — o dual)
}

// a DOWNSAMPLE 2×2 box (o 1/4 de resolução do prompt = 2 reduções de 2×;
// src w×h → dst (w/2)×(h/2), uma redução)
inline void downsample2x2(const u8* src, int w, int h, u8* dst) {
    const int dw = w / 2 > 0 ? w / 2 : 1;
    const int dh = h / 2 > 0 ? h / 2 : 1;
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const int sx0 = x * 2 < w ? x * 2 : w - 1;
            const int sy0 = y * 2 < h ? y * 2 : h - 1;
            const int sx1 = sx0 + 1 < w ? sx0 + 1 : sx0;
            const int sy1 = sy0 + 1 < h ? sy0 + 1 : sy0;
            for (int c = 0; c < 4; ++c) {
                const int v = (src[(size_t(sy0) * w + sx0) * 4 + c] +
                               src[(size_t(sy0) * w + sx1) * 4 + c] +
                               src[(size_t(sy1) * w + sx0) * 4 + c] +
                               src[(size_t(sy1) * w + sx1) * 4 + c]) /
                              4;
                dst[(size_t(y) * dw + x) * 4 + c] = static_cast<u8>(v);
            }
        }
    }
}

// ---- A DECISÃO DA CACHE (pura — «só recalcular se o que está atrás mudou»)
struct CacheState {
    f32 camYaw, camPitch, camDist;   // a câmara de orbit (a cena atrás)
    u32 sceneVersion;                // a versão da cena (edições)
    f32 capX, capY, capW, capH;      // o rect capturado (px)
    f32 surfaceW, surfaceH;          // o tamanho da superfície
};

inline bool glassCacheDirty(const CacheState& a, const CacheState& b) {
    const f32 e = 0.0005f;
    if (std::fabs(a.camYaw - b.camYaw) > e) return true;
    if (std::fabs(a.camPitch - b.camPitch) > e) return true;
    if (std::fabs(a.camDist - b.camDist) > e) return true;
    if (a.sceneVersion != b.sceneVersion) return true;
    if (std::fabs(a.capX - b.capX) > 0.5f) return true;
    if (std::fabs(a.capY - b.capY) > 0.5f) return true;
    if (std::fabs(a.capW - b.capW) > 0.5f) return true;
    if (std::fabs(a.capH - b.capH) > 0.5f) return true;
    if (std::fabs(a.surfaceW - b.surfaceW) > 0.5f) return true;
    if (std::fabs(a.surfaceH - b.surfaceH) > 0.5f) return true;
    return false;
}

// ---- O RUÍDO FRACO do vidro falso (puro — hash inteiro determinístico) ----
// 0..1 por CÉLULA (as células de 16px do chamador): o mesmo ecrã dá o
// MESMO ruído frame a frame (não cintila — o hash é do SÍTIO, não do
// tempo); ~1 em 5 células nasce com o veio (o chamador filtra < 0.2).
inline f32 glassNoise(int cellX, int cellY, u32 seed) {
    u32 h = (u32(cellX) * 374761393u) ^ (u32(cellY) * 668265263u) ^
            (seed * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return f32(h & 0xFFFFu) / 65535.0f;
}

} // namespace glass
} // namespace vv
