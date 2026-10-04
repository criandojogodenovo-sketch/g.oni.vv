// ui/ScriptEditor.cpp — o editor de script (contrato no header). O código
// visual segue o textwin 0.9.1 (a casca) + nºs de linha + coloração por
// classes (VoniHighlight) + Run/Stop + barra de erro com LINHA (§12).
//
// FASE 9 (G0-1/G0-2): caret livre + teclado in-app (mesmo applyEvent do
// IME) + toque no corpo re-pede o IME + lupa (Docs) + esqueleto base para
// script novo (cursor no interior).
#include "ui/ScriptEditor.h"

#include "core/Handle.h"
#include "core/Scene.h"
#include "components/ScriptComp.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "voni/VoniHighlight.h"

#include <cstdio>
#include <cstring>

namespace vv {
struct InputState;

namespace editor {
namespace scriptwin {

// o ESQUELETO base (G0-2 — entry point da spec §3). O caret aponta para
// DENTRO de "allmoments { }" (entre o "{ " e o "}") — o primeiro texto
// digitado cai no interior.
const char* const kSkeleton =
    "central main {\n"
    "  on moment { }\n"
    "  allmoments { }\n"
    "}\n";
const u32 kSkeletonCaret = 46;   // índice do '}' de "allmoments { }"

// 0.9.6 (G2-7b · R-010) — O PLANO DE RENDER: as peças da linha com os GAPS
// PREENCHIDOS (Cls::User). O classificador devolve só os tokens COLORIDOS
// (espaços e pontuação ficam de fora) — o render antigo avançava x apenas
// pelas peças desenhadas e o texto aparecia SEM espaços e sem `{` enquanto
// o cursor (que media a linha INTEIRA) deixava o espaço: guardado !=
// renderizado. AGORA: cada gap entre tokens é uma peça User; a
// concatenação das peças == a linha inteira (a sentinela R-010 afere).
std::vector<RenderPiece> renderPieces(const std::string& line,
                                      voni::hl::BlockCommentState& bc) {
    std::vector<RenderPiece> out;
    const auto tokens = voni::hl::classifyLine(line, bc);
    u32 pos = 0;
    for (const voni::hl::Token& t : tokens) {
        if (t.begin > pos) {
            out.push_back(RenderPiece{pos, t.begin - pos,
                                      voni::hl::Cls::User});
        }
        out.push_back(RenderPiece{t.begin, t.len, t.cls});
        pos = t.begin + t.len;
    }
    if (pos < line.size()) {
        out.push_back(
            RenderPiece{pos, (u32)(line.size() - pos), voni::hl::Cls::User});
    }
    return out;
}

namespace {

f32 lineHeight(UiContext& ui) {
    const TextMetrics m = ui.textMetrics();
    const f32 h = m.ascent + m.descent + 6.0f;
    return h > 28.0f ? h : 28.0f;   // piso 28px (regra F5.0)
}

// ---- caret (G0-1): navegação por CODE POINT (acento morre inteiro) --------

// recua 1 code point a partir de `i` (>=1 byte)
u32 prevCodePoint(const std::string& s, u32 i) {
    if (i == 0) {
        return 0;
    }
    u32 j = i - 1;
    while (j > 0 && (static_cast<unsigned char>(s[j]) & 0xC0u) == 0x80u) {
        --j;
    }
    return j;
}

// avança 1 code point a partir de `i` (< size)
u32 nextCodePoint(const std::string& s, u32 i) {
    if (i >= s.size()) {
        return static_cast<u32>(s.size());
    }
    u32 j = i + 1;
    while (j < s.size() &&
           (static_cast<unsigned char>(s[j]) & 0xC0u) == 0x80u) {
        ++j;
    }
    return j;
}

// início da linha que contém o offset
u32 lineStartOf(const std::string& s, u32 off) {
    u32 i = off < s.size() ? off : static_cast<u32>(s.size());
    while (i > 0 && s[i - 1] != '\n') {
        --i;
    }
    return i;
}

// coluna (bytes desde o início da linha) do offset
u32 columnOf(const std::string& s, u32 off) {
    return off - lineStartOf(s, off);
}

void insertAtCaret(State& st, const char* utf8) {
    if (!utf8 || !*utf8) {
        return;
    }
    st.buf.insert(st.caret, utf8);
    st.caret += static_cast<u32>(std::strlen(utf8));
}

void chopBeforeCaret(State& st) {
    if (st.caret == 0) {
        return;
    }
    const u32 p = prevCodePoint(st.buf, st.caret);
    st.buf.erase(p, st.caret - p);
    st.caret = p;
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

// ---- teclado in-app (G0-1) ------------------------------------------------
//
// 2 páginas: LETRAS (A..Z_, 0..9 — o layout clássico do teclado da casa)
// e SÍMBOLOS (a linguagem precisa de { } ( ) " = + . etc.). As teclas
// EMITEM ime::Event pelo MESMO applyEvent do IME — uma única fonte de
// verdade para a edição.

constexpr u32 kKbRows = 4;          // 4 linhas de teclas + a linha de baixo
constexpr f32 kKbKeyH = 48.0f;      // alvo ≥48dp
constexpr f32 kKbGap  = 6.0f;
constexpr f32 kKbPad  = 8.0f;

// rótulos das páginas (NULL = fim da linha)
const char* const kKbLetters[kKbRows][10] = {
    {"A", "B", "C", "D", "E", "F", "G", "H", "I", nullptr},
    {"J", "K", "L", "M", "N", "O", "P", "Q", "R", nullptr},
    {"S", "T", "U", "V", "W", "X", "Y", "Z", "_", nullptr},
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"},
};
const char* const kKbSymbols[kKbRows][10] = {
    {"{", "}", "(", ")", "[", "]", "<", ">", "=", nullptr},
    {"+", "-", "*", "/", "\\", "!", "?", ":", ";", nullptr},
    {"\"", "'", "@", "#", "$", "%", "&", "|", "~", "^"},
    {".", ",", "0", "1", "2", "3", "4", "5", "6", "7"},
};

// altura total do teclado (4 linhas + linha de baixo)
f32 keyboardHeight() {
    return 5.0f * kKbKeyH + 4.0f * kKbGap + 2.0f * kKbPad;
}

// o label da tecla (página corrente; lower aplica-se só às letras)
const char* keyLabel(const State& st, u32 row, u32 col) {
    const char* s = st.kbSym ? kKbSymbols[row][col] : kKbLetters[row][col];
    if (!s) {
        return "";
    }
    if (!st.kbSym && st.kbLower && s[0] >= 'A' && s[0] <= 'Z' &&
        s[1] == '\0') {
        static const char* kLower[26] = {"a", "b", "c", "d", "e", "f", "g",
                                         "h", "i", "j", "k", "l", "m", "n",
                                         "o", "p", "q", "r", "s", "t", "u",
                                         "v", "w", "x", "y", "z"};
        return kLower[s[0] - 'A'];
    }
    return s;
}

// desenha o teclado DOKADO no fundo (acima da barra de erro; acima do
// INSET DE BAIXO desde 0.9.6 — a última tecla nunca fica sob a barra de
// navegação); devolve true se alguma tecla EMITIU texto (para o log)
bool drawKeyboard(UiContext& ui, State& st, f32 w, f32 kbTop) {
    // 0.9.6 (G1/G3): o teclado vive DENTRO do contentRect (laterais)
    const safe::Insets ins = ui.safeArea();
    const f32 kbX = ins.left;
    const f32 kbW = w - ins.left - ins.right;
    const f32 innerW = kbW - 2.0f * kKbPad;
    const f32 keyW = (innerW - 9.0f * kKbGap) / 10.0f;

    // painel do teclado (surface com risca superior)
    ui.panel(kbX, kbTop, kbW, keyboardHeight(), theme::kTheme.surface);
    ui.panel(kbX, kbTop, kbW, 1.0f, theme::kTheme.border);

    bool typed = false;
    f32 y = kbTop + kKbPad;
    for (u32 row = 0; row < kKbRows; ++row) {
        const u32 n = row == 3 ? 10 : 9;   // linhas 0..2 têm 9 teclas
        const f32 rowW = static_cast<f32>(n) * keyW +
                         static_cast<f32>(n - 1) * kKbGap;
        f32 x = kbX + (kbW - rowW) * 0.5f;
        for (u32 col = 0; col < n; ++col) {
            const char* lbl = keyLabel(st, row, col);
            const u64 id = kKbBase + static_cast<u64>(row) * 10u +
                           static_cast<u64>(col);
            if (ui.button(id, x, y, keyW, kKbKeyH, lbl)) {
                ime::Event ev;
                ev.isText = true;
                ev.text = lbl;
                applyEvent(st, ev);
                typed = true;
            }
            x += keyW + kKbGap;
        }
        // 0.9.6 (G3) · O SHIFT na casa LIVRE da 3ª fila (row 2): alterna
        // maiúsculas/minúsculas — NÃO escreve; aceso quando MAIÚSCULAS
        if (row == 2) {
            const UiRect rs{x, y, keyW, kKbKeyH};
            const bool caps = !st.kbLower;
            const bool held = ui.widgetActive(kKbBase + 46);
            ui.panelRounded(rs.x, rs.y, rs.w, rs.h, theme::kRadiusCard,
                            held ? theme::kTheme.surface2
                                 : (caps ? theme::kTheme.accent
                                         : theme::kTheme.surface));
            ui.frameRounded(rs.x, rs.y, rs.w, rs.h, 1.0f, theme::kRadiusCard,
                            caps ? theme::kTheme.accent
                                 : theme::kTheme.border);
            if (ui.hasFont()) {
                const f32 tw = ui.fontWidth("Aa");
                ui.label(rs.x + (rs.w - tw) * 0.5f, rs.y + 14.0f, "Aa",
                         caps ? theme::kTheme.bg : theme::kTheme.text1);
            }
            if (ui.widgetHit(kKbBase + 46, rs.x, rs.y, rs.w, rs.h)) {
                st.kbLower = !st.kbLower;   // troca de caso — NÃO escreve
            }
        }
        y += kKbKeyH + kKbGap;
    }

    // 0.9.6 (G3) — a linha de baixo COM SETAS (a spec: setas, apagar,
    // enter, espaço): [<][^][v][>][ESPACO 2u][TAB][PAG][APAGA 1.5u][ENTER
    // 1.5u][FECHAR] = 12u + 9g — as teclas continuam >=48dp (1u ~= 54px no
    // retrato 720). As setas EMITEM as Key do IME (o MESMO applyEvent — o
    // caret move-se pelo caminho de sempre; o ^/v sobem/descem linha)
    const f32 unit = (innerW - 9.0f * kKbGap) / 12.0f;
    f32 x = kbX + kKbPad;
    // as SETAS (labels ASCII — o atlas é o da casa; "<" "^" "v" ">")
    {
        struct ArrowKey {
            const char* lbl;
            ime::Key key;
        };
        const ArrowKey arrows[4] = {
            {"<", ime::Key::Left},  {"^", ime::Key::Up},
            {"v", ime::Key::Down},  {">", ime::Key::Right},
        };
        for (int a = 0; a < 4; ++a) {
            if (ui.button(kKbBase + 50 + static_cast<u64>(a), x, y, unit,
                          kKbKeyH, arrows[a].lbl)) {
                ime::Event ev;
                ev.isText = false;
                ev.key = arrows[a].key;
                applyEvent(st, ev);
                typed = true;
            }
            x += unit + kKbGap;
        }
    }
    if (ui.button(kKbBase + 40, x, y, 2.0f * unit, kKbKeyH, "ESPACO")) {
        ime::Event ev;
        ev.isText = true;
        ev.text = " ";
        applyEvent(st, ev);
        typed = true;
    }
    x += 2.0f * unit + kKbGap;
    // 0.9.5 · TAB: os ESQUELETOS do editor que ensina (o mesmo applyEvent
    // do IME — a tecla Tab do GBoard chega aqui pela fila)
    {
        const UiRect rt{x, y, unit, kKbKeyH};
        const bool pressed = ui.widgetHit(kKbBase + 45, rt.x, rt.y, rt.w, rt.h);
        const bool held = ui.widgetActive(kKbBase + 45);
        ui.panelRounded(rt.x, rt.y, rt.w, rt.h, theme::kRadiusCard,
                        held ? theme::kTheme.surface2
                             : theme::kTheme.surface);
        ui.frameRounded(rt.x, rt.y, rt.w, rt.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
        ui.labelStyled(rt.x, rt.y + 14.0f, "TAB",
                       theme::kTheme.accent,
                       theme::fontScale(theme::kFontCaption), 0);
        if (pressed) {
            ime::Event ev;
            ev.isText = false;
            ev.key = ime::Key::Tab;
            applyEvent(st, ev);
            typed = true;
        }
    }
    x += unit + kKbGap;
    {
        char pg[8];
        std::snprintf(pg, sizeof(pg), "%s", st.kbSym ? "ABC" : "123");
        if (ui.button(kKbBase + 41, x, y, unit, kKbKeyH, pg)) {
            st.kbSym = !st.kbSym;   // troca de página — NÃO escreve
        }
    }
    x += unit + kKbGap;
    if (ui.button(kKbBase + 42, x, y, 1.5f * unit, kKbKeyH, "APAGA")) {
        ime::Event ev;
        ev.isText = false;
        ev.key = ime::Key::Del;
        applyEvent(st, ev);
        typed = true;
    }
    x += 1.5f * unit + kKbGap;
    if (ui.button(kKbBase + 43, x, y, 1.5f * unit, kKbKeyH, "ENTER")) {
        ime::Event ev;
        ev.isText = false;
        ev.key = ime::Key::Enter;
        applyEvent(st, ev);
        typed = true;
    }
    x += 1.5f * unit + kKbGap;
    {
        // FECHAR o teclado (o ChevronDown ocupa a última unidade)
        const UiRect r{x, y, unit, kKbKeyH};
        const bool pressed = ui.widgetHit(kKbBase + 44, r.x, r.y, r.w, r.h);
        const bool held = ui.widgetActive(kKbBase + 44);
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        held ? theme::kTheme.surface2
                             : theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::ChevronDown,
                        r.x + (r.w - 24.0f) * 0.5f,
                        r.y + (r.h - 24.0f) * 0.5f, 24.0f,
                        theme::kTheme.text1);
        if (pressed) {
            st.kbOpen = false;
        }
    }
    return typed;
}

} // namespace

void rememberTicName(State& st, const Scene& scene) {
    st.ticName[0] = '\0';
    if (const Tic* t = scene.get(st.tic)) {
        std::snprintf(st.ticName, sizeof(st.ticName), "%.60s",
                      t->name.c_str());
    }
}

bool revalidateTic(State& st, Scene& scene) {
    if (!st.open) {
        return false;
    }
    if (scene.get(st.tic)) {
        return true;   // handle vivo (o load foi in-place)
    }
    if (st.ticName[0]) {
        const Handle h = scene.find(st.ticName);
        if (h.valid()) {
            st.tic = h;
            return true;
        }
    }
    return false;   // o TIC morreu mesmo (cena trocada)
}

void open(State& st, Scene& scene, Handle tic) {
    st.open = true;
    st.tic = tic;
    st.scrollOff = 0.0f;
    st.blink = 0.0f;
    st.errLine = 0;
    st.errMsg.clear();
    st.running = false;
    st.kbOpen = false;
    st.kbSym = false;
    st.kbLower = false;
    // carrega o fonte do componente (se existir); SEM fonte guardada abre
    // com o ESQUELETO base e o cursor NO INTERIOR (G0-2)
    bool loaded = false;
    if (tic.valid()) {
        if (Tic* t = scene.get(tic)) {
            if (const ScriptComp* sc = t->getComponent<ScriptComp>()) {
                if (!sc->source.empty()) {
                    st.buf = sc->source;
                    st.caret = static_cast<u32>(st.buf.size());
                    loaded = true;
                }
            }
        }
    }
    if (!loaded) {
        st.buf = kSkeleton;
        st.caret = kSkeletonCaret;
    }
    rememberTicName(st, scene);
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
    if (st.caret > st.buf.size()) {
        st.caret = static_cast<u32>(st.buf.size());
    }
    // 0.9.5: qualquer EDIÇÃO limpa a explicação do toque (a strip volta à
    // mini-descrição em tempo real da palavra que está a ser digitada)
    if (ev.isText || ev.key == ime::Key::Del || ev.key == ime::Key::Enter) {
        st.helpTapped = false;
        st.helpWord.clear();
    }
    if (ev.isText) {
        insertAtCaret(st, ev.text.c_str());
        return true;
    }
    switch (ev.key) {
        case ime::Key::Del:
            chopBeforeCaret(st);
            return true;
        case ime::Key::Enter:
            insertAtCaret(st, "\n");
            return true;
        case ime::Key::Tab: {
            // 0.9.5 · OS ESQUELETOS POR TAB: a palavra antes do caret é
            // substituída pelo esqueleto da entrada do REGISTO (o texto do
            // resultado INCLUI a palavra: 'exist' → 'exist(){ } notexist{ }'
            // com o caret NO INTERIOR); casando por EXATO e depois por
            // PREFIXO; sem esqueleto → indenta 2 espaços (o clássico)
            const std::string w = wordBeforeCaret(st);
            const voni::reg::Entry* e = nullptr;
            if (!w.empty()) {
                e = voni::reg::find(w);
                if (!e || !e->skeleton || !*e->skeleton) {
                    e = voni::reg::prefixMatch(w);
                }
            }
            if (e && e->skeleton && *e->skeleton &&
                e->skeletonCaret <= std::strlen(e->skeleton)) {
                const u32 ws = st.caret -
                               static_cast<u32>(w.size());
                st.buf.erase(ws, w.size());
                st.caret = ws;
                insertAtCaret(st, e->skeleton);
                st.caret = ws + e->skeletonCaret;
            } else {
                insertAtCaret(st, "  ");
            }
            return true;
        }
        case ime::Key::Left:
            st.caret = prevCodePoint(st.buf, st.caret);
            return true;
        case ime::Key::Right:
            st.caret = nextCodePoint(st.buf, st.caret);
            return true;
        case ime::Key::Up:
        case ime::Key::Down: {
            // mesma coluna (em bytes) na linha anterior/seguinte
            const u32 col = columnOf(st.buf, st.caret);
            if (ev.key == ime::Key::Up) {
                if (lineStartOf(st.buf, st.caret) == 0) {
                    st.caret = 0;   // já na 1ª linha
                    return true;
                }
                const u32 lineStart = lineStartOf(st.buf, st.caret);
                const u32 prevLineStart =
                    lineStartOf(st.buf, lineStart - 1);
                u32 target = prevLineStart + col;
                // não passa do fim da linha anterior
                const u32 lineEnd = lineStart - 1;
                if (target > lineEnd) {
                    target = lineEnd;
                }
                st.caret = target;
            } else {
                const u32 lineStart = lineStartOf(st.buf, st.caret);
                u32 lineEnd = lineStart;
                while (lineEnd < st.buf.size() && st.buf[lineEnd] != '\n') {
                    ++lineEnd;
                }
                if (lineEnd >= st.buf.size()) {
                    st.caret = static_cast<u32>(st.buf.size());  // última linha
                    return true;
                }
                const u32 nextStart = lineEnd + 1;
                u32 nextEnd = nextStart;
                while (nextEnd < st.buf.size() && st.buf[nextEnd] != '\n') {
                    ++nextEnd;
                }
                u32 target = nextStart + col;
                if (target > nextEnd) {
                    target = nextEnd;
                }
                st.caret = target;
            }
            return true;
        }
        default:
            return false;
    }
}

u32 lineCount(const State& st) {
    u32 n = 1;
    for (const char c : st.buf) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// 0.9.5 · O EDITOR QUE ENSINA — as funções PURAS (o registo alimenta tudo:
// a mini-descrição em tempo real, o toque numa palavra, os níveis)
// ---------------------------------------------------------------------------
std::string wordBeforeCaret(const State& st) {
    u32 i = st.caret < st.buf.size() ? st.caret
                                     : static_cast<u32>(st.buf.size());
    u32 s = i;
    while (s > 0) {
        const unsigned char c =
            static_cast<unsigned char>(st.buf[s - 1]);
        if (std::isalnum(c) || c == '_') {
            --s;
        } else {
            break;
        }
    }
    // também acentos já digitados: code points >127 contam para a palavra
    // (o byte líder UTF-8 é >=0xC0 — inclui os acentos da G1-2)
    return st.buf.substr(s, i - s);
}

std::string wordAtOffset(const State& st, u32 byteOffset) {
    const u32 n = static_cast<u32>(st.buf.size());
    if (byteOffset > n) {
        byteOffset = n;
    }
    // recua ao início do identificador
    u32 s = byteOffset;
    while (s > 0) {
        const unsigned char c =
            static_cast<unsigned char>(st.buf[s - 1]);
        if (std::isalnum(c) || c == '_' || c >= 0x80) {
            --s;
        } else {
            break;
        }
    }
    u32 e = byteOffset;
    while (e < n) {
        const unsigned char c = static_cast<unsigned char>(st.buf[e]);
        if (std::isalnum(c) || c == '_' || c >= 0x80) {
            ++e;
        } else {
            break;
        }
    }
    return st.buf.substr(s, e - s);
}

const voni::reg::Entry* helpEntryFor(const State& st) {
    if (st.helpLevel >= 2) {
        return nullptr;   // Silencioso: nada
    }
    if (st.helpTapped && !st.helpWord.empty()) {
        return voni::reg::find(st.helpWord);   // o toque: casamento EXATO
    }
    const std::string w = wordBeforeCaret(st);
    if (w.empty()) {
        return nullptr;
    }
    return voni::reg::prefixMatch(w);         // digitando: prefixo (1ª letra+)
}

std::string helpStripLine1(const State& st) {
    const voni::reg::Entry* e = helpEntryFor(st);
    if (!e) {
        return "";
    }
    return std::string(e->name) + ": " + e->desc;
}

std::string helpStripLine2(const State& st) {
    const voni::reg::Entry* e = helpEntryFor(st);
    if (!e) {
        return "";
    }
    // linha 2 (exemplo): no Iniciante SEMPRE; no Normal só no TOQUE (a
    // explicação pedida vem com exemplo); no Silencioso nunca
    if (st.helpLevel == 0 || (st.helpTapped && st.helpLevel == 1)) {
        return std::string("ex.: ") + e->example;
    }
    return "";
}

// 0.9.6 (G2-7e) · A TROCA: substitui a PALAVRA INTEIRA fixFrom pela
// fixTo na LINHA do erro (a 1ª ocorrência como palavra solta); o caret
// segue a edição (fica logo após o texto inserido) e o erro limpa — o
// buffer é a verdade, o render segue-o (R-010)
void applyFix(State& st) {
    if (st.fixFrom.empty() || st.fixTo.empty() || st.errLine == 0) {
        return;
    }
    // início em bytes da linha errLine (1-based)
    u32 ls = 0;
    for (u32 k = 1; k < st.errLine && ls < st.buf.size(); ++k) {
        while (ls < st.buf.size() && st.buf[ls] != '\n') {
            ++ls;
        }
        if (ls < st.buf.size()) {
            ++ls;
        }
    }
    const u32 le = [&]() {
        u32 e = ls;
        while (e < st.buf.size() && st.buf[e] != '\n') {
            ++e;
        }
        return e;
    }();
    // a palavra INTEIRA (delimitada por não-identificadores)
    const std::string from = st.fixFrom;
    u32 at = ls;
    while (at + from.size() <= le) {
        bool match = true;
        for (u32 k = 0; k < from.size(); ++k) {
            if (st.buf[at + k] != from[k]) {
                match = false;
                break;
            }
        }
        const bool leftOk =
            at == ls || !std::isalnum(static_cast<unsigned char>(
                                         st.buf[at - 1])) &&
                            st.buf[at - 1] != '_';
        const u32 after = at + (u32)from.size();
        const bool rightOk =
            after >= le ||
            (!std::isalnum(static_cast<unsigned char>(st.buf[after])) &&
             st.buf[after] != '_');
        if (match && leftOk && rightOk) {
            st.buf.replace(at, from.size(), st.fixTo);
            st.caret = at + (u32)st.fixTo.size();
            st.errLine = 0;
            st.errMsg.clear();
            st.fixFrom.clear();
            st.fixTo.clear();
            st.blink = 0.0f;
            return;
        }
        ++at;
    }
    // a palavra não está na linha (fonte mudada?) — limpa o botão só
    st.fixFrom.clear();
    st.fixTo.clear();
}

int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h,
         f32 dt) {
    (void)in;
    if (!st.open) {
        return 0;
    }

    // 0.9.6 (G1-1/G1-2 — SAFE AREA): insets REAIS de g_ui.safeArea() (o
    // único sítio onde vivem): fundo na superfície toda, CABEÇALHO no
    // inset do topo + 56dp (a 1ª linha do código nunca sob o cabeçalho e
    // o título nunca sob a faixa preta) e o CORPO/barra de erro/strip/
    // teclado param no inset de baixo (nada sob a barra de navegação —
    // as teclas deixam de ficar tapadas). Em desktop/tests insets=0.
    const safe::Insets ins = ui.safeArea();
    const f32 topY = ins.top;
    const f32 hdrH = kTopH;               // 56dp (a parte ÚTIL)
    const f32 contentW = w - ins.left - ins.right;

    // fundo opaco FULL-SCREEN (bg — o mesmo do editor de código)
    ui.panel(0.0f, 0.0f, w, h, theme::kTheme.bg);

    // ---- barra de topo: inset + 56dp ----------------------------------------
    ui.panel(ins.left, topY, contentW, hdrH, theme::kTheme.surface);
    ui.panel(ins.left, topY + hdrH - 1.0f, contentW, 1.0f,
             theme::kTheme.border);

    icons::drawIcon(ui, icons::Icon::Back, ins.left + 16.0f,
                    topY + hdrH / 2.0f - 12.0f, 24.0f, theme::kTheme.text1);
    int result = 0;
    if (ui.widgetHit(kBackId, ins.left, topY, hdrH, hdrH)) {
        result = 1;
    }

    // título 20sp + hint 12sp (na parte útil — sem corte)
    ui.labelStyled(ins.left + hdrH + 8.0f, topY + theme::kHeaderTitleBase,
                   "Script", theme::kTheme.text1,
                   theme::fontScale(theme::kFontScreen), 0);
    ui.labelStyled(ins.left + hdrH + 8.0f, topY + theme::kHeaderSubBase,
                   "V.ONI · .voni", theme::kTheme.text2,
                   theme::fontScale(theme::kFontCaption), 0);

    // LUPA (G0-3): abre as Docs POR CIMA (a pesquisa filtra as entradas
    // estruturadas e mostra o exemplo). Alvo 48dp.
    {
        const f32 docsX = ins.left + contentW - 72.0f * 2.0f - 16.0f - 8.0f
                          - 48.0f;
        const UiRect r{docsX, topY + (hdrH - 48.0f) * 0.5f, 48.0f, 48.0f};
        const bool held = ui.widgetActive(kDocsId);
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        held ? theme::kTheme.surface2
                             : theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::Search,
                        r.x + (r.w - 24.0f) * 0.5f,
                        r.y + (r.h - 24.0f) * 0.5f, 24.0f,
                        theme::kTheme.text1);
        if (ui.widgetHit(kDocsId, r.x, r.y, r.w, r.h)) {
            result = 4;
        }

        // 0.9.5 · COPIAR REFERÊNCIA (📋): a referência V.ONI COMPLETA como
        // texto colável p/ IAs — o main põe no clipboard via JNI (result 6)
        const UiRect rc{docsX - 56.0f, topY + (hdrH - 48.0f) * 0.5f, 48.0f,
                        48.0f};
        const bool heldC = ui.widgetActive(kCopyRefId);
        ui.panelRounded(rc.x, rc.y, rc.w, rc.h, theme::kRadiusCard,
                        heldC ? theme::kTheme.surface2
                              : theme::kTheme.surface);
        ui.frameRounded(rc.x, rc.y, rc.w, rc.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::Copy,
                        rc.x + (rc.w - 24.0f) * 0.5f,
                        rc.y + (rc.h - 24.0f) * 0.5f, 24.0f,
                        theme::kTheme.text1);
        if (ui.widgetHit(kCopyRefId, rc.x, rc.y, rc.w, rc.h)) {
            result = 6;
        }

        // 0.9.6 (G3) · O BOTÃO DO TECLADO PRÓPRIO: acende quando o teclado
        // está aberto; tocar ABRE o teclado in-app e ESCONDE o IME do
        // sistema (a POLÍTICA: os dois são ALTERNATIVAS — nunca um por
        // cima do outro; o toque no corpo pede o IME do sistema e fecha o
        // teclado próprio)
        {
            const UiRect rk{docsX - 152.0f, topY + (hdrH - 48.0f) * 0.5f,
                            48.0f, 48.0f};
            const bool heldK = ui.widgetActive(kKbToggleId);
            ui.panelRounded(rk.x, rk.y, rk.w, rk.h, theme::kRadiusCard,
                            st.kbOpen ? theme::kTheme.accent
                                      : (heldK ? theme::kTheme.surface2
                                               : theme::kTheme.surface));
            ui.frameRounded(rk.x, rk.y, rk.w, rk.h, 1.0f, theme::kRadiusCard,
                            st.kbOpen ? theme::kTheme.accent
                                      : theme::kTheme.border);
            icons::drawIcon(ui, icons::Icon::Keyboard,
                            rk.x + (rk.w - 24.0f) * 0.5f,
                            rk.y + (rk.h - 24.0f) * 0.5f, 24.0f,
                            st.kbOpen ? theme::kTheme.bg
                                      : theme::kTheme.text1);
            if (ui.widgetHit(kKbToggleId, rk.x, rk.y, rk.w, rk.h)) {
                if (!st.kbOpen) {
                    st.kbOpen = true;
                    result = 7;   // o main esconde o IME do sistema
                } else {
                    st.kbOpen = false;
                }
            }
        }

        // 0.9.5 · O NÍVEL DA AJUDA (I/N/S): Iniciante (desc+exemplo) ·
        // Normal (desc) · Silencioso (nada) — um toque cicla
        const UiRect rl{docsX - 200.0f, topY + (hdrH - 48.0f) * 0.5f, 48.0f,
                        48.0f};
        const bool heldL = ui.widgetActive(kHelpLevelId);
        ui.panelRounded(rl.x, rl.y, rl.w, rl.h, theme::kRadiusCard,
                        heldL ? theme::kTheme.surface2
                              : theme::kTheme.surface);
        ui.frameRounded(rl.x, rl.y, rl.w, rl.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
        const char* lvl = st.helpLevel == 0 ? "I"
                          : st.helpLevel == 1 ? "N" : "S";
        ui.labelStyled(rl.x, rl.y + 14.0f, lvl,
                       st.helpLevel == 2 ? theme::kTheme.text2
                                         : theme::kTheme.accent,
                       theme::fontScale(theme::kFontBody), 0);
        if (ui.widgetHit(kHelpLevelId, rl.x, rl.y, rl.w, rl.h)) {
            st.helpLevel = static_cast<u8>((st.helpLevel + 1) % 3);
        }
    }

    // RUN / STOP 48dp à direita (alvos ≥48; estado: running aceso = Stop)
    const f32 btnW = 72.0f;
    const f32 runX = ins.left + contentW - btnW * 2.0f - 16.0f;
    const f32 stopX = ins.left + contentW - btnW - 8.0f;
    const f32 btnY = topY + (hdrH - 48.0f) / 2.0f;
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
    const f32 kbH = st.kbOpen ? keyboardHeight() : 0.0f;
    // 0.9.5: a STRIP FINA DE AJUDA junto à barra de erro — a mini-descrição
    // em tempo real (DESDE A 1ª LETRA da palavra a meio da digitação) ou a
    // explicação do toque (com exemplo); SEM ENCHER O ECRÃ (some quando
    // não há nada a mostrar / nível Silencioso)
    const std::string strip1 = helpStripLine1(st);
    const std::string strip2 = helpStripLine2(st);
    const f32 stripH = strip1.empty()
                           ? 0.0f
                           : (strip2.empty() ? kHelpStripH : kHelpStrip2H);
    const UiRect body{ins.left, topY + hdrH, contentW,
                      h - ins.bottom - (topY + hdrH) - errBarH - kbH - stripH};
    const f32 lh = lineHeight(ui);
    const u32 nLines = lineCount(st);
    const f32 contentH = static_cast<f32>(nLines) * lh + 16.0f;

    ui.beginScroll(kScrollId, body, contentH);

    // o scroll SEGUE O CARET (não o fim — o caret agora move-se livre):
    // linha do caret visível (baixo se desce, topo se sobe)
    u32 caretLine = 0;
    {
        u32 i = 0;
        while (i < st.caret && i < st.buf.size()) {
            if (st.buf[i] == '\n') {
                ++caretLine;
            }
            ++i;
        }
    }
    const f32 caretTop = 8.0f + static_cast<f32>(caretLine) * lh;
    const f32 caretBot = caretTop + lh;
    f32 want = 0.0f;
    if (contentH > body.h) {
        want = ui.scrollOffset();
        if (caretBot - want > body.h) {
            want = caretBot - body.h;   // caret saiu por baixo — segue
        }
        if (caretTop - want < 0.0f) {
            want = caretTop;            // saiu por cima — segue
        }
        if (want > contentH - body.h) {
            want = contentH - body.h;
        }
    }
    ui.scrollSetOffset(kScrollId, want);
    const f32 off = ui.scrollOffset();

    // gutter de nºs de linha (48dp — fundo surface, texto text2 12sp)
    ui.panel(ins.left, body.y, 48.0f, body.h, theme::kTheme.surface);
    ui.panel(ins.left + 48.0f, body.y, 1.0f, body.h, theme::kTheme.border);
    const f32 xCode = ins.left + 64.0f;

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
        ui.labelStyled(ins.left + 8.0f, y + 4.0f, num,
                       st.errLine == i + 1 ? theme::kTheme.danger
                                           : theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);

        // tokens da linha (classes → cores do Theme) — 0.9.6 (G2-7b): o
        // render usa as PEÇAS COM GAPS (renderPieces): a concatenação é a
        // LINHA INTEIRA — espaços e `{ }` desenham, o x avança pela linha
        // real e o CARET (que mede a linha toda) fica EXATAMENTE onde se vê
        const std::string lineStr(line);
        const auto pieces = renderPieces(lineStr, bcState);
        {
            f32 x = xCode;
            char piece[256];
            for (const RenderPiece& rp : pieces) {
                if (rp.len == 0 || rp.begin >= lineStr.size()) {
                    continue;
                }
                const u32 pl = rp.len < sizeof(piece) - 1
                                   ? rp.len
                                   : (u32)sizeof(piece) - 1;
                std::memcpy(piece, lineStr.data() + rp.begin, pl);
                piece[pl] = '\0';
                ui.label(x, y, piece, clsColor(rp.cls));
                x += ui.fontWidth(piece);
            }
        }

        // CARET piscante NA POSIÇÃO do caret (linha/coluna — G0-1)
        if (i == caretLine) {
            st.blink += dt;
            if (std::fmod(st.blink, 1.2f) < 0.6f) {
                const u32 colBytes = st.caret >= start && st.caret <= end
                                         ? st.caret - start
                                         : 0;
                char before[256];
                const u32 bl = colBytes < sizeof(before)
                                   ? colBytes
                                   : (u32)sizeof(before) - 1;
                std::memcpy(before, line, bl);
                before[bl] = '\0';
                const f32 xCaret = xCode + ui.fontWidth(before) + 2.0f;
                ui.panel(xCaret, y, 2.0f, lh - 8.0f, theme::kTheme.accent);
            }
        }
        start = (end < st.buf.size()) ? end + 1 : end;
    }
    ui.endScroll();

