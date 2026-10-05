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
#include "ui/Theme.h"       // 0.9.6.1 (PASSO 0): dp() — as medidas dp daqui
                            // multiplicam pela densidade AO CALCULAR o rect
                            // (R-018: eram px crus — barra a meia altura no
                            // C33). Testes/harness: densidade 1.0 = igual

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
// GRUPO D (0.9.6.7 — ORÇAMENTO DO EDITOR 3D): os painéis laterais deixaram
// de ser 300dp FIXOS — a largura é ESTADO (divisores arrastáveis, o padrão
// da pega do drawer). kPanelW é o DEFAULT (ecrãs largos ficam IGUAIS).
// O ORÇAMENTO é uma GANGorra de TRÊS (hierarquia | viewport | inspector):
//   • kViewportMinW = 288dp — a TOOLBAR do viewport (272dp + margens) é o
//     piso: o 3D nunca fecha abaixo do que a barra de toque precisa;
//   • kHierMinW = 200dp — o piso da hierarquia (pesquisa/linhas usáveis);
//   • kInspMinW = 272dp — o piso do inspector (a linha X/Y/Z com caixas
//     de 56dp: 184+8+48 = 240dp úteis + paddings);
//   • os DEFAULTS são ASSIMÉTRICOS no aperto: o INSPECTOR mantém o
//     kPanelW (a linha X/Y/Z é o conteúdo mais rígido — as caixas
//     ADAPTAM a 56dp e o R ao lado do título quando a largura não dá) e
//     a HIERARQUIA absorve o resto (os nomes truncam com tip do
//     long-press). No device de 776dp: hier 200 | viewport 288 | insp 288
//     — em vez do viewport de 176dp (22% do ecrã) de antes. O drag de
//     cada divisor clampa contra a largura EFETIVA do outro painel (a
//     gangorra nunca fecha o viewport).
constexpr f32 kPanelW      = 300.0f;  // painéis esquerdo/direito (DEFAULT)
constexpr f32 kHierMinW    = 200.0f;  // piso do drag: hierarquia
constexpr f32 kInspMinW    = 272.0f;  // piso do drag: inspector (X/Y/Z 56)
constexpr f32 kViewportMinW = 288.0f; // piso do viewport (a toolbar 272+margens)
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

// barra de cima (56dp) — [Menu][Cena] · [pause][play][sliders] · [gear]
inline UiRect toolbarRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top, sw - i.left - i.right, theme::dp(kTopBarH)};
}
// tab bar de modo (48, logo por baixo da barra de cima)
inline UiRect modeTabRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top + theme::dp(kTopBarH), sw - i.left - i.right, theme::dp(kModeTabH)};
}
// faixa do chrome de cima (56+48) — os painéis começam DEBAIXO dela
inline UiRect topChromeRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top, sw - i.left - i.right, theme::dp(kToolbarH)};
}
// tab bar do painel de baixo (48, sempre visível — abre/fecha o drawer)
inline UiRect bottomTabRect(f32 sw, f32 sh, const Insets& i) {
    const f32 y = sh - i.bottom - theme::dp(kStatusH) - theme::dp(kBottomTabH);
    return {i.left, y, sw - i.left - i.right, theme::dp(kBottomTabH)};
}
// status line (24 — a última faixa do contentRect)
inline UiRect statusRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, sh - i.bottom - theme::dp(kStatusH), sw - i.left - i.right,
            theme::dp(kStatusH)};
}
// viewport lógico (entre o chrome de cima e a tab bar de baixo) — pai dos
// painéis e do viewport central; o DRAWER come DENTRO dele (por baixo)
inline UiRect viewportRect(f32 sw, f32 sh, const Insets& i) {
    const f32 y = i.top + theme::dp(kToolbarH);
    const f32 h = sh - i.top - i.bottom - theme::dp(kToolbarH) - theme::dp(kStatusH) -
                  theme::dp(kBottomTabH);
    return {i.left, y, sw - i.left - i.right, h > 0.0f ? h : 0.0f};
}

