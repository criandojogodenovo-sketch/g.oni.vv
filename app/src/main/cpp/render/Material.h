#pragma once
// render/Material.h — material mínimo do pipeline 3D (F2).
// Shader lit simples: difusa direcional FIXA no shader + ambient.
// PLACEHOLDER DE LUZ — luzes configuráveis são uma fase própria pós-F4.
// F5-E: o material ACEITA uma textura (albedo) opcional — quando presente,
// o shader multiplica o albedo pelo texel (uv do vértice, F5). Sem
// textura, o comportamento F2 é mantido byte a byte.
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class Texture;   // render/Texture.h (F5-D) — recurso GL não-dono

class LitMaterial {
public:
    bool init();      // compila/linka o programa, cacheia uniforms
    void destroy();

    void use() const;                 // glUseProgram
    void setVP(const Mat4& vp) const;
    void setModel(const Mat4& model) const;
    // F5-E: liga a textura albedo (unit 0) e ativa uHasTex; nullptr desativa
    void setTexture(const Texture* tex) const;

    bool ok() const { return prog_ != 0; }

private:
    u32 prog_ = 0;
    i32 locVP_ = -1;
    i32 locModel_ = -1;
    i32 locTex_ = -1;
    i32 locHasTex_ = -1;
};

// F3: alias pedido pela spec — MeshRenderer guarda `Material*`. A F3 tem um
// único material (o lit da F2, intacto); quando surgir o segundo material
// (fase de luzes), Material vira base comum e o alias é removido.
using Material = LitMaterial;

} // namespace vv
