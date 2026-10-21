#pragma once
// ui/BottomPanel.h — PAINEL DE BAIXO (FASE UI F — a ordem do dono):
//
//   BARRA INFERIOR FINA 32dp (sempre visível):
//     [Painéis ▾]  ................................  60 FPS · 6 TICs
//     à ESQUERDA o botão «Painéis ▾» (CONTORNO BRASA quando o painel
//     está aberto; o ponto vermelho acende quando há erro na consola);
//     à DIREITA o estado «60 FPS · 6 TICs» (o «· bl n/m» SAIU — a ordem)
//   PAINEL ENCAIXADO (NÃO é janela por cima — o viewport SOBE e fica
//     visível; os rects do centro comem a altura pelo panelsRect):
//     ┌ pega 24dp (arrasto — altura lembra-se) ─────────────┐
//     │ [Ficheiros][Assets][Consola][Animação]  (aba 40dp)  │
//     │ conteúdo da aba                                     │
//     └─────────────────────────────────────────────────────┘
//   • abre pelo Painéis com a ÚLTIMA aba aberta (lastTab — persiste)
//   • altura inicial ~30% (Ficheiros/Assets/Consola) · ~40% (Animação)
//     — tabDefaultHPct; o arrasto do dono manda depois (hCustom)
//   • fecha ao tocar em ▾ ou no Painéis (a aba ativa NÃO fecha mais)
//   • Consola com PONTO VERMELHO na aba e no botão quando há erro
//   • Animação: a timeline de sempre (régua + faixas + losangos + o
//     cursor de tempo EM BRASA — ui/Timeline.h intocado)
//
// CONTEÚDO (o de sempre):
//   • FICHEIROS: árvore res:// (pastas reais + contagem → abre Assets
//     filtrado)   • ASSETS: grelha/lista com miniaturas reais
//   • CONSOLA: chips Logs/Erros/Avisos + linhas mono por nível + comando
//   • ANIMAÇÃO: drawTimelineInRect (o main liga)
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
// FASE UI F: o botão «Painéis ▾» da barra (a esquerda; o id fora das
// faixas das tabs — o painel abre/fecha AQUI)
constexpr u64 kPanelsBtnId   = 5638;
constexpr u64 kConsoleScrollId = 47; // região de scroll da consola
constexpr u64 kFilesScrollId  = 48;  // região de scroll dos ficheiros

// 0.9.6.18 (HOTFIX D9): o empty-state da tab Animação (o main desenha-o
// quando a timeline não pode desenhar) — centrado+clipado no rect de
// conteúdo do drawer (o MESMO caminho das outras tabs)
void drawDrawerEmptyState(UiContext& ui, const UiRect& rect,
                          const char* label);

// 0.9.6.18 (HOTFIX D3/D9): o HELPER de empty-state — centrado V/H no rect,
// CLIPADO a ele (ScopedClip) e com a trava no fundo do texto (o fundo
// nunca passa o fundo do rect). Exportado para a sentinela R-035 (o caso
// do rect minúsculo — a mutação M3 fica vermelha)
void drawEmptyState(UiContext& ui, const UiRect& rect, icons::Icon ic,
                    const char* label);

// 0.12-C (C-4) — O ÍCONE do estado vazio por TAB (a FONTE ÚNICA — o draw
// e o pin icones_unicos partilham): o vazio usa o glifo da PRÓPRIA função
// (Assets→Box, Ficheiros→Folder, Animação→Clapper; a CONSOLA não tem
// vazio desenhado — o log vazio mostra os chips na mesma). tab 1..4.
icons::Icon emptyStateIcon(int tab);

