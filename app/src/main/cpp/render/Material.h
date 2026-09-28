#pragma once
// render/Material.h — material mínimo do pipeline 3D (F2).
// Shader lit simples: difusa direcional FIXA no shader + ambient.
// PLACEHOLDER DE LUZ — luzes configuráveis são uma fase própria pós-F4.
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class LitMaterial {
public:
    bool init();      // compila/linka o programa, cacheia uniforms
    void destroy();

    void use() const;                 // glUseProgram
    void setVP(const Mat4& vp) const;
    void setModel(const Mat4& model) const;

    bool ok() const { return prog_ != 0; }

private:
    u32 prog_ = 0;
    i32 locVP_ = -1;
    i32 locModel_ = -1;
};

} // namespace vv