    // ---- teclado in-app (G0-1): dokado no fundo (ACIMA do inset de baixo
    // desde 0.9.6 — nunca sob a barra de navegação), acima da strip de ajuda
    if (st.kbOpen) {
        drawKeyboard(ui, st, w,
                    h - ins.bottom - errBarH - stripH - keyboardHeight());
    }

    // ---- a STRIP DE AJUDA (0.9.5): entre o teclado e a barra de erro -----
    if (stripH > 0.0f) {
        const f32 stripY = h - ins.bottom - errBarH - kbH - stripH;
        ui.panel(ins.left, stripY, contentW, stripH, theme::kTheme.surface);
        ui.panel(ins.left, stripY, contentW, 1.0f, theme::kTheme.border);
        ui.panel(ins.left, stripY + 1.0f, 3.0f, stripH - 1.0f,
                 theme::kTheme.accent);   // risca accent à esquerda
        // linha 1 (truncada à largura útil — labelStyled corta com "…")
        char l1[200];
        std::snprintf(l1, sizeof(l1), "%s", strip1.c_str());
        ui.labelStyled(ins.left + 12.0f, stripY + 8.0f, l1,
                       theme::kTheme.text1,
                       theme::fontScale(theme::kFontCaption),
                       static_cast<u32>(contentW) - 24u);
        if (!strip2.empty()) {
            char l2[200];
            std::snprintf(l2, sizeof(l2), "%s", strip2.c_str());
            ui.labelStyled(ins.left + 12.0f, stripY + 30.0f, l2,
                           theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption),
                           static_cast<u32>(contentW) - 24u);
        }
    }

