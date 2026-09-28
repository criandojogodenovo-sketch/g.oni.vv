#pragma once
// render/Vertex.h — vértice do pipeline 3D (F2).
// Interleaved: pos(3f) + normal(3f) — stride 24 bytes.
#include "math/Math.h"

namespace vv {

struct Vertex {
    Vec3 pos;
    Vec3 normal;
};

} // namespace vv
