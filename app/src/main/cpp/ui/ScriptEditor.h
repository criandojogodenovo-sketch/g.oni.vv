#pragma once
// ui/ScriptEditor.h — EDITOR DE SCRIPT V.ONI (0.9.2 §10 ✅).
//
//   ┌──────────────────────────────────────┐
//   │ [← 56]  Script        [▶ Run][■ Stop]│  ← portrait (720×1536 no C33)
//   │ "ator · .voni" 12sp                  │
//   ├──────┬───────────────────────────────┤
//   │  1   │ v#@+velocidade:Num=5.5        │  ← nºs de linha + COLORAÇÃO
//   │  2   │ central main {                │     (classes do VoniHighlight;
//   │  3 _ │   on moment { }               │      cores no Theme 🔶)
//   │  …   │   allmoments { }              │
//   ├──────┴───────────────────────────────┤
//   │ ⚠ linha 3: 'x' é uma palavra reservada │  ← erro com LINHA (§12)
//   └──────────────────────────────────────┘
//
// O par INSEPARÁVEL 0.9.1 (portrait + IME do sistema) abre/fecha com a
// janela — o main chama os mesmos jniSetOrientation/jniImeShow/Hide do
// textWin. A escrita é a da semente (append + DEL por code point + ENTER;
// o caret vive no fim — cursor livre fica para depois).
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

constexpr f32 kTopH = 56.0f;    // barra de topo (padrão D)
constexpr f32 kErrH = 40.0f;    // barra de erro (1 linha 12sp + ícone)

struct State {
    bool open = false;          // janela visível (gate modal)
    Handle tic{};               // TIC dono do ScriptComp (Handle inválido = nenhum)
    std::string buf;            // fonte .voni em edição
    f32 blink = 0.0f;           // caret
    f32 scrollOff = 0.0f;
    // erro corrente (linha 0 = sem erro) — compile ou runtime
    u32 errLine = 0;
    std::string errMsg;
    // true = a run do EDITOR está ativa (o botão Stop aceso)
    bool running = false;
};

// abre para o TIC dado (carrega o source do ScriptComp; sem componente
// abre com buffer vazio — o main pode criar o componente antes de abrir)
void open(State& st, Scene& scene, Handle tic);
// fecha (o main pede landscape + imeHide — o par exato do textWin)
void close(State& st);

// aplica UM evento IME (append/DEL/ENTER — o modelo da semente 0.9.1)
bool applyEvent(State& st, const ime::Event& ev);

u32 lineCount(const State& st);

// desenha a janela FULL-SCREEN e processa toques. Devolve:
//   1 = BACK tocado (main fecha com landscape+imeHide)
//   2 = RUN tocado  (main chama voni.editorRestart + atualiza running/erro)
//   3 = STOP tocado (main chama voni.editorStop)
int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h,
         f32 dt);

} // namespace scriptwin
} // namespace editor
} // namespace vv
