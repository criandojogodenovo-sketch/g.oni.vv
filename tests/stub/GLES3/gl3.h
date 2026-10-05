// GLES3/gl3.h — stub de hospedeiro (CI/diagnóstico): nenhuma GL real, tudo
// no-op. Suficiente para compilar FontAtlas/Renderer sem NDK; os objetos GL
// são fictícios e nunca são desenhados (os testes leem os batches de CPU).
//
// 0.6.7 (lifecycle GL): CONTADORES de chamadas relevantes do lifecycle
// (glGenTextures/glDeleteTextures/glTexImage2D/glBufferData…) num estado
// global `glstub::stats` — os testes do lifecycle aferem que o atlas é
// re-uploadado após destroy() e que NINGUÉM assume recursos vivos entre
// contexts. Zerar com glstub::reset() no início de cada caso.
//
// F6 (seletor de textura): registo de BIND — o teste do wiring aferiu que o
// render binda a textura aplicada (glBindTexture com o id certo + uHasTex=1
// via glUniform1f; nullptr → uHasTex=0 SEM bind). Inócuo para os outros
// testes (apenas grava números).
//
// 0.7.0 (cor por TIC): registo do glUniform3f (uTint do LitMaterial) — o
// teste aferiu que o render aplica o tint do MeshRenderer (sliders R/G/B) e
// que nullptr = branco (1,1,1). Mesmo padrão: grava, não interfere.
//
// 0.7.8 (separação render 3D↔UI): ESTADO REAL das capacidades que o pass
// de UI afirma — viewport (x,y,w,h), scissor test, depth test, cull face
// e a ÚLTIMA matriz 4x4 enviada (a ortográfica de ecrã do pass de UI). O
// diagnóstico do C33: o pass de UI herda o estado GL do pass 3D da câmara
// de jogo; os testes de pass (test_passgl.cpp) sujam o estado a propósito
// e aferem que a fronteira 3D→UI o repõe (glViewport cheio, scissor off,
// depth off, orto de ecrã).
#pragma once
#include <cstdint>
#include <cstring>

namespace glstub {
struct Stats {
    int genTextures = 0;
    int deleteTextures = 0;
    int texImage2D = 0;          // uploads de atlas/textura não-comprimida
    int compressedTexImage2D = 0;
    int genBuffers = 0;
    int deleteBuffers = 0;
    int bufferData = 0;
    int genVertexArrays = 0;
    int deleteVertexArrays = 0;
    int createProgram = 0;
    int deleteProgram = 0;
    // F6: binds de textura + último uniform float (uHasTex do LitMaterial)
    int boundTextures = 0;
    unsigned int lastBoundTexture = 0;
    float lastUniform1f = -1.0f;
    // 0.7.0: último uniform vec3 (uTint — cor por TIC)
    float lastUniform3f[3] = {-1.0f, -1.0f, -1.0f};
    int uniform3fCalls = 0;
    // 0.7.8: ESTADO da fronteira 3D↔UI (o que o pass de UI herda/afirma)
    int viewport[4] = {0, 0, 0, 0};
    int viewportCalls = 0;
    bool scissorEnabled = false;   // GL_SCISSOR_TEST (default GL: off)
    bool depthEnabled = false;     // GL_DEPTH_TEST (default GL: off)
    bool cullEnabled = false;      // GL_CULL_FACE (default GL: off)
    bool blendEnabled = false;     // GL_BLEND (default GL: off)
    float lastMatrix4fv[16] = {};  // ÚLTIMA glUniformMatrix4fv (orto do UI)
    int matrix4fvCalls = 0;
    int drawArraysCalls = 0;       // submissões reais do pass de UI
    // 0.8.2 (F7): skin — último glUniform1i (uSkin do LitMaterial), nº de
    // uploads de ARRAYS de matrizes (uBones) e contagem de drawElements
    // ("mesh skinado desenha" — o teste lê o stub, como o lifecycle)
    int lastUniform1i = -1;
    int uniform1iCalls = 0;
    int matrix4fvArrayCalls = 0;   // glUniformMatrix4fv com count > 1 (uBones)
    int lastMatrix4fvCount = 1;
    int drawElementsCalls = 0;
};
inline Stats stats;              // inline C++17: 1 instância por binário
// 0.8.7 — INJETOR DE FALHA: simula o device a ficar sem capacidade de gerar
// objetos GL (GL_OUT_OF_MEMORY / contexto doente — a causa INTERMITENTE dos
// "falha ao gerar primitiva" no C33). Liga-se e desliga-se por teste; o
// reset() desliga.
inline bool failNextGenObjects = false;
// 0.8.12 — C33 VIRTUAL: a extensão ASTC LDR como o Mali do device do dono
// (default false = comportamento antigo dos testes; o harness liga antes
// do INIT_WINDOW e o boot loga "ASTC SIM" — o caminho do C33)
inline bool astcLdr = false;
// 0.8.12: o reset zera as ESTATÍSTICAS e o injetor de falha; o astcLdr é
// CONFIGURAÇÃO DO AMBIENTE (o C33 virtual liga-o antes do boot e os resets
// entre fases não podem desligar a "GPU" do device simulado)
inline void reset() { stats = Stats{}; failNextGenObjects = false; }
} // namespace glstub

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned char GLubyte;
typedef unsigned char GLboolean;
typedef float         GLfloat;
typedef char          GLchar;
typedef intptr_t      GLsizeiptr;

