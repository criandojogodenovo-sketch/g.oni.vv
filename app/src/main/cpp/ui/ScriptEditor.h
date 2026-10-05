#pragma once
// ui/ScriptEditor.h — EDITOR DE SCRIPT V.ONI (0.9.2 §10; FASE 9 G0-1/G0-2;
// GRUPO E 0.9.6.8: o teclado da engine SAIU — a BARRA DE SÍMBOLOS de 40dp
// desenha SOBRE o IME do sistema).
//
//   ┌──────────────────────────────────────┐
//   │ [← 56]  Script  [?][📋][🔍] [Run][■]│  ← header FLEXÍVEL (E): os
//   │ "ator · .voni" 12sp (some se estreito)│     botões ancoram à direita,
//   ├──────┬───────────────────────────────┤     o título flexiona/esconde
//   │  1   │ v#@+velocidade:Num=5.5        │  ← nºs de linha + COLORAÇÃO
//   │  2   │ central main {                │     (classes do VoniHighlight;
//   │  3 _ │   on moment { }               │      cores no Theme 🔶)
//   │  …   │   allmoments { }              │
//   ├──────┴───────────────────────────────┤
//   │ ⚠ linha 3: 'x' é uma palavra reservada │  ← erro com LINHA (§12)
//   ├──────────────────────────────────────┤
//   │ [1/3]{ } ( ) [ ] = + - *  │  ← BARRA DE SÍMBOLOS 40dp (spec E):
//   ├──────────────────────────────────────┤     SOBRE o IME do sistema
//   │  (o IME do Android — Gboard etc.)     │     (ime::bottomInset REAL
//   └──────────────────────────────────────┘     medido pela VvActivity)
//
// O par INSEPARÁVEL 0.9.1 (portrait + IME do sistema) abre/fecha com a
// janela — o main chama os mesmos jniSetOrientation/jniImeShow/Hide do
// textWin.
//
// GRUPO E (0.9.6.8 — «header flexível; IME; barra de símbolos 40dp sobre
// o IME (teclado da engine REMOVIDO)»): o QWERTY in-app de 280dp/54 teclas
// SAIU (o dono digita pelo IME do sistema — acentos, gestos, dicionário);
// no lugar dele uma BARRA FINA de 40dp com os 24 símbolos da spec V.ONI
// (a página 123 do teclado antigo) em páginas adaptativas, dokada SOBRE o
// IME com a medida REAL (ime::bottomInset — a VvActivity mede a faixa do
// teclado e empurra por JNI; o caret nunca fica por baixo do teclado). As
// teclas emitem pelo MESMO applyEvent do IME — uma única fonte de verdade.
// A exceção do validador: teclas COMPACTAS (piso 40dp da spec E em vez do
// 48dp da casa) — vigiada pela sentinela R-027.
//
// FASE 9 (G0-1 — "script fecha ao digitar"): o editor ganhou (a) CARET
// livre (offset em bytes; Left/Right/Up/Down do IME movem; inserção/DEL no
// caret — o modelo append-only ia ao fim e nada mais); (b) IME DO SISTEMA
// como teclado ÚNICO (GRUPO E: o teclado in-app de 2 páginas SAIU — a
// barra de símbolos cobre o que o GBoard não tem); (c) toque no corpo =
// devolve 5 (o main RE-PETE o IME — sem perder foco); (d) LUPA (devolve 4
// — abre as Docs por cima; a pesquisa filtra nome/1 linha/sintaxe/exemplo).
//
// FASE 9 (G0-2 — esqueleto base): abrir um Script SEM fonte guardada
// carrega o entry point da spec:
//   central main {
//     on moment { }
//     allmoments { }
//   }
// com o cursor NO INTERIOR (dentro de "allmoments { }"). Fonte guardada
// abre intacta com o cursor no fim.
//
// RUN/STOP (§10): Run = editorRestart no VoniSystem (compila + arranca);
// erro de compile/corrida aparece na BARRA DE ERRO com linha + mensagem E
// no engine.log (via elog) — nunca crash (§12). Stop = editorStop.
//
// MODAL: vive no EditorState (st.scriptWin) — o gate anyOverlayOpen cobre.
// GL-free / Android-free: desenha no quad batch, consome ime::Event.
#include "core/Handle.h"
#include "platform/ImeQueue.h"
#include "ui/SafeArea.h"   // 0.9.6.6: codeX(linha→y/col→x única)
#include "ui/UiContext.h"
#include "voni/Voni.h"
#include "voni/VoniHighlight.h"   // 0.9.6: renderPieces (Cls/BlockCommentState)
#include "voni/VoniRegistry.h"   // 0.9.5: o editor que ensina (registo)

