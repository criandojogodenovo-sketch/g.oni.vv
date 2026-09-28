#include "render/Grid.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>
#include <vector>

namespace vv {

namespace {

// Cor por vértice: tema mono — linhas cinza escuro, eixos mais claros.
struct LineVertex {
    f32 x, y, z;
    f32 r, g, b, a;
};

constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uVP;
out vec3 vWorld;
out vec4 vColor;
void main() {
    vWorld = aPos;
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
})";

constexpr char kFsSrc[] = R"(#version 300 es
precision mediump float;
in vec3 vWorld;
in vec4 vColor;
uniform vec3 uCamPos;
out vec4 outColor;
// fade radial pela distância XZ à câmara — o grid perde-se no fundo antes
// de qualquer borda aparecer ("espaço infinito" do viewport)
const float kFadeStart = 14.0;
const float kFadeEnd   = 48.0;
void main() {
    float d = length(vWorld.xz - uCamPos.xz);
    float fade = 1.0 - smoothstep(kFadeStart, kFadeEnd, d);
    float a = vColor.a * fade;
    if (a <= 0.003) discard;
    outColor = vec4(vColor.rgb, a);
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
        LOGE("Grid: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

bool Grid::init(f32 extent, f32 step) {
    destroy();
    if (extent <= 0.0f || step <= 0.0f || step > extent) {
        LOGE("Grid: parâmetros inválidos (extent=%f step=%f)", extent, step);
        return false;
    }

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
        LOGE("Grid: falha ao linkar programa: %s", log);
        return false;
    }
    locVP_ = glGetUniformLocation(prog_, "uVP");
    locCam_ = glGetUniformLocation(prog_, "uCamPos");

    // Malha procedural: linhas paralelas aos eixos X e Z, no plano y=0.
    // Extensão cobre câmara (zoom máx ~40) + fade (48) + folga → borda nunca visível.
    std::vector<LineVertex> verts;
    const i32 n = static_cast<i32>(extent / step + 0.5f);
    verts.reserve(static_cast<size_t>((2 * n + 2) * 2));
    constexpr f32 kLine[4] = {0.30f, 0.30f, 0.30f, 1.0f};   // linha normal
    constexpr f32 kAxis[4] = {0.55f, 0.55f, 0.55f, 1.0f};   // eixos X/Z
    auto pushLine = [&](f32 x0, f32 z0, f32 x1, f32 z1, const f32 c[4]) {
        verts.push_back({x0, 0.0f, z0, c[0], c[1], c[2], c[3]});
        verts.push_back({x1, 0.0f, z1, c[0], c[1], c[2], c[3]});
    };
    for (i32 i = -n; i <= n; ++i) {
        const f32 t = static_cast<f32>(i) * step;
        const bool axis = (i == 0);
        const f32* c = axis ? kAxis : kLine;
        pushLine(t, -extent, t, extent, c);   // paralela ao Z (x = t)
        pushLine(-extent, t, extent, t, c);   // paralela ao X (z = t)
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(LineVertex)),
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<const void*>(3 * sizeof(f32)));
    glBindVertexArray(0);

    vertexCount_ = static_cast<u32>(verts.size());
    LOGI("Grid: pronto (%u vértices, extent %.0f)", vertexCount_, extent);
    return true;
}

void Grid::destroy() {
    if (vbo_)  { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)  { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    locVP_ = locCam_ = -1;
    vertexCount_ = 0;
}

DrawStats Grid::draw(const Mat4& vp, const Vec3& camPos) const {
    if (!vao_ || vertexCount_ == 0) {
        return {};
    }
    glUseProgram(prog_);
    if (locVP_ >= 0) {
        glUniformMatrix4fv(locVP_, 1, GL_FALSE, vp.m);
    }
    if (locCam_ >= 0) {
        glUniform3f(locCam_, camPos.x, camPos.y, camPos.z);
    }
    // transparente: blend com fade, depth test ligado (respeita o cubo),
    // depth write desligado (nunca oculta nada)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount_));
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    return {vertexCount_, 1};
}

} // namespace vv
