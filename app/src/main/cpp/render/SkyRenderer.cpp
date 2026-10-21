#include "render/SkyRenderer.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>

namespace vv {

namespace {

// o quad fullscreen: 4 vértices (triangle strip) em NDC — o MESMO esqueleto
// do Grid (o z=1.0 é o longe; o draw não escreve depth nem testa)
constexpr f32 kQuadCorners[4][2] = {
    {-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f},
};

constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec2 aCorner;
out vec2 vNdc;
void main() {
    vNdc = aCorner;
    gl_Position = vec4(aCorner, 1.0, 1.0);   // z/w = 1.0 — o fundo de sempre
})";

// o FS REPLICA as puras do render/SkyRenderer.h LINHA A LINHA (o padrão
// Pbr.h — a mutação que diverge o FS das puras morre nos pins).
constexpr char kFsSrc[] = R"(#version 300 es
precision highp float;
in vec2 vNdc;
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec3 uFwd;
uniform float uAspect;
uniform float uTanF;
uniform float uTime;      // o relógio das nuvens (M-Yd1 vive AQUI)
uniform float uSpeed;
uniform float uCoverage;
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform vec3 uSunCol;
uniform vec3 uSunDir;
out vec4 outColor;

// kCloudScale / kCloudSoft / kPlaneEps / kWindDir* — as MESMAS constantes
const float kCloudScale  = 3.0;
const float kCloudSoft   = 0.2;
const float kPlaneEps    = 0.05;
const float kWindDirX    = 1.0;
const float kWindDirY    = 0.35;
const float kSunCosDisk  = 0.9993;
const float kSunCosSoft  = 0.0004;
const float kSunGlowPow  = 24.0;
const float kSunGlowGain = 0.35;
const float kHorizonFadeY = 0.15;
const float kCloudShadow  = 0.75;
const float kCloudLight   = 0.25;
const float kCloudSunMix  = 0.35;

float sstep(float e0, float e1, float x) {
    float d = e1 - e0;
    float t = d != 0.0 ? (x - e0) / d : 0.0;
    t = clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// o hash INTEIRO (a réplica de sky::hashCellSky — sem sin, Z² inteiro)
float hash21(ivec2 c) {
    uint h = uint(c.x) * 374761393u + uint(c.y) * 668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return float(h & 0xFFFFu) / 65535.0;
}

// a réplica de sky::valueNoise2 (bilinear + smoothstep)
float valueNoise2(vec2 p) {
    vec2 g = floor(p);
    ivec2 c = ivec2(g);
    vec2 t = p - g;
    t = t * t * (3.0 - 2.0 * t);
    float a = hash21(c);
    float b = hash21(c + ivec2(1, 0));
    float d = hash21(c + ivec2(0, 1));
    float e = hash21(c + ivec2(1, 1));
    return (a * (1.0 - t.x) + b * t.x) * (1.0 - t.y) +
           (d * (1.0 - t.x) + e * t.x) * t.y;
}

// a réplica de sky::fbm2 (DUAS oitavas — o orçamento mobile)
float fbm2(vec2 p) {
    return 0.65 * valueNoise2(p) +
           0.35 * valueNoise2(p * 2.13 + vec2(17.7, 9.2));
}

// a réplica de sky::cloudAlpha (a cobertura: 0 = limpo, 1 = nublado)
float cloudAlpha(float n, float coverage) {
    float t = 1.0 - coverage;
    return sstep(t, t + kCloudSoft, n);
}

// a réplica de sky::sunDisk / sky::sunGlow / sky::horizonFade
float sunDisk(float cosA) {
    return sstep(kSunCosDisk, kSunCosDisk + kSunCosSoft, cosA);
}
float sunGlow(float cosA) {
    return pow(max(cosA, 0.0), kSunGlowPow) * kSunGlowGain;
}
float horizonFade(float dirY) {
    return sstep(kPlaneEps, kHorizonFadeY, dirY);
}

void main() {
    // a direção da vista (a réplica de sky::viewDir — a FÓRMULA ÚNICA)
    vec3 dir = normalize(uFwd + vNdc.x * uAspect * uTanF * uRight
                              + vNdc.y * uTanF * uUp);
    // o GRADIENTE (a réplica de sky::skyGradient)
    float t = clamp(dir.y, 0.0, 1.0);
    vec3 col = uHorizon + (uZenith - uHorizon) * t;
    // o HALO antes das nuvens (atravessa o véu fino)
    float d = dot(dir, uSunDir);
    col += uSunCol * sunGlow(d);
    // as NUVENS: a projeção planar + O VENTO (a réplica de sky::windOffset
    // — o tempo × a velocidade no UV; o contrato vive SÓ na linha de código
    // abaixo: os comentários NUNCA citam a expressão do pin)
    vec2 cuv = dir.xz / max(dir.y, kPlaneEps) * kCloudScale
             + vec2(kWindDirX, kWindDirY) * (uTime * uSpeed);
    float n = fbm2(cuv);
    float a = cloudAlpha(n, uCoverage) * horizonFade(dir.y);
    // a nuvem: branca com o tint do sol, o ventre escuro pelo ruído
    vec3 cloudCol = mix(vec3(1.0), uSunCol, kCloudSunMix) *
                    (kCloudShadow + kCloudLight * n);
    col = mix(col, cloudCol, a);
    // o DISCO depois das nuvens (morre atrás da nuvem densa)
    col += uSunCol * sunDisk(d) * (1.0 - a);
    outColor = vec4(col, 1.0);
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
        LOGE("SkyRenderer: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

void SkyRenderer::use() const {
    glUseProgram(prog_);
}

bool SkyRenderer::init() {
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
        LOGE("SkyRenderer: falha ao linkar programa: %s", log);
        return false;
    }
    locRight_ = glGetUniformLocation(prog_, "uRight");
    locUp_ = glGetUniformLocation(prog_, "uUp");
    locFwd_ = glGetUniformLocation(prog_, "uFwd");
    locAspect_ = glGetUniformLocation(prog_, "uAspect");
    locTanF_ = glGetUniformLocation(prog_, "uTanF");
    locTime_ = glGetUniformLocation(prog_, "uTime");
    locSpeed_ = glGetUniformLocation(prog_, "uSpeed");
    locCoverage_ = glGetUniformLocation(prog_, "uCoverage");
    locHor_ = glGetUniformLocation(prog_, "uHorizon");
    locZen_ = glGetUniformLocation(prog_, "uZenith");
    locSunCol_ = glGetUniformLocation(prog_, "uSunCol");
    locSunDir_ = glGetUniformLocation(prog_, "uSunDir");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadCorners), kQuadCorners,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32),
                          reinterpret_cast<const void*>(0));
    glBindVertexArray(0);

