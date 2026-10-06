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
#include <vector>
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

// 0.9.6.12 (GRUPO J4 · R-025) — RETICÊNCIA A MEIO (o ellipsize="middle"
// da spec J4 para o campo do PROJETO no rodapé): mantém a CABEÇA e a CAUDA
// com "…" no meio — o fim do nome (a extensão, o sufixo significativo)
// continua visível, e o que se corta é o MEIO (a parte menos identificável
// do nome). O mesmo contrato do ellipsize: medidor injetado, GL-free,
// fronteiras de code point respeitadas, determinístico (o maior par
// cabeça+cauda que caiba, head obtido do primeiro metade dos caracteres
// guardados). `out` cap bytes; string que cabe é copiada integralmente;
// nem "…" cabendo → string vazia (não desenha).
template <typename WidthFn>
inline const char* ellipsizeMiddle(const char* text, f32 maxW,
                                   WidthFn&& width, char* out, u32 cap) {
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
    const u32 len = static_cast<u32>(std::strlen(text));
    const f32 ellW = width(kEllipsis);
    if (ellW > maxW) {
        out[0] = '\0';   // nem a reticência cabe — não desenha
        return out;
    }
    // code point: byte de continuação UTF-8 (0b10xxxxxx)?
    auto isCont = [&text, &len](u32 i) {
        return i > 0 && i < len &&
               (static_cast<unsigned char>(text[i]) & 0xC0u) == 0x80u;
    };
    auto boundaryL = [&](u32 i) {   // recua até ao início de um code point
        while (i > 0 && isCont(i)) {
            --i;
        }
        return i;
    };
    auto boundaryR = [&](u32 i) {   // avança até ao início de um code point
        while (i < len && isCont(i)) {
            ++i;
        }
        return i;
    };
    // o maior nº de glifos guardados (k), repartidos metade cabeça / metade
    // cauda, cujo «cabeça…cauda» caiba em maxW — determinístico
    for (u32 kept = len; kept >= 1; --kept) {
        const u32 headLen = boundaryL(kept / 2);
        u32 tailStart = len - (kept - kept / 2);
        tailStart = boundaryR(tailStart);
        if (headLen == 0 && tailStart >= len) {
            continue;   // nem um glifo guardado
        }
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%.*s%s%s",
                      static_cast<int>(headLen), text, kEllipsis,
                      text + tailStart);
        if (width(buf) <= maxW) {
            std::snprintf(out, cap, "%s", buf);
            return out;
        }
    }
    // último recurso: só a reticência (não desenha o nome)
    if (ellW <= maxW) {
        std::snprintf(out, cap, "%s", kEllipsis);
    }
    return out;
}

} // namespace textfit

// ---- 0.9.6 (G2-5): QUEBRA DE LINHA por palavras ( Docs sem "…") -------------
// O contrato da spec: nas Docs a descrição faz QUEBRA DE LINHA em vez de
// "…". O wrap é por PALAVRAS (nunca corta uma palavra a meio nem um code
// point a meio); palavras maiores que a largura ficam SOZINHAS na linha
// (partidas por largura em fronteiras de code point — o último recurso,
// nomes longos de comandos continuam legíveis). GL-free: medidor injetado.
namespace textwrap {

struct Line {
    u32 begin = 0;   // offset em bytes no texto original
    u32 len   = 0;   // bytes desta linha (SEM o espaço separador)
};

template <typename WidthFn>
inline void wrap(const char* text, f32 maxW, WidthFn&& width,
                 std::vector<Line>& out) {
    out.clear();
    if (!text || !text[0] || maxW <= 0.0f) {
        return;
    }
    const u32 len = std::strlen(text);
    u32 lineStart = 0;
    u32 cursor = 0;        // fim do último texto ACEITE na linha corrente
    u32 i = 0;
    while (i <= len) {
        // começa uma palavra no cursor atual (saltando espaços à frente)
        u32 wordStart = i;
        while (wordStart < len && text[wordStart] == ' ') {
            ++wordStart;
        }
        // fim da palavra (próximo espaço ou fim do texto)
        u32 wordEnd = wordStart;
        while (wordEnd < len && text[wordEnd] != ' ') {
            ++wordEnd;
        }
        if (wordStart >= len) {
            break;   // só espaços no fim — a linha termina em cursor
        }
        // a linha candidata vai de lineStart..wordEnd (com os espaços que
        // lá estiverem — medem algo, mas contam para o limite)
        char buf[512];
        const u32 candLen = wordEnd - lineStart;
        const u32 capped = candLen < sizeof(buf) - 1
                               ? candLen
                               : (u32)sizeof(buf) - 1;
        std::snprintf(buf, sizeof(buf), "%.*s", (int)capped,
                      text + lineStart);
        if (cursor > lineStart && width(buf) > maxW) {
            // NÃO CABE com esta palavra: fecha a linha em cursor e a palavra
            // abre a PRÓXIMA (word-reject: nunca parte no meio)
            out.push_back(Line{lineStart, cursor - lineStart});
            lineStart = wordStart;
            cursor = wordEnd;
        } else if (width(buf) > maxW && cursor == lineStart) {
            // a palavra SOZINHA não cabe: parte-a por LARGURA (fronteiras de
            // code point) — cada pedaço é uma linha
            u32 cut = wordEnd;
            while (cut > wordStart) {
                const u32 partLen = cut - wordStart;
                const u32 cp = partLen < sizeof(buf) - 1
                                   ? partLen
                                   : (u32)sizeof(buf) - 1;
                std::snprintf(buf, sizeof(buf), "%.*s", (int)cp,
                              text + wordStart);
                if (width(buf) <= maxW || cut == wordStart + 1) {
                    break;
                }
                // recua em fronteiras de code point
                --cut;
                while (cut > wordStart + 1 &&
                       (static_cast<unsigned char>(text[cut]) & 0xC0u) ==
                           0x80u) {
                    --cut;
                }
            }
            out.push_back(Line{wordStart, cut - wordStart});
            lineStart = cut;
            cursor = cut;
            i = cut;
            continue;
        } else {
            cursor = wordEnd;
        }
        i = wordEnd;
    }
    if (cursor > lineStart) {
        out.push_back(Line{lineStart, cursor - lineStart});
    }
}

} // namespace textwrap
} // namespace vv
