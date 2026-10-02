// tests/stub/EGL/egl.h — stub de hospedeiro: declarações mínimas para
// COMPILAR platform/main.cpp + EglContext.h no CI (paridade de sintaxe com
// o NDK; nunca liga — as funções reais vivem no device).
#pragma once
#include <cstdint>

typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef int32_t EGLint;
typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;

#define EGL_WIDTH  0x3057
#define EGL_HEIGHT 0x3056

// F5.2: constantes usadas pelo EglContext.cpp (paridade de LINK, sem valores
// reais — no device o <EGL/egl.h> do sistema define tudo)
#define EGL_TRUE 1
#define EGL_FALSE 0
#define EGL_SUCCESS 0x3000
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_NONE 0x3038
#define EGL_OPENGL_ES 0x3099
#define EGL_OPENGL_ES3_BIT 0x0040
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

// 0.8.7 (test_wiring087): o stub EGL deixa de ser só "declaração que
// compila" — init() SEGUE o caminho feliz (display/config/context/surface
// não-nulos, makeCurrent OK) e eglQuerySurface devolve 1280×720 (a
// paisagem do C33). O teste do wiring inclui platform/main.cpp e corre o
// BOOT REAL (onAppCmd INIT_WINDOW) + frame() no hospedeiro — com o
// lifecycle de destroy/recreate a cargo do stub GLES3 (contadores).
inline EGLDisplay eglGetDisplay(EGLNativeDisplayType) {
    return reinterpret_cast<EGLDisplay>(0x1234);   // display fake não-nulo
}
inline int eglInitialize(EGLDisplay, EGLint*, EGLint*) { return 1; }
inline int eglTerminate(EGLDisplay) { return 1; }
inline int eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig* cfg, EGLint, EGLint* n) {
    if (n) *n = 1;
    if (cfg) *cfg = reinterpret_cast<EGLConfig>(0x5678);
    return 1;
}
inline EGLSurface eglCreateWindowSurface(EGLDisplay, EGLConfig, void*, const EGLint*) {
    return reinterpret_cast<EGLSurface>(0x9ABC);
}
inline int eglDestroySurface(EGLDisplay, EGLSurface) { return 1; }
inline EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*) {
    return reinterpret_cast<EGLContext>(0xDEF0);
}
inline int eglDestroyContext(EGLDisplay, EGLContext) { return 1; }
inline int eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext) { return 1; }
inline int eglSwapBuffers(EGLDisplay, EGLSurface) { return 1; }
inline int eglQuerySurface(EGLDisplay, EGLSurface, EGLint attr, EGLint* v) {
    if (v) { *v = (attr == EGL_WIDTH) ? 1280 : 720; }   // paisagem do C33
    return 1;
}
inline int eglGetError() { return 0; }                        // F5.2 link parity
inline int eglSwapInterval(EGLDisplay, EGLint) { return 1; }  // F5.2 link parity

#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
