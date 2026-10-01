#pragma once
// ui/SafeArea.h — matemática PURA da safe-area (F4.2), sem GL — host-testável.
//
// O bug do C33 (B1): a UI assumia que a superfície EGL inteira era
// desenhável, mas as barras do sistema (status/nav) TAPAM parte dela. O
// Inspector media contentHeight 536 contra um visibleHeight INFLADO que
// incluía a faixa da nav bar → maxOffset 0 → o scroll nunca ativava e o
// fundo do painel (BodyComp / velx / add TouchControls) ficava atrás da
// nav bar, inatingível.
//
// Fix raiz: ler android_app->contentRect (APP_CMD_CONTENT_RECT_CHANGED) e
// INSETIR TODO o layout da UI (toolbar, painéis, viewport, status line,
// toast, controlos de toque, overlays) por essa área. Com a altura REAL do
// painel, o overflow do Inspector é detetado e o scroll ativa. Nada de UI é
// desenhado fora do contentRect (o pass 3D continua fullscreen — é fundo,
// não UI; zero render 3D nesta fase).
//
// Este header é a FONTE ÚNICA das constantes de layout F1 (UiContext e
// EditorUi re-exportam os nomes antigos para compat com o código existente).
#include "core/Types.h"
#include "ui/ScrollMath.h"   // UiRect (GL-free)

namespace vv {
namespace safe {

// Alturas fixas da F1 (eram do UiContext) e largura dos painéis (era do
// EditorUi) — agora num só sítio, partilhadas por desenho e testes.
constexpr f32 kToolbarH = 88.0f;
constexpr f32 kStatusH  = 40.0f;
constexpr f32 kPanelW   = 300.0f;

// Distância de cada borda da superfície EGL até à área desenhável
// (contentRect do NativeActivity), em px.
struct Insets {
    f32 left   = 0.0f;
    f32 top    = 0.0f;
    f32 right  = 0.0f;
    f32 bottom = 0.0f;
};

// Converte o contentRect (ARect do android_native_app_glue) em Insets.
//   surfaceW/H   — tamanho da superfície EGL (g_egl.width/height)
//   cL/cT/cR/cB  — contentRect.left/top/right/bottom
// Regras: rect vazio/degenerado (o glue começa a zero e nem toda a ROM o
// envia) → sem insets = comportamento antigo; valores negativos ou além da
// superfície → clamp a 0 (nada de inset negativo).
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

// ---- rects do layout F1 (todos DENTRO do contentRect) ----------------------
inline UiRect toolbarRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top, sw - i.left - i.right, kToolbarH};
}
inline UiRect statusRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, sh - i.bottom - kStatusH, sw - i.left - i.right, kStatusH};
}
// viewport lógico (entre toolbar e status) — pai dos painéis e dos overlays
inline UiRect viewportRect(f32 sw, f32 sh, const Insets& i) {
    return {i.left, i.top + kToolbarH, sw - i.left - i.right,
            sh - i.top - i.bottom - kToolbarH - kStatusH};
}

// ---- painéis do editor (F3) -------------------------------------------------
inline UiRect hierarchyPanelRect(f32 sw, f32 sh, const Insets& i) {
    const UiRect vp = viewportRect(sw, sh, i);
    return {vp.x, vp.y, kPanelW, vp.h};
}
inline UiRect inspectorPanelRect(f32 sw, f32 sh, const Insets& i) {
    const UiRect vp = viewportRect(sw, sh, i);
    return {vp.x + vp.w - kPanelW, vp.y, kPanelW, vp.h};
}
// viewport central — gate da câmara: gestos atrás das barras NÃO orbitam
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i) {
    const UiRect vp = viewportRect(sw, sh, i);
    const f32 w = vp.w - 2.0f * kPanelW;
    return {vp.x + kPanelW, vp.y, w > 0.0f ? w : 0.0f, vp.h};
}
// 0.7.6 — sem o painel DIREITO (o G5 da toolbar escondeu o Inspector: a
// área dele junta-se ao viewport central — os gestos passam a orbitar aí e
// o mini-ecrã 2D cresce para a direita). O painel esquerdo fica SEMPRE.
inline UiRect centerRect(f32 sw, f32 sh, const Insets& i, bool rightPanel) {
    const UiRect vp = viewportRect(sw, sh, i);
    const f32 w = vp.w - kPanelW - (rightPanel ? kPanelW : 0.0f);
    return {vp.x + kPanelW, vp.y, w > 0.0f ? w : 0.0f, vp.h};
}

} // namespace safe
} // namespace vv
