#include "render/Renderer.h"
#include "math/Math.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>

namespace vv {

namespace {

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

    LOGI("Renderer: init ok (clear mono + batch de quads — PLACEHOLDER F1)");
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
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::submit(const QuadBatch& batch, u32 texture) {
    if (subCount_ < 2 && !batch.empty()) {
        subs_[subCount_].batch = &batch;
        subs_[subCount_].tex = texture;
        ++subCount_;
    }
}

void Renderer::endFrame() {
    if (subCount_ == 0) {
        return;
    }
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
    for (u32 i = 0; i < subCount_; ++i) {
        const QuadBatch& b = *subs_[i].batch;
        glBindTexture(GL_TEXTURE_2D, subs_[i].tex);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(b.vertexBytes()),
                     b.vertices(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(b.vertexCount()));
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    subCount_ = 0;
}

void Renderer::shutdown() {
    if (vbo_)      { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)      { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (whiteTex_) { glDeleteTextures(1, &whiteTex_); whiteTex_ = 0; }
    if (prog_)     { glDeleteProgram(prog_); prog_ = 0; }
}

} // namespace vv
