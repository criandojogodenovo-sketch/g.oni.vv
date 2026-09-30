#pragma once
// ui/FontAtlas.h — atlas de fonte via stb_truetype (bitmap baking 512x512).
// F1: sem assets — carrega fonte do SISTEMA (paths padrão do Android).
// PLACEHOLDER: na fase de assets, passa a carregar fonte empacotada do jogo.
// F5.0-fix: métricas verticais REAIS (ascent/descent) expostas — o layout
// do Inspector passa a derivar as alturas das linhas do tamanho REAL dos
// glifos (o bug do C33: linhas de 26 px com uma fonte de 28 px — os glifos
// invadiam as linhas vizinhas e o texto "sobrepunha-se").
//
// 0.6.7 (lifecycle GL): o contexto EGL MORRE em APP_CMD_TERM_WINDOW (o
// EglContext::shutdown destrói surface E contexto — todos os ids GL ficam
// órfãos). O guard `if (tex_) return true` fazia o 2º INIT_WINDOW devolver
// true com o id STALE → o pass UI amostrava uma textura inexistente e TODO
// o texto virava quads brancos ("cubinhos" no C33 ao sair do editor e
// voltar sem matar a app). Contrato novo:
//   • destroy() — chamado no TERM_WINDOW (com contexto ainda corrente):
//     glDeleteTextures + reset do id E das métricas → o atlas fica "fresco";
//   • loadFromPaths() — re-bake + re-upload sempre que tex_ == 0 (o guard
//     agora só impede o upload DUPLICADO no MESMO contexto).
#include <vector>
#include "core/Types.h"

namespace vv {

// Métricas verticais de texto (px, ambas POSITIVAS):
//   ascent  — do baseline ao topo mais alto dos glifos
//   descent — do baseline ao fundo mais baixo (descendentes)
// Fallback (sem fonte): valores de uma sans a 28 px (o caso do device).
struct TextMetrics {
    f32 ascent  = 21.0f;
    f32 descent = 7.0f;
    f32 block() const { return ascent + descent; }   // altura do bloco de texto
};

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

    // 0.6.7: liberta o recurso GL (glDeleteTextures) e regressa ao estado
    // pré-bake (id 0 + métricas a zero) — chamado no APP_CMD_TERM_WINDOW
    // com o contexto AINDA corrente, para que o próximo INIT_WINDOW re-bake
    // e re-upe o atlas no contexto NOVO. Sem isto, tex_ ficava != 0 (stale)
    // e o guard do loadFromPaths saltava o upload → glifos brancos.
    void destroy();

    bool ok() const { return tex_ != 0; }
    u32  texture() const { return tex_; }
    f32  height() const { return height_; }
    // F5.0-fix: métricas verticais medidas NO BAKE (max dos glifos reais)
    f32  ascent()  const { return ascent_; }
    f32  descent() const { return descent_; }
    const Glyph& glyph(char c) const { return glyphs_[c - static_cast<char>(kFirstChar)]; }
    f32  widthOf(const char* text) const;

private:
    Glyph glyphs_[kNumChars]{};
    u32   tex_ = 0;
    f32   height_ = 0.0f;
    f32   ascent_  = 0.0f;   // F5.0-fix: max(-yoff) dos glifos assados
    f32   descent_ = 0.0f;   // F5.0-fix: max(yoff + h) — fundo dos descendentes
};

// Paths típicos de fonte no Android (primeiro que existir vence).
extern const char* const kSystemFontPaths[];
extern const u32 kSystemFontPathCount;

} // namespace vv
