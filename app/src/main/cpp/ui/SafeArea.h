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
// CONSTANTES (PASSO 2, 0.9.6.15): painéis POR PERCENTAGEM do dono —
// hierarquia 18% (piso 140) · inspector 22% (piso 180/teto 260) · o
// viewport central fica ≥55% nos ecrãs de referência · SEM seleção o
// inspector colapsa ao TRILHO de 32dp · drawer: pega 160..400 MAS a
// altura efetiva ≤35% da altura do content (spec PASSO 2) · top bar 36 ·
// tab bar de baixo 32 (status 0 — REMOVIDA no PASSO 1) · A LEI DE OURO
// dos alvos: DESENHO 32dp / TOQUE 40dp — nada ≥48 no editor.
#include "core/Types.h"
#include <cmath>            // std::floor (effectiveDrawerH — passos de 8dp)
#include "ui/ScrollMath.h"   // UiRect (GL-free)
#include "ui/Theme.h"       // 0.9.6.1 (PASSO 0): dp() — as medidas dp daqui
                            // multiplicam pela densidade AO CALCULAR o rect
                            // (R-018: eram px crus — barra a meia altura no
                            // C33). Testes/harness: densidade 1.0 = igual

namespace vv {
namespace safe {

// ---- alturas/larguras do chrome (FONTES ÚNICAS) ----------------------------
// PASSO 1 (0.9.6.14 · spec UI do dono): a barra única de 36dp UMA linha
// (era 56 — os 20dp poupados vão TODOS ao viewport, o critério (c))
constexpr f32 kTopBarH   = 36.0f;   // barra de cima
// FASE 9 (G2-10 — A FUSÃO): a tab bar de modo fundiu-se à top bar
// (as tabs [3D|UI|ÁUDIO] vivem AO CENTRO da barra — ui/Toolbar.cpp)
constexpr f32 kToolbarH  = kTopBarH;           // 36
constexpr f32 kModeTabH  = 48.0f;   // LEGACY: só p/ modeTabRect (compat de testes)
// PASSO 1: a STATUS BAR 24dp desapareceu (a spec: «a status extra sai») —
// o «FPS n · TICs n» vive na tab bar de baixo (direita) e a versão/commit
// em Settings › Sobre. kStatusH fica a 0: statusRect devolve uma faixa
// ALTURA ZERO (compat de testes; nada desenha nela).
constexpr f32 kStatusH   = 0.0f;
// GRUPO D (0.9.6.7 — ORÇAMENTO DO EDITOR 3D): os painéis laterais são
// ESTADO (divisores arrastáveis, o padrão da pega do drawer); kPanelW era
// o default FIXO de 300. PASSO 2 (0.9.6.15 — spec do dono): os DEFAULTS
// são POR PERCENTAGEM da largura do content —
//   • HIERARQUIA 18% com piso kHierMinW = 140dp;
//   • INSPECTOR 22% com piso kInspMinW = 180dp e TETO kInspMaxW = 260dp;
//   • o VIEWPORT CENTRAL fica ≥55% nos ecrãs de referência (776/800/1536)
//     — 18+22 = 40% deixam 60%; os pisos só falam em ecrãs < ~711dp.
//   • SEM seleção o inspector colapsa ao TRILHO kInspTrackW = 32dp (a
//     área junta-se ao viewport; volta ao selecionar um TIC — a regra é
//     do dono, spec PASSO 2).
// A GANGORRA mantém-se no DRAG: kViewportMinW = 288dp (a TOOLBAR do
// viewport 272dp + margens é o piso do 3D), o drag de cada divisor
// clampa contra a largura EFETIVA do outro painel — o viewport nunca
// fecha abaixo do que a barra de toque precisa.
constexpr f32 kPanelW      = 300.0f;  // LEGACY (o default fixo da 0.9.0 —
                                      // wrappers compat 0.8.x ainda o citam)
constexpr f32 kHierPct     = 0.18f;   // PASSO 2: default da hierarquia
constexpr f32 kInspPct     = 0.22f;   // PASSO 2: default do inspector
constexpr f32 kHierMinW    = 140.0f;  // PASSO 2: piso hierarquia (era 200)
constexpr f32 kInspMinW    = 180.0f;  // PASSO 2: piso inspector (era 272)
constexpr f32 kInspMaxW    = 260.0f;  // PASSO 2: teto inspector (NOVO)
constexpr f32 kInspTrackW  = 32.0f;   // PASSO 2: o trilho SEM seleção
constexpr f32 kViewportMinW = 288.0f; // piso do viewport (a toolbar 272+margens)
constexpr f32 kBottomTabH = 32.0f;  // PASSO 1: tab bar de baixo (era 48); o
                                    // «FPS · TICs» vive na faixa da direita
constexpr f32 kDrawerDef  = 240.0f; // spec E: drawer default
constexpr f32 kDrawerMin  = 160.0f; // pega: 160..400 em passos de 8
constexpr f32 kDrawerMax  = 400.0f;
// P-08 (0.9.6.12 · GRUPO J1 · R-022) — O PISO DA ALTURA DO VIEWPORT
// CENTRAL com o drawer aberto. PASSO 1 (0.9.6.14): strip do topo (40) +
// toolbar do viewport (40) + folga (8) = 88dp (era 104 com strip/toolbar
// de 48). O cap do drawer usa ESTE piso. Fonte ÚNICA consumida pelo draw
// do drawer (bottom::layout) e pelos rects do centro (currentDrawerH) —
// as duas medidas JÁ NUNCA divergem.
constexpr f32 kViewportMinH = 88.0f;

// PASSO 2 (0.9.6.15 — spec do dono): o drawer ABERTO ocupa NO MÁXIMO
// kDrawerMaxPct da ALTURA do content (35%). O cap vive AQUI (a fonte
// única) — a pega continua a escrever 160..400 no ESTADO cru, mas o que
// desenha e o que come o viewport é o EFETIVO.
constexpr f32 kDrawerMaxPct = 0.35f;

// P-08 (GRUPO J1 · R-022) — A ALTURA EFETIVA DO DRAWER (a fonte ÚNICA).
// Ordem: clamp da pega (160..400) → o CAP DUPLO pelo piso do viewport
// central E pelo teto de 35% da altura do content (o MENOR manda —
// kViewportMinH garante a toolbar/strip, kDrawerMaxPct garante a spec do
// PASSO 2) → passos de 8dp.
// vpH = safe::viewportRect(sw, sh, in).h (a altura do viewport lógico);
// contentH = a altura do CONTENTRECT em px (sh − in.top − in.bottom) —
// o 35% é da ALTURA QUE O EDITOR VÊ (a superfície menos os insets).
inline f32 effectiveDrawerH(f32 rawDrawerH, f32 vpH, f32 contentH) {
    f32 d = rawDrawerH;
    if (d < theme::dp(kDrawerMin)) {
        d = theme::dp(kDrawerMin);
    }
    if (d > theme::dp(kDrawerMax)) {
        d = theme::dp(kDrawerMax);
    }
    f32 cap = vpH - theme::dp(kViewportMinH);
    if (contentH > 0.0f) {
        const f32 cap35 = contentH * kDrawerMaxPct;
        if (cap35 < cap) {
            cap = cap35;
        }
    }
    const f32 capPos = cap > 0.0f ? cap : 0.0f;
    if (d > capPos) {
        d = capPos;
    }
    if (d <= 0.0f) {
        return 0.0f;
    }
    return std::floor(d / theme::dp(8.0f)) * theme::dp(8.0f);
}

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

// barra de cima (36dp — PASSO 1) — [G][Menu][Cena] · tabs · [▶][⏸][■][⚙]
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
// tab bar do painel de baixo (32 — PASSO 1; sempre visível, o «FPS · TICs»
// à direita; AGORA é a ÚLTIMA faixa do contentRect — a status saiu)
inline UiRect bottomTabRect(f32 sw, f32 sh, const Insets& i) {
    const f32 y = sh - i.bottom - theme::dp(kStatusH) - theme::dp(kBottomTabH);
    return {i.left, y, sw - i.left - i.right, theme::dp(kBottomTabH)};
}
// LEGACY PASSO 1: a status bar saiu — faixa de ALTURA ZERO no fundo (os
// chamadores/testes antigos continuam a compilar; nada desenha nela)
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
// devolve as larguras EFETIVAS em px.
// PASSO 2 (0.9.6.15): os DEFAULTS são 18% (hierarquia, piso 140) e 22%
// (inspector, piso 180 TETO 260); inspTrack=true colapsa o inspector ao
// TRILHO de 32dp (sem seleção — a spec do dono) e a área vai ao viewport.
struct PanelBudget {
    f32 hier = 0.0f;   // px efetivos da hierarquia
    f32 insp = 0.0f;   // px efetivos do inspector
    f32 contentW = 0.0f;
};
inline PanelBudget resolvePanels(f32 contentW, f32 hierRaw, f32 inspRaw,
                                 bool inspTrack = false) {
    PanelBudget b;
    b.contentW = contentW;
    const f32 hMin = theme::dp(kHierMinW);
    const f32 iMin = theme::dp(kInspMinW);
    const f32 iMax = theme::dp(kInspMaxW);
    const f32 vpMin = theme::dp(kViewportMinW);
    // (1) o default do INSPECTOR: 22% afervado ao intervalo [180..260]
    f32 id = contentW * kInspPct;
    id = id < iMin ? iMin : (id > iMax ? iMax : id);
    // (2) o default da HIERARQUIA: 18% com piso 140 — e o viewport manda
    // no teto (a toolbar do 3D cabe SEMPRE, como na gangorra do Grupo D)
    f32 hd = contentW * kHierPct;
    {
        const f32 cap = contentW - id - vpMin;
        const f32 hMax = cap > hMin ? cap : hMin;
        hd = hd < hMin ? hMin : (hd > hMax ? hMax : hd);
    }
    // (3) o ESTADO (drag dos divisores) clampa contra o OUTRO painel:
    // primeiro a hierarquia (contra o inspector RAW-ou-default), depois o
    // inspector (contra a hierarquia JÁ resolvida; AGORA também ao teto
    // 260 da spec PASSO 2) — o piso do viewport nunca cede
    if (hierRaw >= 0.0f) {
        f32 other = id;
        if (inspRaw >= 0.0f) {
            other = inspRaw < iMin ? iMin : (inspRaw > iMax ? iMax : inspRaw);
        }
        f32 mx = contentW - other - vpMin;
        if (mx < hMin) {
            mx = hMin;
        }
        hd = hierRaw < hMin ? hMin : (hierRaw > mx ? mx : hierRaw);
    }
    if (inspRaw >= 0.0f) {
        f32 mx = contentW - hd - vpMin;
        if (mx > iMax) {
            mx = iMax;
        }
        if (mx < iMin) {
            mx = iMin;
        }
        id = inspRaw < iMin ? iMin : (inspRaw > mx ? mx : inspRaw);
    }
    // (4) o TRILHO (PASSO 2): sem seleção o inspector É 32dp — manda sobre
    // tudo o que está acima (o estado do drag inclui)
    if (inspTrack) {
        id = theme::dp(kInspTrackW);
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
// (−1 = default) e resolvem pela fonte ÚNICA (resolvePanels); inspTrack
// (PASSO 2) colapsa o inspector ao trilho de 32dp
inline UiRect hierarchyPanelRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                                 f32 hierRaw = -1.0f, f32 inspRaw = -1.0f,
                                 bool inspTrack = false) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw, inspTrack);
    return {p.x, p.y, b.hier, p.h};
}
inline UiRect inspectorPanelRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                                 f32 inspRaw = -1.0f, f32 hierRaw = -1.0f,
                                 bool inspTrack = false) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw, inspTrack);
    return {p.x + p.w - b.insp, p.y, b.insp, p.h};
}
// viewport central — gate da câmara: gestos atrás das barras NÃO orbitam
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                          f32 hierRaw, f32 inspRaw, bool inspTrack = false) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw, inspTrack);
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
// — a que o EDITOR 3D usa (o scissor, o orbit e o chrome acompanham);
// PASSO 2: inspTrack colapsa o inspector ao trilho (a área vai ao viewport)
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, f32 drawerH,
                          bool rightPanel, f32 hierRaw, f32 inspRaw,
                          bool inspTrack = false) {
    const UiRect p = panelsRect(sw, sh, i, drawerH);
    const PanelBudget b = resolvePanels(p.w, hierRaw, inspRaw, inspTrack);
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
