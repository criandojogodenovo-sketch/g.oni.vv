#pragma once
// ui/ViewportChrome.h — CHROME DO VIEWPORT (0.9.0 spec D; FASE 9 G1-1;
// PASSO 3 · 0.9.6.17 — A SPEC DO DONO: os controlos do viewport ocupam
// ≤10% da área a 60% de alfa, NADA atravessa a largura toda e NADA se
// sobrepõe; 0.9.6.19b · D21 — O [+] DO VIEWPORT MORRE):
//
//   ┌─────────────────────────────────────────────────────────────┐
//   │ [undo][redo][save][⋯]                    (gizmo)            │
//   │ [Sele]                                                       │
//   │ [Mover]              VIEWPORT 3D                             │
//   │ [Rodar]                                                      │
//   │ [Escar]                                                      │
//   │ [Íman ]  legenda                                             │
//   └─────────────────────────────────────────────────────────────┘
//
// 0.9.6.19b (D21 · A DECISÃO DO DONO): o botão redondo [+] do fundo-direito
// foi REMOVIDO do chrome — era redundante (a hierarquia já tem o +, e o
// menu ⋯ ganhou o item «Novo objeto» — o MESMO código) e ATRAPALHAVA: cobria
// a cena e interceptava toques de orbit/seleção nessa zona. Os alvos de 40dp
// do chrome passam a N−1 (10) e a medição de área do chrome desce (a TABELA
// de medidas do relatório). O id 38 fica APOSENTADO (kVpAddTicIdRetired) —
// o pin «nenhum id de add desenhado dentro do rect do viewport» caça-o.
// PASSO 3 (0.9.6.17 — a spec do dono, confirmada no arranque do passo):
//   • O RAIL ESQUERDO: a fileira de ferramentas (Selecionar/Mover/Rodar/
//     Escalar/Íman) vive VERTICAL na borda ESQUERDA (era a toolbar
//     inferior HORIZONTAL — a degradação em colunas do stack antigo
//     mantém-se para viewports baixos);
//   • O GRUPO DO TOPO-ESQUERDO: desfazer/refazer/guardar/⋯ numa fila
//     HORIZONTAL (era o stack vertical de 5 — o duplicar/colar SAÍRAM da
//     viewport: as ações vivem no menu ⋯ itens 10/11 (eram 9/10 — o D21
//     deslocou-as com o «Novo objeto»), o caminho que já
//     existia);
//   • O [+]: REMOVIDO no 0.9.6.19b (D21 — a decisão do dono); o «novo
//     objeto» vive no + da hierarquia e no menu ⋯ («Novo objeto»);
//   • O GIZMO: 40dp no canto superior direito — o atalho mostrar/esconder
//     o gizmo 3D (a transição selectMode↔gizmo que JÁ existe; zero lógica
//     nova — a interpretação do "gizmo 40dp" da spec, documentada no
//     relatório para o dono vetar);
//   • A LEGENDA: o nome da ferramenta ativa, à direita do rail (por
//     baixo da fila do topo);
//   • A STRIP [Cena][Perspetiva][Global] MORREU — era a barra que
//     atravessava a largura toda (PROIBIDA pela spec; os chips eram
//     SEM FUNÇÃO desde o inventário do PASSO 0);
//   • 60% DE ALFA em TUDO o que o chrome desenha (os pais de vidro, os
//     chips, os ícones, a legenda — os glifos DESATIVados ficam nos seus
//     0.4 de sempre, já transluúcidos por desenho);
//   • a LEI DE OURO mantém-se: DESENHO 32dp / TOQUE 40dp, passos de 48.
#include "ui/EditorLayout.h"
#include "ui/Icons.h"
#include "ui/Toolbar.h"     // GizmoModeState
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"
#include "ui/Theme.h"
#include "core/Types.h"
#include "math/Math.h"   // Mat4 (column-major)