    vertexCount_ = 4;
    LOGI("SkyRenderer: pronto (quad fullscreen + céu procedural com nuvens, "
         "%u vértices)",
         vertexCount_);
    return true;
}

void SkyRenderer::destroy() {
    if (vbo_)  { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)  { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    locRight_ = locUp_ = locFwd_ = locAspect_ = locTanF_ = -1;
    locTime_ = locSpeed_ = locCoverage_ = -1;
    locHor_ = locZen_ = locSunCol_ = locSunDir_ = -1;
    vertexCount_ = 0;
}

DrawStats SkyRenderer::draw(const Params& p) const {
    if (!vao_ || vertexCount_ == 0) {
        return {};
    }
    glUseProgram(prog_);
    if (locRight_ >= 0) {
        glUniform3f(locRight_, p.right.x, p.right.y, p.right.z);
    }
    if (locUp_ >= 0) {
        glUniform3f(locUp_, p.up.x, p.up.y, p.up.z);
    }
    if (locFwd_ >= 0) {
        glUniform3f(locFwd_, p.fwd.x, p.fwd.y, p.fwd.z);
    }
    if (locAspect_ >= 0) {
        glUniform1f(locAspect_, p.aspect);
    }
    if (locTanF_ >= 0) {
        glUniform1f(locTanF_, p.tanHalfFovY);
    }
    if (locSpeed_ >= 0) {
        glUniform1f(locSpeed_, p.speed);
    }
    if (locCoverage_ >= 0) {
        glUniform1f(locCoverage_, p.coverage);
    }
    if (locHor_ >= 0) {
        glUniform3f(locHor_, p.horizon[0], p.horizon[1], p.horizon[2]);
    }
    if (locZen_ >= 0) {
        glUniform3f(locZen_, p.zenith[0], p.zenith[1], p.zenith[2]);
    }
    if (locSunCol_ >= 0) {
        glUniform3f(locSunCol_, p.sunCol[0], p.sunCol[1], p.sunCol[2]);
    }
    if (locSunDir_ >= 0) {
        glUniform3f(locSunDir_, p.sunDir[0], p.sunDir[1], p.sunDir[2]);
    }
    // uTime é o ÚLTIMO uniform 1f do draw (a ORDEM é o contrato do pin —
    // o stub só guarda o ÚLTIMO valor por tipo; o tempo é a alma da
    // animação e a mutação M-Yd1 morre nesta leitura)
    if (locTime_ >= 0) {
        glUniform1f(locTime_, p.time);
    }
    // o fundo do frame: SEM depth test/write, SEM blend, SEM cull —
    // tudo o que vem depois pinta por cima (o estado volta exato — o
    // MESMO padrão do Grid: o draw gere a SUA janela de estado)
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    return {vertexCount_, 1};
}

} // namespace vv
