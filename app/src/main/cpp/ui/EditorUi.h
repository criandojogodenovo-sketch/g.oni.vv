#pragma once
// ui/EditorUi.h — painéis do editor sobre o quad batch da F1 (F3).
//
// Layout landscape (tema mono, sem cores novas):
//   ┌─────────┬──────────────────────┬───────────┐
//   │ toolbar (3 botões: Menu/Play/Settings) — F1    │
//   ├─────────┼──────────────────────┼───────────┤
//   │ HIERARQ │  VIEWPORT (pass 3D)  │ INSPECTOR │
//   │ esquerda│   centro (câmara     │  direita  │
//   │  + bot. │    orbit da F2)      │  sliders  │
//   ├─────────┴──────────────────────┴───────────┤
//   │ status line (fps/tics/verts/dc) — F1           │
//   └────────────────────────────────────────────────┘
//
// Overlays ("+" → 3 presets; Menu → Save/Load) desenhados por cima, fecham
// com toque fora do painel. IDs de widgets por faixa: toolbar 1..3 (F1),
// hierarquia 40 e 1000+, presets 20..22, menu de ficheiro 30..31, sliders 2000+.
#include "core/Handle.h"
#include "ui/UiContext.h"

namespace vv {

class Scene;
class InputState;

namespace editor {

constexpr f32 kPanelW = 300.0f;   // largura dos painéis laterais

struct EditorState {
    Handle selected = Handle::invalid();   // TIC selecionado na Hierarchy
    bool   plusMenu = false;               // overlay de criação aberto
    bool   fileMenu = false;               // overlay Menu (Save/Load) aberto
};

// Rect do viewport central (entre os painéis) — usado para o gate da câmara.
UiRect centerRect(f32 sw, f32 sh);

// Painel esquerdo: lista de TICs (clique seleciona) + botão "+".
// Devolve true apenas no frame em que "+" é clicado (main abre o menu).
bool drawHierarchy(UiContext& ui, Scene& scene, EditorState& st);

// Painel direito: componentes do TIC selecionado; Transform3D com 9 sliders
// (pos/rot em graus/scale). Já atualiza tr->updateWorld() ao editar.
// Devolve true se algum slider alterou valores neste frame.
bool drawInspector(UiContext& ui, Scene& scene, EditorState& st);

// Overlays. Devolvem a escolha do frame:
//   drawPlusMenu → 0 nada, 1..3 = PresetKind (1=Player, 2=Character, 3=Static)
//   drawFileMenu → 0 nada, 1 = save, 2 = load
// Ambos fecham com toque fora do painel (mutam st).
int drawPlusMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st);
int drawFileMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st);

} // namespace editor

} // namespace vv
