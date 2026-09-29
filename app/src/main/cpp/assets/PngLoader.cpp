// assets/PngLoader.cpp — decode PNG (stb_image) + gate 4K→2K (F5-D).
//
// ÚNICO TU com STB_IMAGE_IMPLEMENTATION (header vendor em assets/stb/).
#include "assets/PngLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "assets/stb/stb_image.h"

namespace vv {

bool loadPng(const u8* data, size_t len, RawImage& out, std::string& err) {
    out = RawImage{};
    if (!data || len < 8) {
        err = "png: dados vazios ou truncados";
        return false;
    }
    int w = 0, h = 0, comp = 0;
    // force 4 canais → RGBA8 uniforme (cinza/RGB/paleta todos normalizam)
    u8* decoded = stbi_load_from_memory(data, static_cast<int>(len),
                                        &w, &h, &comp, 4);
    if (!decoded) {
        err = std::string("png: ") +
              (stbi_failure_reason() ? stbi_failure_reason() : "falha desconhecida");
        return false;
    }
    if (w <= 0 || h <= 0) {
        stbi_image_free(decoded);
        err = "png: dimensões inválidas";
        return false;
    }
    out.width = static_cast<u32>(w);
    out.height = static_cast<u32>(h);
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
    out.rgba.assign(decoded, decoded + bytes);
    stbi_image_free(decoded);
    return true;
}

bool downscaleTo2K(RawImage& img, std::string& warn) {
    warn.clear();
    if (!img.ok()) {
        warn = "imagem inválida";
        return false;
    }
    constexpr u32 kLimit = 2048u;
    if (img.width <= kLimit && img.height <= kLimit) {
        return false;   // 1K/2K passam sem alteração (e sem aviso)
    }

    const u32 origW = img.width;
    const u32 origH = img.height;
    while (img.width > kLimit || img.height > kLimit) {
        const u32 nw = img.width > 1 ? img.width / 2 : 1;
        const u32 nh = img.height > 1 ? img.height / 2 : 1;
        std::vector<u8> reduced(static_cast<size_t>(nw) * nh * 4);
        for (u32 y = 0; y < nh; ++y) {
            for (u32 x = 0; x < nw; ++x) {
                // box 2×2 (cantos ímpares usam o último pixel disponível)
                const u32 x0 = x * 2;
                const u32 y0 = y * 2;
                const u32 x1 = (x0 + 1 < img.width) ? x0 + 1 : x0;
                const u32 y1 = (y0 + 1 < img.height) ? y0 + 1 : y0;
                for (int c = 0; c < 4; ++c) {
                    const u32 sum =
                        img.rgba[(static_cast<size_t>(y0) * img.width + x0) * 4 + c] +
                        img.rgba[(static_cast<size_t>(y0) * img.width + x1) * 4 + c] +
                        img.rgba[(static_cast<size_t>(y1) * img.width + x0) * 4 + c] +
                        img.rgba[(static_cast<size_t>(y1) * img.width + x1) * 4 + c];
                    reduced[(static_cast<size_t>(y) * nw + x) * 4 + c] =
                        static_cast<u8>(sum / 4u);
                }
            }
        }
        img.width = nw;
        img.height = nh;
        img.rgba = std::move(reduced);
    }
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "textura %ux%u reduzida para %ux%u (gate 2K; compressao real = F5.1)",
                  origW, origH, img.width, img.height);
    warn = buf;
    return true;
}

} // namespace vv
