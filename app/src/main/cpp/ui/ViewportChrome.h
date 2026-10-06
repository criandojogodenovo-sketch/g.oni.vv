#pragma once
// ui/ViewportChrome.h — CHROME DO VIEWPORT (0.9.0 spec D; FASE 9 G1-1):
//
//   ┌─────────────────────────────────────────────────────────────┐
//   │ [redo]                                                       │
//   │ [save]              VIEWPORT 3D                              │
//   │ [dup ]                                                       │
//   │ [paste]                                                      │
//   │                                                               │
//   │ [▲][✥][⟳][⤢] [🧲]                                     [+]   │
//   └─────────────────────────────────────────────────────────────┘
//
// FASE 9 (G1-1 — toolbar ANCORADA À VIEWPORT): a toolbar inferior vivia
// ancorada ao fundo do ECRÃ (centerRect com drawerH=0 HARDCODED) — com o
// painel de baixo ABERTO ela sobrepunha o drawer e o Inspector e o
// "Selecionar" ficava cortado à esquerda. AGORA:
//   • o layout recebe o drawerH REAL (a toolbar SOBE quando o painel de
//     baixo abre e DESCE quando fecha — acompanha o rect da viewport);
//   • SÓ ÍCONES; o NOME só no botão ATIVO (fill accent + palavra);
//   • snap vira BOTÃO DE ÍMAN (estado ativo/inativo, sem texto — G2-9);
//   • "Adicionar TIC" vira "+" no canto inferior DIREITO da viewport;
//   • o botão de settings (sliders) REMOVIDO — estava MORTO desde a
//     0.9.0 (vpSettingsMenu setado, NADA lia — inventário G0-4);
//   • tudo DENTRO do rect da viewport (por construção: âncoras e larguras
//     derivam do rect — nunca por cima de outro painel).
//
//   • stack de toque vertical à ESQUERDA: [undo][redo][save][duplicate]
//     [paste] 48dp com ESTADOS DISABLED (icon text2 40% — sem alvo);
//   • TRIAD REMOVIDO na FASE 9 (G2-10) — os "pontinhos fantasma" do dono;
//   • os gizmos 3D existentes com grab-lock ficam INTACTOS (ui/Gizmo.h).
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
constexpr u64 kVpDupId     = 33;
constexpr u64 kVpPasteId   = 34;
constexpr u64 kVpSelectId  = 35;   // modo Selecionar (cursor — SEM gizmo)
constexpr u64 kVpSnapValId = 36;   // botão de ÍMAN (toggle do snap)
constexpr u64 kVpAddTicId  = 38;   // [+] (canto inferior direito)
// kVpSettingsId (37) REMOVIDO na FASE 9: o botão estava MORTO (inventário
// G0-4 — vpSettingsMenu nunca era lido). O id fica LIVRE.

// ---- estado que o chrome precisa (o main possui TUDO; aqui só flags) -------
struct ChromeState {
    bool canUndo = false;     // há operações na pilha (botão aceso)
    bool canRedo = false;
    bool canPaste = false;    // há TIC copiado na área de transferência
    f32  snapValue = 0.5f;    // o valor do snap (0 = snap off)
};

// ---- LAYOUT PURO (fonte única — desenho e testes) ---------------------------
constexpr f32 kStackBtn  = 48.0f;   // alvo do stack vertical (spec A)
constexpr f32 kStackGap  = 8.0f;    // ≥8dp entre alvos (spec A)
constexpr f32 kBottomH   = 48.0f;   // toolbar inferior (FASE 9: 48dp — só ícones)
constexpr f32 kToolBtn   = 48.0f;   // botão de ferramenta (ícone)
// kToolActiveW REMOVIDO na 0.9.6.1 (G1-2): os 4 botões são IGUAIS de 48dp
// só-ícone; o nome da ferramenta ativa vive numa legenda ACIMA da barra
// (o "Escalar" de 48px estendia-se por cima dos vizinhos)
constexpr f32 kToolActiveW = 48.0f;   // LEGACY (igual a kToolBtn; sem uso novo)

struct Layout {
    UiRect stack[5]{};        // undo redo save dup paste (coluna-major)
    u32    nStack = 5;
    u32    stackCols = 1;     // GRUPO D: 1 nos ecrãs largos; 2/3 nos curtos
    bool   stackVisible = true;  // GRUPO D: false = viewport TÃO curto/estreito
                                 // que nem 1 linha de 5 colunas cabe (a
                                 // degradação honesta: toolbar+viewport mandam)
    UiRect selectBtn{}, moveBtn{}, rotateBtn{}, scaleBtn{};
    UiRect snapBtn{}, addTicBtn{};
    bool   plusTopRight = false;  // GRUPO D: [+] no canto SUP-dir quando a
                                  // viewport não comporta [+] ao lado da
                                  // toolbar (device: viewport de ~288dp)
    UiRect view{};            // o viewport central (para referência)
    // P-08 (0.9.6.12 · GRUPO J1 · R-022): a degradação honesta da altura —
    // com o drawer aberto o viewport central encolhe; quando nem strip +
    // toolbar cabem (viewport sub-piso, só em testes), a strip ESCONDE e a
    // legenda some — a toolbar fica DENTRO do rect (regra §2.2 do contrato
    // docs/LAYOUT_HIERARCHY.md)
    bool   stripVisible = true;   // false = viewport sub-piso (a strip some)
    bool   legendVisible = true;  // false = a legenda da ferramenta some
    // 0.9.6.10 (GRUPO UI · a REGRA DO PAI-PAINEILO — o anti-exemplo da
    // imagem 2: «nunca mais painéis/toolbar sem painel-mãe, sem cabeçalho,
    // sem clip»): TUDO o que flutua sobre a grelha ganha um PAI de vidro
    UiRect strip{};           // a strip do TOPO: [Cena][Perspetiva][Global]
    UiRect stripCena{}, stripPersp{}, stripGlobal{};   // os chips da strip
    UiRect stackPanel{};      // o pai do stack vertical (o rail esquerdo)
    UiRect toolPanel{};       // o pai da toolbar inferior (+ a legenda)
    UiRect plusPanel{};       // o pai do [+]
};

// resolve o layout dentro do rect do viewport central (o view JÁ vem
// encolhido pelo drawer aberto — o chamador passa currentDrawerH())
Layout layout(const UiRect& view);

// ---- DRAW ---------------------------------------------------------------------
// camera = a câmara do editor (para o TRIAD seguir a orientação).
// drawerH = a altura do painel de baixo ABERTO neste frame (0 = fechado) —
// a toolbar acompanha (G1-1).
// Muta gz.mode/gz.snap + st.selectMode (novo: modo Selecionar sem gizmo).
// Devolve as ações do frame (o main executa).
struct Actions {
    bool undoPressed  = false;
    bool redoPressed  = false;
    bool savePressed  = false;
    bool dupPressed   = false;
    bool pastePressed = false;
    bool addTicPressed = false;
};

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH);

} // namespace vpchrome
} // namespace editor
} // namespace vv