#include <string>
#include <vector>
namespace vv {
class InputState;
class Scene;
class VoniSystem;

namespace editor {
namespace scriptwin {

// IDs (faixa 6550..6599 — depois do textwin 6500..6549)
constexpr u64 kBackId = 6550;   // ← fechar (56dp)
constexpr u64 kRunId  = 6551;   // ▶ Run  (48dp)
constexpr u64 kStopId = 6552;   // ■ Stop (48dp)
constexpr u64 kScrollId = 6553; // região de scroll do código
constexpr u64 kDocsId = 6554;   // 🔍 lupa — abre as Docs (G0-3)
constexpr u64 kHelpLevelId = 6555; // I/N/S — nível da ajuda (0.9.5)
constexpr u64 kCopyRefId = 6556;   // 📋 copiar referência V.ONI (0.9.5)
constexpr u64 kFixId = 6557;   // 0.9.6 (G2-7e): botão SUBSTITUIR da barra de erro
constexpr u64 kSymBarPageId = 6558; // 0.9.6.8 (E): a PÁGINA da barra de símbolos
constexpr u64 kHintStripId = 6559; // 0.9.6.1 (G2-8): a dica insere o esqueleto
constexpr u64 kSymKeyBase = 6560;   // 0.9.6.8 (E): as teclas da barra (≤13/página)
// (o antigo kKbBase — as 54 teclas do teclado da engine — SAIU no Grupo E)

constexpr f32 kTopH = 56.0f;    // barra de topo (padrão D)
constexpr f32 kErrH = 40.0f;    // barra de erro (1 linha 12sp + ícone)
constexpr f32 kHelpStripH = 32.0f;   // a strip fina de ajuda (1 linha)
constexpr f32 kHelpStrip2H = 56.0f;  // 2 linhas (Iniciante/toque)

// o ESQUELETO base (G0-2 — entry point da spec §3): script NOVO abre com
// isto e o cursor NO INTERIOR (posição do caret após "allmoments { ").
extern const char* const kSkeleton;
extern const u32 kSkeletonCaret;

// 0.9.6 (G2-7b · R-010) — O PLANO DE RENDER de uma linha: as PEÇAS que o
// editor desenha, cobrindo TODOS os bytes (os GAPS entre tokens — espaços,
// `{ } ( ) = +` — eram SALTADOS pelo classificador e o texto aparecia SEM
// espaços e sem `{` enquanto o cursor deixava o espaço: guardado !=
// renderizado). A sentinela R-010 afere que a concatenação das peças é a
// LINHA INTEIRA — o render nunca mente.
struct RenderPiece {
    u32 begin;                  // offset em bytes na linha
    u32 len;
    voni::hl::Cls cls;          // Cls::User nos gaps (cor neutra)
};
std::vector<RenderPiece> renderPieces(const std::string& line,
                                      voni::hl::BlockCommentState& bc);

struct State {
    bool open = false;          // janela visível (gate modal)
    Handle tic{};               // TIC dono do ScriptComp (Handle inválido = nenhum)
    char ticName[64] = {0};     // nome do dono (re-validação pós-lifecycle)
    std::string buf;            // fonte .voni em edição
    u32 caret = 0;              // offset em BYTES do cursor (G0-1)
    f32 blink = 0.0f;           // caret
    f32 scrollOff = 0.0f;
    // 0.9.6.6 (GRUPO C · PERF+GEOMETRIA): O ÍNDICE DE LINHAS — reconstruído
    // quando o buffer muda (bufVersion bumped em CADA mutação: insert/
    // apagar/tab/fix/dica/open). lineCount, a linha do caret e o início de
    // CADA linha passam a O(1)/O(log n) — ANTES cada um VARRIA o buffer
    // inteiro (o draw fazia 3 varrimentos/frame; o toque mais 2). O draw
    // só TOKENIZA as linhas VISÍVEIS (o custo por frame deixa de ser
    // O(buffer) — a prova é o contador dbgLinesTokenized, afervável no CI).
    u32 bufVersion = 0;                      // muda a cada edição
    std::vector<u32> lineStarts;              // offset do início de cada linha
    u32 indexVersion = 0xFFFFFFFFu;           // a versão que o cache serve
    // estado do comentário de bloco AO FIM de cada linha (para o 1.º
    // visível tokenizar JÁ certo sem varrer desde a linha 0)
    std::vector<voni::hl::BlockCommentState> lineBc;
    // erro corrente (linha 0 = sem erro) — compile ou runtime
    u32 errLine = 0;
    std::string errMsg;
    // true = a run do EDITOR está ativa (o botão Stop aceso)
    bool running = false;
    // 0.9.6.8 (GRUPO E) · A BARRA DE SÍMBOLOS: a página corrente (0-based;
    // o nº de páginas adapta à largura — 3 no device 360dp, 2 no harness
    // 720dp). O teclado da engine (kbOpen/kbSym/kbLower/kbLong*) SAIU — o
    // IME do sistema é o teclado ÚNICO e a barra cobre os símbolos da spec
    u8 symPage = 0;
    // 0.9.5 · EDITOR QUE ENSINA: o nível da ajuda (0=Iniciante com
    // exemplos, 1=Normal 1 linha, 2=Silencioso nada) + a palavra tocada
    // (o toque numa palavra mostra a explicação com exemplo — das Docs)
    u8 helpLevel = 1;
    bool helpTapped = false;    // a strip mostra a explicação do toque
    std::string helpWord;       // a palavra sob o dedo (ou vazia)
    // 0.9.6 (G2-7e) · O BOTÃO SUBSTITUIR: quando o erro-que-ensina tem
    // equivalente de 1 token (if→exist…), a barra de erro acende o botão;
    // o toque troca a palavra estrangeira pela V.ONI no buffer (o caret
    // segue a edição). Vazio = sem botão (ensina mas não substitui:
    // case/default/elif não têm troca direta válida)
    std::string fixFrom;        // a palavra estrangeira (ex.: "if")
    std::string fixTo;          // o equivalente V.ONI (ex.: "exist")
};

// 0.9.6 (G2-7e): aplica o SUBSTITUIR (troca fixFrom→fixTo na linha do
// erro; o caret segue; o erro limpa)
void applyFix(State& st);

// ---- 0.9.6.8 (GRUPO E) · A BARRA DE SÍMBOLOS (a spec: 40dp sobre o IME) ---
// Os 22 símbolos da spec V.ONI (a página «123» do teclado antigo — TODOS
// os que o GBoard não tem à mão: { } ( ) [ ] = + - * / < > ! , . ; : " _
// # @); as teclas emitem pelo MESMO applyEvent do IME. A GEOMETRIA é
// exportada (a sentinela R-027 e a FASE 13.8 aférem a barra pelo MESMO
// caminho do draw — a lição do R-019: nunca 2 cópias).
extern const char* const kSymbols[22];
// a altura da barra em px REAL (40dp — a spec E; densidade aplicada)
f32 symbolBarHeight();
// nº de teclas visíveis (a página ADAPTA à largura: 9 no device 360dp,
// 14 no harness 720dp — as teclas ficam sempre ≥40dp de largura)
u32 symKeysVisible(f32 contentW);
// nº de páginas para a largura dada (ceil(24 / (n-1)) — a 1ª tecla é a
// própria página, o padrão «?123» do GBoard)
u32 symPageCount(f32 contentW);

// guarda o NOME do TIC dono (G0-1: o handle morre no TERM→INIT da rotação
// portrait — o reload do INIT_WINDOW re-cria os TICs; o main re-valida por
// NOME, o mesmo padrão da seleção 0.8.12)
void rememberTicName(State& st, const Scene& scene);
// re-valida o tic (o reload matou o handle?) por NOME; true se ainda válido
bool revalidateTic(State& st, Scene& scene);

// abre para o TIC dado (carrega o source do ScriptComp; SEM fonte guardada
// abre com o ESQUELETO base e o cursor no interior — G0-2)
void open(State& st, Scene& scene, Handle tic);
// fecha (o main pede landscape + imeHide — o par exato do textWin)
void close(State& st);

// aplica UM evento IME (inserção no caret/DEL/ENTER/setas — G0-1)
bool applyEvent(State& st, const ime::Event& ev);

// 0.9.6.6 (GRUPO C · C2/C3) — O ÍNDICE DE LINHAS: garante o cache (rebuild
// só quando o buffer mudou) e devolve-o. lineCount/lados ficam O(1) e o
// draw/tokenizador só percorre o que VÊ. A linha i começa em (*idx)[i].
const std::vector<u32>& ensureLineIndex(State& st);
u32 lineCount(State& st);   // (era const com varredura — agora O(1) pelo índice)

// 0.9.6.6 (GRUPO C · C2) — A GEOMETRIA ÚNICA linha↔y / coluna↔x: a MESMA
// fórmula para o DRAW (onde cada linha desenha), o TOQUE (que linha/coluna
// estava sob o dedo) e o SCROLL-SEGUE-CARET. ANTES a fórmula «dp(8)+i·lh a
// partir de body.y» vivia em TRÊS sítios à mão que podiam driftar (a classe
// exata do R-019: o toque calculava por uma cópia e o draw por outra).
// altura de linha pelas MÉTRICAS DO CONTEXTO (piso 28dp — a regra F5.0)
f32 lineHeight(UiContext& ui);
// y de ecrã do TOPO da linha i (draw e scroll-follow)
inline f32 lineTopOnScreen(f32 bodyY, u32 i, f32 lh, f32 scrollOff) {
    return bodyY + theme::dp(8.0f) + static_cast<f32>(i) * lh - scrollOff;
}
// o INVERSO (o toque): a linha que contém este y de ecrã (>= 0; o chamador
// faz o clamp ao nº de linhas)
inline i32 lineAtScreenY(f32 y, f32 bodyY, f32 lh, f32 scrollOff) {
    const f32 rel = y - bodyY + scrollOff - theme::dp(8.0f);
    return rel <= 0.0f ? 0 : static_cast<i32>(rel / lh);
}
// x da 1ª coluna do código (draw, toque e caret partilham)
inline f32 codeX(const safe::Insets& ins) { return ins.left + theme::dp(64.0f); }
// o inset visual do caret (a sua esquerda da posição)
inline f32 caretInset() { return theme::dp(2.0f); }

// 0.9.6.6 (GRUPO C · C3) — o contador de trabalho do draw (afervável no
// CI: com N linhas e o scroll no meio, o frame TOKENIZA as visíveis+
// folga, não N; reset no início de CADA draw)
extern u32 dbgLinesTokenized;

// ---- 0.9.6.2 (R-019) · A GEOMETRIA DO CURSOR — funções PURAS/aferváveis ---
// O BUG que o dono apanhou no device: o toque no corpo CALCULAVA o offset
// sob o dedo mas NUNCA O APLICAVA ao caret (só alimentava a palavra da
// dica) — o cursor ficava para sempre onde estava (fim do buffer nos
// scripts guardados) e "não dava para escrever onde se quer". Estas funções
// são a fonte única da conversão toque↔offset (o draw usa-as; os testes
// aférram-nas com métricas simuladas):
//
// início (em bytes) da linha que contém o offset / coluna (bytes desde o
// início da linha)
u32 lineStartOfOffset(const std::string& s, u32 off);
u32 columnOfOffset(const std::string& s, u32 off);
// linha (0-based) e fim (offset do \n ou do fim) da linha que contém off
u32 lineIndexOf(const std::string& s, u32 off);
u32 lineEndOf(const std::string& s, u32 lineStart);
// o caret mais próximo de x DENTRO da linha dada, com a largura REAL de
// cada code point (WidthFn: const char* → largura px) — desconta meia
// largura (o toque no meio do carácter escolhe-o); nunca passa do fim da
// linha. UTF-8: mede o CODE POINT inteiro (um acento não conta 2)
template <typename WidthFn>
u32 caretInLineForX(const std::string& line, f32 x, WidthFn&& widthOf) {
    u32 off = 0;
    f32 acc = 0.0f;
    const u32 n = static_cast<u32>(line.size());
    while (off < n) {
        // o code point inteiro (o byte seguinte nunca é continuação)
        u32 nxt = off + 1;
        while (nxt < n &&
               (static_cast<unsigned char>(line[nxt]) & 0xC0u) == 0x80u) {
            ++nxt;
        }
        char one[8];
        const u32 len = nxt - off < sizeof(one) - 1 ? nxt - off
                                                    : (u32)sizeof(one) - 1;
        std::memcpy(one, line.data() + off, len);
        one[len] = '\0';
        const f32 cw = widthOf(one);
        if (acc + cw * 0.5f >= x) {
            return off;   // o toque caiu antes/aqui deste carácter
        }
        acc += cw;
        off = nxt;
    }
    return off;   // além do fim da linha — caret no fim
}
// a indentação da linha dada (espaços iniciais — o ENTER copia-a)
std::string indentationOfLine(const std::string& line);

// ---- 0.9.5 · O EDITOR QUE ENSINA (puro/afervável — o registo alimenta) ---
// a palavra que TERMINA no caret (a meio da digitação — a mini-descrição
// em tempo real funciona DESDE A 1ª LETRA)
std::string wordBeforeCaret(const State& st);
// a palavra sob um offset em bytes (o toque numa palavra)
std::string wordAtOffset(const State& st, u32 byteOffset);
// a entrada do registo p/ a strip: a palavra TOCADA (exata) ou a palavra a
// meio da digitação (prefixo); null = nada a mostrar
const voni::reg::Entry* helpEntryFor(const State& st);
// as linhas da strip (1: "nome: 1-linha"; 2: exemplo — "" quando 1 linha)
// conforme o nível (Iniciante=2 linhas, Normal=1, Silencioso="")
std::string helpStripLine1(const State& st);
std::string helpStripLine2(const State& st);

// desenha a janela FULL-SCREEN e processa toques. Devolve:
//   1 = BACK tocado (main fecha com landscape+imeHide)
//   2 = RUN tocado  (main chama voni.editorRestart + atualiza running/erro)
//   3 = STOP tocado (main chama voni.editorStop)
//   4 = LUPA tocada (main abre o ecrã de Docs POR CIMA — G0-3)
//   5 = toque no CORPO (main re-pede o IME — sem perder foco, G0-1)
//   6 = COPIAR REFERÊNCIA tocada (main põe a referência V.ONI completa no
//       clipboard via JNI — texto colável p/ IAs, 0.9.5)
int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h,
         f32 dt);

} // namespace scriptwin
} // namespace editor
} // namespace vv
