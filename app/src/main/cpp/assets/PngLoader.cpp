// assets/PngLoader.cpp — decode PNG (stb_image) + gate 2K do fallback (F5-D).
//
// ÚNICO TU com STB_IMAGE_IMPLEMENTATION (header vendor em assets/stb/).
// A partir da F5.1-A o gate 2K só se aplica ao FALLBACK sem compressão de
// hardware (a decisão vive em TexturePipeline; com ETC2/ASTC a textura 4K
// entra inteira). A matemática do box 2×2 é partilhada com os mips
// (assets/MipGen).
#include "assets/PngLoader.h"
#include "assets/MipGen.h"

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
        RawImage reduced;
        halveImageRGBA(img, reduced);   // box 2×2 (mesma matemática dos mips)
        img = std::move(reduced);
    }
    char buf[112];
    std::snprintf(buf, sizeof(buf),
                  "textura %ux%u reduzida para %ux%u (gate 2K: sem compressão de hardware)",
                  origW, origH, img.width, img.height);
    warn = buf;
    return true;
}

} // namespace vv
