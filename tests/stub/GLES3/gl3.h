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
#pragma once
#include <cstdint>

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
};
inline Stats stats;              // inline C++17: 1 instância por binário
inline void reset() { stats = Stats{}; }
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

inline void glGenTextures(GLint n, GLuint* t) { glstub::stats.genTextures += (int)n; if (t) for (GLint i = 0; i < n; ++i) t[i] = 1u + i; }
inline void glDeleteTextures(GLint n, const GLuint*) { glstub::stats.deleteTextures += (int)n; }
inline void glBindTexture(GLenum, GLuint t) { ++glstub::stats.boundTextures; glstub::stats.lastBoundTexture = t; }
inline void glPixelStorei(GLenum, GLint) {}
inline void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) { ++glstub::stats.texImage2D; }
inline void glCompressedTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLsizei, const void*) { ++glstub::stats.compressedTexImage2D; }
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
inline const GLubyte* glGetString(GLenum) { return reinterpret_cast<const GLubyte*>(""); }
inline void glGetIntegerv(GLenum p, GLint* v) { if (v) *v = (p == GL_NUM_EXTENSIONS) ? 0 : 0; }
inline const GLubyte* glGetStringi(GLenum, GLuint) { return reinterpret_cast<const GLubyte*>(""); }
inline GLenum glGetError() { return 0; }

inline GLuint glCreateShader(GLenum) { return 1; }
inline void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
inline void glCompileShader(GLuint) {}
inline void glGetShaderiv(GLuint, GLenum, GLint* ok) { if (ok) *ok = GL_TRUE; }
inline void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glDeleteShader(GLuint) {}
inline GLuint glCreateProgram() { ++glstub::stats.createProgram; return 1; }
inline void glAttachShader(GLuint, GLuint) {}
inline void glLinkProgram(GLuint) {}
inline void glGetProgramiv(GLuint, GLenum, GLint* ok) { if (ok) *ok = GL_TRUE; }
inline void glDeleteProgram(GLuint) { ++glstub::stats.deleteProgram; }
inline void glUseProgram(GLuint) {}
inline GLint glGetUniformLocation(GLuint, const GLchar*) { return 0; }
inline void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*) {}
inline void glUniform1i(GLint, GLint) {}
inline void glUniform1f(GLint, GLfloat v) { glstub::stats.lastUniform1f = v; }
inline void glUniform3f(GLint, GLfloat x, GLfloat y, GLfloat z) {
    glstub::stats.lastUniform3f[0] = x;
    glstub::stats.lastUniform3f[1] = y;
    glstub::stats.lastUniform3f[2] = z;
    ++glstub::stats.uniform3fCalls;
}
inline void glUniform2f(GLint, GLfloat, GLfloat) {}

inline void glGenBuffers(GLsizei n, GLuint* t) { glstub::stats.genBuffers += (int)n; if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 1u + i; }
inline void glDeleteBuffers(GLsizei n, const GLuint*) { glstub::stats.deleteBuffers += (int)n; }
inline void glBindBuffer(GLenum, GLuint) {}
inline void glBufferData(GLenum, intptr_t, const void*, GLenum) { ++glstub::stats.bufferData; }
inline void glGenVertexArrays(GLsizei n, GLuint* t) { glstub::stats.genVertexArrays += (int)n; if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 1u + i; }
inline void glDeleteVertexArrays(GLsizei n, const GLuint*) { glstub::stats.deleteVertexArrays += (int)n; }
inline void glBindVertexArray(GLuint) {}
inline void glEnableVertexAttribArray(GLuint) {}
inline void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {}
inline void glDrawArrays(GLenum, GLint, GLsizei) {}
inline void glDrawElements(GLenum, GLsizei, GLenum, const void*) {}

inline void glViewport(GLint, GLint, GLsizei, GLsizei) {}
inline void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
inline void glClear(GLenum) {}
inline void glEnable(GLenum) {}
inline void glDisable(GLenum) {}
inline void glDepthMask(GLboolean) {}
inline void glDepthFunc(GLenum) {}
inline void glBlendFunc(GLenum, GLenum) {}
inline void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) {}
