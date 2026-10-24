// render/ParticleRenderer.cpp — o GPU particle system (0.12-Y-F1).
//
// UM draw call por emissor (glDrawArraysInstanced — o GLES 3.0 core);
// o buffer de seeds nasce UMA VEZ com o teto kSeedCapacity (32 KB de
// floats — nada) e NUNCA regenera: o count do draw é o maxParticles do
// componente clampado ao buffer. O VS faz a física (ParticleShader.h).
#include "render/ParticleRenderer.h"
#include "render/GlDebug.h"   // 0.12-Z F0: contadores GL
#include "components/ParticleEmitter.h"
#include "assets/PngLoader.h"
#include "assets/Assets.h"
#include "core/ProjectStorage.h"
#include "render/ParticleShader.h"
#include "render/Texture.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>
#include <cstring>

namespace vv {

namespace {
// o TETO do buffer de seeds (a capacidade fixa — o buffer nasce 1× e o
// slider maxParticles SÓ muda o COUNT do draw; 2000 × 4 floats = 32 KB)
constexpr u32 kSeedCapacity = static_cast<u32>(
    ParticleEmitter::kMaxParticlesMax);
static_assert(ParticleEmitter::kMaxParticlesMax == 2000,
              "o teto do buffer de seeds casa com o teto do componente");

constexpr f32 kQuadCorners[8] = {
    -1.0f, -1.0f,   1.0f, -1.0f,   -1.0f, 1.0f,   1.0f, 1.0f,
};

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOGE("ParticleRenderer: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

bool ParticleRenderer::init() {
    destroy();
    gldbg::onCreated(gldbg::Kind::Program, 1);   // 0.12-Z F0
    gldbg::onCreated(gldbg::Kind::Vao, 1);
    gldbg::onCreated(gldbg::Kind::Buffer, 1);
    const GLuint vs = compileShader(GL_VERTEX_SHADER, fx::kVertexSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fx::kFragmentSrc);
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
        LOGE("ParticleRenderer: falha ao linkar programa: %s", log);
        return false;
    }
    locVP_ = glGetUniformLocation(prog_, "uVP");
    locRight_ = glGetUniformLocation(prog_, "uRight");
    locUp_ = glGetUniformLocation(prog_, "uUp");
    locPos_ = glGetUniformLocation(prog_, "uPos");
    locVel_ = glGetUniformLocation(prog_, "uVel");
    locGrav_ = glGetUniformLocation(prog_, "uGrav");
    locWind_ = glGetUniformLocation(prog_, "uWind");   // 0.12-Y-F2
    locStartSize_ = glGetUniformLocation(prog_, "uStartSize");
    locEndSize_ = glGetUniformLocation(prog_, "uEndSize");
    locStartCol_ = glGetUniformLocation(prog_, "uStartCol");
    locEndCol_ = glGetUniformLocation(prog_, "uEndCol");
    locHasTex_ = glGetUniformLocation(prog_, "uHasTex");
    locTex_ = glGetUniformLocation(prog_, "uTex");
    locTime_ = glGetUniformLocation(prog_, "uTime");

    // os CANTOS do quad (loc 0, divisor 0) + os SEEDS (loc 1, divisor 1)
    // — o buffer de seeds é o ÚNICO trabalho do CPU, feito AQUI uma vez
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vboCorner_);
    glGenBuffers(1, &vboSeeds_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vboCorner_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadCorners), kQuadCorners,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32),
                          reinterpret_cast<const void*>(0));
    // os seeds: xyz = a origem aleatória, w = a semente (fx::generateSeeds)
    static f32 seeds[kSeedCapacity * 4];
    fx::generateSeeds(kSeedCapacity, seeds);
    glBindBuffer(GL_ARRAY_BUFFER, vboSeeds_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(seeds), seeds, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(f32),
                          reinterpret_cast<const void*>(0));
    glVertexAttribDivisor(1, 1);   // 1 seed por INSTÂNCIA (o GLES 3.0 core)
    glBindVertexArray(0);
    bufferCount_ = kSeedCapacity;
    LOGI("ParticleRenderer: %u seeds no buffer (1 draw call por emissor, "
         "a física no VS)", kSeedCapacity);
    return true;
}