// ---- painéis do editor -----------------------------------------------------
// drawerH = altura do drawer ABERTO (0 = fechado); os painéis laterais e o
// viewport central ENCOLHEM pelo drawer (o drawer é full-width, spec E).
//
// GRUPO D — A RESOLUÇÃO ÚNICA DAS LARGURAS: resolvePanels() é a fonte que
// o draw dos painéis, o centerRect (scissor/orbit/chrome), o drag dos
// divisores e os testes partilham. Recebe o ESTADO cru (−1 = default) e
// devolve as larguras EFETIVAS em px — com a gangorra dos três pisos.
struct PanelBudget {
    f32 hier = 0.0f;   // px efetivos da hierarquia
    f32 insp = 0.0f;   // px efetivos do inspector
    f32 contentW = 0.0f;
};
inline PanelBudget resolvePanels(f32 contentW, f32 hierRaw, f32 inspRaw) {
    PanelBudget b;
    b.contentW = contentW;
    const f32 def = theme::dp(kPanelW);
    const f32 hMin = theme::dp(kHierMinW);
    const f32 iMin = theme::dp(kInspMinW);
    const f32 vpMin = theme::dp(kViewportMinW);
    // (1) o default do INSPECTOR (mantém o kPanelW; cede só ao impossível)
    f32 id = def;
    f32 cap = contentW - hMin - vpMin;
    if (id > cap) {
        id = cap < iMin ? iMin : cap;
    }
    // (2) o default da HIERARQUIA absorve o resto (o inspector manda)
    f32 hd = def;
    cap = contentW - id - vpMin;
    if (hd > cap) {
        hd = cap < hMin ? hMin : cap;
    }
    // (3) o ESTADO (drag dos divisores) clampa contra o OUTRO painel:
    // primeiro a hierarquia (contra o inspector RAW-ou-default), depois o
    // inspector (contra a hierarquia JÁ resolvida) — o piso do viewport
    // nunca cede por ordem de aplicação
    if (hierRaw >= 0.0f) {
        f32 other = id;
        if (inspRaw >= 0.0f) {
            other = inspRaw < iMin ? iMin : inspRaw;
        }
        f32 mx = contentW - other - vpMin;
        if (mx < hMin) {
            mx = hMin;
        }
        hd = hierRaw < hMin ? hMin : (hierRaw > mx ? mx : hierRaw);
    }
    if (inspRaw >= 0.0f) {
        f32 mx = contentW - hd - vpMin;
        if (mx < iMin) {
            mx = iMin;
        }
        id = inspRaw < iMin ? iMin : (inspRaw > mx ? mx : inspRaw);
    }
    b.hier = hd;
    b.insp = id;
    return b;
}
inline UiRect panelsRect(f32 sw, f32 sh, const Insets& i, f32 drawerH) {
    const UiRect vp = viewportRect(sw, sh, i);
    const f32 h = vp.h - drawerH;
    return {vp.x, vp.y, vp.w, h > 0.0f ? h : 0.0f};
}
// os rects dos painéis/viewport: recebem o ESTADO cru dos divisores
// (−1 = default) e resolvem pela fonte ÚNICA (resolvePanels)
inline UiRect hierarchyPanelRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                                 f32 hierRaw = -1.0f, f32 inspRaw = -1.0f) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw);
    return {p.x, p.y, b.hier, p.h};
}
inline UiRect inspectorPanelRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                                 f32 inspRaw = -1.0f, f32 hierRaw = -1.0f) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw);
    return {p.x + p.w - b.insp, p.y, b.insp, p.h};
}
// viewport central — gate da câmara: gestos atrás das barras NÃO orbitam
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                          f32 hierRaw, f32 inspRaw) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw);
    const f32 w = p.w - b.hier - b.insp;
    return {p.x + b.hier, p.y, w > 0.0f ? w : 0.0f, p.h};
}
// 0.7.6: sem o painel DIREITO (o Inspector escondeu: a área dele junta-se ao
// viewport central — os gestos passam a orbitar aí e o mini-ecrã 2D cresce)
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                          bool rightPanel) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const f32 w = p.w - theme::dp(kPanelW) - (rightPanel ? theme::dp(kPanelW) : 0.0f);
    return {p.x + theme::dp(kPanelW), p.y, w > 0.0f ? w : 0.0f, p.h};
}
// GRUPO D: a versão COMPLETA (larguras de ESTADO + painel direito opcional)
// — a que o EDITOR 3D usa (o scissor, o orbit e o chrome acompanham)
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                          bool rightPanel, f32 hierRaw, f32 inspRaw) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw);
    const f32 rw = rightPanel ? b.insp : 0.0f;
    const f32 w = p.w - b.hier - rw;
    return {p.x + b.hier, p.y, w > 0.0f ? w : 0.0f, p.h};
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
