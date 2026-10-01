#include "render/Renderer.h"
#include "render/Mesh.h"
#include "math/Math.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>

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

    LOGI("Renderer: pipeline 3D (Mesh/LitMaterial) + batch de quads UI pronto");
    return true;
}

void Renderer::resize(i32 w, i32 h) {
    w_ = w;
    h_ = h;
    if (w_ > 0 && h_ > 0) {
        glViewport(0, 0, w_, h_);
    }
}

void Renderer::beginFrame() {
    glClearColor(0.0784314f, 0.0784314f, 0.0784314f, 1.0f);   // BG #141414
    glDepthMask(GL_TRUE);                                     // restore pós-grid
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

DrawStats Renderer::drawMesh(const Mesh& mesh, const Mat4& model, const Mat4& vp,
                             const Texture* tex, const f32* tint) {
    if (!mesh.ok()) {
        return {};
    }
    lit_.use();
    lit_.setVP(vp);
    lit_.setModel(model);
    lit_.setTexture(tex);   // F5-E: albedo opcional (unit 0 + uHasTex)
    lit_.setTint(tint);     // 0.7.0: cor por TIC (nullptr = branco, o de sempre)
    // pass 3D: depth visível (faces frontais ocluem as traseiras) + cull
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    mesh.bind();
    mesh.draw();
    glBindVertexArray(0);
    return {mesh.indexCount(), 1};
}

void Renderer::submit(const QuadBatch& batch, u32 texture) {
    // batch INTEIRO (comportamento 0.7.0 — atlas/legado)
    submit(batch, texture, 0, batch.vertexCount());
}

// 0.7.4 — submissão por RANGE de vértices (z-order sólidos↔texturas:
// o UiContext submete RUNS na ordem real de emissão)
void Renderer::submit(const QuadBatch& batch, u32 texture, u32 firstVertex,
                      u32 vertexCount) {
    if (subCount_ < kMaxSubs && vertexCount > 0 &&
        firstVertex < batch.vertexCount()) {
        subs_[subCount_].batch = &batch;
        subs_[subCount_].tex = texture;
        subs_[subCount_].firstVertex = firstVertex;
        // clamp: o range nunca passa do fim do batch
        u32 end = firstVertex + vertexCount;
        if (end > batch.vertexCount()) {
            end = batch.vertexCount();
        }
        subs_[subCount_].vertexCount = end - firstVertex;
        ++subCount_;
    }
}

DrawStats Renderer::endFrame() {
    if (subCount_ == 0) {
        return {};
    }
    // pass UI por cima do 3D: sem depth test/write, sem cull (winding y-down)
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
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
    for (u32 i = 0; i < subCount_; ++i) {
        const QuadBatch& b = *subs_[i].batch;
        glBindTexture(GL_TEXTURE_2D, subs_[i].tex);
        // 0.7.4: range de vértices (runs) — o buffer sobe INTEIRO uma vez
        // por batch distinto seria o ideal; subir por submissão mantém a
        // simplicidade (os batches da UI são pequenos e poucos)
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(b.vertexBytes()),
                     b.vertices(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>(subs_[i].firstVertex),
                     static_cast<GLsizei>(subs_[i].vertexCount));
        st.vertices += subs_[i].vertexCount;
        st.drawCalls += 1;
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    subCount_ = 0;
    return st;
}

void Renderer::shutdown() {
    lit_.destroy();
    if (vbo_)      { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)      { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (whiteTex_) { glDeleteTextures(1, &whiteTex_); whiteTex_ = 0; }
    if (prog_)     { glDeleteProgram(prog_); prog_ = 0; }
}

} // namespace vv
