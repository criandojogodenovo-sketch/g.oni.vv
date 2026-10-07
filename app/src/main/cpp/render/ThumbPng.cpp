// render/ThumbPng.cpp — PNG puro para a miniatura do projeto (0.9.0).
//
// PNG mínimo VÁLIDO: assinatura + IHDR (RGB8, sem filtro) + IDAT (deflate
// REAL do zlib — a mesma libz do ZIP da 0.8.10) + IEND, com CRC32 IEEE por
// tabela própria (o crc32() do zlib também serviria; a tabela própria
// mantém o módulo afervel de forma independente).
#include "render/ThumbPng.h"

#include <cstring>
#include <zlib.h>

namespace vv {
namespace thumb {

CropRect crop169(u32 vw, u32 vh) {
    CropRect r;
    if (vw == 0 || vh == 0) {
        return r;
    }
    // maior 16:9 que cabe: compara área por largura fixa vs altura fixa
    const u32 byW = (vw * 9u) / 16u;    // altura se a largura manda
    const u32 byH = (vh * 16u) / 9u;    // largura se a altura manda
    if (byW <= vh) {
        r.w = vw;
        r.h = byW;
    } else {
        r.w = byH;
        r.h = vh;
    }
    r.x = (vw - r.w) / 2u;
    r.y = (vh - r.h) / 2u;
    return r;
}

void downsampleRgb(const u8* srcRgba, u32 sw, u32 sh, u32 dw, u32 dh,
                   u8* dst) {
    if (!srcRgba || !dst || sw == 0 || sh == 0 || dw == 0 || dh == 0) {
        return;
    }
    // box filter: cada pixel destino = média de TODOS os pixéis fonte da
    // sua célula (célula exata por mapeamento proporcional — nunca "furada"
    // como nearest; fonte menor que destino = nearest por segurança)
    for (u32 dy = 0; dy < dh; ++dy) {
        const u32 y0 = (dy * sh) / dh;
        u32 y1 = ((dy + 1u) * sh) / dh;
        if (y1 <= y0) {
            y1 = y0 + 1u;
        }
        if (y1 > sh) {
            y1 = sh;
        }
        for (u32 dx = 0; dx < dw; ++dx) {
            const u32 x0 = (dx * sw) / dw;
            u32 x1 = ((dx + 1u) * sw) / dw;
            if (x1 <= x0) {
                x1 = x0 + 1u;
            }
            if (x1 > sw) {
                x1 = sw;
            }
            u32 sr = 0, sg = 0, sb = 0, n = 0;
            for (u32 y = y0; y < y1; ++y) {
                const u8* row = srcRgba + (size_t)y * sw * 4u;
                for (u32 x = x0; x < x1; ++x) {
                    sr += row[x * 4u + 0u];
                    sg += row[x * 4u + 1u];
                    sb += row[x * 4u + 2u];
                    ++n;
                }
            }
            u8* out = dst + ((size_t)dy * dw + dx) * 3u;
            out[0] = static_cast<u8>(n ? (sr + n / 2u) / n : 0);
            out[1] = static_cast<u8>(n ? (sg + n / 2u) / n : 0);
            out[2] = static_cast<u8>(n ? (sb + n / 2u) / n : 0);
        }
    }
}

void flipVerticalRgba(u8* buf, u32 w, u32 h) {
    if (!buf || w == 0 || h < 2u) {
        return;
    }
    std::vector<u8> row(w * 4u);
    for (u32 y = 0; y < h / 2u; ++y) {
        u8* a = buf + (size_t)y * w * 4u;
        u8* b = buf + (size_t)(h - 1u - y) * w * 4u;
        std::memcpy(row.data(), a, row.size());
        std::memcpy(a, b, row.size());
        std::memcpy(b, row.data(), row.size());
    }
}

// 0.9.6.18 (HOTFIX D7): o alvo da spec do dono é ~256×144 (PNG ≤~60KB,
// encode ≤~50ms off-thread) — era ≤480 (o PNG 4× maior do necessário p/
// um card de galeria). Nunca ACIMA (a ampliação não faz sentido — o card
// desenha no máximo ~200dp) e nunca 0.
u32 targetWidth(u32 cropW) {
    if (cropW == 0) {
        return 0;
    }
    if (cropW <= kThumbTargetW) {
        return cropW;
    }
    return kThumbTargetW;
}

// ---- CRC32 IEEE (poly 0xEDB88320, init 0xFFFFFFFF, xor final) ---------------
namespace {
struct CrcTable {
    u32 t[256];
    CrcTable() {
        for (u32 i = 0; i < 256u; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
    }
};
const CrcTable kCrc;
} // namespace

u32 crc32(const u8* data, size_t n, u32 seed) {
    u32 c = seed ? seed : 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c = kCrc.t[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

namespace {

// chunk PNG: [len BE4][type 4][data][crc32(type+data) BE4]
void pushBe32(std::vector<u8>& out, u32 v) {
    out.push_back((v >> 24) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back(v & 0xFF);
}

void pushChunkImpl(std::vector<u8>& out, const char* type, const u8* data,
                   size_t n) {
    pushBe32(out, static_cast<u32>(n));
    const size_t typeAt = out.size();
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<u8>(type[i]));
    }
    if (data && n) {
        out.insert(out.end(), data, data + n);
    }
    // CRC sobre type+data: começa no type e corre 4+n bytes
    const u32 crc = crc32(out.data() + typeAt, 4u + n);
    pushBe32(out, crc);
}

} // namespace

std::vector<u8> encodePngRgb(const u8* rgb, u32 w, u32 h) {
    std::vector<u8> out;
    if (!rgb || w == 0 || h == 0) {
        return out;
    }
    // assinatura
    static const u8 kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    out.insert(out.end(), kSig, kSig + 8);

    // ---- IHDR ----
    u8 ihdr[13] = {};
    ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF;
    ihdr[2] = (w >> 8) & 0xFF;  ihdr[3] = w & 0xFF;
    ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF;
    ihdr[6] = (h >> 8) & 0xFF;  ihdr[7] = h & 0xFF;
    ihdr[8]  = 8;    // bit depth
    ihdr[9]  = 2;    // color type RGB
    ihdr[10] = 0;    // compression
    ihdr[11] = 0;    // filter
    ihdr[12] = 0;    // interlace
    pushChunkImpl(out, "IHDR", ihdr, 13);

    // ---- IDAT: scanlines com filtro 0 + deflate REAL ----
    const size_t rawSize = (size_t)h * (1u + (size_t)w * 3u);
    std::vector<u8> raw(rawSize);
    for (u32 y = 0; y < h; ++y) {
        raw[(size_t)y * (1u + (size_t)w * 3u)] = 0;   // filter None
        std::memcpy(&raw[(size_t)y * (1u + (size_t)w * 3u) + 1u],
                    rgb + (size_t)y * w * 3u, (size_t)w * 3u);
    }
    uLongf compSize = compressBound(rawSize);
    std::vector<u8> comp(compSize);
    const int zrc = compress2(comp.data(), &compSize, raw.data(),
                              static_cast<uLong>(rawSize), 6);
    if (zrc != Z_OK) {
        return std::vector<u8>();   // falha honesta → sem PNG
    }
    comp.resize(compSize);
    pushChunkImpl(out, "IDAT", comp.data(), comp.size());

    // ---- IEND ----
    pushChunkImpl(out, "IEND", nullptr, 0);
    return out;
}

} // namespace thumb
} // namespace vv
