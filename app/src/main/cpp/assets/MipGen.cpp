// assets/MipGen.cpp — cadeia de mips em CPU (F5.1-A; PASSO 5A linear-space).
#include "assets/MipGen.h"
#include <cmath>

namespace vv {

// ---- transferência sRGB (a fórmula oficial; LUT no decode — 256 entradas) ---
float srgbToLinear(u8 v) {
    static const float* lut = [] {
        static float table[256];
        for (int i = 0; i < 256; ++i) {
            const float c = static_cast<float>(i) / 255.0f;
            table[i] = c <= 0.04045f
                           ? c / 12.92f
                           : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        return table;
    }();
    return lut[v];
}

u8 linearToSrgbByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    const float s = v <= 0.0031308f
                        ? v * 12.92f
                        : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    // half-up consistente (o teste aferiu os cantos e o meio)
    return static_cast<u8>(std::lround(s * 255.0f));
}

namespace {

// média de 2×2 num canal — a soma em u32 de sempre (sem overflow)
inline u8 avgByte(u32 a, u32 b, u32 c, u32 d) {
    return static_cast<u8>((a + b + c + d) / 4u);
}

} // namespace

void halveImageRGBA(const RawImage& in, RawImage& out, MipSpace space) {
    out = RawImage{};
    if (!in.ok()) {
        return;
    }
    const u32 nw = in.width > 1 ? in.width / 2 : 1;
    const u32 nh = in.height > 1 ? in.height / 2 : 1;
    out.width = nw;
    out.height = nh;
    out.rgba.resize(static_cast<size_t>(nw) * nh * 4);
    for (u32 y = 0; y < nh; ++y) {
        for (u32 x = 0; x < nw; ++x) {
            const u32 x0 = x * 2;
            const u32 y0 = y * 2;
            const u32 x1 = (x0 + 1 < in.width) ? x0 + 1 : x0;
            const u32 y1 = (y0 + 1 < in.height) ? y0 + 1 : y0;
            const size_t i00 =
                (static_cast<size_t>(y0) * in.width + x0) * 4;
            const size_t i01 =
                (static_cast<size_t>(y0) * in.width + x1) * 4;
            const size_t i10 =
                (static_cast<size_t>(y1) * in.width + x0) * 4;
            const size_t i11 =
                (static_cast<size_t>(y1) * in.width + x1) * 4;
            u8* o = &out.rgba[(static_cast<size_t>(y) * nw + x) * 4];
            if (space == MipSpace::SrgbLinear) {
                // PASSO 5A: RGB em LUZ (decode → média → encode); o ALPHA é
                // cobertura — média em bytes como sempre
                for (int c = 0; c < 3; ++c) {
                    const float lin = 0.25f *
                        (srgbToLinear(in.rgba[i00 + c]) +
                         srgbToLinear(in.rgba[i01 + c]) +
                         srgbToLinear(in.rgba[i10 + c]) +
                         srgbToLinear(in.rgba[i11 + c]));
                    o[c] = linearToSrgbByte(lin);
                }
                o[3] = avgByte(in.rgba[i00 + 3], in.rgba[i01 + 3],
                               in.rgba[i10 + 3], in.rgba[i11 + 3]);
            } else {
                for (int c = 0; c < 4; ++c) {
                    o[c] = avgByte(in.rgba[i00 + c], in.rgba[i01 + c],
                                   in.rgba[i10 + c], in.rgba[i11 + c]);
                }
            }
        }
    }
}

void genMipChainRGBA(const RawImage& img, std::vector<RawImage>& chain,
                     MipSpace space) {
    chain.clear();
    if (!img.ok()) {
        return;
    }
    chain.push_back(img);
    while (chain.back().width > 1 || chain.back().height > 1) {
        RawImage next;
        halveImageRGBA(chain.back(), next, space);
        chain.push_back(std::move(next));
    }
}

} // namespace vv