namespace vv {

class UiContext;
class Camera;

namespace editor {

struct EditorState;

namespace vpchrome {

// ---- ids (faixa 30..39 — livre desde a F5; presets 20..23, scroll 41..46) --
constexpr u64 kVpUndoId    = 30;
constexpr u64 kVpRedoId    = 31;
constexpr u64 kVpSaveId    = 32;
constexpr u64 kVpSelectId  = 35;   // modo Selecionar (cursor — SEM gizmo)
constexpr u64 kVpSnapValId = 36;   // botão de ÍMAN (toggle do snap)
constexpr u64 kVpMenuId    = 37;   // PASSO 3: o ⋯ do topo-esquerdo (abre o
                                   // menu de ficheiro ancorado a ele — o id
                                   // 37 do settings morto volta a usar-se)
// 0.9.6.19b (D21): kVpAddTicId (38) APOSENTADO — o [+] do viewport morreu.
// O id PERMANECE aqui SÓ para o pin (nenhum widget com este id desenha
// dentro do rect do viewport, em estado nenhum — a mutação M-D21 repõe um
// botão com ele e o pin fica VERMELHO).
inline constexpr u64 kVpAddTicIdRetired = 38;
constexpr u64 kVpGizmoId   = 39;   // PASSO 3: o gizmo 40dp do topo-direito
// kVpDupId (33) / kVpPasteId (34) REMOVIDOS no PASSO 3: os botões saíram da
// viewport (a spec do dono lista desfazer/refazer/guardar/⋯ no topo) — as
// ações Duplicar/Colar vivem no menu ⋯ (itens 10/11 desde o D21; o caminho
// da 0.9.6.10)

// ---- estado que o chrome precisa (o main possui TUDO; aqui só flags) -------
struct ChromeState {
    bool canUndo = false;     // há operações na pilha (botão aceso)
    bool canRedo = false;
    f32  snapValue = 0.5f;    // o valor do snap (0 = snap off)
    // canPaste REMOVIDO no PASSO 3 (o botão Colar saiu da viewport; o menu
    // ⋯ cola pelo estado g_clipValid do main — nada a apagar aqui)
};

// ---- LAYOUT PURO (fonte única — desenho e testes) ---------------------------
// A LEI DE OURO (PASSO 1): DESENHO 32dp / TOQUE 40dp, passos de 48 — nada
// ≥48 no editor (o [+] era 56: desce a 40 no PASSO 3, a spec do dono).
constexpr f32 kRailBtn   = 40.0f;  // alvo dos botões do rail (desenho 32)
constexpr f32 kRailGap   = 8.0f;   // ≥8dp entre alvos (spec A)
constexpr f32 kQuickBtn  = 40.0f;  // alvo da fila do topo
constexpr f32 kQuickGap  = 8.0f;
// 0.9.6.19b (D21): kCornerBtn MORREU (era «o [+] e o gizmo» — o [+] saiu).
// O gizmo do canto superior direito mantém o alvo 40dp com nome PRÓPRIO:
constexpr f32 kGizmoBtnDp = 40.0f; // o gizmo do topo-direito (a spec PASSO 3)
// PASSO 3: a ALFA do chrome do viewport (a spec do dono: «controlos a 60%»)
constexpr f32 kChromeAlpha = 0.60f;
// o multiplicador de alfa do chrome (inline no header para ser PERSISTENTE
// na prova: a sentinela R-034 afere o composto e a mutação da alfa morre
// no CI; os glifos desativados ficam nos 0.4 de sempre, fora disto)
inline void chromeCol(const f32* c, f32 out[4]) {
    out[0] = c[0];
    out[1] = c[1];
    out[2] = c[2];
    out[3] = c[3] * kChromeAlpha;
}

struct Layout {
    UiRect rail[5]{};         // Selecionar/Mover/Rodar/Escalar/Íman (coluna-
                              // major; a degradação em colunas é a do stack
                              // antigo — viewports baixos dividem a coluna)
    u32    railCols = 1;
    bool   railVisible = true;   // false = viewport tão baixo que nem 2
                                 // colunas cabem (a degradação honesta)
    UiRect quick[4]{};        // desfazer/refazer/guardar/⋯ (fila do topo)
    UiRect gizmoBtn{};        // o gizmo 40dp do topo-direito
    // 0.9.6.19b (D21): addTicBtn/plusPanel REMOVIDOS — o [+] morreu
    // os PAIS DE VIDRO (a regra do pai-painelinho — nada flutua sem pai)
    UiRect railPanel{};       // o pai do rail esquerdo
    UiRect quickPanel{};      // o pai da fila do topo
    UiRect gizmoPanel{};      // o pai do gizmo
    // a LEGENDA: o nome da ferramenta ativa, à direita do rail (por baixo
    // da fila do topo — o rect é a área do texto; some quando não cabe)
    bool   legendVisible = true;
    UiRect legend{};
    UiRect view{};            // o viewport central (para referência)
};

// resolve o layout dentro do rect do viewport central (o view JÁ vem
// encolhido pelo drawer aberto — o chamador passa currentDrawerH()).
// PASSO 3: a strip e o ChipWidths saíram (a barra full-width é proibida e
// os chips eram SEM FUNÇÃO — o inventário do PASSO 0)
Layout layout(const UiRect& view);

// ---- DRAW ---------------------------------------------------------------------
// camera = a câmara do editor (o TRIAD foi removido na FASE 9; o parâmetro
// fica por compat). drawerH = a altura do painel de baixo ABERTO neste
// frame (0 = fechado). Muta gz.mode/gz.snap + st.selectMode; o ⋯ arma o
// menu de ficheiro (st.fileMenu + as âncoras st.menuAx/menuAy); o gizmo
// alterna st.selectMode. Devolve as ações do frame (o main executa).
struct Actions {
    bool undoPressed  = false;
    bool redoPressed  = false;
    bool savePressed  = false;
    bool menuPressed  = false;    // PASSO 3: o ⋯ do topo-esquerdo
    // 0.9.6.19b (D21): addTicPressed REMOVIDO — o [+] do viewport morreu
};

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH);

} // namespace vpchrome
} // namespace editor
} // namespace vv
