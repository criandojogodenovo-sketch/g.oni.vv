// ui/ScriptEditor.cpp — o editor de script (contrato no header). O código
// visual segue o textwin 0.9.1 (a casca) + nºs de linha + coloração por
// classes (VoniHighlight) + Run/Stop + barra de erro com LINHA (§12).
#include "ui/ScriptEditor.h"

#include "core/Handle.h"
#include "core/Scene.h"
#include "components/ScriptComp.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "voni/VoniHighlight.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace vv {
struct InputState;

namespace editor {
namespace scriptwin {

namespace {

f32 lineHeight(UiContext& ui) {
    const TextMetrics m = ui.textMetrics();
    const f32 h = m.ascent + m.descent + 6.0f;
    return h > 28.0f ? h : 28.0f;   // piso 28px (regra F5.0)
}

void chopOneCodePoint(std::string& s) {
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0u) != 0x80u) {
            break;
        }
    }
}

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

const f32* clsColor(voni::hl::Cls c) {
    // AS CORES VIVEM NO THEME (spec §10 🔶) — o parser só deu a CLASSE
    using voni::hl::Cls;
    switch (c) {
        case Cls::Reserved: return theme::kTheme.voniReserved;
        case Cls::Engine:   return theme::kTheme.voniEngine;
        case Cls::User:     return theme::kTheme.voniUser;
        case Cls::Number:   return theme::kTheme.voniNumber;
        case Cls::Str:      return theme::kTheme.voniString;
        case Cls::Comment:  return theme::kTheme.voniComment;
        default:            return theme::kTheme.text1;
    }
}

} // namespace

void open(State& st, Scene& scene, Handle tic) {
    st.open = true;
    st.tic = tic;
    st.buf.clear();
    st.scrollOff = 0.0f;
    st.blink = 0.0f;
    st.errLine = 0;
    st.errMsg.clear();
    st.running = false;
    // carrega o fonte do componente (se existir)
    if (tic.valid()) {
        if (Tic* t = scene.get(tic)) {
            if (const ScriptComp* sc = t->getComponent<ScriptComp>()) {
                st.buf = sc->source;
            }
        }
    }
}

void close(State& st) {
    st.open = false;
    // o buffer NÃO se descarta ao fechar por acidente: o main salva no
    // componente quando o back é tocado (persistir §10) — o estado mantém
    // o buffer até o main decidir (o close() limpa só a janela).
    st.running = false;
}

