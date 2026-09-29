#include "render/Material.h"
#include "render/Texture.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>

namespace vv {

namespace {

// Luz direcional FIXA no shader (PLACEHOLDER — luzes configuráveis pós-F4).
// Tema mono: albedo cinza claro, ambient escuro — nada de cor.
// F5-E: aUV/vUV (location 2, F5) + uTex/uHasTex — a textura albedo é
// opcional: sem ela o resultado é IDÊNTICO ao F2 (uHasTex=0).
constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uVP * uModel * vec4(aPos, 1.0);
})";

constexpr char kFsSrc[] = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTex;
uniform float uHasTex;
const vec3 kLightDir = vec3(0.4545, 0.7435, 0.2891);  // já normalizada, PARA a luz
const vec3 kAmbient  = vec3(0.16);
const vec3 kAlbedo   = vec3(0.58);
void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, kLightDir), 0.0);
    vec3 alb = kAlbedo;
    if (uHasTex > 0.5) {
        alb *= texture(uTex, vUV).rgb;
    }
    vec3 c = kAmbient + alb * diff;
    outColor = vec4(c, 1.0);
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
        LOGE("LitMaterial: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

bool LitMaterial::init() {
    destroy();
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
        LOGE("LitMaterial: falha ao linkar programa: %s", log);
        return false;
    }
    locVP_ = glGetUniformLocation(prog_, "uVP");
    locModel_ = glGetUniformLocation(prog_, "uModel");
    locTex_ = glGetUniformLocation(prog_, "uTex");
    locHasTex_ = glGetUniformLocation(prog_, "uHasTex");
    LOGI("LitMaterial: lit difusa fixa + ambient + textura opcional pronto");
    return true;
}

void LitMaterial::destroy() {
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    locVP_ = locModel_ = locTex_ = locHasTex_ = -1;
}

void LitMaterial::use() const {
    glUseProgram(prog_);
}

void LitMaterial::setVP(const Mat4& vp) const {
    if (locVP_ >= 0) {
        glUniformMatrix4fv(locVP_, 1, GL_FALSE, vp.m);
    }
}

void LitMaterial::setModel(const Mat4& model) const {
    if (locModel_ >= 0) {
        glUniformMatrix4fv(locModel_, 1, GL_FALSE, model.m);
    }
}

// F5-E: liga a textura albedo (unit 0) e ativa uHasTex; nullptr desativa —
// sem textura o shader produz EXATAMENTE o resultado da F2.
void LitMaterial::setTexture(const Texture* tex) const {
    if (locHasTex_ >= 0) {
        glUniform1f(locHasTex_, tex ? 1.0f : 0.0f);
    }
    if (tex && locTex_ >= 0) {
        tex->bind(0);   // albedo no unit 0
        glUniform1i(locTex_, 0);
    }
}

} // namespace vv

