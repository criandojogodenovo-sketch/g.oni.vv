// ui/TextWindow.cpp — a janela de texto pesado (contrato em TextWindow.h).
// Desenho no quad batch (panel/label/scroll do UiContext) + ícone Back do
// conjunto único. O IME é QUEM escreve: aqui só se consome a fila ime::.
#include "ui/TextWindow.h"

#include "ui/Icons.h"
#include "ui/ScrollMath.h"
#include "ui/Theme.h"

#include <cmath>
#include <cstring>

namespace vv {
struct InputState;

namespace editor {

namespace textwin {

namespace {

// linha visual: altura deriva das MÉTRICAS REAIS da fonte (regra F5.0 —
// nunca constantes cegas); piso 28px como o resto do editor
f32 lineHeight(UiContext& ui) {
    const TextMetrics m = ui.textMetrics();
    const f32 h = m.ascent + m.descent + 6.0f;
    return h > 28.0f ? h : 28.0f;
}

// apaga 1 code point UTF-8 do FIM (bytes de continuação 10xxxxxx morrem
// com o byte-líder — um acento é UMA tecla DEL)
void chopOneCodePoint(std::string& s) {
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0u) != 0x80u) {
            break;   // byte-líder (ou ASCII) — o code point inteiro saiu
        }
    }
}

// linhas não vazias em contagem (borda: buffer vazio = 1 linha — o caret
// precisa de onde ficar)
u32 countLines(const std::string& s) {
    if (s.empty()) {
        return 1;
    }
    u32 n = 1;
    for (const char c : s) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

} // namespace

void open(State& st) {
    st.open = true;
    st.buf.clear();
    st.scrollOff = 0.0f;
    st.blink = 0.0f;
}

void close(State& st) {
    st.open = false;
    st.buf.clear();   // a janela v0 é rascunho: fechar descarta (o editor de
                      // script 0.9.2 persistirá o buffer no componente Script)
}

bool applyEvent(State& st, const ime::Event& ev) {
    if (!st.open) {
        return false;   // IME fora da janela não vinga (coexistência: o
                        // teclado IN-APP é que trata os outros casos)
    }
    if (ev.isText) {
        st.buf += ev.text;
        return true;
    }
    switch (ev.key) {
        case ime::Key::Del:
            chopOneCodePoint(st.buf);
            return true;
        case ime::Key::Enter:
            st.buf += '\n';
            return true;
        default:
            return false;   // setas v0: sem cursor editável (0.9.2)
    }
}

u32 lineCount(const State& st) {
    return countLines(st.buf);
}

int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h,
         f32 dt) {
    (void)in;
    if (!st.open) {
        return 0;
    }

    // 0.9.6 (G1-1/G1-2 — SAFE AREA): fundo na superfície toda, cabeçalho
    // no inset do topo + 56dp, corpo até ao inset de baixo (insets=0 em
    // desktop/tests — layout de sempre)
    const safe::Insets ins = ui.safeArea();
    const f32 topY = ins.top;
    const f32 hdrH = theme::dp(kTopH);   // 56dp REAL (R-018)
    const f32 contentW = w - ins.left - ins.right;

    // fundo opaco FULL-SCREEN (bg do tema — nada do editor atrás)
    ui.panel(0.0f, 0.0f, w, h, theme::kTheme.bg);

    // ---- barra de topo: inset + 56dp (surface, como as barras da spec D) --
    ui.panel(ins.left, topY, contentW, hdrH, theme::kTheme.surface);
    ui.panel(ins.left, topY + hdrH - 1.0f, contentW, 1.0f,
             theme::kTheme.border);

    // BACK 56dp (alvo ≥48 — a célula inteira é o alvo)
    icons::drawIcon(ui, icons::Icon::Back, ins.left + theme::dp(16.0f),
                    topY + hdrH / 2.0f - theme::dp(12.0f), theme::dp(24.0f),
                    theme::kTheme.text1);
    int result = 0;
    if (ui.widgetHit(kBackId, ins.left, topY, hdrH, hdrH)) {
        result = 1;   // o main fecha (landscape + imeHide + log)
    }

    // título 20sp + hint 12sp (tipografia da spec A; na parte útil)
    {
        const TextMetrics m = ui.textMetrics();
        const theme::HeaderBaselines hb =
            theme::headerBaselines(m.ascent, m.descent, hdrH);
        ui.labelStyled(ins.left + hdrH + theme::dp(8.0f), topY + hb.title,
                       "Texto", theme::kTheme.text1,
                       theme::fontScale(theme::kFontScreen), 0);
        ui.labelStyled(ins.left + hdrH + theme::dp(8.0f), topY + hb.sub,
                       "portrait · IME do sistema",
                       theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);
    }

    // ---- corpo: linhas do buffer em região de scroll ----------------------
    const UiRect body{ins.left, topY + hdrH, contentW,
                      h - ins.bottom - (topY + hdrH)};
    const f32 lh = lineHeight(ui);
    const u32 nLines = countLines(st.buf);
    const f32 contentH = static_cast<f32>(nLines) * lh + theme::dp(16.0f);

    // o scroll SEGUE O FIM (v0 append-only — o caret vive no fim; o cursor
    // editável + scroll livre entram com o editor de script 0.9.2).
    // O setOffset SÓ apanha slots EXISTENTES (contrato F5.2) — por isso corre
    // DEPOIS do beginScroll: o offset deste frame já é o do fundo e o draw
    // usa-o (sem lag de 1 frame).
    ui.beginScroll(kScrollId, body, contentH);
    const f32 want = contentH > body.h ? contentH - body.h : 0.0f;
    ui.scrollSetOffset(kScrollId, want);
    const f32 off = ui.scrollOffset();

    // split do buffer em linhas e desenho (labels recortadas pela região)
    const f32 xText = body.x + 16.0f;
    u32 start = 0;
    for (u32 i = 0; i < nLines; ++i) {
        u32 end = start;
        while (end < st.buf.size() && st.buf[end] != '\n') {
            ++end;
        }
        const u32 len = end - start;
        char line[256];
        if (len < sizeof(line)) {
            std::memcpy(line, st.buf.data() + start, len);
            line[len] = '\0';
        } else {
            std::memcpy(line, st.buf.data() + start, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        }
        const f32 yContent = 8.0f + static_cast<f32>(i) * lh;
        ui.label(xText, body.y + yContent - off, line, theme::kTheme.text1);
        // CARET piscante no fim da ÚLTIMA linha (0.6s on / 0.6s off)
        if (i == nLines - 1) {
            st.blink += dt;
            if (std::fmod(st.blink, 1.2f) < 0.6f) {
                const f32 xCaret = xText + ui.fontWidth(line) + 2.0f;
                ui.panel(xCaret, body.y + yContent - off, 2.0f, lh - 8.0f,
                         theme::kTheme.accent);
            }
        }
        start = (end < st.buf.size()) ? end + 1 : end;
    }
    ui.endScroll();

    return result;
}

} // namespace textwin
} // namespace editor
} // namespace vv
