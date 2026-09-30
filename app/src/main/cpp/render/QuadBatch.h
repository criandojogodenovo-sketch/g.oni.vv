#pragma once
// render/QuadBatch.h — batch de quads 2D (pos+uv+cor) para a UI.
// Vértices acumulados na CPU; o Renderer sobe e desenha em endFrame.
// Parte do PLACEHOLDER de render da F1 (ver Renderer.h).
//
// 0.6.9: line() — SEGMENTO de ecrã com espessura (retângulo ROTACIONADO
// por 6 vértices) para os gizmos de transformação (eixos/anéis projetados
// no espaço de ecrã). O quad comum continua axis-aligned (UI).
#include <cmath>
#include <vector>
#include "core/Types.h"

namespace vv {

struct QuadVertex {
    f32 x, y;        // posição em pixels (origem topo-esquerda)
    f32 u, v;        // uv no atlas
    f32 r, g, b, a;  // cor do vértice
};

class QuadBatch {
public:
    void reserve(size_t quads) { verts_.reserve(quads * 6); }
    void clear() { verts_.clear(); }

    void quad(f32 x, f32 y, f32 w, f32 h,
              f32 u0, f32 v0, f32 u1, f32 v1,
              f32 r, f32 g, f32 b, f32 a) {
        const QuadVertex v[6] = {
            {x,     y,     u0, v0, r, g, b, a},
            {x,     y + h, u0, v1, r, g, b, a},
            {x + w, y + h, u1, v1, r, g, b, a},
            {x,     y,     u0, v0, r, g, b, a},
            {x + w, y + h, u1, v1, r, g, b, a},
            {x + w, y,     u1, v0, r, g, b, a},
        };
        verts_.insert(verts_.end(), v, v + 6);
    }

    // 0.6.9 — segmento (x0,y0)→(x1,y1) com espessura em px: retângulo
    // ROTACIONADO alinhado à direção do segmento (normal = perpendicular
    // normalizada × meia-espessura). Degenerado (len < ε) = não emite.
    // SEM clip (chamado fora de regiões de scroll — os gizmos vivem no
    // viewport central, onde o clip_ é infinito).
    void line(f32 x0, f32 y0, f32 x1, f32 y1, f32 thickness,
              f32 r, f32 g, f32 b, f32 a) {
        const f32 dx = x1 - x0;
        const f32 dy = y1 - y0;
        const f32 len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-5f) {
            return;
        }
        const f32 half = thickness * 0.5f;
        const f32 nx = -dy / len * half;
        const f32 ny =  dx / len * half;
        const QuadVertex v[6] = {
            {x0 + nx, y0 + ny, 0.0f, 0.0f, r, g, b, a},
            {x0 - nx, y0 - ny, 0.0f, 1.0f, r, g, b, a},
            {x1 - nx, y1 - ny, 1.0f, 1.0f, r, g, b, a},
            {x0 + nx, y0 + ny, 0.0f, 0.0f, r, g, b, a},
            {x1 - nx, y1 - ny, 1.0f, 1.0f, r, g, b, a},
            {x1 + nx, y1 + ny, 1.0f, 0.0f, r, g, b, a},
        };
        verts_.insert(verts_.end(), v, v + 6);
    }

    const QuadVertex* vertices() const { return verts_.data(); }
    u32    vertexCount() const { return static_cast<u32>(verts_.size()); }
    size_t vertexBytes() const { return verts_.size() * sizeof(QuadVertex); }
    bool   empty() const { return verts_.empty(); }

private:
    std::vector<QuadVertex> verts_;
};

} // namespace vv
