#include "render/Renderer.h"
#include "render/Mesh.h"
#include "math/Math.h"
#include "platform/Log.h"
#include "ui/Theme.h"   // 0.9.6.9: o clear lê o token bg (o vidro expõe o fundo)
#include "ui/Sdf.h"     // 0.12-D (D-1): os parâmetros do nível (o toggle)
#include <GLES3/gl3.h>
#include <vector>

namespace vv {

namespace {

// Shader do batch de quads da UI (F1, mantido).
constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform mat4 uProj;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
})";

constexpr char kFsSrc[] = R"(#version 300 es
precision mediump float;
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 outColor;
void main() {
    outColor = vColor * texture(uTex, vUV).r;
})";

// 0.12-D (D-1) — o VS do PASS SDF (o shader único de widget): os atributos
// viajam (aLocal/aShape/aColTop/aColBot); o FS desenha TUDO.
constexpr char kSdfVsSrc[] = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aLocal;
layout(location = 2) in vec4 aShape;
layout(location = 3) in vec4 aColTop;
layout(location = 4) in vec4 aColBot;
uniform mat4 uProj;
out vec4 vLocal;
out vec4 vShape;
out vec4 vTop;
out vec4 vBot;
void main() {
    vLocal = aLocal;
    vShape = aShape;
    vTop = aColTop;
    vBot = aColBot;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
})";

