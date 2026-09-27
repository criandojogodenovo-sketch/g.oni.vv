#pragma once
// ui/FontAtlas.h — atlas de fonte via stb_truetype (bitmap baking 512x512).
// F1: sem assets — carrega fonte do SISTEMA (paths padrão do Android).
// PLACEHOLDER: na fase de assets, passa a carregar fonte empacotada do jogo.
#include <vector>
#include "core/Types.h"

namespace vv {

struct Glyph {
    f32 u0, v0, u1, v1;   // uv no atlas
    f32 xoff, yoff;       // offset em relação à caneta, a partir da BASELINE (px)
    f32 xadv;             // avanço horizontal (px)
    f32 w, h;             // tamanho do glyph em px
};

class FontAtlas {
public:
    static constexpr u32 kFirstChar = 32;   // ' '
    static constexpr u32 kNumChars  = 95;   // ASCII 32..126

    // Tenta os paths na ordem; usa o primeiro que abrir e couber no atlas.
    bool loadFromPaths(const char* const* paths, u32 count, f32 heightPx);

    bool ok() const { return tex_ != 0; }
    u32  texture() const { return tex_; }
    f32  height() const { return height_; }
    const Glyph& glyph(char c) const { return glyphs_[c - static_cast<char>(kFirstChar)]; }
    f32  widthOf(const char* text) const;

private:
    Glyph glyphs_[kNumChars]{};
    u32   tex_ = 0;
    f32   height_ = 0.0f;
};

// Paths típicos de fonte no Android (primeiro que existir vence).
extern const char* const kSystemFontPaths[];
extern const u32 kSystemFontPathCount;

} // namespace vv
