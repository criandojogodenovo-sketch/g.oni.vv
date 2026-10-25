#pragma once
// ui/FontAtlas.h — atlas de fonte via stb_truetype.
// F1: sem assets — carrega fonte do SISTEMA (paths padrão do Android).
// F5.0-fix: métricas verticais REAIS (ascent/descent) expostas — o layout
// do Inspector passa a derivar as alturas das linhas do tamanho REAL dos
// glifos (o bug do C33: linhas de 26 px com uma fonte de 28 px — os glifos
// invadiam as linhas vizinhas e o texto "sobrepunha-se").
//
// FASE 9 (G1-2 — ACENTOS): o atlas era ASCII 32..126 APENAS — "ÁUDIO"
// ficava "UDIO", "seleção" ficava "sele  o" (o byte fora do range avançava
// a pena sem desenhar nada). AGORA o atlas cobre:
//   • Latin básico (32..126);
//   • Latin-1 Supplement (0xA0..0xFF): Á À Â Ã Ç É Ê Í Ó Ô Õ Ú Ü á à â ã ç
//     é ê í ó ô õ ú ü — PORTUGUÊS completo;
//   • Latin Extended-A (0x100..0x17F);
//   • U+2026 (… — a elipse do "trunca com '…'" da hierarquia).
// A iteração do texto é UTF-8 (a fonte de sempre da app é UTF-8); glifos
// ausentes avançam 0.30·altura (o espaço de sempre — nunca crash).
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

// ---- UTF-8 (FASE 9 G1-2): descodifica 1 code point; devolve os BYTES ------
// devolve o code point (U+FFFD se malformado) e escreve o nº de bytes em
// *bytes (1..4, sempre ≥1 — o iterador avança)
inline u32 utf8Decode(const char* s, u32* bytes) {
    const unsigned char c0 = static_cast<unsigned char>(s[0]);
    if (c0 < 0x80u) {
        *bytes = 1;
        return c0;
    }
    // 0.12-Z2 F5 (o ACHADO F5-B do ASan): o lead byte SÓ autoriza a
    // leitura dos bytes da PRÓPRIA sequência — a versão antiga lia
    // s[1..3] SEMPRE (um 0xFF solitário no FIM de um buffer de texto lia
    // 3 bytes além do fim; o regress_glyph_coverage sob ASan apanhou-o).
    // O RESULTADO é idêntico em TODAS as entradas (o mesmo code point e o
    // mesmo *bytes) — as leituras é que passam a ser as mínimas.
    const int need = (c0 & 0xE0u) == 0xC0u ? 1
                   : (c0 & 0xF0u) == 0xE0u ? 2
                   : (c0 & 0xF8u) == 0xF0u ? 3
                                           : 0;
    if (need == 0) {
        *bytes = 1;   // lead inválido — avança 1 byte (nunca crash, nunca loop)
        return 0xFFFDu;
    }
    unsigned char c[3] = {0u, 0u, 0u};
    for (int i = 1; i <= need; ++i) {
        c[i - 1] = static_cast<unsigned char>(s[i]);
        if ((c[i - 1] & 0xC0u) != 0x80u) {
            *bytes = 1;   // malformado — avança 1 byte (nunca crash, nunca loop)
            return 0xFFFDu;
        }
    }
    *bytes = static_cast<u32>(need + 1);
    if (need == 1) {
        return (static_cast<u32>(c0 & 0x1Fu) << 6) |
                static_cast<u32>(c[0] & 0x3Fu);
    }
    if (need == 2) {
        return (static_cast<u32>(c0 & 0x0Fu) << 12) |
               (static_cast<u32>(c[0] & 0x3Fu) << 6) |
                static_cast<u32>(c[1] & 0x3Fu);
    }
    return (static_cast<u32>(c0 & 0x07u) << 18) |
           (static_cast<u32>(c[0] & 0x3Fu) << 12) |
           (static_cast<u32>(c[1] & 0x3Fu) << 6) |
            static_cast<u32>(c[2] & 0x3Fu);
}

class FontAtlas {
public:
    // os INTERVALOS de code points assados no atlas (FASE 9 G1-2). A ordem
    // é a ordem dos índices no banco interno (kRangeCount entradas).
    static constexpr u32 kRanges[][2] = {
        {32, 126},        // ASCII
        {0xA0, 0xFF},     // Latin-1 Supplement (Á Ã Ç É Í Ó Õ Ú …)
        {0x100, 0x17F},   // Latin Extended-A
        {0x2026, 0x2026}, // … (horizontal ellipsis)
    };
    static constexpr u32 kRangeCount = 4;
    static constexpr u32 kFirstChar = 32;   // compat: início do 1º range
    static constexpr u32 kNumChars  = 95;   // compat: tamanho do 1º range

    // Tenta os paths na ordem; usa o primeiro que abrir e couber no atlas.
    // FASE 9: SÓ ACEITA fontes com COBERTURA dos acentos exigidos (ç ã Ã õ
    // é í) — o Roboto/Noto do Android tem; se uma fonte OEM não tiver, a
    // próxima da lista é tentada (a UI nunca mais perde o "Á" em silêncio).
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

    // ---- GLIFO por CODE POINT (FASE 9): a chave é o código Unicode, não o
    // byte. nullptr = code point fora do atlas (o chamador avança 0.30·h).
    const Glyph* glyphFor(u32 cp) const;
    // o glifo de UM BYTE ASCII (compat com os usos de sempre: glyph('a'))
    const Glyph& glyph(char c) const { return glyphs_[c - 32]; }
    // o atlas TEM o code point? (a sentinela R-008 afere por aqui)
    bool hasGlyph(u32 cp) const { return glyphFor(cp) != nullptr; }

    f32  widthOf(const char* text) const;   // UTF-8 (G1-2)

private:
    // banco por RANGE: o code point mapeia para range→índice local
    Glyph glyphs_[95 + 96 + 128 + 1]{};   // a soma dos 4 ranges
    u32   tex_ = 0;
    f32   height_ = 0.0f;
    f32   ascent_  = 0.0f;   // F5.0-fix: max(-yoff) dos glifos assados
    f32   descent_ = 0.0f;   // F5.0-fix: max(yoff + h) — fundo dos descendentes
};

// Paths típicos de fonte no Android (primeiro que existir vence).
extern const char* const kSystemFontPaths[];
extern const u32 kSystemFontPathCount;

// FASE UI H — os paths MONO do código (o 2.º atlas; o mesmo contrato).
extern const char* const kMonoFontPaths[];
extern const u32 kMonoFontPathCount;

} // namespace vv
