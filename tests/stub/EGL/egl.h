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

#define EGL_WIDTH  0x3057
#define EGL_HEIGHT 0x3056

inline EGLDisplay eglGetDisplay(void*) { return nullptr; }
inline int eglInitialize(EGLDisplay, EGLint*, EGLint*) { return 1; }
inline int eglTerminate(EGLDisplay) { return 1; }
inline int eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*) { return 1; }
inline EGLSurface eglCreateWindowSurface(EGLDisplay, EGLConfig, void*, const EGLint*) { return nullptr; }
inline int eglDestroySurface(EGLDisplay, EGLSurface) { return 1; }
inline EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*) { return nullptr; }
inline int eglDestroyContext(EGLDisplay, EGLContext) { return 1; }
inline int eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext) { return 1; }
inline int eglSwapBuffers(EGLDisplay, EGLSurface) { return 1; }
inline int eglQuerySurface(EGLDisplay, EGLSurface, EGLint, EGLint*) { return 1; }

#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