// ===== A RÉPLICA GLSL de ui/Sdf.h::shade() — LINHA A LINHA =================
// (GLSL não inclui C++; a fonte única vive em ui/Sdf.h — alterar EM AMBOS.
//  Os pins do stub (test_012d_sdf.cpp) aferem os MESMOS pixéis via
//  ui/Sdf.h; o C33 do dono prova o device.)
constexpr char kSdfFsSrc[] = R"(#version 300 es
precision mediump float;
in vec4 vLocal;   // lx, ly (locais ao topo-esq) + hw, hh
in vec4 vShape;   // radius, level (reservado D-3), _, _
in vec4 vTop;
in vec4 vBot;
uniform float uSdfBevel;
uniform float uSdfSpecDepth;
uniform float uSdfGlowW;
uniform float uSdfShadowDy;
uniform float uSdfShadowSoft;
uniform float uSdfShadowA;
uniform float uSdfSpecOn;
uniform float uSdfGlowOn;
uniform float uSdfSpecAdd;
uniform float uSdfGlowAdd;
uniform float uSdfAA;
// 0.12-D (D-2): o ramo do VIDRO REAL — o widget amostra a textura da cena
// borrada (a captura 1/4 + o blur dual Kawase) com a TINTA ESCURA
uniform sampler2D uSdfTex;
uniform vec4 uSdfCap;    // o rect capturado (x,y,w,h em px de ecrã y-down)
uniform float uSdfSurfH; // a altura da superfície (o flip do fragCoord)
out vec4 outColor;
float sdfClamp01(float x) { return clamp(x, 0.0, 1.0); }
float roundRectSdf(vec2 p, vec2 b, float r) {
    float rr = min(r, min(b.x, b.y));
    if (rr <= 0.0) {
        vec2 d = abs(p) - b;
        vec2 o = max(d, vec2(0.0));
        return (o.x > 0.0 || o.y > 0.0) ? length(o) : max(d.x, d.y);
    }
    vec2 q = abs(p) - b + rr;
    vec2 a = max(q, vec2(0.0));
    return length(a) + min(max(q.x, q.y), 0.0) - rr;
}
void roundRectNormal(vec2 p, vec2 b, float r, out vec2 n) {
    float rr = min(r, min(b.x, b.y));
    vec2 q = abs(p) - b + rr;
    if (q.x > 0.0 || q.y > 0.0) {
        vec2 a = max(q, vec2(0.0));
        float l = length(a);
        n = l > 1e-6 ? a / l : vec2(1.0, 0.0);
        if (p.x < 0.0) n.x = -n.x;
        if (p.y < 0.0) n.y = -n.y;
    } else if (q.x > q.y) {
        n = vec2(p.x >= 0.0 ? 1.0 : -1.0, 0.0);
    } else {
        n = vec2(0.0, p.y >= 0.0 ? 1.0 : -1.0);
    }
}
void main() {
    // vLocal = (lx, ly LOCAIS ao topo-esq, hw, hh) — o SDF é CENTER-RELATIVE
    vec2 p = vLocal.xy - vLocal.zw;
    vec2 b = vLocal.zw;
    float r = vShape.x;
    float aa = uSdfAA > 1e-4 ? uSdfAA : 1.0;
    float d = roundRectSdf(p, b, r);
    float aW = sdfClamp01(0.5 - d / aa);
    float aS = 0.0;
    if (uSdfShadowA > 0.0) {
        float dS = roundRectSdf(p - vec2(0.0, uSdfShadowDy), b, r);
        float soft = uSdfShadowSoft > 1e-4 ? uSdfShadowSoft : aa;
        aS = uSdfShadowA * sdfClamp01(0.5 - dS / soft) * (1.0 - aW);
    }
    float aOut = sdfClamp01(aW + aS);
    if (aOut <= 0.002) { outColor = vec4(0.0); return; }
    // a cor base: o GRADIENTE vertical dos vértices (a tinta do widget) —
    // ou a CENA BORRADA (o vidro real: o ramo uSdfTexOn do aShape.z, o
    // UV em espaço de ECRÃ sobre o rect capturado)
    float t = b.y > 1e-4 ? sdfClamp01((p.y + b.y) / (2.0 * b.y)) : 0.0;
    vec3 rgb = mix(vTop.rgb, vBot.rgb, t);
    if (vShape.z > 0.5) {
        float sy = uSdfSurfH - gl_FragCoord.y;
        vec2 suv = vec2((gl_FragCoord.x - uSdfCap.x) / max(uSdfCap.z, 1.0),
                        (sy - uSdfCap.y) / max(uSdfCap.w, 1.0));
        vec3 scene = texture(uSdfTex, suv).rgb;
        // a TINTA ESCURA: a cena borrada × a tinta; o α da tinta mistura
        // com a cor plana (o MESMO produto do rasterizador do stub)
        rgb = mix(vTop.rgb, scene * vTop.rgb, sdfClamp01(vTop.a));
    }
    vec2 n = vec2(0.0, -1.0);
    if (uSdfBevel > 0.0 || uSdfSpecOn > 0.5) {
        roundRectNormal(p, b, r, n);
    }
    if (uSdfBevel > 0.0 && aW > 0.0) {
        float lam = sdfClamp01(-0.5534 * n.x - 0.8329 * n.y);
        float edge = sdfClamp01(1.0 + d / max(uSdfBevel, 1e-4));
        rgb *= 1.0 + (lam - 0.5) * 0.9 * edge;   // topo +30%, fundo −45%
    }
    if (uSdfSpecOn > 0.5) {
        float up = sdfClamp01(-n.y);
        float band = sdfClamp01(1.0 + d / max(uSdfSpecDepth, 1e-4));
        rgb += uSdfSpecAdd * band * band * up * up;
    }
    if (uSdfGlowOn > 0.5) {
        float depth = -d - uSdfBevel;
        float ring = exp(-max(depth, 0.0) / max(uSdfGlowW, 1e-4));
        float ga = uSdfGlowAdd * ring;
        rgb += (1.0 - rgb) * 0.6 * ga;
    }
    outColor = vec4(rgb * aW / aOut, aOut);
})";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOGE("Renderer: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