// ---- estado (o main é o dono; PERSISTE no layout.json — spec G) -----------
struct BottomState {
    // 0.9.6.10 (GRUPO UI · a imagem 1): o dock inferior passou a 4 tabs —
    // Ficheiros (a árvore res://) · Assets (a grelha com miniaturas) ·
    // Consola · Animação (a timeline de sempre)
    // FASE UI F: 0 = FECHADO (o painel abre/fecha pelo botão Painéis;
    // a ÚLTIMA aba fica em lastTab — o Painéis reabre-a)
    int bottomTab = 0;        // 0 fechado · 1 ficheiros · 2 assets ·
                              // 3 consola · 4 animação
    // FASE UI F: a ÚLTIMA aba aberta (persiste no layout.json; os
    // ficheiros antigos não têm a linha → default 1)
    int  lastTab = 1;
    // FASE UI F: o dono JÁ ARRASTOU a altura nesta abertura (os defaults
    // ~30%/~40% da tab só se aplicam sem isto; morre ao fechar)
    bool hCustom = false;
    f32  drawerH = 240.0f;    // default spec E (160..400)
    bool consoleOnlyErrors = false;
    bool consoleAutoScroll = true;
    int  consoleExpanded = -1;   // linha expandida (toque expande — spec K)
    // 0.9.6.10: o browser de assets — a pasta selecionada na árvore (-1 =
    // tudo) e a vista (grelha/lista — o toggle da imagem 1)
    int  filesFolder = -1;
    bool assetsList = false;
    // 0.10-M (PASSO 4 · APOSENTADO NA FASE UI F): as métricas de BLOCOS
    // do frame — o «· bl n/m» SAIU do estado (a ordem do dono); os
    // campos ficam (compat dos chamadores) mas NADA os desenha
    u32  blDrawn = 0;
    u32  blTotal = 0;
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
// FASE UI F: a ALTURA INICIAL por tab (a ordem do dono: ~30% nos
// Ficheiros/Assets/Consola, ~40% na Animação) — a FONTE ÚNICA do
// default; o draw aplica-a ao ENTRAR na tab (open ou troca) enquanto o
// dono não arrastou a pega (hCustom)
inline f32 tabDefaultHPct(int tab) { return tab == 4 ? 0.40f : 0.30f; }
// a ALTURA INICIAL em px do tab dado (contentH em px — o contentRect;
// clamp ao intervalo cru da pega 160..400 — o cap EFETIVO é o
// safe::effectiveDrawerH, que fala depois no layout)
inline f32 tabDefaultH(int tab, f32 contentH) {
    f32 d = contentH * tabDefaultHPct(tab);
    if (d < safe::kDrawerMin) d = safe::kDrawerMin;
    if (d > safe::kDrawerMax) d = safe::kDrawerMax;
    return d;
}
// PASSO 1 (0.9.6.14): a faixa FPS·TICs da barra (o canto direito) — a
// largura é RESERVA FIXA em dp (o layout é PURO, não mede fonte; o draw
// right-alinha o texto e o fit trunca honestamente se não couber)
constexpr f32 kFpsW = 112.0f;   // dp — o canto direito da barra
// FASE UI F: o botão «Painéis ▾» (a esquerda) — reserva fixa dp (o
// rótulo + o chevron cabem; o layout é PURO)
constexpr f32 kPanelsBtnW = 96.0f;   // dp
// PASSO 2 (0.9.6.15 — spec do dono): a PEGA do drawer tem 24dp de altura
// (era 12 — a MESMA medida dos divisores dos painéis, kDividerHitW) e os
// chips INTERNOS da consola são de CABEÇALHO 28dp (era 36 — o piso
// kHeadFloorDp; a chip «Consola» duplicada SAIU)
constexpr f32 kDrawerHandleH = 24.0f;  // dp — a pega de arrasto do drawer
// FASE UI F: a fila de TABS DENTRO do painel (40dp — o toque mínimo da
// fase; as tabs SAÍRAM da barra de 32 para o cabeçalho do painel)
constexpr f32 kTabRowH = 40.0f;        // dp
constexpr f32 kConChipH      = 28.0f;  // dp — os chips Logs/Erros/Avisos
//
// P2-bis (0.9.6.16 — A DECISÃO DO DONO sobre «consola ≥60%»): com a tab
// Consola ativa, a LISTA de log ocupa ≥60% da altura de CONTEÚDO do
// drawer (chips+extras ≤40%) — SEM exceções. O extra que CEdE é o campo
// de comando: se com ele a lista fica <60%, o campo SAI (o drawer
// pequeno vive com chips 28dp + a lista no resto — exatamente as
// palavras do dono). Os números da regra, todos NOMEADOS:
constexpr f32 kConListMinPct = 0.60f;  // a lista não desce abaixo disto
constexpr f32 kConChipPad    = 2.0f;   // dp — respiro acima das chips (era 4)
constexpr f32 kConListGap    = 2.0f;   // dp — chips → lista (era 8; a linha
                                       // separadora de 1px vive DENTRO
                                       // deste intervalo)
constexpr f32 kConCmdH       = 40.0f;  // dp — o campo de comando (a LEI DE
                                       // OURO: toque 40 — nunca encolhe)
constexpr f32 kConCmdPad     = 4.0f;   // dp — a folga de baixo do campo
//
// a decisão PURA da regra (em dp — densidade-invariante, a lição R-018):
// o campo de comando só existe se a lista com ele ainda cumpre o piso de
// kConListMinPct; o draw e os testes partilham ESTA pergunta (a fonte
// ÚNICA — o padrão da casa)
inline bool conCmdVisible(f32 contentHdp) {
    const f32 top = kConChipPad + kConChipH + kConListGap;
    const f32 cmd = kConCmdH + kConCmdPad;
    return (contentHdp - top - cmd) >= contentHdp * kConListMinPct;
}

struct Layout {
    UiRect tabBar{};             // a BARRA INFERIOR fina 32dp (bottomTabRect)
    UiRect panelsBtn{};          // FASE UI F: o botão «Painéis ▾» (a esquerda)
    UiRect fps{};                // o canto direito «N FPS · N TICs»
    UiRect drawer{};             // o PAINEL ENCAIXADO (por cima da barra)
    UiRect handle{};             // pega de arrasto (borda sup. do drawer)
    UiRect tabRow{};             // FASE UI F: a fila de tabs DENTRO do painel
    UiRect tab[4]{};             // Ficheiros/Assets/Consola/Animação (na fila)
    UiRect underline{};          // do tab ativo (2dp accent)
    UiRect content{};            // o conteúdo da aba (sob a fila de tabs)
    f32    drawerTop = 0.0f;     // y da pega
};

// resolve o layout (drawerH CLAMPADO a 160..400 em passos de 8; a altura
// EFETIVA é capada pela fonte única — kViewportMinH e 35% do content; a
// tab Animação cap a 40% pela ordem da FASE UI F)
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
    bool importPressed = false;         // 0.12-C (C-4): o IMPORTAR do estado
                                        // vazio dos Assets (o main abre o
                                        // navegador na pasta Download)
};