bool applyEvent(State& st, const ime::Event& ev) {
    if (!st.open) {
        return false;
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
            return false;
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

    // fundo opaco FULL-SCREEN (bg — o mesmo do editor de código)
    ui.panel(0.0f, 0.0f, w, h, theme::kTheme.bg);

    // ---- barra de topo 56dp ------------------------------------------------
    ui.panel(0.0f, 0.0f, w, kTopH, theme::kTheme.surface);
    ui.panel(0.0f, kTopH - 1.0f, w, 1.0f, theme::kTheme.border);

    icons::drawIcon(ui, icons::Icon::Back, 16.0f, kTopH / 2.0f - 12.0f, 24.0f,
                    theme::kTheme.text1);
    int result = 0;
    if (ui.widgetHit(kBackId, 0.0f, 0.0f, kTopH, kTopH)) {
        result = 1;
    }

    // título 20sp + hint 12sp
    ui.labelStyled(kTopH + 8.0f, 14.0f, "Script", theme::kTheme.text1,
                   theme::fontScale(theme::kFontScreen), 0);
    ui.labelStyled(kTopH + 8.0f, 40.0f, "V.ONI · .voni",
                   theme::kTheme.text2,
                   theme::fontScale(theme::kFontCaption), 0);

    // RUN / STOP 48dp à direita (alvos ≥48; estado: running aceso = Stop)
    const f32 btnW = 72.0f;
    const f32 runX = w - btnW * 2.0f - 16.0f;
    const f32 stopX = w - btnW - 8.0f;
    const f32 btnY = (kTopH - 48.0f) / 2.0f;
    const bool running = st.running;
    // Run: accent quando disponível; esbatido enquanto corre
    ui.panelRounded(runX, btnY, btnW, 48.0f, theme::kRadiusCard,
                    running ? theme::kTheme.surface2 : theme::kTheme.accent);
    ui.labelStyled(runX, btnY + 14.0f, "Run", running ? theme::kTheme.text2
                                                      : theme::kTheme.bg,
                   theme::fontScale(theme::kFontBody), 0);
    if (!running && ui.widgetHit(kRunId, runX, btnY, btnW, 48.0f)) {
        result = 2;
    }
    // Stop: danger quando a correr
    ui.panelRounded(stopX, btnY, btnW, 48.0f, theme::kRadiusCard,
                    running ? theme::kTheme.danger : theme::kTheme.surface2);
    ui.labelStyled(stopX, btnY + 14.0f, "Stop",
                   running ? theme::kTheme.bg : theme::kTheme.text2,
                   theme::fontScale(theme::kFontBody), 0);
    if (running && ui.widgetHit(kStopId, stopX, btnY, btnW, 48.0f)) {
        result = 3;
    }

    // ---- corpo: nºs de linha + código colorido em scroll -------------------
    const f32 errBarH = st.errLine ? kErrH : 0.0f;
    const UiRect body{0.0f, kTopH, w, h - kTopH - errBarH};
    const f32 lh = lineHeight(ui);
    const u32 nLines = countLines(st.buf);
    const f32 contentH = static_cast<f32>(nLines) * lh + 16.0f;

    ui.beginScroll(kScrollId, body, contentH);
    // o scroll SEGUE O FIM (escrita append-only — o caret vive no fim)
    const f32 want = contentH > body.h ? contentH - body.h : 0.0f;
    ui.scrollSetOffset(kScrollId, want);
    const f32 off = ui.scrollOffset();

    // gutter de nºs de linha (48dp — fundo surface, texto text2 12sp)
    ui.panel(0.0f, body.y, 48.0f, body.h, theme::kTheme.surface);
    ui.panel(48.0f, body.y, 1.0f, body.h, theme::kTheme.border);
    const f32 xCode = 64.0f;

    // split em linhas + coloração por classes (o parser classifica; as
    // CORES vêm do Theme — spec §10)
    voni::hl::BlockCommentState bcState;
    u32 start = 0;
    char num[16];
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
        const f32 y = body.y + yContent - off;

        // nº da linha (12sp text2; a linha do ERRO acende em danger)
        std::snprintf(num, sizeof(num), "%u", i + 1);
        ui.labelStyled(8.0f, y + 4.0f, num,
                       st.errLine == i + 1 ? theme::kTheme.danger
                                           : theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);

        // tokens da linha (classes → cores do Theme)
        const std::string lineStr(line);
        auto tokens = voni::hl::classifyLine(lineStr, bcState);
        if (tokens.empty()) {
            // linha fora de bloco sem tokens: desenha como user (cru)
            ui.label(xCode, y, line, theme::kTheme.voniUser);
        } else {
            f32 x = xCode;
            for (const auto& t : tokens) {
                if (t.len == 0 || t.begin >= lineStr.size()) {
                    continue;
                }
                char piece[128];
                const u32 pl = t.len < sizeof(piece) - 1
                                   ? t.len
                                   : (u32)sizeof(piece) - 1;
                std::memcpy(piece, lineStr.data() + t.begin, pl);
                piece[pl] = '\0';
                ui.label(x, y, piece, clsColor(t.cls));
                x += ui.fontWidth(piece);
            }
        }

        // CARET piscante no fim da última linha (0.6s on/off)
        if (i == nLines - 1) {
            st.blink += dt;
            if (std::fmod(st.blink, 1.2f) < 0.6f) {
                const f32 xCaret = xCode + ui.fontWidth(line) + 2.0f;
                ui.panel(xCaret, y, 2.0f, lh - 8.0f, theme::kTheme.accent);
            }
        }
        start = (end < st.buf.size()) ? end + 1 : end;
    }
    ui.endScroll();

    // ---- barra de ERRO com linha + mensagem (§12) ---------------------------
    if (st.errLine) {
        ui.panel(0.0f, h - kErrH, w, kErrH, theme::kTheme.danger);
        char msg[160];
        std::snprintf(msg, sizeof(msg), "linha %u: %s", st.errLine,
                      st.errMsg.empty() ? "erro" : st.errMsg.c_str());
        ui.labelStyled(12.0f, h - kErrH + 12.0f, msg, theme::kTheme.bg,
                       theme::fontScale(theme::kFontCaption), 0);
    }

    return result;
}

} // namespace scriptwin
} // namespace editor
} // namespace vv
