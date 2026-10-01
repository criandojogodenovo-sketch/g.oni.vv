#pragma once
// ui/Toolbar.h — BARRA SUPERIOR FINAL do editor (0.7.6): 5 grupos por função.
//
// PRINCÍPIOS (spec 0.7.6):
//   • texto só para identidade/ações raras → [Menu ▾][Cena ▾] são TEXTO;
//   • uso frequente = ÍCONES, nunca texto → playback/transformação/painéis
//     são os 8 ícones vetoriais (ui/Icons.h — polilinhas no line batch);
//   • agrupar por função com SEPARADOR VISUAL (linha fina vertical);
//   • estados mutuamente exclusivos = SEGMENTED CONTROL (3D|UI e
//     mover/rodar/escalar — fundo de marca no ativo);
//   • esconder o que não se aplica: o G4 (transformação) SÓ existe com
//     seleção ativa em 3D — some da barra, não fica cinzento;
//   • zero emoji.
//
// GRUPOS:
//   G1 sistema   [Menu ▾][Cena ▾]   — Menu→dropdown(Settings, Guardar, …,
//                                     Sair); Cena→dropdown(cenas do projeto)
//   G2 playback  [pause][play]      — ícones
//   G3 modo      [3D][UI]           — segmented (texto ok: 2 estados curtos)
//   G4 transf.   [mover][rodar][escalar][snap] — segmented, SÓ com seleção
//                                     (mover/rodar/escalar exclusivos; snap
//                                     é um TOGGLE do grupo — liga/desliga o
//                                     snapping do modo ativo)
//   G5 painéis   [inspector]        — mostra/esconde o painel direito
//
// LAYOUT DINÂMICO (stacks auto-size): a largura disponível reparte-se pelos
// grupos presentes; com/sem G4 nada sobrepõe nunca (o G5 fica ancorado à
// direita). O solver é PURO (layout()) — desenho e testes partilham-no.
//
// TEMA: a barra lê TUDO de theme::kTheme (ui/Theme.h) — fundo bg #0B0E13,
// ícones brand #8AB4F8, ativo = fundo brand + ícone/texto brandInk. Os
// PAINÉIS do editor continuam no tema mono de sempre (exceção documentada).
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

// ---- estado do gizmo (movido de EditorUi.h — a toolbar é o dono agora) ------
// mode: 0=Mover, 1=Rodar, 2=Escalar (gizmo::Mode); snap = toggle de snapping
struct GizmoModeState {
    int  mode = 0;
    bool snap = false;
};

// ---- ids dos widgets da barra (faixas exclusivas — UiContext é im-mode) -----
// Menu=1 / Play=2 / Pause=3 herdaram os números da toolbar antiga (Menu=1,
// Play=2, Settings=3 — o Settings deixou de ser botão próprio e o 3 passou
// ao Pause). 3D/UI = 11/12, gizmo = 7/8/9/10 (inalterados), inspector = 13,
// cena = 14.
constexpr u64 kTbMenuId      = 1;
constexpr u64 kTbPlayId      = 2;
constexpr u64 kTbPauseId     = 3;
constexpr u64 kGizmoIds[3]   = {7, 8, 9};    // mover/rodar/escalar
constexpr u64 kTbSnapId      = 10;
constexpr u64 kMode3dId      = 11;
constexpr u64 kModeUiId      = 12;
constexpr u64 kTbInspectId   = 13;
constexpr u64 kTbCenaId      = 14;

// ---- layout PURO (fonte única — desenho e testes) ----------------------------
struct Layout {
    UiRect bar{};                 // a faixa toda (safe::toolbarRect)
    UiRect menu{}, cena{};        // G1 (texto + caret)
    UiRect pause{}, play{};       // G2 (ícones)
    UiRect mode3d{}, modeUi{};    // G3 (segmented, texto)
    UiRect giz[4]{};              // G4 (ícones; válido só com g4Visible)
    bool   g4Visible = false;     // o grupo existe neste layout?
    UiRect inspector{};           // G5 (ícone, ancorado à direita)
    UiRect sep[4]{};              // separadores finos (1px) entre grupos
    u32    sepCount = 0;          // 4 com G4, 3 sem
    f32    iconSize = 32.0f;      // lado do ícone dentro do botão
    f32    sepGap   = 28.0f;      // vão entre grupos (inclui o separador)
};

// resolve o layout da barra. g4Visible = há grupo de transformação (seleção
// ativa em modo 3D). Larguras naturais; se não couberem, encolhem
// proporcionalmente (nada sobrepõe, nenhum grupo sai do ecrã).
Layout layout(f32 sw, f32 sh, const safe::Insets& in, bool g4Visible);

// ---- desenho + input -----------------------------------------------------------

// resultado do frame (o chamador decide o que fazer; nada de mutação de
// overlays aqui dentro — a toolbar só reporta)
struct Actions {
    bool menuDropdown = false;     // G1 Menu clicado
    bool cenaDropdown = false;     // G1 Cena clicado
    bool playPressed  = false;     // G2 play
    bool pausePressed = false;     // G2 pause
};

// desenha a barra COMPLETA e processa os toques. Muta:
//   st.uiMode (G3 — ao sair do UI limpa a seleção de elemento, como o
//              separador antigo fazia), st.showInspector (G5),
//              gz.mode/gz.snap (G4).
// hasSelection = há TIC selecionado (o G4 só aparece com seleção E modo 3D).
Actions draw(UiContext& ui, EditorState& st, GizmoModeState& gz,
             bool hasSelection);

} // namespace toolbar
} // namespace editor
} // namespace vv
