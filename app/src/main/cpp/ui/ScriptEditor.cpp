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

#include "platform/EngineLog.h"   // 0.9.6.2 (R-019): o log do cursor/toque

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

// (0.9.6.6 · GRUPO C: a fórmula interna pura — a EXPORTADA está fora do
// anon ns e é a que o draw/o toque/os testes partilham)
f32 lineHeightImpl(const TextMetrics& m) {
    const f32 h = m.ascent + m.descent + 6.0f;
    return h > theme::dp(28.0f) ? h : theme::dp(28.0f);   // piso 28dp (F5.0)
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

} // namespace

// ---- 0.9.6.2 (R-019) · A GEOMETRIA DO CURSOR — as versões EXPORTADAS ----
// (o toque no corpo e os testes partilham ESTAS funções — a fonte única da
// conversão y/x de ecrã → offset no buffer)
u32 lineStartOfOffset(const std::string& s, u32 off) {
    return lineStartOf(s, off);
}
u32 columnOfOffset(const std::string& s, u32 off) {
    return columnOf(s, off);
}
u32 lineIndexOf(const std::string& s, u32 off) {
    u32 line = 0;
    u32 i = off < s.size() ? off : static_cast<u32>(s.size());
    for (u32 k = 0; k < i; ++k) {
        if (s[k] == '\n') {
            ++line;
        }
    }
    return line;
}
u32 lineEndOf(const std::string& s, u32 lineStart) {
    u32 e = lineStart < s.size() ? lineStart : static_cast<u32>(s.size());
    while (e < s.size() && s[e] != '\n') {
        ++e;
    }
    return e;
}
std::string indentationOfLine(const std::string& line) {
    u32 i = 0;
    while (i < line.size() && line[i] == ' ') {
        ++i;
    }
    return line.substr(0, i);
}

// ---- 0.9.6.6 (GRUPO C · C2/C3) · A GEOMETRIA/ÍNDICE ÚNICOS -----------------

f32 lineHeight(UiContext& ui) {
    return lineHeightImpl(ui.textMetrics());
}

// O CONTADOR de trabalho do draw (reset em cada draw; a FASE afere que o
// culling deixa de ser O(buffer): 800 linhas scrolled → ~20 tokenizadas)
u32 dbgLinesTokenized = 0;

const std::vector<u32>& ensureLineIndex(State& st) {
    const bool stale = st.indexVersion != st.bufVersion ||
                       st.lineStarts.empty() ||
                       st.lineStarts.back() > st.buf.size();
    if (!stale) {
        return st.lineStarts;
    }
    // rebuild O(linhas) POR EDIÇÃO (o utilizador digita devagar; o draw e o
    // toque passam a O(1)/O(log n) e o tokenizador só vê as visíveis)
    st.lineStarts.clear();
    st.lineBc.clear();
    st.lineStarts.push_back(0);
    voni::hl::BlockCommentState bc;
    st.lineBc.push_back(bc);   // estado ANTES da linha 0 (o default)
    for (u32 i = 0; i < st.buf.size(); ++i) {
        if (st.buf[i] == '\n') {
            const u32 ls = st.lineStarts.back();
            // o estado bc AO FIM desta linha (tokeniza-a UMA VEZ por edição;
            // o 1.º visível do draw começa JÁ certo — nunca varre desde 0)
            char line[256];
            const u32 len = (i - ls) < sizeof(line) - 1
                                ? (i - ls)
                                : (u32)sizeof(line) - 1;
            std::memcpy(line, st.buf.data() + ls, len);
            line[len] = '\0';
            renderPieces(std::string(line), bc);
            st.lineBc.push_back(bc);
            st.lineStarts.push_back(i + 1);
        }
    }
    st.indexVersion = st.bufVersion;
    return st.lineStarts;
}

u32 lineCount(State& st) {
    ensureLineIndex(st);
    return static_cast<u32>(st.lineStarts.size());
}

