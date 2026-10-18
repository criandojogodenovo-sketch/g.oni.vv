#pragma once
// ui/ScrollMath.h — matemática PURA do scroll (F4.1), sem GL — host-testável.
//
// O primitivo: uma região do ecrã mostra um conteúdo mais alto (contentHeight
// > region.h) deslocado por um offset em [0, contentHeight − region.h].
// Interação (1 dedo, slot 0 — igual aos restantes widgets):
//   • press edge DENTRO da região e nenhum widget reclamou → scroll reclama;
//   • drag vertical → offset = startOffset + (startY − y) (arrasto natural);
//   • release com deslocação < kTapPx → foi TAP (re-despachado ao painel);
//   • clamp severo nos dois extremos — nunca passa do fim.
//
// Protocolo de conflito com os widgets (mesmo active_ partilhado):
//   • SLIDER tem prioridade: reclama o gesto mesmo dentro da região (drag
//     horizontal num slider = slider — regra da spec);
//   • BUTTON dentro de região só DESENHA (não reclama): o painel re-despacha
//     o tap via hit-test do próprio rect — assim "drag em qualquer sítio =
//     scroll" e "tap num item/botão = ação" coexistem;
//   • fora de região os botões comportam-se como sempre (toolbar, overlays).
#include "core/Types.h"
#include <cmath>