// 0.9.6.5 (GRUPO B · FERRAMENTAS DE VERIFICAÇÃO): o FRAMEBUFFER REAL.
// Com glstub::fb::enabled = true o stub deixa de ser no-op e RASTERIZA a
// sério (RGBA8 + depth f32) — as MESMAS regras do Mali pelas MESMAS funções
// que a engine chama (os DOIS shaders da casa + a grelha adaptativa).
// enabled = false (default) = o no-op de SEMPRE, zero mudança para os
// testes existentes; os CONTADORES de stats continuam a contar nos dois
// caminhos (os testes que lêem stats não mentem com o fb ligado).
#include "../glstub_fb.h"

#define GL_FALSE 0
#define GL_TRUE  1

#define GL_R8 0x8229
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_RED 0x1903
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_2D 0x0DE1
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_UNSIGNED_SHORT 0x1403

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE0 0x84C0
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_LESS 0x0201

inline void glGenTextures(GLint n, GLuint* t) {
    glstub::stats.genTextures += (int)n;
    if (glstub::failNextGenObjects) { if (t) for (GLint i = 0; i < n; ++i) t[i] = 0u; return; }
    if (glstub::fb::enabled) {
        if (t) for (GLint i = 0; i < n; ++i) {
            t[i] = glstub::fb::genObjects();
            glstub::fb::onGenTexture(t[i]);
        }
        return;
    }
    if (t) for (GLint i = 0; i < n; ++i) t[i] = 1u + i;
}
inline void glDeleteTextures(GLint n, const GLuint* t) {
    glstub::stats.deleteTextures += (int)n;
    if (glstub::fb::enabled && t) for (GLint i = 0; i < n; ++i) glstub::fb::delTexture(t[i]);
}
inline void glBindTexture(GLenum, GLuint t) {
    ++glstub::stats.boundTextures;
    glstub::stats.lastBoundTexture = t;
    if (glstub::fb::enabled) glstub::fb::bindTexture(t);
}
inline void glPixelStorei(GLenum, GLint) {}
inline void glTexImage2D(GLenum, GLint, GLint internalformat, GLsizei w, GLsizei h, GLint, GLenum format, GLenum, const void* pixels) {
    ++glstub::stats.texImage2D;
    if (glstub::fb::enabled) glstub::fb::texImage2D(internalformat, w, h, format, pixels);
}
inline void glCompressedTexImage2D(GLenum, GLint, GLsizei, GLsizei w, GLsizei h, GLint, GLsizei, const void*) {
    ++glstub::stats.compressedTexImage2D;
    if (glstub::fb::enabled) glstub::fb::compressedTexImage2D(w, h);
}
inline void glTexParameteri(GLenum, GLenum, GLint) {}
inline void glGenerateMipmap(GLenum) {}
inline void glActiveTexture(GLenum) {}

// F5.1-A: consulta de extensões (glAstcSupported usa glGetStringi)
#define GL_NUM_EXTENSIONS 0x821D
#define GL_EXTENSIONS 0x1F03
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
inline const GLubyte* glGetString(GLenum name) {
    if (name == GL_EXTENSIONS && glstub::astcLdr) {
        return reinterpret_cast<const GLubyte*>(
            "GL_OES_vertex_array_object GL_KHR_texture_compression_astc_ldr");
    }
    return reinterpret_cast<const GLubyte*>("");
}
inline void glGetIntegerv(GLenum p, GLint* v) { if (v) *v = 0; }
inline const GLubyte* glGetStringi(GLenum, GLuint index) {
    if (glstub::astcLdr && index == 0) {
        return reinterpret_cast<const GLubyte*>(
            "GL_KHR_texture_compression_astc_ldr");
    }
    return reinterpret_cast<const GLubyte*>("");
}
inline GLenum glGetError() { return 0; }