    // ---- barra de ERRO com linha + mensagem (§12) + SUBSTITUIR (G2-7e) ----
    if (st.errLine) {
        const f32 errY = h - ins.bottom - kErrH;
        ui.panel(ins.left, errY, contentW, kErrH, theme::kTheme.danger);
        char msg[160];
        std::snprintf(msg, sizeof(msg), "linha %u: %s", st.errLine,
                      st.errMsg.empty() ? "erro" : st.errMsg.c_str());
        // a mensagem abre espaço para o botão quando há substituição
        const bool hasFix = !st.fixFrom.empty() && !st.fixTo.empty();
        ui.labelFitted(ins.left + 12.0f, errY + 12.0f, msg,
                       theme::kTheme.bg,
                       hasFix ? contentW - 148.0f : contentW - 24.0f);
        // 0.9.6 (G2-7e) · O BOTÃO SUBSTITUIR: troca a palavra estrangeira
        // pelo equivalente V.ONI NO BUFFER (a linha do erro, palavra
        // inteira); o caret segue a edição e o erro limpa — o dono vê o
        // código ficar certo com UM toque
        if (hasFix) {
            const UiRect fb{ins.left + contentW - 128.0f, errY + 4.0f,
                            120.0f, kErrH - 8.0f};
            const bool held = ui.widgetActive(kFixId);
            ui.panelRounded(fb.x, fb.y, fb.w, fb.h, theme::kRadiusCard,
                            held ? theme::kTheme.bg : theme::kTheme.danger);
            ui.frameRounded(fb.x, fb.y, fb.w, fb.h, 1.0f,
                            theme::kRadiusCard, theme::kTheme.bg);
            char lbl[96];
            std::snprintf(lbl, sizeof(lbl), "Substituir %s",
                          st.fixTo.c_str());
            if (ui.hasFont()) {
                const f32 tw = ui.fontWidth(lbl);
                ui.label(fb.x + (fb.w - tw) * 0.5f, fb.y + 14.0f, lbl,
                         theme::kTheme.bg);
            }
            if (ui.widgetHit(kFixId, fb.x, fb.y, fb.w, fb.h)) {
                applyFix(st);
            }
        }
    }

