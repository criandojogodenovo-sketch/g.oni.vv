// GLES3/gl3.h — stub de hospedeiro (CI/diagnóstico): nenhuma GL real, tudo
// no-op. Suficiente para compilar FontAtlas/Renderer sem NDK; os objetos GL
// são fictícios e nunca são desenhados (os testes leem os batches de CPU).
#pragma once
#include <cstdint>

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

inline void glGenTextures(GLint n, GLuint* t) { if (t) for (GLint i = 0; i < n; ++i) t[i] = 1u + i; }
inline void glDeleteTextures(GLint, const GLuint*) {}
inline void glBindTexture(GLenum, GLuint) {}
inline void glPixelStorei(GLenum, GLint) {}
inline void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}
inline void glTexParameteri(GLenum, GLenum, GLint) {}
inline void glGenerateMipmap(GLenum) {}
inline void glActiveTexture(GLenum) {}

inline GLuint glCreateShader(GLenum) { return 1; }
inline void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
inline void glCompileShader(GLuint) {}
inline void glGetShaderiv(GLuint, GLenum, GLint* ok) { if (ok) *ok = GL_TRUE; }
inline void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glDeleteShader(GLuint) {}
inline GLuint glCreateProgram() { return 1; }
inline void glAttachShader(GLuint, GLuint) {}
inline void glLinkProgram(GLuint) {}
inline void glGetProgramiv(GLuint, GLenum, GLint* ok) { if (ok) *ok = GL_TRUE; }
inline void glDeleteProgram(GLuint) {}
inline void glUseProgram(GLuint) {}
inline GLint glGetUniformLocation(GLuint, const GLchar*) { return 0; }
inline void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*) {}
inline void glUniform1i(GLint, GLint) {}
inline void glUniform1f(GLint, GLfloat) {}
inline void glUniform3f(GLint, GLfloat, GLfloat, GLfloat) {}

inline void glGenBuffers(GLsizei n, GLuint* t) { if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 1u + i; }
inline void glDeleteBuffers(GLsizei, const GLuint*) {}
inline void glBindBuffer(GLenum, GLuint) {}
inline void glBufferData(GLenum, intptr_t, const void*, GLenum) {}
inline void glGenVertexArrays(GLsizei n, GLuint* t) { if (t) for (GLsizei i = 0; i < n; ++i) t[i] = 1u + i; }
inline void glDeleteVertexArrays(GLsizei, const GLuint*) {}
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