Actions draw(UiContext& ui, const InputState& in, EditorState& st,
             BottomState& bs, const AssetCatalog& catalog,
             const std::vector<std::string>& logLines, int fps, u32 ticCount,
             const FilesTree& tree = FilesTree{});

// ---- persistência (spec G: layout.json) ---------------------------------------
// serializa/parse PURO do estado do layout (bottom + inspector + painéis) —
// afervel no CI. Formato (uma linha por campo, chave=valor):
//   bottomTab=N drawerH=N inspector=0/1 inspCollapsed=0x..
// FASE UI F: lastTab=N (a ÚLTIMA aba aberta — AUSENTE nos ficheiros
// antigos = default 1; o formato é retrocompatível)
// GRUPO D: hierW=N inspW=N (larguras dp dos divisores; AUSENTE nos
// ficheiros antigos = default −1 — o formato é retrocompatível)
// P2-bis: inspPinned=0/1 (o pin do inspector; AUSENTE nos ficheiros
// antigos = false — o formato é retrocompatível)
std::string serializeLayout(const BottomState& bs, bool showInspector,
                            u32 inspCollapsed, f32 hierW = -1.0f,
                            f32 inspW = -1.0f, bool inspPinned = false,
                            // 0.11-H (FASE 1): os NOMES das TICs recolhidas
                            // da hierarquia (a lista da cena aberta; vazio
                            // = tudo aberto — os ficheiros antigos não têm
                            // as linhas e lêem vazio)
                            const std::vector<std::string>& hierCollapsed = {});
// devolve false se ilegível (o chamador usa DEFAULTS — "Repor layout").
// hierW/inspW/inspPinned opcionais (nullptr = não ler; compat dos testes)
// 0.11-H: hierCollapsed opcional (nullptr = não ler — os chamadores antigos
// ficam byte a byte; preenchido SEMPRE que presente, vazio se sem linhas)
bool parseLayout(const std::string& data, BottomState& bs, bool& showInspector,
                 u32& inspCollapsed, f32* hierW = nullptr,
                 f32* inspW = nullptr, bool* inspPinned = nullptr,
                 std::vector<std::string>* hierCollapsed = nullptr);

} // namespace bottom
} // namespace editor
} // namespace vv
