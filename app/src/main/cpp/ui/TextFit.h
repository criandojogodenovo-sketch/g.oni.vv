#pragma once
// ui/TextFit.h — truncagem horizontal de labels (F4.2/B2), sem GL —
// host-testável.
//
// O bug do C33: labels como "body: rigid - sphere - chao: sim" excedem a
// largura útil do Inspector (w − 2*kPad) e os glifos cortam na borda direita
// (dentro de região de scroll o clip engole metade do glifo; fora, sangram
// para fora do painel).
//
// Fix: medir; se não couber, truncar com reticência ASCII "..." mantendo o
// MAIOR prefixo que caiba. "..." e não "…" (U+2026): o atlas da F1 só tem
// ASCII 32..126 (ui/FontAtlas.h) — o glyph de reticência não existe nele.
//
// A medição é INJETADA (WidthFn) para o algoritmo ficar GL-free e testável
// no CI sem fonte real: os testes passam um medidor fake (ex.: mono 10 px/
// char); o UiContext passa um lambda sobre FontAtlas::widthOf.
#include <cstdio>
#include <cstring>
#include "core/Types.h"

namespace vv {
namespace textfit {

constexpr const char* kEllipsis = "...";

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
        // prefixo p bytes + "..." (buffer local: labels da UI são curtos)
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