namespace vv {

// Rect de UI (era do UiContext.h — movido para cá para a matemática ser GL-free)
struct UiRect {
    f32 x, y, w, h;
};

namespace scroll {

// limiar tap-vs-drag (px). Abaixo = tap (re-despacha); acima = drag de scroll.
constexpr f32 kTapPx = 12.0f;

// estado persistente de UMA região (guardado pelo UiContext num slot por id)
struct State {
    f32 offset      = 0.0f;   // deslocação atual do conteúdo
    f32 startX      = 0.0f;   // pos do dedo no beginDrag
    f32 startY      = 0.0f;
    f32 startOffset = 0.0f;   // offset no beginDrag
    f32 maxMove     = 0.0f;   // maior deslocação do dedo (tap-vs-drag)
    bool active     = false;  // gesto de scroll em curso
};

// máximo offset útil (conteúdo mais baixo que a região → 0)
inline f32 maxOffset(f32 contentH, f32 visibleH) {
    const f32 m = contentH - visibleH;
    return m > 0.0f ? m : 0.0f;
}

// offset clampado a [0, max] — usado no drag E para sanear offsets stale
// (conteúdo encolheu: TIC apagado, componente removido)
inline f32 clampOffset(f32 offset, f32 contentH, f32 visibleH) {
    const f32 m = maxOffset(contentH, visibleH);
    if (offset < 0.0f) return 0.0f;
    return offset > m ? m : offset;
}

inline bool inside(const UiRect& r, f32 x, f32 y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// 0.7.4 — interseção de dois rects (clip composto). Rects que não se
// intersetam devolvem um rect DEGENERADO (w/h ≤ 0) — o clipQuad recusa
// quads dentro dele (nada desenha fora da região de clip).
inline UiRect intersectRects(const UiRect& a, const UiRect& b) {
    const f32 x0 = a.x > b.x ? a.x : b.x;
    const f32 y0 = a.y > b.y ? a.y : b.y;
    const f32 x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    const f32 y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x0 >= x1 || y0 >= y1) {
        return {0.0f, 0.0f, 0.0f, 0.0f};   // degenerado: recusa tudo
    }
    return {x0, y0, x1 - x0, y1 - y0};
}

// ---- protocolo de claim (codificado para os testes) -----------------------
// O scroll reclama o press edge se nasceu dentro da região e NENHUM widget
// reclamou antes (slider tem prioridade; botões dentro de região não reclamam).
inline bool scrollClaims(bool pressEdgeInsideRegion, bool widgetClaimed) {
    return pressEdgeInsideRegion && !widgetClaimed;
}
// Botões dentro de região: só desenho — o tap é re-despachado pelo painel.
inline bool buttonCaptures(bool inScrollRegion) { return !inScrollRegion; }
// Sliders: capturam sempre (prioridade da spec, dentro ou fora de região).
inline bool sliderCaptures(bool /*inScrollRegion*/) { return true; }

// ---- gesto -----------------------------------------------------------------
inline void beginDrag(State& s, f32 px, f32 py) {
    s.active      = true;
    s.startX      = px;
    s.startY      = py;
    s.startOffset = s.offset;
    s.maxMove     = 0.0f;
}

// arrasto: offset segue o dedo (arrasto natural — puxar para baixo revela o
// topo), clampado; maxMove acumula para o veredicto tap-vs-drag
inline void dragTo(State& s, f32 px, f32 py, f32 contentH, f32 visibleH) {
    if (!s.active) {
        return;
    }
    s.offset = clampOffset(s.startOffset + (s.startY - py), contentH, visibleH);
    const f32 dx = std::fabs(px - s.startX);
    const f32 dy = std::fabs(py - s.startY);
    s.maxMove = dx > dy ? dx : dy;
}

// o gesto acabou de ser um tap (sem arrasto acima do limiar)?
inline bool isTap(const State& s) { return s.active && s.maxMove < kTapPx; }

// termina o gesto: devolve true se foi tap; desativa sempre o estado
inline bool endDrag(State& s) {
    if (!s.active) {
        return false;
    }
    const bool tap = isTap(s);
    s.active = false;
    return tap;
}

// ---- clip de quads (recorte por interseção; UV proporcional) ---------------
struct Clipped {
    f32 x, y, w, h;
    f32 u0, v0, u1, v1;
};

// intersecta o quad (x,y,w,h) com o rect de clip; false = totalmente fora.
// UV interpolado pela fração visível — glifos cortados no topo/fundo do painel
// aparecem meio desenhados em vez de sangrarem fora da região.
inline bool clipQuad(f32 x, f32 y, f32 w, f32 h,
                     f32 u0, f32 v0, f32 u1, f32 v1,
                     const UiRect& clip, Clipped& out) {
    if (w <= 0.0f || h <= 0.0f) {
        return false;
    }
    const f32 x0 = x,               y0 = y;
    const f32 x1 = x + w,           y1 = y + h;
    const f32 cx0 = clip.x,         cy0 = clip.y;
    const f32 cx1 = clip.x + clip.w, cy1 = clip.y + clip.h;
    const f32 nx0 = x0 > cx0 ? x0 : cx0;
    const f32 ny0 = y0 > cy0 ? y0 : cy0;
    const f32 nx1 = x1 < cx1 ? x1 : cx1;
    const f32 ny1 = y1 < cy1 ? y1 : cy1;
    if (nx0 >= nx1 || ny0 >= ny1) {
        return false;
    }
    out.x  = nx0;
    out.y  = ny0;
    out.w  = nx1 - nx0;
    out.h  = ny1 - ny0;
    out.u0 = u0 + (u1 - u0) * ((nx0 - x0) / w);
    out.u1 = u0 + (u1 - u0) * ((nx1 - x0) / w);
    out.v0 = v0 + (v1 - v0) * ((ny0 - y0) / h);
    out.v1 = v0 + (v1 - v0) * ((ny1 - y0) / h);
    return true;
}

// ---- indicador discreto (tema mono: barra fina à direita) ------------------
// geometria do polegar: altura proporcional ao rácio visível (mín 24 px),
// posição proporcional ao offset; desenhado só quando há overflow.
inline void indicator(const UiRect& region, f32 contentH, f32 offset,
                      f32& bx, f32& by, f32& bw, f32& bh) {
    const f32 visibleH = region.h;
    const f32 mo = maxOffset(contentH, visibleH);
    f32 thumbH = visibleH * visibleH / (contentH > 1.0f ? contentH : 1.0f);
    if (thumbH < 24.0f) thumbH = 24.0f;
    if (thumbH > visibleH) thumbH = visibleH;
    const f32 t = mo > 0.0f ? offset / mo : 0.0f;
    bx = region.x + region.w - 4.0f;
    by = region.y + t * (visibleH - thumbH);
    bw = 3.0f;
    bh = thumbH;
}

} // namespace scroll
} // namespace vv
