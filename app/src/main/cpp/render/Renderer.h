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

    // 0.7.8 — FRONTEIRA EXPLÍCITA 3D→UI: o pass de UI deixou de HERDAR o
    // estado GL do pass 3D (no Play com câmara ativa o C33 via a UI de jogo
    // gigante/cortada — o viewport/scissor/depth vinham do que o LitMaterial
    // e o grid deixaram cair). Chamar DEPOIS do render 3D, ANTES dos widgets:
    // repõe o viewport CHEIO (com o tamanho ATUAL do ecrã, o mesmo que o
    // layout usa), desliga scissor/depth/cull (a UI nunca é ocluída nem
    // recortada) e refresca w_/h_ (a ortográfica de ecrã do endFrame fica
    // COERENTE com o viewport e com o resolver de layout do mesmo frame).
    void beginUiPass(i32 w, i32 h);
    void beginFrame();  // clear mono (BG #141414) + depth

    // pass 3D: desenha com depth test + backface cull; devolve métricas.
    // F5-E: tex opcional — albedo do material (nullptr = cinza F2)
    // 0.7.0: tint opcional — cor por TIC (nullptr = branco, o de sempre)
    // 0.8.2 (F7): bones opcional — matrizes de skin (uBones+uSkin; nullptr
    // = mesh estático, o caminho de sempre byte a byte)
    DrawStats drawMesh(const Mesh& mesh, const Mat4& model, const Mat4& vp,
                       const Texture* tex = nullptr, const f32* tint = nullptr,
                       const Mat4* bones = nullptr, u32 boneCount = 0);

    // pass UI (F1 mantido): submissões desenhadas em endFrame, sem depth
    void submit(const QuadBatch& batch, u32 texture);
    // 0.7.4 — submissão por RANGE de vértices (z-order sólidos↔texturas:
    // o UiContext submete RUNS na ordem real de emissão; um mesmo batch
    // pode aparecer em vários runs com ranges diferentes)
    void submit(const QuadBatch& batch, u32 texture, u32 firstVertex,
                u32 vertexCount);
    DrawStats endFrame();   // upload + draw das submissões (proj ortho)
    void shutdown();

    // hooks de TESTE (CI): nº de submissões aceites neste frame (o wiring
    // 0.8.4 garante runs + glifos SEMPRE aceites — ver test_wiring084)
    u32 submissionsForTest() const {
        return static_cast<u32>(subs_.size());
    }

    u32 whiteTexture() const { return whiteTex_; }
    i32 width() const { return w_; }
    i32 height() const { return h_; }

    // F3: material lit partilhado — presets/serializer ligam MeshRenderer.material aqui.
    LitMaterial* litMaterial() { return &lit_; }

private:
    // pass UI (F1)
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
        u32 firstVertex;   // 0.7.4: range do batch (submissão por runs)
        u32 vertexCount;
    };
    // 0.8.4 (fix "funções que param" do C33): o cap FIXO de 32 descartava
    // submissões em silêncio — e como os glifos são SEMPRE a última, com
    // 32 runs o TEXTO INTEIRO saía do ecrã. Agora o armazenamento é
    // dinâmico (reserve 64 = zero realloc em steady state) e NADA é
    // descartado; kSubWarn é um LIMIAR DE AVISO (log 1× por frame quando o
    // frame passa — sintoma de overlay a desenhar demasiado, nunca perda).
    static constexpr u32 kSubWarn = 66;   // kMaxRuns(64) + glifos + folga
    std::vector<Submission> subs_;
    bool droppedLogged_ = false;   // aviso de threshold 1× por frame
};

} // namespace vv
