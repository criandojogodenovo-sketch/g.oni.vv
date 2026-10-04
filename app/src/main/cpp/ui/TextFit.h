#pragma once
// ui/TextFit.h — truncagem horizontal de labels (F4.2/B2), sem GL —
// host-testável.
//
// O bug do C33: labels como "body: rigid - sphere - chao: sim" excedem a
// largura útil do Inspector (w − 2*kPad) e os glifos cortam na borda direita
// (dentro de região de scroll o clip engole metade do glifo; fora, sangram
// para fora do painel).
//
// Fix: medir; se não couber, truncar com reticência "…" mantendo o MAIOR
// prefixo que caiba. FASE 9 (G1-2): "…" é U+2026 (3 bytes UTF-8) — o atlas
// agora TEM o glifo (Latin-1 + Ext-A + …); a truncagem respeita FRONTeIRAS
// de code point (nunca corta um acento ao meio).
//
// A medição é INJETADA (WidthFn) para o algoritmo ficar GL-free e testável
// no CI sem fonte real: os testes passam um medidor fake (ex.: mono 10 px/
// char); o UiContext passa um lambda sobre FontAtlas::widthOf (UTF-8).
//
// Compat dos testes: se o medidor NÃO conhece "…" (fake mono conta BYTES),
// a reticência continua a funcionar — o algoritmo mede o que lhe dão.
#include <cstdio>
#include <cstring>
#include "core/Types.h"

namespace vv {
namespace textfit {

constexpr const char* kEllipsis = "\xE2\x80\xA6";   // U+2026 …

// Copia para `out` (cap bytes, snprintf — nunca overrun) `text` truncado com
// "..." quando width(text) > maxW. Devolve `out`. Determinístico: mantém o
// maior prefixo (em bytes ASCII) cujo prefixo + "..." caiba em maxW; nem o
// "..." cabendo, devolve string vazia (não desenha nada). Strings que cabem
// são copiadas integralmente.
template <typename WidthFn>
inline const char* ellipsize(const char* text, f32 maxW, WidthFn&& width,
                             char* out, u32 cap) {
    if (!out || cap == 0) {
        return out;
    }
    out[0] = '\0';
    if (!text || !text[0]) {
        return out;
    }
    if (width(text) <= maxW) {
        std::snprintf(out, cap, "%s", text);
        return out;
    }
    const f32 ellW = width(kEllipsis);
    const u32 len = std::strlen(text);
    for (u32 p = len; ; --p) {
        if (p == 0) {
            if (ellW <= maxW) {
                std::snprintf(out, cap, "%s", kEllipsis);
            } else {
                out[0] = '\0';   // nem a reticência cabe — não desenha
            }
            return out;
        }
        // FASE 9 (G1-2): p só pára em FRONTEIRAS de code point — recua
        // enquanto o byte ANTERIOR for um byte de continuação UTF-8 (0b10…)
        if (p < len && (static_cast<unsigned char>(text[p]) & 0xC0u) == 0x80u) {
            continue;   // p cai no MEIO de um code point — recua mais
        }
        // prefixo p bytes + "…" (buffer local: labels da UI são curtos)
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%.*s%s",
                      static_cast<int>(p), text, kEllipsis);
        if (width(buf) <= maxW) {
            std::snprintf(out, cap, "%s", buf);
            return out;
        }
    }
}

} // namespace textfit
} // namespace vv