inline GLuint glCreateShader(GLenum) { return 1; }
inline void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
inline void glCompileShader(GLuint) {}
inline void glGetShaderiv(GLuint, GLenum, GLint* ok) { if (ok) *ok = GL_TRUE; }
inline void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glDeleteShader(GLuint) {}
inline GLuint glCreateProgram() {
    ++glstub::stats.createProgram;
    if (glstub::fb::enabled) {
        const GLuint id = glstub::fb::genObjects();
        glstub::fb::onCreateProgram(id);
        return id;
    }
    return 1;
}
inline void glAttachShader(GLuint, GLuint) {}
inline void glLinkProgram(GLuint) {}
inline void glGetProgramiv(GLuint, GLenum, GLint* ok) { if (ok) *ok = GL_TRUE; }
inline void glDeleteProgram(GLuint id) {
    ++glstub::stats.deleteProgram;
    if (glstub::fb::enabled) glstub::fb::delProgram(id);
}
inline void glUseProgram(GLuint id) { if (glstub::fb::enabled) glstub::fb::useProgram(id); }
inline GLint glGetUniformLocation(GLuint prog, const GLchar* name) {
    if (glstub::fb::enabled) return glstub::fb::getUniformLocation(prog, name);
    return 0;
}
inline void glUniformMatrix4fv(GLint loc, GLsizei count, GLboolean, const GLfloat* m) {
    ++glstub::stats.matrix4fvCalls;
    if (count > 1) {
        // 0.8.2: upload de ARRAY de matrizes (uBones do skin)
        ++glstub::stats.matrix4fvArrayCalls;
        glstub::stats.lastMatrix4fvCount = (int)count;
    }
    if (m) {
        for (int i = 0; i < 16; ++i) {
            glstub::stats.lastMatrix4fv[i] = m[i];
        }
    }
    if (glstub::fb::enabled) glstub::fb::uniformMatrix4fv(loc, count, m);
}
// 0.8.2: uSkin do LitMaterial (liga/desliga o ramo de bones)
inline void glUniform1i(GLint loc, GLint v) {
    glstub::stats.lastUniform1i = (int)v;
    ++glstub::stats.uniform1iCalls;
    if (glstub::fb::enabled) glstub::fb::uniform1i(loc, int(v));
}
inline void glUniform1f(GLint loc, GLfloat v) {
    glstub::stats.lastUniform1f = v;
    if (glstub::fb::enabled) glstub::fb::uniform1f(loc, v);
}
inline void glUniform3f(GLint loc, GLfloat x, GLfloat y, GLfloat z) {
    glstub::stats.lastUniform3f[0] = x;
    glstub::stats.lastUniform3f[1] = y;
    glstub::stats.lastUniform3f[2] = z;
    ++glstub::stats.uniform3fCalls;
    if (glstub::fb::enabled) glstub::fb::uniform3f(loc, x, y, z);
}
inline void glUniform2f(GLint loc, GLfloat x, GLfloat y) {
    if (glstub::fb::enabled) glstub::fb::uniform2f(loc, x, y);
}

