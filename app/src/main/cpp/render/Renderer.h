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
#include "render/SdfBatch.h"   // 0.12-D (D-1): o batch do shader SDF de widget
#include "ui/Glass.h"          // 0.12-D (D-2): o blur dual Kawase + a cache
#include <vector>

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
    // 0.11-M (FASE 2): props opcional — as PROPRIEDADES do material glTF
    // (alphaMode/twoSided/factors/transform UV do GTB2; nullptr = os
    // defaults de sempre = o comportamento byte a byte pré-FASE 2). O
    // estado GL (blend/depthMask/cull) segue as props e é RESTAURADO aqui
    // (o UI/grid setam os seus próprios — nunca estado a vingar).
    DrawStats drawMesh(const Mesh& mesh, const Mat4& model, const Mat4& vp,
                       const Texture* tex = nullptr, const f32* tint = nullptr,
                       const Mat4* bones = nullptr, u32 boneCount = 0,
                       const DrawProps* props = nullptr);

    // 0.11-W (3.5b) — o IRMÃO INSTANCIADO do drawMesh: N cópias da MESMA
    // geometria num draw (glDrawElementsInstanced). A matriz de CADA cópia
    // viaja no UBO "Instances" (uInst[i] × uModel no VS; uModel = IDENTIDADE
    // aqui — a instância leva TUDO, §3.5b.3). `instCount` ≤ 64 é o contrato
    // do chamador (o BlockMesh::drawInstancedGroup fatia o teto do GE8320).
    // O estado GL segue as props e é RESTAURADO como no drawMesh.
    DrawStats drawMeshInstanced(const Mesh& mesh, const Mat4* instMats,
                                u32 instCount, const Mat4& vp,
                                const Texture* tex = nullptr,
                                const f32* tint = nullptr,
                                const DrawProps* props = nullptr);

    // 0.12-D (D-2) — O VIDRO REAL: a captura da cena para textura a 1/4
    // de resolução + o BLUR DUAL KAWASE (ui/Glass.h) + o upload. A CACHE:
    // só recalcula quando glassInvalidate() foi chamada (o main invalida
    // quando o que está atrás muda — a órbita, a cena, o rect); devolve o
    // id da textura e o rect CAPTURADO em px de ecrã (y-down).
    u32 captureGlass(i32 x, i32 y, i32 w, i32 h);
    void glassInvalidate() { glassDirty_ = true; }
    // o estado da cache (os pins do vidro_cache aferem o contador)
    u32 glassCapturesForTest() const { return glassCaptures_; }
    bool glassDirtyForTest() const { return glassDirty_; }
    u32 glassTextureForTest() const { return glassTex_; }
    // o rect capturado (px de ecrã y-down; o FS e o stub derivam o UV)
    const f32* glassCapRectForTest() const { return glassCapRect_; }

    // pass UI (F1 mantido): submissões desenhadas em endFrame, sem depth
    void submit(const QuadBatch& batch, u32 texture);
    // 0.12-D (D-1): a submissão do PASS SDF (o shader único de widget) —
    // mesmo estado GL do pass UI (sem depth, blend SRC_ALPHA); os uniforms
    // do nível (Efeitos Alto/Médio/Baixo) setam-se UMA vez por frame
    // 0.12-D (D-2): `tex` != 0 liga o ramo uSdfTexOn (o VIDRO REAL — o
    // widget amostra a textura da cena borrada com a tinta escura)
    void submitSdf(const SdfBatch& batch, u32 firstVertex, u32 vertexCount,
                   u32 tex = 0, bool blendAdd = false);
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
    // 0.12-Z F3: o PROGRAMA da UI (a sonda do contexto — glIsProgram)
    u32 uiProgram() const { return prog_; }
    // 0.12-E (E-3 · FASE 3.4): a TEXTURA DE RUÍDO 2D das bordas ativas
    // (64×64, tileável, GL_REPEAT — o upload no init come o ui/Noise2D.h,
    // a FONTE ÚNICA da matemática; o emissor é o UiContext::fireBorder2D)
    u32 noiseTexture() const { return noiseTex_; }
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
    u32 noiseTex_ = 0;   // 0.12-E (E-3): a textura de ruído 2D das bordas
    i32 locProj_ = -1;
    i32 locTex_ = -1;
    // 0.12-D (D-1) — o PASS SDF (o 3.º programa): o shader único de widget
    u32 progSdf_ = 0;
    u32 vaoSdf_ = 0;
    u32 vboSdf_ = 0;
    i32 locSdfProj_ = -1;
    i32 locSdfBevel_ = -1;
    i32 locSdfSpecDepth_ = -1;
    i32 locSdfGlowW_ = -1;
    i32 locSdfShadowDy_ = -1;
    i32 locSdfShadowSoft_ = -1;
    i32 locSdfShadowA_ = -1;
    i32 locSdfSpecOn_ = -1;
    i32 locSdfGlowOn_ = -1;
    i32 locSdfSpecAdd_ = -1;
    i32 locSdfGlowAdd_ = -1;
    i32 locSdfAA_ = -1;
    // 0.12-D (D-2): o ramo do VIDRO REAL no mesmo shader (uSdfTexOn vem no
    // aShape.z por widget; a captura e a superfície em uniforms)
    i32 locSdfTex_ = -1;
    i32 locSdfCap_ = -1;      // uSdfCap (x,y,w,h do rect capturado, px)
    i32 locSdfSurfH_ = -1;    // uSdfSurfH (a altura da superfície — o flip y)
    // 0.12-D (D-3): o FOGO (o tempo + a rampa)
    i32 locSdfTime_ = -1;
    i32 locFire1_ = -1;
    i32 locFire3_ = -1;
    // 3D (F2)
    LitMaterial lit_;

    i32 w_ = 0;
    i32 h_ = 0;

    // 0.12-D (D-2): a CACHE do vidro (a textura borrada + o carimbo)
    u32 glassTex_ = 0;
    i32 glassW_ = 0, glassH_ = 0;      // as dims da textura (1/4)
    bool glassDirty_ = true;           // nasce suja (a 1.ª captura)
    u32 glassCaptures_ = 0;            // o contador (o pin vidro_cache)
    f32 glassCapRect_[4] = {0, 0, 0, 0};
    std::vector<u8> glassRead_;        // o palco do glReadPixels (full-res)
    std::vector<u8> glassHalf_;        // o 1/2
    std::vector<u8> glassQuarter_;     // o 1/4
    std::vector<u8> glassTmp_;         // o ping do kawase
    std::vector<u8> glassBlur_;        // o pong (o resultado)

    struct Submission {
        const QuadBatch* batch;        // prog 0 (quads UI)
        const SdfBatch* sdfBatch;      // 0.12-D: prog 1 (widgets SDF)
        u32 tex;
        u32 firstVertex;   // 0.7.4: range do batch (submissão por runs)
        u32 vertexCount;
        bool sdf;          // 0.12-D: a submissão é do programa SDF
        bool blendAdd = false;   // 0.12-D (D-3): o fogo em MISTURA ADITIVA
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
