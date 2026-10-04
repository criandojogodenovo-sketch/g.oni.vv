#pragma once
// ui/SafeArea.h — matemática PURA da safe-area (F4.2), sem GL — host-testável.
//
// 0.9.0 — LAYOUT DO EDITOR (spec D/E, mockups do autor):
//   ┌──────────────────────────────────────────────────────────────┬─────┐
//   │ TOP BAR 56dp  [Menu ≡][Cena ▾]      [pause][play][sliders] [gear]│ bg │
//   │ TAB BAR 48dp  [3D][UI][ÁUDIO] (ícone+palavra, underline 2dp) │     │
//   ├─────────┬──────────────────────────────────────┬─────────────┤     │
//   │ HIERARQ │            VIEWPORT 3D               │  INSPECTOR  │surface
//   │ (panel) │  [undo/redo/save/dup/paste] vertical │   (panel)   │     │
//   │         │  triad 64dp sup-dir                  │             │     │
//   │         │  [Sel][Mov][Rod][Esc] [snap] [+]     │             │     │
//   ├─────────┴──────────────────────────────────────┴─────────────┤     │
//   │ TAB BAR 48dp [Ficheiros][Consola][Animação] + DRAWER (0..400)│     │
//   ├──────────────────────────────────────────────────────────────┤     │
//   │ STATUS 24dp  FPS 60 · TICs 4                                  │ bg │
//   └──────────────────────────────────────────────────────────────┴─────┘
//
// CONSTANTES (spec A/E/G): top bar 56 · tab bars 48 · status 24 · drawer
// default 240 (pega 160–400, passos de 8) · painéis 300 · alvos ≥48.
#include "core/Types.h"
#include "ui/ScrollMath.h"   // UiRect (GL-free)

namespace vv {
namespace safe {

// ---- alturas/larguras do chrome (FONTES ÚNICAS) ----------------------------
constexpr f32 kTopBarH   = 56.0f;   // spec D: barra de cima
// FASE 9 (G2-10 — A FUSÃO): a tab bar de modo (48dp) FUNDEU-SE à top bar
// (as tabs [3D|UI|ÁUDIO] vivem AO CENTRO da barra de 56dp — ui/Toolbar.cpp);
// kToolbarH passa a 56 — os ~48px poupados vão TODOS ao viewport
constexpr f32 kToolbarH  = kTopBarH;           // 56 (era 56+48=104)
constexpr f32 kModeTabH  = 48.0f;   // LEGACY: só p/ modeTabRect (compat de testes)
constexpr f32 kStatusH   = 24.0f;   // spec E: FPS 60 · TICs 4 (12sp text-2)
constexpr f32 kPanelW    = 300.0f;  // painéis esquerdo/direito
constexpr f32 kBottomTabH = 48.0f;  // spec E: tab bar do painel de baixo
constexpr f32 kDrawerDef  = 240.0f; // spec E: drawer default
constexpr f32 kDrawerMin  = 160.0f; // pega: 160..400 em passos de 8
constexpr f32 kDrawerMax  = 400.0f;

// Distância de cada borda da superfície EGL até à área desenhável
// (contentRect do NativeActivity), em px.
struct Insets {
    f32 left   = 0.0f;
    f32 top    = 0.0f;
    f32 right  = 0.0f;
    f32 bottom = 0.0f;
};

// Converte o contentRect (ARect do android_native_app_glue) em Insets.
inline Insets insetsFromContentRect(f32 surfaceW, f32 surfaceH,
                                    i32 cL, i32 cT, i32 cR, i32 cB) {
    Insets in;
    if (surfaceW <= 0.0f || surfaceH <= 0.0f) {
        return in;
    }
    if (cR <= cL || cB <= cT) {
        return in;   // rect zero/inválido → sem informação de safe-area
    }
    in.left  = cL > 0 ? static_cast<f32>(cL) : 0.0f;
    in.top   = cT > 0 ? static_cast<f32>(cT) : 0.0f;
    const f32 r = surfaceW - static_cast<f32>(cR);
    const f32 b = surfaceH - static_cast<f32>(cB);
    in.right  = r > 0.0f ? r : 0.0f;
    in.bottom = b > 0.0f ? b : 0.0f;
    return in;
}

// O contentRect em si (superfície menos insets) — para os testes.
inline UiRect contentRect(f32 sw, f32 sh, const Insets& i) {
    const f32 w = sw - i.left - i.right;
    const f32 h = sh - i.top - i.bottom;
    return {i.left, i.top, w > 0.0f ? w : 0.0f, h > 0.0f ? h : 0.0f};
}

// inner totalmente contido em outer (tolerância 0.01 px para flutuantes) —
// a INVARIANTE central da fase: nenhum rect de UI sai do contentRect.
inline bool rectInside(const UiRect& inner, const UiRect& outer,
                       f32 eps = 0.01f) {
    return inner.x >= outer.x - eps &&
           inner.y >= outer.y - eps &&
           inner.x + inner.w <= outer.x + outer.w + eps &&
           inner.y + inner.h <= outer.y + outer.h + eps;
}

// ---- rects do layout 0.9.0 (todos DENTRO do contentRect) --------------------

// barra de cima (56) — [Menu][Cena] · [pause][play][sliders] · [gear]
inline UiRect toolbarRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top, sw - i.left - i.right, kTopBarH};
}
// tab bar de modo (48, logo por baixo da barra de cima)
inline UiRect modeTabRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top + kTopBarH, sw - i.left - i.right, kModeTabH};
}
// faixa do chrome de cima (56+48) — os painéis começam DEBAIXO dela
inline UiRect topChromeRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top, sw - i.left - i.right, kToolbarH};
}
// tab bar do painel de baixo (48, sempre visível — abre/fecha o drawer)
inline UiRect bottomTabRect(f32 sw, f32 sh, const Insets& i) {
    const f32 y = sh - i.bottom - kStatusH - kBottomTabH;
    return {i.left, y, sw - i.left - i.right, kBottomTabH};
}
// status line (24 — a última faixa do contentRect)
inline UiRect statusRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, sh - i.bottom - kStatusH, sw - i.left - i.right, kStatusH};
}
// viewport lógico (entre o chrome de cima e a tab bar de baixo) — pai dos
// painéis e do viewport central; o DRAWER come DENTRO dele (por baixo)
inline UiRect viewportRect(f32 sw, f32 sh, const Insets& i) {
    const f32 y = i.top + kToolbarH;
    const f32 h = sh - i.top - i.bottom - kToolbarH - kStatusH - kBottomTabH;
    return {i.left, y, sw - i.left - i.right, h > 0.0f ? h : 0.0f};
}