void ParticleRenderer::destroy() {
    if (tex_) {
        delete static_cast<Texture*>(tex_);
        tex_ = nullptr;
    }
    texPath_.clear();
    texFailed_ = false;
    if (vboCorner_) { glDeleteBuffers(1, &vboCorner_); vboCorner_ = 0; }
    if (vboSeeds_) { glDeleteBuffers(1, &vboSeeds_); vboSeeds_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    locVP_ = locRight_ = locUp_ = locPos_ = locVel_ = locGrav_ = -1;
    locWind_ = -1;   // 0.12-Y-F2
    locStartSize_ = locEndSize_ = locStartCol_ = locEndCol_ = -1;
    locHasTex_ = locTex_ = -1;
    locTime_ = -1;
    bufferCount_ = 0;
}

void ParticleRenderer::setTexture(ProjectStorage* storage,
                                  const std::string& path) {
    if (path == texPath_) {
        return;   // o MESMO path — nada a fazer (o cache pelo path)
    }
    texPath_ = path;
    if (tex_) {
        delete static_cast<Texture*>(tex_);
        tex_ = nullptr;
    }
    if (path.empty() || storage == nullptr) {
        return;   // o sprite procedural (uHasTex 0) — o MVP sem asset
    }
    std::vector<u8> bytes;
    if (!storage->readBytes(path, bytes) || bytes.empty()) {
        if (!texFailed_) {
            LOGE("particle: textura '%s' não lida — o sprite procedural "
                 "fica (o LOG fala 1×)", path.c_str());
        }
        texFailed_ = true;
        return;
    }
    RawImage img;
    std::string err;
    if (!loadPng(bytes.data(), bytes.size(), img, err) || !img.ok()) {
        if (!texFailed_) {
            LOGE("particle: textura '%s' não decodifica (%s) — o sprite "
                 "procedural fica", path.c_str(), err.c_str());
        }
        texFailed_ = true;
        return;
    }
    Texture* t = new Texture();
    if (!t->createFromRGBA(img.rgba.data(), img.width, img.height)) {
        delete t;
        if (!texFailed_) {
            LOGE("particle: textura '%s' não sobe à GPU — o sprite fica",
                 path.c_str());
        }
        texFailed_ = true;
        return;
    }
    tex_ = t;
    texFailed_ = false;
}

DrawStats ParticleRenderer::draw(const Params& p) const {
    DrawStats st;
    if (prog_ == 0 || p.maxParticles <= 0 || bufferCount_ == 0) {
        return st;
    }
    const i32 count = (p.maxParticles < static_cast<i32>(bufferCount_))
                          ? p.maxParticles
                          : static_cast<i32>(bufferCount_);
    if (count <= 0) {
        return st;
    }
    glUseProgram(prog_);
    // o estado transparente da casa: blend ON, depth test ON, depth
    // write OFF (as partículas NUNCA escrevem o z) — e o restauro é o
    // MESMO do céu (o MESMO contrato com o passe de malhas/UI seguinte)
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (locVP_ >= 0 && p.vp) {
        glUniformMatrix4fv(locVP_, 1, GL_FALSE, p.vp);
    }
    if (locRight_ >= 0) {
        glUniform3f(locRight_, p.right[0], p.right[1], p.right[2]);
    }
    if (locUp_ >= 0) {
        glUniform3f(locUp_, p.up[0], p.up[1], p.up[2]);
    }
    if (locPos_ >= 0) {
        glUniform3f(locPos_, p.pos[0], p.pos[1], p.pos[2]);
    }
    if (locVel_ >= 0) {
        glUniform3f(locVel_, p.vel[0], p.vel[1], p.vel[2]);
    }
    if (locGrav_ >= 0) {
        glUniform3f(locGrav_, p.grav[0], p.grav[1], p.grav[2]);
    }
    // o VENTO GLOBAL (0.12-Y-F2 — o desvio acumulado uWind·t no VS; o
    // zero = o caminho de sempre da F1 byte a byte)
    if (locWind_ >= 0) {
        glUniform3f(locWind_, p.wind[0], p.wind[1], p.wind[2]);
    }
    if (locStartSize_ >= 0) {
        glUniform1f(locStartSize_, p.startSize);
    }
    if (locEndSize_ >= 0) {
        glUniform1f(locEndSize_, p.endSize);
    }
    if (locStartCol_ >= 0) {
        glUniform4f(locStartCol_, p.startCol[0], p.startCol[1],
                    p.startCol[2], p.startCol[3]);
    }
    if (locEndCol_ >= 0) {
        glUniform4f(locEndCol_, p.endCol[0], p.endCol[1], p.endCol[2],
                    p.endCol[3]);
    }
    // a textura (unit 0) — o sprite procedural quando não há (o hasTex
    // do PARAMS manda: o load falhou → o flag texFailed_ não desenha
    // textura nenhuma)
    const bool useTex = p.hasTex && tex_ != nullptr;
    if (locTex_ >= 0) {
        glUniform1i(locTex_, 0);
    }
    if (useTex) {
        static_cast<const Texture*>(tex_)->bind(0);
    }
    if (locHasTex_ >= 0) {
        glUniform1f(locHasTex_, useTex ? 1.0f : 0.0f);
    }
    glBindVertexArray(vao_);
    // O ÚNICO DRAW — as instâncias = o count do componente (o teto do
    // buffer manda no máx; NUNCA regenera)
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // o uTime é o ÚLTIMO 1f do draw (o contrato da casa — o MESMO padrão
    // do uTime do céu: o pin aferra a ordem)
    if (locTime_ >= 0) {
        glUniform1f(locTime_, p.time);
    }
    st.drawCalls = 1;
    st.vertices = static_cast<u32>(count) * 4u;
    return st;
}

} // namespace vv
