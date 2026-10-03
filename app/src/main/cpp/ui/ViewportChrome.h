#pragma once
// ui/ViewportChrome.h — CHROME DO VIEWPORT 0.9.0 (spec D, mockups):
//
//   ┌─────────────────────────────────────────────────────────────┐
//   │ [undo]                                        ⊞ triad 64dp  │
//   │ [redo]                                                       │
//   │ [save]              VIEWPORT 3D                              │
//   │ [dup ]                                                       │
//   │ [paste]                                                      │
//   │                                                               │
//   │ [Selecionar][Mover][Rodar][Escalar] [snap: 0.5] [≡] [+ TIC] │
//   └─────────────────────────────────────────────────────────────┘
//
//   • stack de toque vertical à ESQUERDA: [undo][redo][save][duplicate]
//     [paste] 48dp com ESTADOS DISABLED (icon text2 40% — sem alvo);
//   • triad de orientação 64dp no canto superior direito (eixos X/Y/Z
//     projetados pela câmara — vermelho/verde/azul, as cores de eixo
//     documentadas dos gizmos);
//   • toolbar inferior: [Selecionar][Mover][Rodar][Escalar] 56dp
//     ROTULADOS (ativo = fill accent + accentInk) + [snap: <valor>]
//     (chip 4dp que cicla o valor) + [viewport settings] 🔶 (popover) +
//     [Adicionar TIC];
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
constexpr u64 kVpSnapValId = 36;   // chip [snap: <valor>] (cicla)
constexpr u64 kVpSettingsId= 37;   // viewport settings 🔶 (popover)
constexpr u64 kVpAddTicId  = 38;   // [Adicionar TIC]

// ---- estado que o chrome precisa (o main possui TUDO; aqui só flags) -------
struct ChromeState {
    bool canUndo = false;     // há operações na pilha (botão aceso)
    bool canRedo = false;
    bool canPaste = false;    // há TIC copiado na área de transferência
    f32  snapValue = 0.5f;    // o valor do chip (0 = snap off)
};

// ---- LAYOUT PURO (fonte única — desenho e testes) ---------------------------
constexpr f32 kStackBtn  = 48.0f;   // alvo do stack vertical (spec A)
constexpr f32 kStackGap  = 8.0f;    // ≥8dp entre alvos (spec A)
constexpr f32 kBottomH   = 56.0f;   // toolbar inferior rotulada (spec D)
constexpr f32 kTriad     = 64.0f;   // triad de orientação (spec D)

struct Layout {
    UiRect stack[5]{};        // undo redo save dup paste
    u32    nStack = 5;
    UiRect selectBtn{}, moveBtn{}, rotateBtn{}, scaleBtn{};
    UiRect snapChip{}, settingsBtn{}, addTicBtn{};
    UiRect triad{};           // quadrado 64dp do triad (canto sup-dir)
    UiRect view{};            // o viewport central (para referência)
};

// resolve o layout dentro do rect do viewport central
Layout layout(const UiRect& view);

// ---- DRAW ---------------------------------------------------------------------
// camera = a câmara do editor (para o TRIAD seguir a orientação).
// Muta gz.mode/gz.snap + st.selectMode (novo: modo Selecionar sem gizmo).
// Devolve as ações do frame (o main executa).
struct Actions {
    bool undoPressed  = false;
    bool redoPressed  = false;
    bool savePressed  = false;
    bool dupPressed   = false;
    bool pastePressed = false;
    bool addTicPressed = false;
    bool settingsPressed = false;   // abre o popover snap/grelha 🔶
};

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera);

} // namespace vpchrome
} // namespace editor
} // namespace vv
