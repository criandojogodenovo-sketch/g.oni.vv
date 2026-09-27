#include "ui/FontAtlas.h"
#include <GLES3/gl3.h>
#include <cstdio>

#define STB_TRUETYPE_IMPLEMENTATION
#include "ui/stb/stb_truetype.h"

namespace vv {

const char* const kSystemFontPaths[] = {
    "/system/fonts/Roboto-Regular.ttf",
    "/system/fonts/roboto-Regular.ttf",
    "/system/fonts/DroidSans.ttf",
    "/system/fonts/NotoSans-Regular.ttf",
    "/system/fonts/op-sans.ttf",
    "/system/fonts/OPPO-Sans-R.ttf",
};

const u32 kSystemFontPathCount =
    static_cast<u32>(sizeof(kSystemFontPaths) / sizeof(kSystemFontPaths[0]));

namespace {

bool readFile(const char* path, std::vector<u8>* out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(f);
        return false;
    }
    out->resize(static_cast<size_t>(size));
    const size_t got = std::fread(out->data(), 1, static_cast<size_t>(size), f);
    std::fclose(f);
    return got == static_cast<size_t>(size);
}

u32 makeTexture(const u8* bitmap, i32 w, i32 h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

} // namespace

bool FontAtlas::loadFromPaths(const char* const* paths, u32 count, f32 heightPx) {
    if (tex_) {
        return true;   // já carregado
    }

    constexpr i32 kW = 512;
    constexpr i32 kH = 512;
    std::vector<u8> fileData;
    for (u32 i = 0; i < count; ++i) {
        if (!readFile(paths[i], &fileData)) {
            continue;
        }

        std::vector<u8> bitmap(static_cast<size_t>(kW) * static_cast<size_t>(kH));
        stbtt_bakedchar baked[kNumChars];
        const int res = stbtt_BakeFontBitmap(fileData.data(), 0, heightPx,
                                             bitmap.data(), kW, kH,
                                             static_cast<int>(kFirstChar),
                                             static_cast<int>(kNumChars), baked);
        if (res <= 0) {
            continue;   // não coube no atlas / fonte inválida — tenta a próxima
        }

        tex_ = makeTexture(bitmap.data(), kW, kH);
        if (!tex_) {
            return false;
        }

        for (u32 c = 0; c < kNumChars; ++c) {
            const stbtt_bakedchar& b = baked[c];
            Glyph& g = glyphs_[c];
            g.u0 = static_cast<f32>(b.x0) / static_cast<f32>(kW);
            g.v0 = static_cast<f32>(b.y0) / static_cast<f32>(kH);
            g.u1 = static_cast<f32>(b.x1) / static_cast<f32>(kW);
            g.v1 = static_cast<f32>(b.y1) / static_cast<f32>(kH);
            g.xoff = b.xoff;
            g.yoff = b.yoff;
            g.xadv = b.xadvance;
            g.w = static_cast<f32>(b.x1 - b.x0);
            g.h = static_cast<f32>(b.y1 - b.y0);
        }
        height_ = heightPx;
        return true;
    }
    return false;
}

f32 FontAtlas::widthOf(const char* text) const {
    f32 w = 0.0f;
    for (const char* p = text; *p; ++p) {
        const char c = *p;
        if (c < static_cast<char>(kFirstChar) ||
            c >= static_cast<char>(kFirstChar + kNumChars)) {
            w += height_ * 0.30f;
            continue;
        }
        w += glyph(c).xadv;
    }
    return w;
}

} // namespace vv