bool Renderer::init() {
    // 0.8.4: armazenamento dinâmico das submissões (reserve = zero realloc
    // em steady state; o frame anómalo cresce SEM cortar nada — só loga)
    subs_.clear();
    subs_.reserve(kSubWarn);

    // ---- pass UI (F1)
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVsSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFsSrc);
    if (!vs || !fs) {
        return false;
    }
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);
    glLinkProgram(prog_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512];
        glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
        LOGE("Renderer: falha ao linkar programa: %s", log);
        return false;
    }
    locProj_ = glGetUniformLocation(prog_, "uProj");
    locTex_ = glGetUniformLocation(prog_, "uTex");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // QuadVertex: x,y,u,v,r,g,b,a — 8 floats, stride 32
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex),
                          reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex),
                          reinterpret_cast<const void*>(2 * sizeof(f32)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(QuadVertex),
                          reinterpret_cast<const void*>(4 * sizeof(f32)));
    glBindVertexArray(0);

    // ---- pass SDF (0.12-D D-1) — o 3.º programa da engine
    const GLuint sdfVs = compileShader(GL_VERTEX_SHADER, kSdfVsSrc);
    const GLuint sdfFs = compileShader(GL_FRAGMENT_SHADER, kSdfFsSrc);
    if (!sdfVs || !sdfFs) {
        return false;
    }
    progSdf_ = glCreateProgram();
    glAttachShader(progSdf_, sdfVs);
    glAttachShader(progSdf_, sdfFs);
    glLinkProgram(progSdf_);
    glDeleteShader(sdfVs);
    glDeleteShader(sdfFs);
    ok = GL_FALSE;
    glGetProgramiv(progSdf_, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512];
        glGetProgramInfoLog(progSdf_, sizeof(log), nullptr, log);
        LOGE("Renderer: falha ao linkar o programa SDF: %s", log);
        return false;
    }
    locSdfProj_ = glGetUniformLocation(progSdf_, "uProj");
    locSdfBevel_ = glGetUniformLocation(progSdf_, "uSdfBevel");
    locSdfSpecDepth_ = glGetUniformLocation(progSdf_, "uSdfSpecDepth");
    locSdfGlowW_ = glGetUniformLocation(progSdf_, "uSdfGlowW");
    locSdfShadowDy_ = glGetUniformLocation(progSdf_, "uSdfShadowDy");
    locSdfShadowSoft_ = glGetUniformLocation(progSdf_, "uSdfShadowSoft");
    locSdfShadowA_ = glGetUniformLocation(progSdf_, "uSdfShadowA");
    locSdfSpecOn_ = glGetUniformLocation(progSdf_, "uSdfSpecOn");
    locSdfGlowOn_ = glGetUniformLocation(progSdf_, "uSdfGlowOn");
    locSdfSpecAdd_ = glGetUniformLocation(progSdf_, "uSdfSpecAdd");
    locSdfGlowAdd_ = glGetUniformLocation(progSdf_, "uSdfGlowAdd");
    locSdfAA_ = glGetUniformLocation(progSdf_, "uSdfAA");
    locSdfTex_ = glGetUniformLocation(progSdf_, "uSdfTex");
    locSdfCap_ = glGetUniformLocation(progSdf_, "uSdfCap");
    locSdfSurfH_ = glGetUniformLocation(progSdf_, "uSdfSurfH");

    glGenVertexArrays(1, &vaoSdf_);
    glGenBuffers(1, &vboSdf_);
    glBindVertexArray(vaoSdf_);
    glBindBuffer(GL_ARRAY_BUFFER, vboSdf_);
    // SdfVertex: 20 floats, stride 80 — loc0 pos2, loc1 local4, loc2 shape4,
    // loc3 top4, loc4 bot4
    constexpr GLsizei kSdfStride = static_cast<GLsizei>(sizeof(SdfVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, kSdfStride,
                          reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, kSdfStride,
                          reinterpret_cast<const void*>(2 * sizeof(f32)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, kSdfStride,
                          reinterpret_cast<const void*>(6 * sizeof(f32)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, kSdfStride,
                          reinterpret_cast<const void*>(10 * sizeof(f32)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, kSdfStride,
                          reinterpret_cast<const void*>(14 * sizeof(f32)));
    glBindVertexArray(0);

    // Textura branca 1x1: quads sólidos passam pelo mesmo shader (texel = 1).
    glGenTextures(1, &whiteTex_);
    glBindTexture(GL_TEXTURE_2D, whiteTex_);
    const u8 white = 0xFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // ---- pass 3D (F2)
    if (!lit_.init()) {
        LOGE("Renderer: material lit falhou");
        return false;
    }

    LOGI("Renderer: pipeline 3D (Mesh/LitMaterial) + batch de quads UI + "
         "pass SDF de widget prontos");
    return true;
}

void Renderer::resize(i32 w, i32 h) {
    w_ = w;
    h_ = h;
    if (w_ > 0 && h_ > 0) {
        glViewport(0, 0, w_, h_);
    }
}

// 0.7.8 — fronteira 3D→UI (ver Renderer.h). Idempotente e BARATA: quatro
// chamadas de estado por frame; NENHUMA submissão. O tamanho vem do frame
// ATUAL (o main lê do EGL fresco) — viewport, ortográfica e resolver de
// layout ficam os três coerentes entre si, venha o estado de onde vier
// (pass 3D da câmara de jogo, grid, re-criação de superfície, driver).
void Renderer::beginUiPass(i32 w, i32 h) {
    if (w > 0 && h > 0) {
        w_ = w;
        h_ = h;
    }
    glViewport(0, 0, w_, h_);       // viewport CHEIO (nunca o do pass 3D)
    glDisable(GL_SCISSOR_TEST);     // UI nunca recortada por scissor velho
    glDisable(GL_DEPTH_TEST);       // UI por cima — nunca ocluída pelo 3D
    glDisable(GL_CULL_FACE);        // winding y-down do quad batch
}

void Renderer::beginFrame() {
    droppedLogged_ = false;   // 0.8.4: o aviso de cap vale 1× por frame
    // 0.9.6.9 (GRUPO F): o clear LÊ O TOKEN (era o literal #141414 — o BG DA
    // F1! — que sobreviveu à spec A por TUDO ser opaco: ninguém via o fundo.
    // COM VIDRO os painéis deixam-no À VISTA: o fundo por trás do vidro é
    // PARTE da identidade e tem de vir da tabela única; com o mono de volta
    // o valor É o mesmo #141414, mas agora por CONTRATO, não por fóssil)
    glClearColor(theme::kTheme.bg[0], theme::kTheme.bg[1],
                 theme::kTheme.bg[2], 1.0f);
    glDepthMask(GL_TRUE);                                     // restore pós-grid
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

DrawStats Renderer::drawMesh(const Mesh& mesh, const Mat4& model, const Mat4& vp,
                             const Texture* tex, const f32* tint,
                             const Mat4* bones, u32 boneCount,
                             const DrawProps* props) {
    if (!mesh.ok()) {
        return {};
    }
    lit_.use();
    lit_.setVP(vp);
    lit_.setModel(model);
    // 0.11-W (3.5b): o ramo de instâncias EXPLICITAMENTE off no draw de
    // sempre (o contrato das uniforms da casa — o estado nunca vinga de um
    // draw instanciado anterior no MESMO programa)
    lit_.setInstancing(false);
    // 0.11-M (FASE 2): as props do material A CADA draw (nullptr = o
    // default de sempre — o byte a byte pré-FASE 2). ANTES do tex/tint/
    // skin: o ÚLTIMO glUniform1f/3f/1i do draw continua a ser o uHasTex/
    // uTint/uSkin (o contrato dos pins do glstub fica byte a byte — as
    // uniforms novas u4f/mat2/2f não têm leitor antes da FASE 2).
    lit_.setProps(props);
    lit_.setTexture(tex);   // F5-E: albedo opcional (unit 0 + uHasTex)
    lit_.setTint(tint);     // 0.7.0: cor por TIC (nullptr = branco, o de sempre)
    // 0.8.2 (F7): skin — uSkin+uBones quando há matrizes; SEMPRE definido
    // (uniforms nascem a 0 e 0 = estático, mas o default é EXPLÍCITO como
    // o uTint — nunca deixar estado do draw anterior vingar)
    const bool useSkin = mesh.skinned() && bones != nullptr && boneCount > 0;
    lit_.setSkin(useSkin);
    if (useSkin) {
        lit_.setBones(bones, boneCount);
    }
    // pass 3D: depth visível (faces frontais ocluem as traseiras) + cull
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    // 0.11-M (FASE 2): o ESTADO GL segue as props — BLEND liga o blend
    // clássico (o MESMO par do grid/UI) e desliga a ESCRITA de depth (o
    // Blend não oclui — o pin do c33 lê o estado); twoSided desliga o
    // culling (o verso existe). TUDO restaurado no fim do draw (o grid/
    // UI setam os seus, mas nunca estado de 3D a vingar entre draws).
    const bool blending = props != nullptr && props->alphaMode == 2;
    const bool twoSided = props != nullptr && props->twoSided;
    if (blending) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    if (twoSided) {
        glDisable(GL_CULL_FACE);
    }
    mesh.bind();
    mesh.draw();
    if (twoSided) {
        glEnable(GL_CULL_FACE);
    }
    if (blending) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
    return {mesh.indexCount(), 1};
}

// 0.11-W (3.5b) — o irmão instanciado do drawMesh (§ Renderer.h). O estado
// GL segue as props EXATAMENTE como no drawMesh (o MESMO par de enable/
// restore — os pins do estado do c33 leem o MESMO); a única diferença é o
// uModel (identidade — a instância leva tudo) e o ramo uInstOn/uInst.
DrawStats Renderer::drawMeshInstanced(const Mesh& mesh, const Mat4* instMats,
                                      u32 instCount, const Mat4& vp,
                                      const Texture* tex, const f32* tint,
                                      const DrawProps* props) {
    if (!mesh.ok() || instMats == nullptr || instCount == 0) {
        return {};
    }
    lit_.use();
    lit_.setVP(vp);
    static const Mat4 kIdentity = Mat4::identity();
    lit_.setModel(kIdentity);   // a instância leva TUDO (uInst[i] × I)
    lit_.setProps(props);
    lit_.setTexture(tex);
    lit_.setTint(tint);
    lit_.setSkin(false);        // instâncias nunca skinam (a pose é por TIC)
    lit_.setInstancing(true);
    lit_.setInstances(instMats, instCount);   // ≤ 64 (o contrato)
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    const bool blending = props != nullptr && props->alphaMode == 2;
    const bool twoSided = props != nullptr && props->twoSided;
    if (blending) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    if (twoSided) {
        glDisable(GL_CULL_FACE);
    }
    mesh.bind();
    mesh.drawInstanced(instCount);
    if (twoSided) {
        glEnable(GL_CULL_FACE);
    }
    if (blending) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    lit_.setInstancing(false);   // o estado do programa fica limpo
    glBindVertexArray(0);
    // 0.11-W (3.5b): o 3.º campo É o contador do prompt (§3.5b.5) — N
    // blocos fundidos neste draw instanciado
    return {mesh.indexCount() * instCount, 1, instCount};
}

void Renderer::submit(const QuadBatch& batch, u32 texture) {
    // batch INTEIRO (comportamento 0.7.0 — atlas/legado)
    submit(batch, texture, 0, batch.vertexCount());
}

// 0.12-D (D-1) — a submissão do PASS SDF (o shader único de widget); o
// estado GL é o do pass UI (afirmado em endFrame); os uniforms do nível
// setam-se UMA vez por frame na primeira submissão SDF. 0.12-D (D-2):
// `tex` != 0 = o VIDRO REAL (o widget amostra a cena borrada).
void Renderer::submitSdf(const SdfBatch& batch, u32 firstVertex,
                         u32 vertexCount, u32 tex) {
    if (vertexCount == 0 || firstVertex >= batch.vertexCount()) {
        return;
    }
    u32 end = firstVertex + vertexCount;
    if (end > batch.vertexCount()) {
        end = batch.vertexCount();
    }
    subs_.push_back(Submission{nullptr, &batch, tex, firstVertex,
                               end - firstVertex, true});
}

// 0.7.4 — submissão por RANGE de vértices (z-order sólidos↔texturas:
// o UiContext submete RUNS na ordem real de emissão). 0.8.4: acima do
// limiar de aviso LOGA (o dono vê no log viewer) — nunca mais descarta.
void Renderer::submit(const QuadBatch& batch, u32 texture, u32 firstVertex,
                      u32 vertexCount) {
    if (vertexCount == 0 || firstVertex >= batch.vertexCount()) {
        return;
    }
    if (subs_.size() >= kSubWarn) {
        // 0.8.4: frame anómalo — LOGA 1× por frame (o dono correlaciona no
        // log viewer); a submissão ENTRA na mesma (nunca mais perda
        // silenciosa de quads/texto)
        if (!droppedLogged_) {
            LOGE("Renderer: %u submissões num frame (>%u) — ecrã excessivo "
                 "(overlay a desenhar demais?); NADA cortado, só aviso",
                 (unsigned)subs_.size() + 1u, kSubWarn);
            droppedLogged_ = true;
        }
    }
    u32 end = firstVertex + vertexCount;
    if (end > batch.vertexCount()) {
        end = batch.vertexCount();   // clamp: o range nunca passa do fim
    }
    subs_.push_back(Submission{&batch, nullptr, texture, firstVertex,
                               end - firstVertex, false});
}

DrawStats Renderer::endFrame() {
    // 0.7.8: o estado do pass de UI é AFIRMADO SEMPRE — mesmo sem submissões.
    // (O early-return antigo deixava o depth/cull do pass 3D ligados quando
    // não havia quads; o frame seguinte herdava. A fronteira é beginUiPass,
    // mas o ponto de submissão re-afirma — defesa em profundidade.)
    glViewport(0, 0, w_, h_);       // full surface — coerente com a orto abaixo
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    if (subs_.empty()) {
        return {};
    }
    // pass UI por cima do 3D: sem depth test/write, sem cull (winding y-down)
    const Mat4 proj = Mat4::ortho(0.0f, static_cast<f32>(w_), static_cast<f32>(h_), 0.0f,
                                  -1.0f, 1.0f);   // y para baixo (origem topo-esquerda)
    glUseProgram(prog_);
    glUniformMatrix4fv(locProj_, 1, GL_FALSE, proj.m);
    if (locTex_ >= 0) {
        glUniform1i(locTex_, 0);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    DrawStats st;
    bool sdfUniformsDone = false;   // os uniforms do nível: 1× por frame
    for (const Submission& s : subs_) {
        if (s.sdf) {
            // ---- 0.12-D (D-1): o PASS SDF (o 3.º programa) ----
            const SdfBatch& b = *s.sdfBatch;
            glUseProgram(progSdf_);
            if (!sdfUniformsDone) {
                // os PARÂMETROS do nível lidos no draw (o toggle
                // sdf::g_level vale no frame inteiro — todos os widgets
                // partilham; a densidade entra UMA vez aqui)
                const sdf::LevelParams lp =
                    sdf::paramsFor(sdf::g_level, theme::dp(1.0f));
                const Mat4 proj = Mat4::ortho(
                    0.0f, static_cast<f32>(w_), static_cast<f32>(h_), 0.0f,
                    -1.0f, 1.0f);
                glUniformMatrix4fv(locSdfProj_, 1, GL_FALSE, proj.m);
                glUniform1f(locSdfBevel_, lp.bevel ? lp.bevelW : 0.0f);
                glUniform1f(locSdfSpecDepth_, lp.specDepth);
                glUniform1f(locSdfGlowW_, lp.glowW);
                glUniform1f(locSdfShadowDy_, lp.shadowDy);
                glUniform1f(locSdfShadowSoft_, lp.shadowSoft);
                glUniform1f(locSdfShadowA_,
                            (lp.bevel && lp.shadow) ? lp.shadowA : 0.0f);
                glUniform1f(locSdfSpecOn_, lp.spec ? 1.0f : 0.0f);
                glUniform1f(locSdfGlowOn_, lp.glow ? 1.0f : 0.0f);
                glUniform1f(locSdfSpecAdd_, lp.specAdd);
                glUniform1f(locSdfGlowAdd_, lp.glowAdd);
                glUniform1f(locSdfAA_, 1.0f);
                if (locSdfCap_ >= 0) {
                    glUniform4f(locSdfCap_, glassCapRect_[0], glassCapRect_[1],
                                glassCapRect_[2], glassCapRect_[3]);
                }
                if (locSdfSurfH_ >= 0) {
                    glUniform1f(locSdfSurfH_, static_cast<f32>(h_));
                }
                if (locSdfTex_ >= 0) {
                    glUniform1i(locSdfTex_, 0);
                }
                sdfUniformsDone = true;
            }
            if (s.tex != 0) {
                // 0.12-D (D-2): o VIDRO REAL — a textura da cena borrada
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, s.tex);
            }
            glBindVertexArray(vaoSdf_);
            glBindBuffer(GL_ARRAY_BUFFER, vboSdf_);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(b.vertexBytes()),
                         b.vertices(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>(s.firstVertex),
                         static_cast<GLsizei>(s.vertexCount));
            st.vertices += s.vertexCount;
            st.drawCalls += 1;
            continue;
        }
        const QuadBatch& b = *s.batch;
        glUseProgram(prog_);
        // 0.12-D (D-1): o ARRAY_BUFFER pode estar no vboSdf_ (o pass SDF
        // alterna) — o bind do UI re-afirma-se POR submissão (o upload é
        // SEMPRE no buffer certo, como o VAO)
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBindTexture(GL_TEXTURE_2D, s.tex);
        // 0.7.4: range de vértices (runs) — o buffer sobe INTEIRO uma vez
        // por batch distinto seria o ideal; subir por submissão mantém a
        // simplicidade (os batches da UI são pequenos e poucos)
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(b.vertexBytes()),
                     b.vertices(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>(s.firstVertex),
                     static_cast<GLsizei>(s.vertexCount));
        st.vertices += s.vertexCount;
        st.drawCalls += 1;
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    subs_.clear();   // 0.8.4: clear mantém a capacidade (zero realloc/frame)
    return st;
}

void Renderer::shutdown() {
    lit_.destroy();
    if (glassTex_) { glDeleteTextures(1, &glassTex_); glassTex_ = 0; }
    if (vboSdf_)   { glDeleteBuffers(1, &vboSdf_); vboSdf_ = 0; }
    if (vaoSdf_)   { glDeleteVertexArrays(1, &vaoSdf_); vaoSdf_ = 0; }
    if (progSdf_)  { glDeleteProgram(progSdf_); progSdf_ = 0; }
    if (vbo_)      { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)      { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (whiteTex_) { glDeleteTextures(1, &whiteTex_); whiteTex_ = 0; }
    if (prog_)     { glDeleteProgram(prog_); prog_ = 0; }
}

// 0.12-D (D-2) — O VIDRO REAL: a captura da região do ECRÃ (o viewport,
// onde os menus flutuam), a redução a 1/4 (duas boxes 2×2), o BLUR DUAL
// KAWASE (ui/Glass.h — a MESMA função pura que os pins aferem) e o upload
// GL_RGBA. A CACHE: só corre quando glassDirty_ (o main invalida quando o
// que está atrás muda); o contador serve o pin vidro_cache.
u32 Renderer::captureGlass(i32 x, i32 y, i32 w, i32 h) {
    if (!glassDirty_ && glassTex_ != 0) {
        return glassTex_;   // a CACHE — o blur só corre quando preciso
    }
    if (w <= 0 || h <= 0) {
        return glassTex_;
    }
    // o rect capturado (clampado à superfície) — o que o FS usa para o UV
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > w_) w = w_ - x;
    if (y + h > h_) h = h_ - y;
    if (w <= 0 || h <= 0) {
        return glassTex_;
    }
    glassCapRect_[0] = static_cast<f32>(x);
    glassCapRect_[1] = static_cast<f32>(y);
    glassCapRect_[2] = static_cast<f32>(w);
    glassCapRect_[3] = static_cast<f32>(h);

    // (1) a leitura da região (full-res; o MESMO caminho do glReadPixels
    // que o pipeline de thumbs usa — o stub rasteriza e lê a sério). O y
    // chega em ECRÃ (y-down) e o glReadPixels quer GL (y-up: origem no
    // fundo) — a conversão é AQUI (o chamador pensa em ecrã sempre)
    glassRead_.assign(size_t(w) * size_t(h) * 4u, 0);
    glReadPixels(x, h_ - (y + h), w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                 glassRead_.data());
    // o glReadPixels devolve a ORIGEM NO FUNDO (GL) — FLIP das linhas para
    // a textura ficar em ECRÃ (y-down, a mesma convenção do FS): linha 0
    // da textura = o TOPO do rect capturado
    {
        std::vector<u8> tmpRow(size_t(w) * 4u);
        for (int r = 0; r < h / 2; ++r) {
            u8* a = glassRead_.data() + size_t(r) * size_t(w) * 4u;
            u8* b = glassRead_.data() + size_t(h - 1 - r) * size_t(w) * 4u;
            std::memcpy(tmpRow.data(), a, size_t(w) * 4u);
            std::memcpy(a, b, size_t(w) * 4u);
            std::memcpy(b, tmpRow.data(), size_t(w) * 4u);
        }
    }

    // (2) a redução a 1/4 (duas boxes 2×2)
    const int hw = w / 2 > 0 ? w / 2 : 1;
    const int hh = h / 2 > 0 ? h / 2 : 1;
    glassHalf_.assign(size_t(hw) * size_t(hh) * 4u, 0);
    glass::downsample2x2(glassRead_.data(), w, h, glassHalf_.data());
    const int qw = hw / 2 > 0 ? hw / 2 : 1;
    const int qh = hh / 2 > 0 ? hh / 2 : 1;
    glassQuarter_.assign(size_t(qw) * size_t(qh) * 4u, 0);
    glass::downsample2x2(glassHalf_.data(), hw, hh, glassQuarter_.data());

    // (3) o BLUR DUAL KAWASE (2 passagens — a função pura partilhada)
    glassTmp_.assign(glassQuarter_.size(), 0);
    glassBlur_.assign(glassQuarter_.size(), 0);
    glass::kawaseBlurDual(glassQuarter_.data(), qw, qh, glassTmp_.data(),
                          glassBlur_.data());

    // (4) o upload (GL_RGBA — o stub guarda os pixéis a sério)
    if (glassTex_ == 0) {
        glGenTextures(1, &glassTex_);
    }
    glBindTexture(GL_TEXTURE_2D, glassTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, qw, qh, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, glassBlur_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glassW_ = qw;
    glassH_ = qh;
    glassDirty_ = false;
    ++glassCaptures_;
    return glassTex_;
}

} // namespace vv
