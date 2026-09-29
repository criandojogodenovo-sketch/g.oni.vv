#pragma once
// render/Renderer.h
//
// Pipeline de draw da F2: pass 3D (Mesh + LitMaterial, depth test + cull)
// seguido do pass UI (batch de quads da F1 por cima, sem depth test/write —
// a UI nunca é ocluída pelo mundo). O quad batch da F1 é consumidor da
// interface, não o núcleo dela — substitui o PLACEHOLDER da F1.
#include "core/Types.h"
#include "math/Math.h"
#include "render/DrawStats.h"
#include "render/Material.h"
#include "render/QuadBatch.h"

namespace vv {

class Mesh;
class Texture;

class Renderer {
public:
    bool init();        // UI (shader/VAO/VBO/whiteTex) + material lit
    void resize(i32 w, i32 h);
    void beginFrame();  // clear mono (BG #141414) + depth

    // pass 3D: desenha com depth test + backface cull; devolve métricas.
    // F5-E: tex opcional — albedo do material (nullptr = cinza F2)
    DrawStats drawMesh(const Mesh& mesh, const Mat4& model, const Mat4& vp,
                       const Texture* tex = nullptr);

    // pass UI (F1 mantido): submissões desenhadas em endFrame, sem depth
    void submit(const QuadBatch& batch, u32 texture);
    DrawStats endFrame();   // upload + draw das submissões (proj ortho)
    void shutdown();

    u32 whiteTexture() const { return whiteTex_; }
    i32 width() const { return w_; }
    i32 height() const { return h_; }

    // F3: material lit partilhado — presets/serializer ligam MeshRenderer.material aqui.
    LitMaterial* litMaterial() { return &lit_; }

private:
    // UI (F1)
    u32 prog_ = 0;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    u32 whiteTex_ = 0;
    i32 locProj_ = -1;
    i32 locTex_ = -1;
    // 3D (F2)
    LitMaterial lit_;

    i32 w_ = 0;
    i32 h_ = 0;

    struct Submission {
        const QuadBatch* batch;
        u32 tex;
    };
    Submission subs_[2];
    u32 subCount_ = 0;
};

} // namespace vv
