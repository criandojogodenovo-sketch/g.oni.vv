#include "platform/EglContext.h"
#include "platform/Log.h"

namespace vv {

bool EglContext::init(ANativeWindow* window) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("EGL: display nulo");
        return false;
    }
    // 0.12-Z2-F1BIS: major/minor REAIS — o eglInitialize(dpy, nullptr,
    // nullptr) é legal por spec mas o glvnd (libEGL de hospedeiro, Mesa)
    // devolve EGL_FALSE 0x3001 com ponteiros nulos (PROVADO por probe
    // isolado com os headers de Khronos: nullptr FALHA, &maj/&mn OK — o
    // raster harness não arrancava). No Android o padrão &maj/&mn é o
    // idiomático; nenhum device muda.
    EGLint eglVerMajor = 0;
    EGLint eglVerMinor = 0;
    if (!eglInitialize(display_, &eglVerMajor, &eglVerMinor)) {
        LOGE("EGL: initialize falhou (0x%x)", eglGetError());
        return false;
    }

    const EGLint cfgAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,   // F2: depth buffer para o depth test do pass 3D
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(display_, cfgAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
        LOGE("EGL: nenhum config GLES3 (0x%x)", eglGetError());
        return false;
    }

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, ctxAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("EGL: contexto GLES3 falhou (0x%x)", eglGetError());
        return false;
    }

    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("EGL: surface falhou (0x%x)", eglGetError());
        // 0.12-Z2-F1BIS: AUTO-LIMPEZA — o contexto criado acima não fica
        // sem dono (o próximo init não cria um segundo)
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        return false;
    }

    if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
        LOGE("EGL: makeCurrent falhou (0x%x)", eglGetError());
        // 0.12-Z2-F1BIS: AUTO-LIMPEZA (surface + contexto)
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
        return false;
    }

    refreshSize();
    eglSwapInterval(display_, 1);   // vsync
    lastSwapOk_ = true;   // 0.12-Z2-F1BIS: o estado do swap recomeça limpo
    LOGI("EGL: GLES3 pronto (%dx%d)", (int)width_, (int)height_);
    return true;
}

void EglContext::refreshSize() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglQuerySurface(display_, surface_, EGL_WIDTH, &width_);
        eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);
    }
}

void EglContext::swap() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        // 0.12-Z2-F1BIS: o RETORNO do swap é VERIFICADO — o EGL_FALSE
        // aqui (EGL_BAD_NATIVE_WINDOW/EGL_CONTEXT_LOST) é sinal de perda
        // e alimenta a sonda do frame (contextLostNow)
        lastSwapOk_ = (eglSwapBuffers(display_, surface_) == EGL_TRUE);
    }
}

void EglContext::shutdown() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
    }
    width_ = 0;
    height_ = 0;
}

} // namespace vv
