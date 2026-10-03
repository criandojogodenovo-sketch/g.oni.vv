#pragma once
// ui/BottomPanel.h — PAINEL DE BAIXO 0.9.0 (spec E/K):
//
//   TAB BAR 48dp largura total: [Ficheiros][Consola][Animação] — ícone+
//   palavra (pasta/terminal/clapper), ativo com texto accent + underline 2dp
//   DRAWER: abre por baixo da tab bar; DEFAULT 240dp, PEGA de arrasto na
//   borda superior (160..400, passos de 8dp — spec E); tocar na tab ATIVA
//   fecha (o drawer desaparece, a tab bar fica)
//   STATUS 24dp (a ÚLTIMA faixa): "FPS 60 · TICs 4" 12sp text-2 — as
//   abreviaturas da 0.8.x morreram (spec E)
//
// CONTEÚDO do drawer (spec K):
//   • FICHEIROS: grelha de cards 96dp (miniatura por imageQuad quando há
//     textura, ícone de tipo caso contrário) + nome 12sp — o toque aplica
//     ao TIC selecionado pelo MESMO caminho do seletor (applyAssetPick)
//   • CONSOLA: linhas mono 12sp coloridas por nível (I=text-2, W=warn,
//     E=danger), chips [todos][erros], toggle auto-scroll, [Export]; o
//     toque numa linha EXPANDE (texto inteiro — o truncado abre)
//   • ANIMAÇÃO: a TIMELINE de sempre dentro do rect do drawer (tracks +
//     régua/keys + transporte + scrub — ui/Timeline.h intocado)
#include "ui/EditorLayout.h"
#include "ui/Icons.h"
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"
#include "ui/Theme.h"
#include "ui/EditorUi.h"   // AssetCatalog
#include "core/Types.h"

#include <vector>
#include <string>

namespace vv {

class UiContext;
class Scene;
class TimelineState;

namespace editor {

struct EditorState;

namespace bottom {

// ---- ids (faixa 5600..5639 — livre entre hierarquia 55xx e inspector 57xx)
constexpr u64 kTabFilesId  = 5600;   // tab Ficheiros
constexpr u64 kTabConsoleId = 5601;  // tab Consola
constexpr u64 kTabAnimId   = 5602;   // tab Animação
constexpr u64 kDrawerHandleId = 5603;// pega de arrasto do drawer
constexpr u64 kChipAllId   = 5604;   // chip [todos]
constexpr u64 kChipErrId   = 5605;   // chip [erros]
constexpr u64 kAutoScrollId = 5606;  // toggle auto-scroll
constexpr u64 kExportId    = 5607;   // [Export]
constexpr u64 kFileCardBase = 5610;  // cards de ficheiro (+i, cap 24)
constexpr u64 kConsoleScrollId = 47; // região de scroll da consola
constexpr u64 kFilesScrollId  = 48;  // região de scroll dos ficheiros

// ---- estado (o main é o dono; PERSISTE no layout.json — spec G) -----------
struct BottomState {
    int bottomTab = 0;        // 0 fechado · 1 ficheiros · 2 consola · 3 anim
    f32  drawerH = 240.0f;    // default spec E (160..400)
    bool consoleOnlyErrors = false;
    bool consoleAutoScroll = true;
    int  consoleExpanded = -1;   // linha expandida (toque expande — spec K)
    // drag da pega (redimensionar o drawer — spec E)
    bool dragActive = false;
    f32  dragStartY = 0.0f;
    f32  dragBaseH = 240.0f;
};

// ---- layout PURO --------------------------------------------------------------
struct Layout {
    UiRect tabBar{};             // faixa 48dp (bottomTabRect)
    UiRect tab[3]{};             // Ficheiros/Consola/Animação (terços)
    UiRect underline{};          // do tab ativo (2dp accent)
    UiRect drawer{};             // conteúdo (por CIMA da tab bar)
    UiRect handle{};             // pega de arrasto (borda sup. do drawer)
    UiRect status{};             // 24dp (statusRect)
    f32    drawerTop = 0.0f;     // y da pega
};

// resolve o layout (drawerH CLAMPADO a 160..400 em passos de 8)
Layout layout(f32 sw, f32 sh, const safe::Insets& in, const BottomState& st);

// ---- DRAW -----------------------------------------------------------------------
// catalog: meshes/textures/audio do projeto (cards de Ficheiros)
// logLines: tail do engine.log (Consola); logDumps NÃO (só o viewer de sempre)
// timeline visible: desenha a timeline dentro do drawer (o main liga)
struct Actions {
    bool exportPressed = false;         // [Export] da consola
    int  filePick = 0;                  // 1.. = card i escolhido (aplicar)
    int  filePickKind = 0;              // 1 mesh, 2 tex, 5 áudio (applyAssetPick)
};

Actions draw(UiContext& ui, const InputState& in, EditorState& st,
             BottomState& bs, const AssetCatalog& catalog,
             const std::vector<std::string>& logLines, int fps, u32 ticCount);

// ---- STATUS BAR 24dp (spec E: SÓ "FPS N · TICs N" — zero abreviaturas) --------
void drawStatusBar(UiContext& ui, f32 sw, f32 sh, const safe::Insets& in,
                   int fps, u32 ticCount);

// ---- persistência (spec G: layout.json) ---------------------------------------
// serializa/parse PURO do estado do layout (bottom + inspector + painéis) —
// afervel no CI. Formato (uma linha por campo, chave=valor):
//   bottomTab=N drawerH=N inspector=0/1 inspCollapsed=0x..
std::string serializeLayout(const BottomState& bs, bool showInspector,
                            u32 inspCollapsed);
// devolve false se ilegível (o chamador usa DEFAULTS — "Repor layout")
bool parseLayout(const std::string& data, BottomState& bs, bool& showInspector,
                 u32& inspCollapsed);

} // namespace bottom
} // namespace editor
} // namespace vv
