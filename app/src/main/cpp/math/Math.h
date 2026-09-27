#pragma once
// math/Math.h — mínimo necessário na F1: projeção ortográfica da UI.
// Sem vetores/matrizes 3D, física ou interpolação nesta fase (CLÁUSULA CALMA).
#include "core/Types.h"

namespace vv {

// Matriz 4x4 column-major (layout compatível com glUniformMatrix4fv sem transpose).
struct Mat4 {
    f32 m[16];

    static Mat4 ortho(f32 left, f32 right, f32 bottom, f32 top, f32 zNear, f32 zFar) {
        Mat4 r{};
        const f32 rl = right - left;
        const f32 tb = top - bottom;
        const f32 fn = zFar - zNear;
        r.m[0]  = 2.0f / rl;
        r.m[1]  = 0.0f;
        r.m[2]  = 0.0f;
        r.m[3]  = 0.0f;
        r.m[4]  = 0.0f;
        r.m[5]  = 2.0f / tb;
        r.m[6]  = 0.0f;
        r.m[7]  = 0.0f;
        r.m[8]  = 0.0f;
        r.m[9]  = 0.0f;
        r.m[10] = -2.0f / fn;
        r.m[11] = 0.0f;
        r.m[12] = -(right + left) / rl;
        r.m[13] = -(top + bottom) / tb;
        r.m[14] = -(zFar + zNear) / fn;
        r.m[15] = 1.0f;
        return r;
    }
};

} // namespace vv
