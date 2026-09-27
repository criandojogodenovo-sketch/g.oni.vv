#pragma once
// render/Renderer.h
//
// PLACEHOLDER (F1) — NÃO É IMPLEMENTAÇÃO FINAL.
// Motivo: estabelecer a interface de draw (beginFrame/submit/endFrame + batch
// de quads) sem fechar o pipeline 3D. Sem meshes 3D, sem luzes, sem materiais.
// Substituição na F2: pipeline Vertex/Mesh/Material — o batch de quads passa
// a ser um consumidor da nova interface, não o núcleo dela.
#include "core/Types.h"
#include "render/QuadBatch.h"

namespace vv {

class Renderer {
public:
    bool init();        // shaders, VAO/VBO, textura branca 1x1
    void resize(i32 w, i32 h);
    void beginFrame();  // clear mono (BG #141414)
    void submit(const QuadBatch& batch, u32 texture);
    void endFrame();    // upload + draw das submissões (proj ortho da UI)
    void shutdown();

    u32 whiteTexture() const { return whiteTex_; }
    i32 width() const { return w_; }
    i32 height() const { return h_; }

private:
    u32 prog_ = 0;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    u32 whiteTex_ = 0;
    i32 locProj_ = -1;
    i32 locTex_ = -1;
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
