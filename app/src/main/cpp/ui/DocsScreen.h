#pragma once
// ui/DocsScreen.h — SETTINGS → DOCS (0.9.2 §11 ✅): pesquisa com lupa +
// entradas por comando/linker/tyker/componente com nome, 1 linha, sintaxe
// e exemplo curto. 0.9.2 povoa COMANDOS (§9) e LINGUAGEM (§§3-8); as
// categorias linker/tyker/componente existem na estrutura e entram na
// 0.9.3 (spec 8).
//
//   ┌──────────────────────────────────────┐
//   │ [← 56]  Docs 20sp                    │  ← landscape (o editor segue)
//   ├──────────────────────────────────────┤
//   │ [🔍 Docs da V.ONI…]  [cat ▾]         │  ← campo 48dp (teclado in-app
//   ├──────────────────────────────────────┤     propósito 9) + filtro
//   │ central main        Linguagem        │
//   │ Bloco de arranque do TIC…            │  ← tap EXPANDE (sintaxe+exemplo)
//   │ …                                    │
//   └──────────────────────────────────────┘
//
// GL-free / host-testável.
#include "ui/UiContext.h"

#include <string>

namespace vv {
class InputState;

namespace editor {
namespace docswin {

// IDs (faixa 6600..6649)
constexpr u64 kBackId   = 6600;
constexpr u64 kSearchId = 6601;   // campo de pesquisa (abre teclado in-app)
constexpr u64 kScrollId = 6602;
constexpr u64 kEntryBase = 6610;  // +índice (tap expande)

constexpr f32 kTopH = 56.0f;
constexpr f32 kRowH = 48.0f;

struct State {
    bool open = false;
    // query (o teclado in-app propósito 9 escreve AQUI via EditorState)
    char query[32] = "";
    u32 queryLen = 0;
    // entrada expandida (-1 = nenhuma) — mostra sintaxe + exemplo
    int expanded = -1;
};

// desenha FULL-SCREEN (landscape — é um ecrã do Settings, não janela de
// texto pesado). Devolve 1 = BACK (o main fecha).
int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h);

} // namespace docswin
} // namespace editor
} // namespace vv
