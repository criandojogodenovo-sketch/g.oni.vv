#include "ui/FontAtlas.h"
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstring>

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

// FASE 9 (G1-2): os code points que o PORTUGUÊS EXIGE — a cobertura é
// verificada NO BAKE (uma fonte OEM sem eles é rejeitada; a próxima da
// lista entra). A sentinela R-008 afere por hasGlyph().
static const u32 kRequiredCp[] = {
    0xE7,   // ç
    0xE3,   // ã
    0xC3,   // Ã
    0xF5,   // õ
    0xE9,   // é
    0xED,   // í
    0xC1,   // Á
    0xF3,   // ó
    0xE7, 0x2026,
};

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

// o offset do code point no BANCO interno (a soma dos ranges anteriores);
// kGlyphTotal = tamanho total do banco
constexpr u32 kRangeSize(u32 r) {
    return FontAtlas::kRanges[r][1] - FontAtlas::kRanges[r][0] + 1;
}
constexpr u32 kGlyphTotal = kRangeSize(0) + kRangeSize(1) + kRangeSize(2) +
                            kRangeSize(3);

} // namespace

void FontAtlas::destroy() {
    // 0.6.7 — chamado no APP_CMD_TERM_WINDOW com o contexto AINDA corrente:
    // devolve o nome da textura ao contexto que vai morrer e regressa ao
    // estado pré-bake. O próximo loadFromPaths (novo contexto) re-faz tudo —
    // bake + glTexImage2D + métricas. Sem isto, o guard `if (tex_)` saltava
    // o re-upload e os glifos saíam brancos ("cubinhos" do C33).
    if (tex_ != 0) {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    height_ = 0.0f;
    ascent_ = 0.0f;
    descent_ = 0.0f;
}

bool FontAtlas::loadFromPaths(const char* const* paths, u32 count, f32 heightPx) {
    if (tex_) {
        return true;   // já carregado NESTE contexto (upload duplicado = desperdício)
    }

    // FASE 9 (G1-2): 1024×512 — o atlas ASCII 512×512 não caberia os 3
    // ranges + elipse (320 glifos a 28 px). R8 = 512 KB (o C33 tem folga).
    constexpr i32 kW = 1024;
    constexpr i32 kH = 512;
    std::vector<u8> fileData;
    for (u32 i = 0; i < count; ++i) {
        if (!readFile(paths[i], &fileData)) {
            continue;
        }

        // stbtt font info + a COBERTURA exigida (os acentos do português)
        stbtt_fontinfo info;
        if (!stbtt_InitFont(&info, fileData.data(), 0)) {
            continue;
        }
        bool coverage = true;
        for (const u32 cp : kRequiredCp) {
            const int gi = stbtt_FindGlyphIndex(&info, static_cast<int>(cp));
            if (gi <= 0) {
                coverage = false;
                break;
            }
        }
        if (!coverage) {
            // a fonte NÃO tem os acentos — a UI perderia o "Á" em silêncio.
            // LOGA e tenta a próxima (o sistema tem sempre o Roboto/Noto)
            std::printf("font: '%s' sem cobertura latina exigida — a "
                        "próxima da lista é tentada\n",
                        paths[i]);
            continue;
        }

        std::vector<u8> bitmap(static_cast<size_t>(kW) * static_cast<size_t>(kH));
        stbtt_pack_context pc;
        if (!stbtt_PackBegin(&pc, bitmap.data(), kW, kH, kW, 1, nullptr)) {
            continue;
        }
        // os 4 ranges (ASCII + Latin-1 + Latin Ext-A + …) num ÚNICO passe
        stbtt_pack_range ranges[kRangeCount];
        stbtt_packedchar packed[95 + 96 + 128 + 1];
        u32 cursor = 0;
        for (u32 r = 0; r < kRangeCount; ++r) {
            ranges[r].first_unicode_codepoint_in_range =
                static_cast<int>(kRanges[r][0]);
            ranges[r].array_of_unicode_codepoints = nullptr;
            ranges[r].num_chars =
                static_cast<int>(kRanges[r][1] - kRanges[r][0] + 1);
            ranges[r].chardata_for_range = packed + cursor;
            ranges[r].font_size = heightPx;
            cursor += kRanges[r][1] - kRanges[r][0] + 1;
        }
        const int res = stbtt_PackFontRanges(&pc, fileData.data(), 0, ranges,
                                             static_cast<int>(kRangeCount));
        stbtt_PackEnd(&pc);
        if (res <= 0) {
            continue;   // não coube no atlas / fonte inválida — tenta a próxima
        }

        tex_ = makeTexture(bitmap.data(), kW, kH);
        if (!tex_) {
            return false;
        }

        // o banco: os ranges consecutivos no MESMO layout do array packed
        ascent_ = 0.0f;
        descent_ = 0.0f;
        cursor = 0;
        for (u32 r = 0; r < kRangeCount; ++r) {
            const u32 n = kRanges[r][1] - kRanges[r][0] + 1;
            for (u32 k = 0; k < n; ++k) {
                const stbtt_packedchar& b = packed[cursor + k];
                Glyph& g = glyphs_[cursor + k];
                g.u0 = static_cast<f32>(b.x0) / static_cast<f32>(kW);
                g.v0 = static_cast<f32>(b.y0) / static_cast<f32>(kH);
                g.u1 = static_cast<f32>(b.x1) / static_cast<f32>(kW);
                g.v1 = static_cast<f32>(b.y1) / static_cast<f32>(kH);
                g.xoff = b.xoff;
                g.yoff = b.yoff;
                g.xadv = b.xadvance;
                g.w = static_cast<f32>(b.x1 - b.x0);
                g.h = static_cast<f32>(b.y1 - b.y0);
                // F5.0-fix: métricas verticais REAIS — o maior bloco entre
                // todos os glifos assados (topo mais alto acima do baseline
                // + fundo dos descendentes abaixo)
                const f32 top    = -b.yoff;                       // baseline → topo
                const f32 bottom = b.yoff + g.h;                  // baseline → fundo
                if (top > ascent_)  ascent_  = top;
                if (bottom > descent_) descent_ = bottom;
            }
            cursor += n;
        }
        height_ = heightPx;
        return true;
    }
    return false;
}

const Glyph* FontAtlas::glyphFor(u32 cp) const {
    u32 cursor = 0;
    for (u32 r = 0; r < kRangeCount; ++r) {
        if (cp >= kRanges[r][0] && cp <= kRanges[r][1]) {
            return &glyphs_[cursor + (cp - kRanges[r][0])];
        }
        cursor += kRanges[r][1] - kRanges[r][0] + 1;
    }
    return nullptr;
}

f32 FontAtlas::widthOf(const char* text) const {
    // FASE 9 (G1-2): UTF-8 — cada CODE POINT avança o seu glifo (acento
    // morre inteiro, nunca byte-a-byte)
    f32 w = 0.0f;
    for (const char* p = text; *p;) {
        u32 bytes = 1;
        const u32 cp = utf8Decode(p, &bytes);
        const Glyph* g = glyphFor(cp);
        if (g) {
            w += g->xadv;
        } else {
            w += height_ * 0.30f;   // fora do atlas — o espaço de sempre
        }
        p += bytes;
    }
    return w;
}

} // namespace vv
