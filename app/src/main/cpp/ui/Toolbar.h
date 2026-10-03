#pragma once
// ui/Toolbar.h — CHROME DE CIMA 0.9.0 (spec D, mockups do autor):
//
//   TOP BAR 56dp:  [Menu ≡][Cena ▾]   ·   [pause][play][sliders]   ·   [gear]
//   TAB BAR 48dp:  [3D][UI][ÁUDIO] — ícone+palavra, ativo com UNDERLINE
//                  accent 2dp (corrige o "UDIO" da 0.8.11: agora é a palavra
//                  inteira + ícone, nunca abreviado)
//
// O que SAI da barra (0.7.6→0.9.0): o grupo G4 de transformação desce para
// a TOOLBAR DO VIEWPORT (bottom: [Selecionar][Mover][Rodar][Escalar] 56dp
// rotulados — ui/ViewportChrome.h); o toggle do Inspector vira PEGA de
// recolher no próprio painel (spec G). O "sliders" abre o POPOVER de
// snap/grelha 🔶 (id novo); o gear abre a PÁGINA de Settings (0.9.0 — spec I,
// já não é dropdown).
//
// TEMA: tudo lido de theme::kTheme (ui/Theme.h — tabela spec A): fundo bg,
// ícones text1, ativo = underline accent 2dp + texto accent (tab bar) ou
// fill accent + accentInk (botões premidos da top bar). Zero emoji.
#include "ui/EditorLayout.h"   // ids partilhados + kPad
#include "ui/Icons.h"
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"     // UiRect
#include "ui/Theme.h"
#include "core/Types.h"

namespace vv {

class UiContext;

namespace editor {

struct EditorState;   // ui/EditorUi.h (dependência só de declaração)

namespace toolbar {

// ---- estado do gizmo (o modo vive aqui; o SNAP desceu p/ a toolbar do ------
// viewport — spec D: [snap: <valor>] é um controlo LÁ, não um botão da barra)
struct GizmoModeState {
    int  mode = 0;    // 0=Mover, 1=Rodar, 2=Escalar (gizmo::Mode)
    bool snap = false;
};

// ---- ids dos widgets (faixas exclusivas — UiContext é im-mode) -------------
// Herdados: Menu=1, Play=2, Pause=3, 3D/UI=11/12, cena=14, ÁUDIO=15.
// Novos 0.9.0: sliders (popover snap/grelha 🔶) = 16, gear (Settings) = 17.
constexpr u64 kTbMenuId      = 1;
constexpr u64 kTbPlayId      = 2;
constexpr u64 kTbPauseId     = 3;
constexpr u64 kGizmoIds[3]   = {7, 8, 9};    // mover/rodar/escalar (toolbar do viewport 0.9.0)
constexpr u64 kTbSnapId      = 10;            // (legacy: o snap vive agora no chip do viewport)
constexpr u64 kMode3dId      = 11;
constexpr u64 kModeUiId      = 12;
constexpr u64 kTbInspectId   = 13;            // (legacy: pega do painel — spec G)
constexpr u64 kTbCenaId      = 14;
constexpr u64 kModeAudioId   = 15;
constexpr u64 kTbSlidersId   = 16;   // 0.9.0: popover snap/grelha 🔶
constexpr u64 kTbGearId      = 17;   // 0.9.0: PÁGINA de Settings (spec I)

// ---- TOP BAR (56dp) ----------------------------------------------------------
struct TopBarLayout {
    UiRect bar{};                 // a faixa toda (safe::toolbarRect — 56dp)
    UiRect menu{}, cena{};        // esquerda (ícone hamburger/chevron + texto)
    UiRect pause{}, play{};       // centro (ícones)
    UiRect sliders{};             // centro (popover snap/grelha 🔶)
    UiRect gear{};                // direita (ancorado)
    f32    iconSize = 24.0f;      // 24dp dentro dos alvos 48dp (spec A)
};

// resolve o layout da top bar (larguras naturais, encolhe proporcionalmente
// se não couberem; o gear fica SEMPRE ancorado à direita)
TopBarLayout topbarLayout(f32 sw, f32 sh, const safe::Insets& in);

// resultado do frame da top bar
struct TopBarActions {
    bool menuDropdown = false;
    bool cenaDropdown = false;
    bool playPressed  = false;
    bool pausePressed = false;
    bool slidersPressed = false;   // abre o popover de snap/grelha 🔶
    bool gearPressed  = false;     // abre a página de Settings (spec I)
};

// desenha a top bar COMPLETA e processa os toques (nada de mutação de
// overlays aqui — só reporta; o main decide)
TopBarActions drawTopBar(UiContext& ui, const EditorState& st);

// ---- TAB BAR DE MODO (48dp, largura total) ------------------------------------
struct ModeTabsLayout {
    UiRect bar{};                  // faixa 48dp por baixo da top bar
    UiRect tab3d{}, tabUi{}, tabAudio{};
    UiRect underline{};            // do tab ATIVO (2dp accent — spec D)
    u32    active = 0;             // 0=3D, 1=UI, 2=ÁUDIO
};

ModeTabsLayout modetabsLayout(f32 sw, f32 sh, const safe::Insets& in,
                              bool uiMode, bool audioMode);

// desenha a tab bar e MUDA os modos (st.uiMode/st.audioMode exclusivos).
// Devolve true se o modo mudou neste frame.
bool drawModeTabs(UiContext& ui, EditorState& st);

// ---- compat 0.8.x: a API antiga de UM draw() — agora compõe top bar + tabs --
// (mantida para o main não migrar tudo de uma vez; O G4 morreu — a toolbar
// de transformação vive no viewport, ui/ViewportChrome.h)
struct Actions {
    bool menuDropdown = false;
    bool cenaDropdown = false;
    bool playPressed  = false;
    bool pausePressed = false;
    bool slidersPressed = false;
    bool gearPressed  = false;
    bool modeChanged  = false;
};

Actions draw(UiContext& ui, EditorState& st);

} // namespace toolbar
} // namespace editor
} // namespace vv
