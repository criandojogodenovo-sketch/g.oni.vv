#pragma once
// render/QuadBatch.h — batch de quads 2D (pos+uv+cor) para a UI.
// Vértices acumulados na CPU; o Renderer sobe e desenha em endFrame.
// Parte do PLACEHOLDER de render da F1 (ver Renderer.h).
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

    const QuadVertex* vertices() const { return verts_.data(); }
    u32    vertexCount() const { return static_cast<u32>(verts_.size()); }
    size_t vertexBytes() const { return verts_.size() * sizeof(QuadVertex); }
    bool   empty() const { return verts_.empty(); }

private:
    std::vector<QuadVertex> verts_;
};

} // namespace vv