namespace {

void insertAtCaret(State& st, const char* utf8) {
    if (!utf8 || !*utf8) {
        return;
    }
    st.buf.insert(st.caret, utf8);
    st.caret += static_cast<u32>(std::strlen(utf8));
    ++st.bufVersion;   // 0.9.6.6: o índice de linhas rebuilda na próxima uso
}

void chopBeforeCaret(State& st) {
    if (st.caret == 0) {
        return;
    }
    const u32 p = prevCodePoint(st.buf, st.caret);
    st.buf.erase(p, st.caret - p);
    st.caret = p;
    ++st.bufVersion;   // 0.9.6.6
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

// ----------------------------------------------------------------------------
// 0.9.6.8 (GRUPO E) · A BARRA DE SÍMBOLOS — o substituto do teclado da engine
// ----------------------------------------------------------------------------
// O rastreador E manda: «barra de símbolos 40dp sobre o IME (teclado da
// engine REMOVIDO)». O QWERTY in-app de 280dp/54 teclas (0.9.6.1 G2-6,
// FASE 9 G0-1) SAIU — o dono digita pelo IME DO SISTEMA (GBoard: acentos,
// gestos, dicionário, deslizante) e a BARRA cobre o que o GBoard NÃO tem
// à mão: os 22 símbolos da spec V.ONI (a página «123» do teclado antigo —
// { } ( ) [ ] = + - * / < > ! , . ; : " _ # @ TODOS, como a spec exigia).
//
// A barra desenha SOBRE o IME (dokada na faixa que a VvActivity MEDE —
// ime::bottomInset, o rootHeight−visibleFrame.bottom da janela) e as
// teclas emitem pelo MESMO applyEvent do IME: uma única fonte de verdade
// para a edição (o contrato do teclado antigo, mantido).
//
// A PÁGINA adaptativa: a 1ª tecla é o SELETOR (o padrão «?123» do GBoard,
// à esquerda) mostrando «1/3»; as restantes são símbolos. O nº de teclas
// visíveis adapta à largura (9 no device 360dp, 14 no harness 720dp) — as
// teclas ficam SEMPRE ≥40dp de largura e a barra é EXATAMENTE 40dp (a
// spec E; a exceção compacta do validador, vigiada pela sentinela R-027).
// ----------------------------------------------------------------------------

// os 22 símbolos da spec (a página 123 do teclado antigo, SEM os extras
// $ % & | ~ ^ \ ' — o GBoard já os tem na própria página de símbolos):
// { } ( ) [ ] = + - * / < > ! , . ; : " _ # @ — TODOS, como a spec exigia
const char* const kSymbols[22] = {
    "{", "}", "(", ")", "[", "]", "=", "+", "-", "*",
    "/", "<", ">", "!", ",", ".", ";", ":", "\"", "_",
    "#", "@",
};

f32 symbolBarHeight() {
    return theme::dp(40.0f);   // a spec E (a R-027 recalibra a R-018: o
                               // teclado de 48dp/tecla morreu com o teclado)
}

u32 symKeysVisible(f32 contentW) {
    // teclas de >=40dp de largura: quantas cabem (piso 9, teto 14 — no
    // device 360dp dá 9 exato; no harness 720dp o teto evita «baías»)
    const f32 minKey = theme::dp(40.0f);
    u32 n = static_cast<u32>(contentW / minKey);
    if (n < 9) {
        n = 9;
    }
    if (n > 14) {
        n = 14;
    }
    return n;
}

u32 symPageCount(f32 contentW) {
    const u32 perPage = symKeysVisible(contentW) - 1;   // a 1ª é o seletor
    return (22u + perPage - 1u) / perPage;              // ceil(22/perPage)
}

// desenha a barra dokada SOBRE o IME (y = o TOPO da barra); devolve true
// se alguma tecla EMITIU texto (o log de cada símbolo sai AQUI)
bool drawSymbolBar(UiContext& ui, State& st, f32 w, f32 y) {
    const safe::Insets ins = ui.safeArea();
    const f32 contentW = w - ins.left - ins.right;
    const f32 barH = symbolBarHeight();
    const u32 nVis = symKeysVisible(contentW);
    const f32 keyW = contentW / static_cast<f32>(nVis);
    const u32 perPage = nVis - 1;
    const u32 nPages = symPageCount(contentW);
    if (st.symPage >= nPages) {
        st.symPage = 0;   // a largura mudou (rotação/drag) — recomeça
    }

    // a faixa da barra (surface com risca no topo — a linguagem visual
    // do teclado antigo, agora a 40dp)
    ui.panel(ins.left, y, contentW, barH, theme::kTheme.surface);
    ui.panel(ins.left, y, contentW, 1.0f, theme::kTheme.border);

    f32 x = ins.left;
    // A TECLA DO SELETOR (a 1ª — o padrão «?123» do GBoard): «1/3»
    {
        char pg[8];
        std::snprintf(pg, sizeof(pg), "%u/%u", st.symPage + 1, nPages);
        if (ui.buttonCompact(kSymBarPageId, x, y, keyW, barH, pg)) {
            st.symPage = static_cast<u8>((st.symPage + 1) % nPages);
            elog::info("editor: barra de simbolos -> pagina %u/%u",
                       st.symPage + 1, nPages);
        }
        x += keyW;
    }
    // as TECLAS dos símbolos da página corrente
    bool typed = false;
    const u32 first = st.symPage * perPage;
    for (u32 k = 0; k < perPage; ++k) {
        const u32 idx = first + k;
        if (idx >= 22) {
            break;   // a última página pode ser curta
        }
        if (ui.buttonCompact(kSymKeyBase + static_cast<u64>(k), x, y, keyW,
                             barH, kSymbols[idx])) {
            ime::Event ev;
            ev.isText = true;
            ev.text = kSymbols[idx];
            applyEvent(st, ev);   // o MESMO caminho do IME — fonte única
            typed = true;
            elog::info("editor: simbolo '%s' pela barra (pagina %u)",
                       kSymbols[idx], st.symPage + 1);
        }
        x += keyW;
    }
    return typed;
}

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
    st.symPage = 0;   // 0.9.6.8 (E): a barra de símbolos recomeça na 1ª página
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
    ++st.bufVersion;   // 0.9.6.6: o open() trocou o buffer inteiro
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
    const u32 caretBefore = st.caret;
    const bool wasEnd = st.caret == st.buf.size();
    // 0.9.5: qualquer EDIÇÃO limpa a explicação do toque (a strip volta à
    // mini-descrição em tempo real da palavra que está a ser digitada)
    if (ev.isText || ev.key == ime::Key::Del || ev.key == ime::Key::Enter) {
        st.helpTapped = false;
        st.helpWord.clear();
    }
    if (ev.isText) {
        insertAtCaret(st, ev.text.c_str());
        // 0.9.6.2 (R-019): o LOG do cursor (o evento com o índice ANTES e
        // DEPOIS — o dono segue a edição linha a linha no engine.log)
        elog::info("editor: texto '%s' caret %u -> %u %s",
                   ev.text.c_str(), caretBefore, st.caret,
                   wasEnd ? "(fim)" : "");
        return true;
    }
    switch (ev.key) {
        case ime::Key::Del:
            chopBeforeCaret(st);
            elog::info("editor: apagar caret %u -> %u", caretBefore,
                       st.caret);
            return true;
        case ime::Key::Enter: {
            // 0.9.6.2 (R-019): o ENTER herda a INDENTAÇÃO da linha corrente
            // (o novo código nasce alinhado — nos dois teclados, o mesmo
            // applyEvent). O teclado do sistema nunca trata Enter como
            // "concluir": a ponte manda KEYCODE_ENTER (VvActivity) e o
            // IME_FLAG_NO_ENTER_ACTION está posto (0.9.6.1-d)
            const u32 ls = lineStartOf(st.buf, st.caret);
            const std::string indent = indentationOfLine(
                st.buf.substr(ls, lineEndOf(st.buf, ls) - ls));
            std::string nl = "\n";
            nl += indent;
            insertAtCaret(st, nl.c_str());
            elog::info("editor: enter caret %u -> %u (indent %u)",
                       caretBefore, st.caret, (u32)indent.size());
            return true;
        }
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
                ++st.bufVersion;   // 0.9.6.6: o Tab trocou a palavra
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
            ++st.bufVersion;   // 0.9.6.6: o SUBSTITUIR mexeu no buffer
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
    const f32 hdrH = theme::dp(kTopH);    // 56dp REAL (R-018: era 56px)
    const f32 contentW = w - ins.left - ins.right;

    // fundo opaco FULL-SCREEN (bg — o mesmo do editor de código)
    ui.panel(0.0f, 0.0f, w, h, theme::kTheme.bg);

    // ---- barra de topo: inset + 56dp ----------------------------------------
    ui.panel(ins.left, topY, contentW, hdrH, theme::kTheme.surface);
    ui.panel(ins.left, topY + hdrH - 1.0f, contentW, 1.0f,
             theme::kTheme.border);

    icons::drawIcon(ui, icons::Icon::Back, ins.left + theme::dp(16.0f),
                    topY + hdrH / 2.0f - theme::dp(12.0f), theme::dp(24.0f),
                    theme::kTheme.text1);
    int result = 0;
    if (ui.widgetHit(kBackId, ins.left, topY, hdrH, hdrH)) {
        result = 1;
    }

    // ---- 0.9.6.8 (GRUPO E) · O HEADER FLEXÍVEL -------------------------------
    // O header ANTIGO media os botões a partir da LUPA (docsX−200/−152/−56):
    // no device portrait (360dp de conteúdo) o botão do nível ficava a
    // −56dp e o do teclado a −8dp — DOIS botões FORA DO ECRÃ (invisíveis e
    // intocáveis; a medição do Grupo E). O layout novo ancora OS BOTÕES À
    // DIREITA (Stop, Run, lupa, copiar, nível — nesta ordem, da direita)
    // e o TÍTULO FLEXIONA com o que sobra: o subtítulo some primeiro
    // (zona < 120dp), o título por último (zona < 48dp — no device 360dp
    // o header é Back + 5 botões, ZERO sobreposição). Os alvos NUNCA
    // encolhem: 48dp de altura sempre; Run/Stop 72dp nos largos e 48dp
    // (o rótulo cabe) nos estreitos (< 420dp) — o padrão da top bar do
    // Grupo D: os ícones inteiros, o k divide só o texto.
    const f32 hdrBtnY = topY + (hdrH - theme::dp(48.0f)) * 0.5f;
    const f32 hdrBtnH = theme::dp(48.0f);
    const bool narrow = contentW < theme::dp(420.0f);
    const f32 actionW = narrow ? theme::dp(48.0f) : theme::dp(72.0f);
    const f32 stopX = ins.left + contentW - theme::dp(8.0f) - actionW;
    const f32 runX = stopX - theme::dp(8.0f) - actionW;
    const f32 lupaX = runX - theme::dp(12.0f) - theme::dp(48.0f);
    const f32 copyX = lupaX - theme::dp(8.0f) - theme::dp(48.0f);
    const f32 helpX = copyX - theme::dp(8.0f) - theme::dp(48.0f);
    const f32 titleX = ins.left + hdrH + theme::dp(8.0f);
    const f32 titleW = helpX - theme::dp(8.0f) - titleX;

    // título 20sp + hint 12sp — SÓ se a zona aguenta (o header flexível:
    // 120dp para os dois, 48dp só para o título; menos = só botões)
    {
        const TextMetrics m = ui.textMetrics();
        const theme::HeaderBaselines hb =
            theme::headerBaselines(m.ascent, m.descent, hdrH);
        if (titleW >= theme::dp(48.0f)) {
            ui.labelFittedStyled(titleX, topY + hb.title, "Script",
                                 theme::kTheme.text1,
                                 titleW > 0.0f ? titleW : 1.0f,
                                 theme::fontScale(theme::kFontScreen), 0);
        }
        if (titleW >= theme::dp(120.0f)) {
            ui.labelStyled(titleX, topY + hb.sub, "V.ONI · .voni",
                           theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption), 0);
        }
    }

    // LUPA (G0-3): abre as Docs POR CIMA (a pesquisa filtra as entradas
    // estruturadas e mostra o exemplo). Alvo 48dp.
    {
        const UiRect r{lupaX, hdrBtnY, theme::dp(48.0f), theme::dp(48.0f)};
        const bool held = ui.widgetActive(kDocsId);
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusCard),
                        held ? theme::kTheme.surface2
                             : theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::Search,
                        r.x + (r.w - theme::dp(24.0f)) * 0.5f,
                        r.y + (r.h - theme::dp(24.0f)) * 0.5f,
                        theme::dp(24.0f), theme::kTheme.text1);
        if (ui.widgetHit(kDocsId, r.x, r.y, r.w, r.h)) {
            result = 4;
        }

