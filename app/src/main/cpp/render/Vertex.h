#pragma once
// render/Vertex.h — vértice do pipeline 3D (F2; uv entra na F5).
// Interleaved: pos(3f) + normal(3f) + uv(2f) — stride 32 bytes.
// uv é usado pelo Material quando o MeshRenderer tem textura (F5-E); os
// importadores OBJ/glTF preenchem a partir de vt / TEXCOORD_0.
#include "math/Math.h"

namespace vv {

struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv{0.0f, 0.0f};
};

} // namespace vv
