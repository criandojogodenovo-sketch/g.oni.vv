#pragma once
// platform/EglContext.h — init EGL → GLES3 para NativeActivity.
// Responsabilidade única: display/config/context/surface + makeCurrent + swap.
// 0.12-Z2 F1: o swap TRATADO — eglSwapBuffers devolve EGL_FALSE com
// EGL_CONTEXT_LOST quando a GPU morre (o device real); o retorno era
// IGNORADO. Agora swap() devolve bool e GUARDA o eglGetError da falha
// (lastSwapError) — o chamador (swapAndWatch do main) decide a
// recuperação. Nada mais muda: o caminho feliz é byte a byte.
// 0.12-Z2-F1BIS: o lastSwapOk_ (o ÚLTIMO swap foi TRUE?) alimenta a
// sonda de perda do frame (contextLostNow); o init repõe true. E o
// F1-R FUNDE os dois: o ackSwapLoss() — o contextRecover ACEITA a
// falha já tratada (a sonda do frame seguinte não re-dispara a perda
// antiga; o próximo swap reescreve a verdade).
#include "core/Types.h"
#include <EGL/egl.h>
#include <android/native_window.h>

namespace vv {

class EglContext {
public:
    bool init(ANativeWindow* window);   // display → config GLES3 → contexto 3 → surface → current
    void refreshSize();                 // re-consulta EGL_WIDTH/EGL_HEIGHT (APP_CMD_WINDOW_RESIZED)
    bool swap();                        // eglSwapBuffers — false na falha (ver lastSwapError)
    void shutdown();                    // destroy surface/context (mantém o display do processo)

    i32 width() const { return width_; }
    i32 height() const { return height_; }

    // 0.12-Z2-F1BIS: o ÚLTIMO eglSwapBuffers foi TRUE? (o false alimenta
    // a sonda de perda do frame — contextLostNow; o init repõe true)
    bool lastSwapOk() const { return lastSwapOk_; }

    // 0.12-Z2 F1: o erro EGL da ÚLTIMA falha de swap (0 se a última
    // chamada correu bem — nunca «esquece» o erro de uma falha)
    EGLint lastSwapError() const { return lastSwapError_; }

    // 0.12-Z2 F1-R: a perda do último swap foi ACEITE e TRATADA (o
    // contextRecover correu) — o lastSwapOk_ volta a true para a sonda
    // do frame seguinte NÃO re-disparar a falha antiga (o desenho por
    // causa do swapAndWatch mantém-se: o BAD_SURFACE NÃO é perda); o
    // lastSwapError_ fica (o pin troca-o a ferro — «nunca esquece»)
    void ackSwapLoss() { lastSwapOk_ = true; }

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    i32 width_ = 0;
    i32 height_ = 0;
    bool lastSwapOk_ = true;   // 0.12-Z2-F1BIS: o estado do último swap
    EGLint lastSwapError_ = 0; // 0.12-Z2 F1: a causa da última falha
};

} // namespace vv
