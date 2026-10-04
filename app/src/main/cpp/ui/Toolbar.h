#pragma once
// ui/Toolbar.h — CHROME DE CIMA (FASE 9 G2-9/G2-10: UMA BARRA SÓ de 56dp):
//
//   [≡ Menu][Cena ▾]    [3D][UI][ÁUDIO]    [pause][play][gear]
//
// FASE 9 (G2-10 — o mock do dono): a MENU BAR (56dp) e a TAB BAR de modo
// (48dp) eram DUAS faixas = 104dp de chrome; AGORA é UMA barra de 56dp —
// os ~48px poupados vão TODOS para o viewport (safe::kToolbarH = 56). As
// tabs [3D|UI|ÁUDIO] vivem AO CENTRO da mesma barra com o UNDERLINE accent
// 2dp NO FUNDO da barra (o indicador do modo ativo).
//
// FASE 9 (G2-9 — inventário G0-4): o botão [sliders] REMOVIDO — estava
// MORTO desde a 0.9.0 (kTbSlidersId → slidersPressed NUNCA consumido; o
// id 16 fica LIVRE). O snap vive no ÍMAN da toolbar do viewport (G1-1).
//
// O que SAI da barra (0.7.6→0.9.0): o grupo G4 de transformação desce para
// a TOOLBAR DO VIEWPORT (bottom — ui/ViewportChrome.h); o toggle do
// Inspector vira PEGA de recolher no próprio painel (spec G); o gear abre
// a PÁGINA de Settings (spec I).
//
// TEMA: tudo lido de theme::kTheme (ui/Theme.h — tabela spec A): fundo bg,
// ícones text1, ativo = underline accent 2dp + ícone/palavra accent (tabs)
// ou fill surface2 (botões premidos). Zero emoji.
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
// viewport — spec D: o íman é um controlo LÁ, não um botão da barra)
struct GizmoModeState {
    int  mode = 0;    // 0=Mover, 1=Rodar, 2=Escalar (gizmo::Mode)
    bool snap = false;
};

// ---- ids dos widgets (faixas exclusivas — UiContext é im-mode) -------------
// Herdados: Menu=1, Play=2, Pause=3, 3D/UI=11/12, cena=14, ÁUDIO=15.
// kTbSlidersId (16) REMOVIDO na FASE 9 (G0-4/G2-9: o botão estava MORTO —
// slidersPressed nunca era consumido). O id fica LIVRE.
constexpr u64 kTbMenuId      = 1;
constexpr u64 kTbPlayId      = 2;
constexpr u64 kTbPauseId     = 3;
constexpr u64 kGizmoIds[3]   = {7, 8, 9};    // mover/rodar/escalar (toolbar do viewport 0.9.0)
constexpr u64 kTbSnapId      = 10;            // (legacy: o snap vive agora no íman do viewport)
constexpr u64 kMode3dId      = 11;
constexpr u64 kModeUiId      = 12;
constexpr u64 kTbInspectId   = 13;            // (legacy: pega do painel — spec G)
constexpr u64 kTbCenaId      = 14;
constexpr u64 kModeAudioId   = 15;
constexpr u64 kTbGearId      = 17;   // 0.9.0: PÁGINA de Settings (spec I)

// ---- A BARRA ÚNICA (56dp — FASE 9 G2-10) --------------------------------------
struct TopBarLayout {
    UiRect bar{};                 // a faixa toda (safe::toolbarRect — 56dp)
    UiRect menu{}, cena{};        // esquerda (ícone hamburger/folder + texto)
    UiRect tab3d{}, tabUi{}, tabAudio{};   // CENTRO — tabs de modo (G2-10)
    UiRect underline{};           // do tab ATIVO (2dp accent, fundo da barra)
    u32    active = 0;            // 0=3D, 1=UI, 2=ÁUDIO
    UiRect pause{}, play{};       // direita (ícones)
    UiRect gear{};                // extrema direita (ancorado)
    f32    iconSize = 24.0f;      // 24dp dentro dos alvos 48dp (spec A)
};

// resolve o layout da barra única (tabs ao centro, gear SEMPRE à direita,
// encolhe gracioso em ecrãs estreitos)
TopBarLayout topbarLayout(f32 sw, f32 sh, const safe::Insets& in,
                          bool uiMode, bool audioMode);

// resultado do frame da barra única
struct TopBarActions {
    bool menuDropdown = false;
    bool cenaDropdown = false;
    bool playPressed  = false;
    bool pausePressed = false;
    bool gearPressed  = false;    // abre a página de Settings (spec I)
    bool modeChanged  = false;    // uma das tabs [3D|UI|ÁUDIO] mudou o modo
};

// desenha a barra única COMPLETA (menu/cena + tabs + pause/play/gear) e
// processa os toques — MUDA st.uiMode/st.audioMode nas tabs (exclusivos)
TopBarActions drawTopBar(UiContext& ui, EditorState& st);

// ---- compat 0.8.x/0.9.x: a API de UM draw() — AGORA é a barra única --------
// (mantida para o main/tests; drawModeTabs/modetabsLayout MORRERAM na
// fusão G2-10 — as tabs vivem DENTRO da top bar)
struct Actions {
    bool menuDropdown = false;
    bool cenaDropdown = false;
    bool playPressed  = false;
    bool pausePressed = false;
    bool gearPressed  = false;
    bool modeChanged  = false;
};

Actions draw(UiContext& ui, EditorState& st);

} // namespace toolbar
} // namespace editor
} // namespace vv