        // 0.9.5 · COPIAR REFERÊNCIA (📋): a referência V.ONI COMPLETA como
        // texto colável p/ IAs — o main põe no clipboard via JNI (result 6)
        const UiRect rc{copyX, hdrBtnY, theme::dp(48.0f), theme::dp(48.0f)};
        const bool heldC = ui.widgetActive(kCopyRefId);
        ui.panelRounded(rc.x, rc.y, rc.w, rc.h, theme::dp(theme::kRadiusCard),
                        heldC ? theme::kTheme.surface2
                              : theme::kTheme.surface);
        ui.frameRounded(rc.x, rc.y, rc.w, rc.h, 1.0f,
                        theme::dp(theme::kRadiusCard), theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::Copy,
                        rc.x + (rc.w - theme::dp(24.0f)) * 0.5f,
                        rc.y + (rc.h - theme::dp(24.0f)) * 0.5f,
                        theme::dp(24.0f), theme::kTheme.text1);
        if (ui.widgetHit(kCopyRefId, rc.x, rc.y, rc.w, rc.h)) {
            result = 6;
        }

        // (0.9.6 G3 · O BOTÃO DO TECLADO PRÓPRIO SAIU no Grupo E — o
        // teclado da engine morreu com ele; o IME do sistema é o ÚNICO
        // teclado e a BARRA DE SÍMBOLOS cobre a página 123)

