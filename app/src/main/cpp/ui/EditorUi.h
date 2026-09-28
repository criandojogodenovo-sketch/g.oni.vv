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
// F4.1: a lista de TICs e o Inspector são ScrollRegions (ui/ScrollMath.h +
// UiContext::beginScroll) — conteúdo mais alto que o painel chega ao fundo
// (BodyComp / add TouchControls / TICs >10). Dentro da região: drag =
// scroll, tap re-despachado pelo painel (seleção/ação), slider mantém
// prioridade. O viewport central NÃO é região de scroll (orbit/pinch
// intactos). Overlays ("+" → 4 presets; Menu → Save/Load) fecham com toque
// fora do painel. IDs: toolbar 1..3 (F1), scroll 41/42, hierarquia 40 e
// 1000+, presets 20..23, menu de ficheiro 30..31, sliders 2000+, velx 2100,
// add TouchControls 3001.
#include "core/Handle.h"
#include "ui/UiContext.h"
#include "ui/EditorLayout.h"

namespace vv {

class Scene;
class InputState;
class TouchControls;

namespace editor {

constexpr f32 kPanelW = 300.0f;   // largura dos painéis laterais

struct EditorState {
    Handle selected = Handle::invalid();   // TIC selecionado na Hierarchy
    bool   plusMenu = false;               // overlay de criação aberto
    bool   fileMenu = false;               // overlay Menu (Save/Load) aberto
};

// Rect do viewport central (entre os painéis) — usado para o gate da câmara.
UiRect centerRect(f32 sw, f32 sh);

// Painel esquerdo: lista de TICs COM SCROLL (todas as entradas, sem corte) +
// botão "+" no cabeçalho. Tap numa linha seleciona (re-despacho do scroll).
// Devolve true apenas no frame em que "+" é clicado (main abre o menu).
bool drawHierarchy(UiContext& ui, Scene& scene, EditorState& st);

// Painel direito: componentes do TIC selecionado COM SCROLL — Transform3D
// com 9 sliders (pos/rot graus/scale), MeshRenderer, InputMap, BodyComp
// (tipo·forma·chão + velx) e botão "add TouchControls" no fundo, sempre
// alcançável. Já atualiza tr->updateWorld() ao editar.
// Devolve true se algum slider alterou valores neste frame.
bool drawInspector(UiContext& ui, Scene& scene, EditorState& st);

// F4: controlos de toque (só em modo Play, só se algum TIC ativo tem o
// componente) — joystick quadrado + botão JUMP no quad batch, tema mono.
void drawTouchControls(UiContext& ui, const TouchControls& tc, f32 sw, f32 sh);

// Overlays. Devolvem a escolha do frame:
//   drawPlusMenu → 0 nada, 1..4 = PresetKind (1=Player, 2=Character,
//                  3=Static, 4=Rigid)
//   drawFileMenu → 0 nada, 1 = save, 2 = load
// Ambos fecham com toque fora do painel (mutam st).
int drawPlusMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st);
int drawFileMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st);

} // namespace editor

} // namespace vv