// ---- painéis do editor -----------------------------------------------------
// drawerH = altura do drawer ABERTO (0 = fechado); os painéis laterais e o
// viewport central ENCOLHEM pelo drawer (o drawer é full-width, spec E)
inline UiRect panelsRect(f32 sw, f32 sh, const Insets& i, f32 drawerH) {
    const UiRect vp = viewportRect(sw, sh, i);
    const f32 h = vp.h - drawerH;
    return {vp.x, vp.y, vp.w, h > 0.0f ? h : 0.0f};
}
inline UiRect hierarchyPanelRect(f32 sw, f32 sh, const Insets& i, f32 drawerH) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    return {p.x, p.y, kPanelW, p.h};
}
inline UiRect inspectorPanelRect(f32 sw, f32 sh, const Insets& i, f32 drawerH) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    return {p.x + p.w - kPanelW, p.y, kPanelW, p.h};
}
// viewport central — gate da câmara: gestos atrás das barras NÃO orbitam
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const f32 w = p.w - 2.0f * kPanelW;
    return {p.x + kPanelW, p.y, w > 0.0f ? w : 0.0f, p.h};
}
// 0.7.6: sem o painel DIREITO (o Inspector escondeu: a área dele junta-se ao
// viewport central — os gestos passam a orbitar aí e o mini-ecrã 2D cresce)
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                          bool rightPanel) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const f32 w = p.w - kPanelW - (rightPanel ? kPanelW : 0.0f);
    return {p.x + kPanelW, p.y, w > 0.0f ? w : 0.0f, p.h};
}
// compat 0.8.x: as assinaturas de sempre (drawer fechado) — os callers antigos
// e os testes herdaram-nas; wrappers explícitos para não os partir
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i) {
    return centerRect(sw, sh, i, 0.0f, true);
}
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, bool rightPanel) {
    return centerRect(sw, sh, i, 0.0f, rightPanel);
}
inline UiRect hierarchyPanelRect(f32 sw, f32 sh, const Insets& i) {
    return hierarchyPanelRect(sw, sh, i, 0.0f);
}
inline UiRect inspectorPanelRect(f32 sw, f32 sh, const Insets& i) {
    return inspectorPanelRect(sw, sh, i, 0.0f);
}
inline UiRect viewportRect(f32 sw, f32 sh, const Insets& i, f32 /*drawerH*/) {
    return viewportRect(sw, sh, i);
}

} // namespace safe
} // namespace vv