inline void glGenBuffers(GLsizei n, GLuint* t) {
    glstub::stats.genBuffers += (int)n;
    if (glstub::failNextGenObjects) { if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 0u; return; }
    if (glstub::fb::enabled) {
        if (t) for (GLsizei i = 0; i < n; ++i) {
            t[i] = glstub::fb::genObjects();
            glstub::fb::onGenBuffer(t[i]);
        }
        return;
    }
    if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 1u + i;
}
inline void glDeleteBuffers(GLsizei n, const GLuint* t) {
    glstub::stats.deleteBuffers += (int)n;
    if (glstub::fb::enabled && t) for (GLsizei i = 0; i < n; ++i) glstub::fb::delBuffer(t[i]);
}
inline void glBindBuffer(GLenum target, GLuint id) {
    if (glstub::fb::enabled) glstub::fb::bindBuffer(target, id);
}
inline void glBufferData(GLenum target, intptr_t size, const void* data, GLenum) {
    ++glstub::stats.bufferData;
    if (glstub::fb::enabled) glstub::fb::bufferData(target, size, data);
}
inline void glGenVertexArrays(GLsizei n, GLuint* t) {
    glstub::stats.genVertexArrays += (int)n;
    if (glstub::failNextGenObjects) { if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 0u; return; }
    if (glstub::fb::enabled) { if (t) for (GLsizei i = 0; i < n; ++i) { t[i] = glstub::fb::genObjects(); glstub::fb::onGenVao(t[i]); } return; }
    if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 1u + i;
}
inline void glDeleteVertexArrays(GLsizei n, const GLuint* t) {
    glstub::stats.deleteVertexArrays += (int)n;
    if (glstub::fb::enabled && t) for (GLsizei i = 0; i < n; ++i) glstub::fb::delVao(t[i]);
}
inline void glBindVertexArray(GLuint id) {
    if (glstub::fb::enabled) glstub::fb::bindVao(id);
}
inline void glEnableVertexAttribArray(GLuint loc) {
    if (glstub::fb::enabled) glstub::fb::enableAttrib(int(loc));
}
inline void glVertexAttribPointer(GLuint loc, GLint size, GLenum, GLboolean, GLsizei stride, const void* off) {
    if (glstub::fb::enabled) glstub::fb::attribPointer(int(loc), size, stride, off);
}
inline void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    ++glstub::stats.drawArraysCalls;
    if (glstub::fb::enabled) glstub::fb::drawArrays(mode, first, count);
}
inline void glDrawElements(GLenum mode, GLsizei count, GLenum, const void*) {
    ++glstub::stats.drawElementsCalls;
    if (glstub::fb::enabled) glstub::fb::drawElements(mode, count);
}

// 0.7.8: os no-ops passam a GRAVAR o estado (inócuo p/ os testes antigos —
// apenas leituras novas; nada do que existia lia estas funções)
#define GL_SCISSOR_TEST 0x0C11
inline void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    ++glstub::stats.viewportCalls;
    glstub::stats.viewport[0] = x;
    glstub::stats.viewport[1] = y;
    glstub::stats.viewport[2] = w;
    glstub::stats.viewport[3] = h;
    if (glstub::fb::enabled) glstub::fb::setViewport(x, y, w, h);
}
inline void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if (glstub::fb::enabled) glstub::fb::setClearColor(r, g, b, a);
}
inline void glClear(GLenum mask) {
    if (glstub::fb::enabled) glstub::fb::clear(mask);
}
inline void glEnable(GLenum cap) {
    if (cap == GL_SCISSOR_TEST) glstub::stats.scissorEnabled = true;
    else if (cap == GL_DEPTH_TEST) glstub::stats.depthEnabled = true;
    else if (cap == GL_CULL_FACE) glstub::stats.cullEnabled = true;
    else if (cap == GL_BLEND) glstub::stats.blendEnabled = true;
    if (glstub::fb::enabled) {
        if (cap == GL_SCISSOR_TEST) glstub::fb::setScissorEnabled(true);
        else if (cap == GL_DEPTH_TEST) glstub::fb::setDepthEnabled(true);
        else if (cap == GL_CULL_FACE) glstub::fb::setCullEnabled(true);
        else if (cap == GL_BLEND) glstub::fb::setBlendEnabled(true);
    }
}
inline void glDisable(GLenum cap) {
    if (cap == GL_SCISSOR_TEST) glstub::stats.scissorEnabled = false;
    else if (cap == GL_DEPTH_TEST) glstub::stats.depthEnabled = false;
    else if (cap == GL_CULL_FACE) glstub::stats.cullEnabled = false;
    else if (cap == GL_BLEND) glstub::stats.blendEnabled = false;
    if (glstub::fb::enabled) {
        if (cap == GL_SCISSOR_TEST) glstub::fb::setScissorEnabled(false);
        else if (cap == GL_DEPTH_TEST) glstub::fb::setDepthEnabled(false);
        else if (cap == GL_CULL_FACE) glstub::fb::setCullEnabled(false);
        else if (cap == GL_BLEND) glstub::fb::setBlendEnabled(false);
    }
}
inline void glDepthMask(GLboolean b) {
    if (glstub::fb::enabled) glstub::fb::setDepthMask(b != 0);
}
inline void glDepthFunc(GLenum) {}
inline void glBlendFunc(GLenum, GLenum) {}
inline void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum, GLenum, void* dst) {
    if (glstub::fb::enabled) glstub::fb::readPixels(x, y, w, h, dst);
}

inline void glScissor(int x, int y, int w, int h) {
    if (glstub::fb::enabled) glstub::fb::setScissor(x, y, w, h);
}
