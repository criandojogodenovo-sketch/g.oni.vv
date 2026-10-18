#pragma once
// ui/TextWindow.h — JANELA DE TEXTO PESADO (0.9.1 §1/§2):
//
//   ┌──────────────────────────────────┐
//   │ [← 56]  Texto  20sp              │  ← portrait (720×1536 no C33)
//   │ "portrait · IME do sistema" 12sp │
//   ├──────────────────────────────────┤
//   │ linha 1                          │  ← buffer UTF-8, scroll,
//   │ linha 2_                         │     caret piscante no fim (v0:
//   │ …                                │     escrita APENDICA — o cursor
//   └──────────────────────────────────┘     editável é do editor 0.9.2)
//
// É a SEMENTE do editor de script (0.9.2): a CASCA (portrait + IME do
// sistema + scroll de linhas) é desta sub-fase; coloração/Run/Stop/parse
// entram com a linguagem. Enquanto a janela vive:
//   • a engine pediu PORTRAIT via JNI (ime::setOrientation + jniSetOrientation)
//   • o IME do sistema está pedido (jniImeShow) — o texto chega pela
//     fila ime:: (pushText/pushKey do Java, poll por frame)
// Ao fechar: landscape + imeHide (o par EXATO, testado).
//
// MODAL: vive no EditorState (st.textWin) — o gate anyOverlayOpen cobre
// (nada do editor desenha/interage atrás). O teclado IN-APP de sempre
// (renomear rápido, landscape) NÃO abre com esta janela ativa — os dois
// inputs coexistem por exclusão de contexto (0.9.1 §4/§5).
//
// GL-free / Android-free: desenha no quad batch e consome ime::Event —
// testável no hospedeiro (test_wiring091).
#include "platform/ImeQueue.h"
#include "ui/UiContext.h"

#include <string>

namespace vv {
class InputState;

namespace editor {

namespace textwin {

// IDs (faixa 6500..6549 — livre entre o viewer de logs 6400+ e os overlays
// seguintes; UiContext é immediate-mode: IDs únicos POR FRAME)
constexpr u64 kBackId   = 6500;   // ← fechar (56dp)
constexpr u64 kScrollId = 6501;   // região de scroll do buffer

// altura da barra de topo (56dp — o padrão das barras da spec D)
constexpr f32 kTopH = 56.0f;

struct State {
    bool open = false;          // janela visível (gate modal do editor)
    std::string buf;            // conteúdo UTF-8 ('\n' = quebra de linha)
    f32 blink = 0.0f;           // relógio do caret (soma dt no draw)
    f32 scrollOff = 0.0f;       // offset do scroll (0 = topo; o caret no fim
                                //     auto-roda quando escreve)
};

// abre (limpa o buffer — a janela v0 é rascunho, sem ficheiro) e devolve o
// que o chamador TEM de pedir ao sistema: portrait + IME show
void open(State& st);

// fecha (o chamador pede landscape + imeHide)
void close(State& st);

// aplica UM evento da fila IME (devolve true se era aplicável). Escrita
// APENDICA v0: texto no fim, ENTER quebra linha, DEL apaga 1 code point
// UTF-8 (o acento morre inteiro — nunca meio glifo).
bool applyEvent(State& st, const ime::Event& ev);

// nº de linhas do buffer (puro — o scroll e os testes usam)
u32 lineCount(const State& st);

// desenha a janela FULL-SCREEN (0,0,w,h — no portrait o ecrã inteiro) e
// processa os toques. Devolve 1 = o BACK foi tocado (o main fecha com o
// par landscape+imeHide). blink soma dt (o frame passa g_frameDt).
int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h,
         f32 dt);

} // namespace textwin
} // namespace editor
} // namespace vv
