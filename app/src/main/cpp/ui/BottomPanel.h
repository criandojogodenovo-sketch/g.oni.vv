#pragma once
// ui/BottomPanel.h — PAINEL DE BAIXO 0.9.0 (spec E/K) · PASSO 1 (0.9.6.14):
//
//   TAB BAR 32dp largura total (era 48): [Ficheiros][Assets][Consola]
//   [Animação] à ESQUERDA — ícone 20 + palavra 12sp, ativo com texto
//   accent + underline 2dp; à DIREITA o «FPS n · TICs n» (a status bar de
//   24dp MORREU — a spec PASSO 1: a faixa extra sai; a versão/commit
//   vivem em Settings › Sobre)
//   DRAWER: abre por baixo da tab bar; DEFAULT 240dp, PEGA de arrasto na
//   borda superior (160..400, passos de 8dp — spec E); tocar na tab ATIVA
//   fecha (o drawer desaparece, a tab bar fica)
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
constexpr u64 kTabAssetsId = 5608;   // 0.9.6.10 (GRUPO UI): tab Assets
constexpr u64 kAssetsViewId = 5609;  // toggle grelha/lista do browser
constexpr u64 kFileCardBase = 5610;  // cards de ficheiro (+i, cap 48)
constexpr u64 kTreeRowBase = 5620;   // 0.9.6.10: as linhas da árvore res://
constexpr u64 kTreeScrollId = 49;    // scroll da árvore res://
constexpr u64 kAssetsListRowBase = 5630;  // linhas da vista em LISTA
constexpr u64 kConsoleTabBase = 5632;  // 0.9.6.10: tabs Consola/Logs/Erros/Avisos
constexpr u64 kCmdFieldId    = 5636;  // 0.9.6.10: o campo de comando da consola
constexpr u64 kConsoleScrollId = 47; // região de scroll da consola
constexpr u64 kFilesScrollId  = 48;  // região de scroll dos ficheiros

// ---- estado (o main é o dono; PERSISTE no layout.json — spec G) -----------
struct BottomState {
    // 0.9.6.10 (GRUPO UI · a imagem 1): o dock inferior passou a 4 tabs —
    // Ficheiros (a árvore res://) · Assets (a grelha com miniaturas) ·
    // Consola · Animação (a timeline de sempre)
    int bottomTab = 0;        // 0 fechado · 1 ficheiros · 2 assets ·
                              // 3 consola · 4 animação
    f32  drawerH = 240.0f;    // default spec E (160..400)
    bool consoleOnlyErrors = false;
    bool consoleAutoScroll = true;
    int  consoleExpanded = -1;   // linha expandida (toque expande — spec K)
    // 0.9.6.10: o browser de assets — a pasta selecionada na árvore (-1 =
    // tudo) e a vista (grelha/lista — o toggle da imagem 1)
    int  filesFolder = -1;
    bool assetsList = false;
    // 0.9.6.10: a consola da imagem 1 — TABS Consola/Logs/Erros/Avisos
    // (0=tudo · 1=info · 2=erros · 3=avisos; o filtro de chips antigo morre)
    int  consoleTab = 0;
    // drag da pega (redimensionar o drawer — spec E)
    bool dragActive = false;
    f32  dragStartY = 0.0f;
    f32  dragBaseH = 240.0f;
};

// ---- 0.9.6.10 (GRUPO UI) · A ÁRVORE res:// (a imagem 1: «Ficheiros») --------
// As pastas REAIS do projeto (o main alimenta em refreshCatalog — zero IO
// por frame); a árvore mostra dir + rótulo + contagem e o toque ABRE o
// browser de Assets FILTRADO à pasta
struct TreeEntry {
    const char* dir;     // o caminho real ("assets")
    const char* label;   // o rótulo apresentado ("Modelos")
    u32 count;           // ficheiros na pasta
};
struct FilesTree {
    TreeEntry entries[8];
    u32 n = 0;
    u32 sceneCount = 0;   // cenas do manifesto (a linha res://)
};

// ---- layout PURO --------------------------------------------------------------
// PASSO 1 (0.9.6.14): a faixa FPS·TICs da tab bar (o canto direito) — a
// largura é RESERVA FIXA em dp (o layout é PURO, não mede fonte; o draw
// right-alinha o texto e o fit trunca honestamente se não couber)
constexpr f32 kFpsW = 112.0f;   // dp — o canto direito da tab bar

struct Layout {
    UiRect tabBar{};             // faixa 32dp (bottomTabRect)
    UiRect tab[4]{};             // 0.9.6.10: Ficheiros/Assets/Consola/Animação
                                 // (PASSO 1: à esquerda do canto FPS·TICs)
    UiRect fps{};                // PASSO 1: o canto direito «FPS n · TICs n»
    UiRect underline{};          // do tab ativo (2dp accent)
    UiRect drawer{};             // conteúdo (por CIMA da tab bar)
    UiRect handle{};             // pega de arrasto (borda sup. do drawer)
    f32    drawerTop = 0.0f;     // y da pega
};

// resolve o layout (drawerH CLAMPADO a 160..400 em passos de 8)
Layout layout(f32 sw, f32 sh, const safe::Insets& in, const BottomState& st);

// ---- DRAW -----------------------------------------------------------------------
// catalog: meshes/textures/audio do projeto (cards de Ficheiros)
// logLines: tail do engine.log (Consola); logDumps NÃO (só o viewer de sempre)
// timeline visible: desenha a timeline dentro do drawer (o main liga)
// PASSO 1: o «FPS n · TICs n» vive no CANTO DIREITO da tab bar (a status
// bar de 24dp foi REMOVIDA — a versão/commit estão em Settings › Sobre;
// StatusBarData/drawStatusBar apagados pela spec)
struct Actions {
    bool exportPressed = false;         // [Export] da consola
    int  filePick = 0;                  // 1.. = card i escolhido (aplicar)
    int  filePickKind = 0;              // 1 mesh, 2 tex, 5 áudio (applyAssetPick)
    bool commandPressed = false;        // 0.9.6.10: o campo de comando (o
                                        // main abre o teclado — propósito 9)
};

Actions draw(UiContext& ui, const InputState& in, EditorState& st,
             BottomState& bs, const AssetCatalog& catalog,
             const std::vector<std::string>& logLines, int fps, u32 ticCount,
             const FilesTree& tree = FilesTree{});

// ---- persistência (spec G: layout.json) ---------------------------------------
// serializa/parse PURO do estado do layout (bottom + inspector + painéis) —
// afervel no CI. Formato (uma linha por campo, chave=valor):
//   bottomTab=N drawerH=N inspector=0/1 inspCollapsed=0x..
// GRUPO D: hierW=N inspW=N (larguras dp dos divisores; AUSENTE nos
// ficheiros antigos = default −1 — o formato é retrocompatível)
std::string serializeLayout(const BottomState& bs, bool showInspector,
                            u32 inspCollapsed, f32 hierW = -1.0f,
                            f32 inspW = -1.0f);
// devolve false se ilegível (o chamador usa DEFAULTS — "Repor layout").
// hierW/inspW opcionais (nullptr = não ler; compat dos testes antigos)
bool parseLayout(const std::string& data, BottomState& bs, bool& showInspector,
                 u32& inspCollapsed, f32* hierW = nullptr,
                 f32* inspW = nullptr);

} // namespace bottom
} // namespace editor
} // namespace vv
