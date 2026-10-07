#pragma once
// render/ThumbPng.h — MINIATURA DO PROJETO (0.9.0, spec F): PNG puro.
//
// PIPELINE (a parte GL — glReadPixels — vive no main; AQUI é tudo PURO e
// afervel no CI):
//   RGBA cru (bottom-up, da viewport) → flip vertical → crop 16:9 centrado
//   → downsample box até ≤480 de largura → drop alpha → PNG (IHDR/IDAT/IEND
//   com deflate REAL do zlib e CRC32 próprio).
//
// O ficheiro sai como thumb.png NA RAIZ do projeto (ProjectStorage stream)
// — a tela de projetos (Java) lê-o e faz cache; default = logo G.One.
//
// Zero dependências além de <zlib.h> (a mesma libz do ZIP da 0.8.10).
#include "core/Types.h"
#include <vector>

namespace vv {
namespace thumb {

// 0.9.6.18 (HOTFIX D7): a LARGURA-ALVO da miniatura (a spec do dono:
// ~256×144 — PNG ≤~60KB, encode ≤~50ms off-thread). Afervel: a sentinela
// R-036.1 afirma ESTE valor.
constexpr u32 kThumbTargetW = 256u;

// ---- PURE (host-testável no CI) ---------------------------------------------

// crop 16:9 CENTRADO dentro de um viewport (vw×vh): devolve o rect (origem
// topo-esquerda) da maior área 16:9 que cabe. Viewport já 16:9 → inteiro.
struct CropRect {
    u32 x = 0, y = 0, w = 0, h = 0;
};
CropRect crop169(u32 vw, u32 vh);

// downsample BOX FILTER (fator real arbitrário — média de todos os pixéis
// fonte de cada célula destino; SEM aliasing de nearest). dst deve ter
// 3*dw*dh bytes (RGB).
void downsampleRgb(const u8* srcRgba, u32 sw, u32 sh, u32 dw, u32 dh, u8* dst);

// flip vertical IN PLACE de um buffer RGBA w×h (glReadPixels é bottom-up)
void flipVerticalRgba(u8* buf, u32 w, u32 h);

// largura final da miniatura a partir da largura do crop (≤480, múltiplo de
// 16 p/ o box filter ficar exato... 480/16=30: qualquer dw serve; o filtro
// é de média flutuante) — altura derivada 16:9
u32 targetWidth(u32 cropW);

// PNG encode: RGB8 (top-down, scanlines sem filtro) → bytes de um PNG
// VÁLIDO (IHDR RGB8 + IDAT deflate zlib + IEND; CRC32 por poly 0xEDB88320).
// Devolve vazio = falha (zlib erro).
std::vector<u8> encodePngRgb(const u8* rgb, u32 w, u32 h);

// CRC32 (IEEE) — exposto para os testes aferirem contra valores conhecidos
u32 crc32(const u8* data, size_t n, u32 seed = 0);

} // namespace thumb
} // namespace vv
