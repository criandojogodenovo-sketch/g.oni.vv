#pragma once
// render/DrawStats.h — métricas mínimas de render por draw (F2, status line).
#include "core/Types.h"

namespace vv {

struct DrawStats {
    u32 vertices  = 0;   // vértices/índices processados pelo draw
    u32 drawCalls = 0;
};

inline DrawStats operator+(const DrawStats& a, const DrawStats& b) {
    return {a.vertices + b.vertices, a.drawCalls + b.drawCalls};
}

} // namespace vv