    // toque PARADO no corpo (o scroll devolve o tap — o mesmo padrão da
    // Hierarchy/Settings): o main RE-PETE o IME do sistema (G0-1 — sem
    // perder foco) e o teclado in-app abre se não estava aberto. Digitar
    // NUNCA fecha (só o back fecha — por construção).
    // 0.9.5: a PALAVRA sob o dedo alimenta a strip (a explicação com
    // exemplo vem das Docs = o registo) — "toque numa palavra → linha de
    // explicação com exemplo"
    if (result == 0) {
        f32 tx = 0.0f, ty = 0.0f;
        if (ui.scrollTap(kScrollId, tx, ty)) {
            // 0.9.6 (G3) · A POLÍTICA DE COEXISTÊNCIA (a decisão primeiro,
            // como a spec pede): o teclado PRÓPRIO e o IME do SISTEMA são
            // ALTERNATIVAS — ambos ancoram no fundo e sobrepõam-se. O
            // toque no corpo pede o IME DO SISTEMA (GBoard: acentos,
            // gestos) e o teclado próprio FECHA; o botao do teclado no
            // cabeçalho faz o INVERSO (abre o próprio + esconde o IME,
            // result 7). NUNCA os dois ao mesmo tempo.
            st.kbOpen = false;
            // a linha/coluna do toque → o offset em bytes → a palavra
            {
                const f32 lh2 = lineHeight(ui);
                const f32 off2 = ui.scrollOffset();
                f32 rel = ty - body.y + off2 - 8.0f;
                if (rel < 0.0f) {
                    rel = 0.0f;
                }
                u32 li = static_cast<u32>(rel / lh2);
                const u32 nL = lineCount(st);
                if (li >= nL) {
                    li = nL - 1;
                }
                // início em bytes da linha li
                u32 ls2 = 0;
                for (u32 k = 0; k < li && ls2 < st.buf.size(); ++k) {
                    while (ls2 < st.buf.size() && st.buf[ls2] != '\n') {
                        ++ls2;
                    }
                    if (ls2 < st.buf.size()) {
                        ++ls2;
                    }
                }
                // coluna: acumula a largura até passar o x do toque
                const f32 xCode = ins.left + 64.0f;
                u32 bo = ls2;
                f32 acc = 0.0f;
                while (bo < st.buf.size() && st.buf[bo] != '\n') {
                    char one[2] = {st.buf[bo], 0};
                    const f32 cw = ui.fontWidth(one);
                    if (acc + cw * 0.5f >= tx - xCode) {
                        break;
                    }
                    acc += cw;
                    ++bo;
                }
                const std::string word = wordAtOffset(st, bo);
                if (!word.empty() && voni::reg::find(word)) {
                    st.helpTapped = true;
                    st.helpWord = word;
                } else {
                    st.helpTapped = false;
                    st.helpWord.clear();
                }
            }
            result = 5;
        }
    }

    return result;
}

} // namespace scriptwin
} // namespace editor
} // namespace vv
