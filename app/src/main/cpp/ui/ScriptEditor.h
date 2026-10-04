#pragma once
// ui/ScriptEditor.h — EDITOR DE SCRIPT V.ONI (0.9.2 §10; FASE 9 G0-1/G0-2).
//
//   ┌──────────────────────────────────────┐
//   │ [← 56]  Script  [🔍]    [▶ Run][■ Stop]│  ← portrait (720×1536 no C33)
//   │ "ator · .voni" 12sp                  │
//   ├──────┬───────────────────────────────┤
//   │  1   │ v#@+velocidade:Num=5.5        │  ← nºs de linha + COLORAÇÃO
//   │  2   │ central main {                │     (classes do VoniHighlight;
//   │  3 _ │   on moment { }               │      cores no Theme 🔶)
//   │  …   │   allmoments { }              │
//   ├──────┴───────────────────────────────┤
//   │ A B C D E F G H I        │  ← TECLADO IN-APP (G0-1: o editor aceita
//   │ J K L M N O P Q R        │     texto do teclado in-app E do IME —
//   │ S T U V W X Y Z _        │     o mesmo applyEvent para os dois)
//   │ 0 1 2 3 4 5 6 7 8 9      │
//   │ [ESPACO][ABC][APAGA][ENTER][▼]       │
//   ├──────────────────────────────────────┤
//   │ ⚠ linha 3: 'x' é uma palavra reservada │  ← erro com LINHA (§12)
//   └──────────────────────────────────────┘
//
// O par INSEPARÁVEL 0.9.1 (portrait + IME do sistema) abre/fecha com a
// janela — o main chama os mesmos jniSetOrientation/jniImeShow/Hide do
// textWin.
//
// FASE 9 (G0-1 — "script fecha ao digitar"): o editor ganhou (a) CARET
// livre (offset em bytes; Left/Right/Up/Down do IME movem; inserção/DEL no
// caret — o modelo append-only ia ao fim e nada mais); (b) TECLADO IN-APP
// (2 páginas ABC/123; as teclas emitem pelo MESMO applyEvent do IME —
// uma única fonte de verdade); (c) toque no corpo = devolve 5 (o main
// RE-PETE o IME — sem perder foco); (d) LUPA (devolve 4 — abre as Docs
// por cima; a pesquisa filtra nome/1 linha/sintaxe/exemplo).
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
#include "ui/UiContext.h"
#include "voni/Voni.h"

#include <string>

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
constexpr u64 kKbBase = 6560;   // teclas do teclado in-app (40 + 6 da base)

constexpr f32 kTopH = 56.0f;    // barra de topo (padrão D)
constexpr f32 kErrH = 40.0f;    // barra de erro (1 linha 12sp + ícone)

// o ESQUELETO base (G0-2 — entry point da spec §3): script NOVO abre com
// isto e o cursor NO INTERIOR (posição do caret após "allmoments { ").
extern const char* const kSkeleton;
extern const u32 kSkeletonCaret;

struct State {
    bool open = false;          // janela visível (gate modal)
    Handle tic{};               // TIC dono do ScriptComp (Handle inválido = nenhum)
    char ticName[64] = {0};     // nome do dono (re-validação pós-lifecycle)
    std::string buf;            // fonte .voni em edição
    u32 caret = 0;              // offset em BYTES do cursor (G0-1)
    f32 blink = 0.0f;           // caret
    f32 scrollOff = 0.0f;
    // erro corrente (linha 0 = sem erro) — compile ou runtime
    u32 errLine = 0;
    std::string errMsg;
    // true = a run do EDITOR está ativa (o botão Stop aceso)
    bool running = false;
    // teclado in-app (G0-1): visível + página (false=letras, true=símbolos)
    bool kbOpen = false;
    bool kbSym = false;
    bool kbLower = false;       // abc/ABC
};

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

u32 lineCount(const State& st);

// desenha a janela FULL-SCREEN e processa toques. Devolve:
//   1 = BACK tocado (main fecha com landscape+imeHide)
//   2 = RUN tocado  (main chama voni.editorRestart + atualiza running/erro)
//   3 = STOP tocado (main chama voni.editorStop)
//   4 = LUPA tocada (main abre o ecrã de Docs POR CIMA — G0-3)
//   5 = toque no CORPO (main re-pede o IME — sem perder foco, G0-1)
int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h,
         f32 dt);

} // namespace scriptwin
} // namespace editor
} // namespace vv