        // 0.9.5 · O NÍVEL DA AJUDA (I/N/S): Iniciante (desc+exemplo) ·
        // Normal (desc) · Silencioso (nada) — um toque cicla
        const UiRect rl{helpX, hdrBtnY, theme::dp(48.0f), theme::dp(48.0f)};
        const bool heldL = ui.widgetActive(kHelpLevelId);
        ui.panelRounded(rl.x, rl.y, rl.w, rl.h, theme::dp(theme::kRadiusCard),
                        heldL ? theme::kTheme.surface2
                              : theme::kTheme.surface);
        ui.frameRounded(rl.x, rl.y, rl.w, rl.h, 1.0f,
                        theme::dp(theme::kRadiusCard), theme::kTheme.border);
        // 0.9.6.1 (G1-1): o ícone Question = "ajuda"; aceso (accent)
        // quando ela aparece
        const f32 iq = theme::dp(24.0f);
        icons::drawIcon(ui, icons::Icon::Question,
                        rl.x + (rl.w - iq) * 0.5f,
                        rl.y + (rl.h - iq) * 0.5f, iq,
                        st.helpLevel == 2 ? theme::kTheme.text2
                                          : theme::kTheme.accent);
        if (ui.widgetHit(kHelpLevelId, rl.x, rl.y, rl.w, rl.h)) {
            st.helpLevel = static_cast<u8>((st.helpLevel + 1) % 3);
        }
    }

    // RUN / STOP à direita (alvos ≥48 SEMPRE; 72dp nos largos, 48dp nos
    // estreitos — o rótulo cabe; estado: running aceso = Stop)
    const f32 btnW = actionW;
    const f32 btnY = hdrBtnY;
    const f32 btnH = hdrBtnH;
    const bool running = st.running;
    // Run: accent quando disponível; esbatido enquanto corre
    ui.panelRounded(runX, btnY, btnW, btnH, theme::dp(theme::kRadiusCard),
                    running ? theme::kTheme.surface2 : theme::kTheme.accent);
    ui.labelStyled(runX + (btnW - ui.fontWidth("Run")) * 0.5f,
                   theme::centeredBaseline(ui.textMetrics().ascent,
                                           ui.textMetrics().descent,
                                           btnY, btnH, 14.0f),
                   "Run", running ? theme::kTheme.text2 : theme::kTheme.bg,
                   theme::fontScale(theme::kFontBody), 0);
    if (!running && ui.widgetHit(kRunId, runX, btnY, btnW, btnH)) {
        result = 2;
    }
    // Stop: danger quando a correr
    ui.panelRounded(stopX, btnY, btnW, btnH, theme::dp(theme::kRadiusCard),
                    running ? theme::kTheme.danger : theme::kTheme.surface2);
    ui.labelStyled(stopX + (btnW - ui.fontWidth("Stop")) * 0.5f,
                   theme::centeredBaseline(ui.textMetrics().ascent,
                                           ui.textMetrics().descent,
                                           btnY, btnH, 14.0f),
                   "Stop",
                   running ? theme::kTheme.bg : theme::kTheme.text2,
                   theme::fontScale(theme::kFontBody), 0);
    if (running && ui.widgetHit(kStopId, stopX, btnY, btnW, btnH)) {
        result = 3;
    }

    // ---- corpo: nºs de linha + código colorido em scroll -------------------
    const f32 errBarH = st.errLine ? theme::dp(kErrH) : 0.0f;
    // 0.9.6.8 (GRUPO E) · O LIMITE DE BAIXO É O IME REAL: o inset medido
    // pela VvActivity (ime::bottomInset — rootHeight−visibleFrame.bottom)
    // diz ONDE o teclado do Android está; o corpo, a strip, a barra de
    // erro e a BARRA DE SÍMBOLOS param TODOS acima dele (o caret nunca
    // fica por baixo do teclado — o defeito do textWin 0.9.1 corrigido
    // na raiz). O inset JÁ inclui a nav bar quando o IME está aberto —
    // não se soma ins.bottom (senão desconta 2×); fechado, o limite é o
    // de sempre (h − ins.bottom: layout PIXEL-IGUAL ao Grupo D).
    const f32 imeIns = ime::bottomInset();
    const f32 bottomLim = imeIns > 0.0f ? (h - imeIns) : (h - ins.bottom);
    const f32 symH = imeIns > 0.0f ? symbolBarHeight() : 0.0f;
    // 0.9.5: a STRIP FINA DE AJUDA junto à barra de erro — a mini-descrição
    // em tempo real (DESDE A 1ª LETRA da palavra a meio da digitação) ou a
    // explicação do toque (com exemplo); SEM ENCHER O ECRÃ (some quando
    // não há nada a mostrar / nível Silencioso)
    // 0.9.6.6 (GRUPO C): kHelpStrip* em dp REAL (eram px crus)
    const std::string strip1 = helpStripLine1(st);
    const std::string strip2 = helpStripLine2(st);
    const f32 stripH =
        strip1.empty()
            ? 0.0f
            : (strip2.empty() ? theme::dp(kHelpStripH) : theme::dp(kHelpStrip2H));
    const UiRect body{ins.left, topY + hdrH, contentW,
                      bottomLim - (topY + hdrH) - errBarH - symH - stripH};
    const f32 lh = lineHeight(ui);
    const std::vector<u32>& lineIdx = ensureLineIndex(st);
    const u32 nLines = static_cast<u32>(lineIdx.size());
    const f32 contentH = static_cast<f32>(nLines) * lh + theme::dp(16.0f);

    ui.beginScroll(kScrollId, body, contentH);

    // o scroll SEGUE O CARET (não o fim — o caret agora move-se livre):
    // linha do caret visível (baixo se desce, topo se sobe).
    // 0.9.6.6 (GRUPO C · C2): a linha do caret pelo ÍNDICE O(log n) — a
    // MESMA fonte do draw/toque (antes: varria o buffer TODO por frame)
    u32 caretLine = 0;
    {
        // upper_bound: o 1.º início de linha > caret − 1 é a linha do caret
        u32 lo = 0, hi = nLines;
        while (lo < hi) {
            const u32 mid = (lo + hi) / 2;
            if (lineIdx[mid] <= st.caret) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        caretLine = lo > 0 ? lo - 1 : 0;
    }
    // (a fórmula do topo da linha em CONTENT coordenadas — a MESMA do draw
    // e do toque: dp(8) + linha*lh; lineTopOnScreen soma body.y e o scroll)
    const f32 caretTop = theme::dp(8.0f) + static_cast<f32>(caretLine) * lh;
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
    ui.panel(ins.left, body.y, theme::dp(48.0f), body.h, theme::kTheme.surface);
    ui.panel(ins.left + theme::dp(48.0f), body.y, 1.0f, body.h,
             theme::kTheme.border);
    // 0.9.6.6 (GRUPO C · C2): xCode/codeX/caretInset — a GEOMETRIA ÚNICA
    // (draw, toque e testes partilham as MESMAS funções do header)
    const f32 xCode = codeX(ins);

    // 0.9.6.6 (GRUPO C · C3) · O CULLING: só as linhas VISÍVEIS tokenizam.
    // ANTES o draw percorria o buffer INTEIRO por frame (n linhas ×
    // renderPieces × 60fps — o custo crescia com o script); agora a janela
    // [firstVis..lastVis] (+1 de folga por lado para o meio-desenhado) e o
    // estado do comentário de bloco do 1.º visível vem do ÍNDICE (lineBc —
    // pago por EDIÇÃO, não por frame). O contador dbgLinesTokenized é a
    // prova afervável no CI (a FASE 13.6/C3 planta 800 linhas scrolled e
    // espera ~20, não 800).
    dbgLinesTokenized = 0;
    u32 firstVis = 0;
    if (off > theme::dp(8.0f)) {
        firstVis = static_cast<u32>((off - theme::dp(8.0f)) / lh);
    }
    u32 lastVis = nLines - 1;
    {
        const f32 yEnd = off + body.h - theme::dp(8.0f);
        if (yEnd > 0.0f) {
            const u32 lv = static_cast<u32>(yEnd / lh) + 1;
            if (lv < nLines) {
                lastVis = lv;
            }
        }
    }
    // o estado bc ANTES da 1.ª visível (do cache; a linha 0 é o default)
    voni::hl::BlockCommentState bcState =
        firstVis < st.lineBc.size() ? st.lineBc[firstVis]
                                    : voni::hl::BlockCommentState{};
    char num[16];
    for (u32 i = firstVis; i <= lastVis; ++i) {
        const u32 start = lineIdx[i];
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
        // 0.9.6.6 (C2): o y pela FÓRMULA ÚNICA (o toque usa o INVERSO dela)
        const f32 y = lineTopOnScreen(body.y, i, lh, off);

        // nº da linha (12sp text2; a linha do ERRO acende em danger)
        std::snprintf(num, sizeof(num), "%u", i + 1);
        ui.labelStyled(ins.left + theme::dp(8.0f), y + theme::dp(4.0f), num,
                       st.errLine == i + 1 ? theme::kTheme.danger
                                           : theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);

        // tokens da linha (classes → cores do Theme) — 0.9.6 (G2-7b): o
        // render usa as PEÇAS COM GAPS (renderPieces): a concatenação é a
        // LINHA INTEIRA — espaços e `{ }` desenham, o x avança pela linha
        // real e o CARET (que mede a linha toda) fica EXATAMENTE onde se vê
        const std::string lineStr(line);
        const auto pieces = renderPieces(lineStr, bcState);
        ++dbgLinesTokenized;
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

        // CARET piscante NA POSIÇÃO do caret (linha/coluna — G0-1).
        // (0.9.6.6 · C2: o caret também pode estar numa linha FORA da
        // janela — o scroll-follow acima já a trouxe para dentro)
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
                const f32 xCaret = xCode + ui.fontWidth(before) +
                                   caretInset();
                ui.panel(xCaret, y, caretInset(), lh - theme::dp(8.0f),
                         theme::kTheme.accent);
            }
        }
    }
    ui.endScroll();

    // ---- 0.9.6.8 (GRUPO E) · A BARRA DE SÍMBOLOS SOBRE O IME -----------------
    // Dokada no limite REAL do teclado do sistema (a faixa de 40dp da
    // spec E, com os 24 símbolos da linguagem em páginas adaptativas). O
    // teclado da engine (280dp/54 teclas) SAIU — o rastreador E manda.
    if (symH > 0.0f) {
        drawSymbolBar(ui, st, w, bottomLim - symH);
    }

    // ---- a STRIP DE AJUDA (0.9.5): entre o teclado e a barra de erro ----
    // 0.9.6.1 (G2-8): a dica QUEBRA POR PALAVRAS (textwrap::wrap — a
    // métrica real da fonte) com ALTURA MÁXIMA DE 2 LINHAS (o texto cortava
    // no limite direito do ecrã) e um TOQUE NA DICA insere o esqueleto da
    // palavra (o mesmo Tab do editor que ensina). A faixa fica ACIMA do
    // teclado e FORA da área do código (o corpo já lhe dá a altura).
    if (stripH > 0.0f) {
        // 0.9.6.8 (GRUPO E): a strip sobe com o IME (acima da barra de
        // erro, que por sua vez está acima da barra de símbolos)
        const f32 stripY = bottomLim - symH - errBarH - stripH;
        ui.panel(ins.left, stripY, contentW, stripH, theme::kTheme.surface);
        ui.panel(ins.left, stripY, contentW, 1.0f, theme::kTheme.border);
        ui.panel(ins.left, stripY + 1.0f, theme::dp(3.0f), stripH - 1.0f,
                 theme::kTheme.accent);   // risca accent à esquerda
        // a linha 1 (nome: descrição) QUEBRADA por palavras — o caso de 2
        // linhas só acontece quando NÃO há exemplo (a linha do exemplo
        // ocupa o 2.º slot); o máximo é SEMPRE 2 linhas
        char l1[200];
        std::snprintf(l1, sizeof(l1), "%s", strip1.c_str());
        const f32 maxW = contentW - theme::dp(24.0f);
        std::vector<textwrap::Line> lines;
        textwrap::wrap(l1, strip2.empty() ? maxW : maxW,
                       [&](const char* s) { return ui.fontWidth(s); },
                       lines);
        if (lines.size() > 2) {
            lines.resize(2);   // a altura máxima da strip
        }
        const f32 lhStrip = theme::dp(20.0f);
        f32 ly = stripY + theme::dp(10.0f);
        for (const textwrap::Line& ln : lines) {
            char seg[200];
            const u32 n = ln.len < sizeof(seg) - 1 ? ln.len
                                                   : (u32)sizeof(seg) - 1;
            std::memcpy(seg, l1 + ln.begin, n);
            seg[n] = '\0';
            ui.labelStyled(ins.left + theme::dp(12.0f), ly, seg,
                           theme::kTheme.text1,
                           theme::fontScale(theme::kFontCaption),
                           static_cast<u32>(maxW));
            ly += lhStrip;
        }
        if (!strip2.empty() && lines.size() < 2) {
            char l2[200];
            std::snprintf(l2, sizeof(l2), "%s", strip2.c_str());
            ui.labelStyled(ins.left + theme::dp(12.0f),
                           stripY + theme::dp(30.0f), l2,
                           theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption),
                           static_cast<u32>(maxW));
        }
        // O TOQUE NA DICA insere o ESQUELETO da palavra (a palavra antes do
        // caret trocada pelo esqueleto do registo — o MESMO caminho do Tab)
        if (ui.widgetHit(kHintStripId, ins.left, stripY, contentW, stripH)) {
            const voni::reg::Entry* e = helpEntryFor(st);
            if (e && e->skeleton && *e->skeleton &&
                e->skeletonCaret <= std::strlen(e->skeleton)) {
                const std::string w0 = wordBeforeCaret(st);
                const u32 ws =
                    st.caret - static_cast<u32>(w0.size());
                if (w0.size() <= st.caret) {
                    st.buf.erase(ws, w0.size());
                    st.caret = ws;
                    ++st.bufVersion;   // 0.9.6.6: a dica inseriu o esqueleto
                    insertAtCaret(st, e->skeleton);
                    st.caret = ws + e->skeletonCaret;
                    st.helpTapped = false;
                    st.helpWord.clear();
                    elog::info("editor: a dica inseriu o esqueleto de '%s' "
                               "(toque na strip)", e->name);
                }
            }
        }
    }

    // ---- barra de ERRO com linha + mensagem (§12) + SUBSTITUIR (G2-7e) ----
    if (st.errLine) {
        // 0.9.6.8 (GRUPO E): a barra de erro sobe com o IME (acima da
        // barra de símbolos — nunca por baixo do teclado do sistema)
        const f32 errY = bottomLim - symH - theme::dp(kErrH);
        // 0.9.6.6 (GRUPO C): a ALTURA também é dp (era px cru — a barra
        // posicionava-se por dp(40) mas MEDIA 40px em qualquer densidade)
        ui.panel(ins.left, errY, contentW, theme::dp(kErrH),
                 theme::kTheme.danger);
        char msg[160];
        std::snprintf(msg, sizeof(msg), "linha %u: %s", st.errLine,
                      st.errMsg.empty() ? "erro" : st.errMsg.c_str());
        // a mensagem abre espaço para o botão quando há substituição
        const bool hasFix = !st.fixFrom.empty() && !st.fixTo.empty();
        ui.labelFitted(ins.left + theme::dp(12.0f),
                       errY + theme::dp(12.0f), msg,
                       theme::kTheme.bg,
                       hasFix ? contentW - theme::dp(148.0f)
                              : contentW - theme::dp(24.0f));
        // 0.9.6 (G2-7e) · O BOTÃO SUBSTITUIR: troca a palavra estrangeira
        // pelo equivalente V.ONI NO BUFFER (a linha do erro, palavra
        // inteira); o caret segue a edição e o erro limpa — o dono vê o
        // código ficar certo com UM toque
        if (hasFix) {
            const UiRect fb{ins.left + contentW - theme::dp(128.0f),
                            errY + theme::dp(4.0f), theme::dp(120.0f),
                            theme::dp(kErrH) - theme::dp(8.0f)};
            const bool held = ui.widgetActive(kFixId);
            ui.panelRounded(fb.x, fb.y, fb.w, fb.h,
                            theme::dp(theme::kRadiusCard),
                            held ? theme::kTheme.bg : theme::kTheme.danger);
            ui.frameRounded(fb.x, fb.y, fb.w, fb.h, 1.0f,
                            theme::dp(theme::kRadiusCard),
                            theme::kTheme.bg);
            char lbl[96];
            std::snprintf(lbl, sizeof(lbl), "Substituir %s",
                          st.fixTo.c_str());
            if (ui.hasFont()) {
                const f32 tw = ui.fontWidth(lbl);
                ui.label(fb.x + (fb.w - tw) * 0.5f,
                         theme::centeredBaseline(ui.textMetrics().ascent,
                                                 ui.textMetrics().descent,
                                                 fb.y, fb.h, 14.0f),
                         lbl, theme::kTheme.bg);
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
            // 0.9.6.8 (GRUPO E): o IME DO SISTEMA é o teclado ÚNICO (a
            // política de coexistência 0.9.6 G3 MORREU com o teclado da
            // engine) — o toque no corpo pede o IME e a BARRA DE SÍMBOLOS
            // dokada sobre ele aparece com o inset real (ime::bottomInset)
            // 0.9.6.2 (R-019) · O TOQUE MOVE O CARET — a linha vem do y+scroll
            // e a coluna da largura REAL de cada code point, descontando a
            // coluna dos números de linha. A CAUSA do bug: o offset era
            // CALCULADO e nunca aplicado — o cursor ficava para sempre onde
            // estava. Agora: caret = o carácter mais próximo; a dica de
            // palavra continua a ler o MESMO offset.
            // 0.9.6.6 (GRUPO C · C2): a linha/coluna pela GEOMETRIA ÚNICA —
            // lineAtScreenY é o INVERSO EXATO do lineTopOnScreen do draw e
            // o início da linha sai do ÍNDICE O(1) (antes: a fórmula e o
            // arranque de linha viviam NESTE bloco, uma 2.ª cópia à mão que
            // driftava da do draw — a classe exata do R-019)
            {
                const f32 lh2 = lineHeight(ui);
                const f32 off2 = ui.scrollOffset();
                i32 li2 = lineAtScreenY(ty, body.y, lh2, off2);
                const std::vector<u32>& idx = ensureLineIndex(st);
                const u32 nL = static_cast<u32>(idx.size());
                u32 li = li2 < 0 ? 0u : static_cast<u32>(li2);
                if (li >= nL) {
                    li = nL - 1;
                }
                const u32 ls2 = idx[li];   // O(1) — o início da linha do ÍNDICE
                // coluna: o carácter mais próximo pelas métricas REAIS, por
                // CODE POINT (um acento não conta 2 — o old media por BYTE)
                const f32 xCode2 = codeX(ins);
                const u32 le2 = lineEndOf(st.buf, ls2);
                const std::string line =
                    st.buf.substr(ls2, le2 - ls2);
                const u32 col = caretInLineForX(
                    line, tx - xCode2 - caretInset(),
                    [&](const char* one) { return ui.fontWidth(one); });
                const u32 bo = ls2 + col;
                // ★ O FIX: o offset sob o dedo PASSA A SER o cursor ★
                st.caret = bo;
                st.blink = 0.0f;   // o caret acende logo (o dono vê o salto)
                // o LOG do toque (px E dp, scroll, linha/coluna, índice)
                const f32 d = theme::g_density;
                elog::info("editor: toque x=%.0fpx y=%.0fpx (%.0f %.0f dp) "
                           "scroll=%.0f -> linha %u col %u caret %u",
                           (double)tx, (double)ty, (double)(tx / d),
                           (double)(ty / d), (double)off2, li + 1,
                           col + 1, st.caret);
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
