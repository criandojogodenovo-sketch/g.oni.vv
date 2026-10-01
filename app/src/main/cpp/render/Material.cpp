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
// 0.7.0: uTint — cor por TIC (gestão de TICs). Multiplicativo sobre a cor
// FINAL; default (1,1,1) definido a CADA draw (setTint) = resultado 0.6.x
// byte a byte. O tema mono fica INTACTO: a cor só muda quando o utilizador
// mexe nos sliders R/G/B do Inspector.
// 0.8.2 (F7): SKINNING no VERTEX SHADER — uSkin liga o ramo de bones;
// aJoints/aWeights (locations 3/4) só existem no VAO de um mesh SKINADO
// (mesh estático: atributos não ligados → valor constante, uSkin=0 ignora).
// A matriz efetiva é a soma ponderada das 4 influências (a MESMA conta do
// skinVertex de CPU — o teste de CI aferiu as duas contra o mesmo valor).
constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform mat4 uVP;
uniform mat4 uModel;
uniform int uSkin;
uniform mat4 uBones[64];
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;
    if (uSkin != 0) {
        mat4 b = uBones[int(aJoints.x)] * aWeights.x
               + uBones[int(aJoints.y)] * aWeights.y
               + uBones[int(aJoints.z)] * aWeights.z
               + uBones[int(aJoints.w)] * aWeights.w;
        pos = b * pos;
        nrm = mat3(b) * nrm;
    }
    vNormal = mat3(uModel) * nrm;
    vUV = aUV;
    gl_Position = uVP * uModel * pos;
})";

constexpr char kFsSrc[] = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTex;
uniform float uHasTex;
uniform vec3 uTint;
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
    outColor = vec4(c * uTint, 1.0);
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
    locTint_ = glGetUniformLocation(prog_, "uTint");
    locSkin_ = glGetUniformLocation(prog_, "uSkin");        // 0.8.2
    locBones_ = glGetUniformLocation(prog_, "uBones[0]");   // 0.8.2
    LOGI("LitMaterial: lit difusa fixa + ambient + textura opcional + uTint "
         "+ skin (uBones[64]) pronto");
    return true;
}

void LitMaterial::destroy() {
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    locVP_ = locModel_ = locTex_ = locHasTex_ = locTint_ = -1;
    locSkin_ = locBones_ = -1;   // 0.8.2
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

// 0.7.0 — cor por TIC: uTint multiplicativo. nullptr = branco (o
// comportamento de sempre — uniforms GL nascem a 0, por isso o default é
// definido EXPLICITAMENTE em cada draw, nunca deixado ao acaso).
void LitMaterial::setTint(const f32 rgb[3]) const {
    if (locTint_ < 0) {
        return;
    }
    if (rgb) {
        glUniform3f(locTint_, rgb[0], rgb[1], rgb[2]);
    } else {
        glUniform3f(locTint_, 1.0f, 1.0f, 1.0f);
    }
}

// 0.8.2 (F7) — ramo de bones: uSkin liga/desliga (A CADA draw, como o
// uTint — uniforms nascem a 0 e 0 = caminho estático, que é o default
// correto); setBones faz upload do array (cap 64 = kMaxBones do shader).
void LitMaterial::setSkin(bool on) const {
    if (locSkin_ >= 0) {
        glUniform1i(locSkin_, on ? 1 : 0);
    }
}

void LitMaterial::setBones(const Mat4* bones, u32 count) const {
    if (!bones || count == 0 || locBones_ < 0) {
        return;
    }
    const u32 n = count < kMaxBones ? count : kMaxBones;
    glUniformMatrix4fv(locBones_, static_cast<GLsizei>(n), GL_FALSE,
                       bones->m);
}

} // namespace vv

