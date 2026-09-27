#pragma once
// platform/EglContext.h — init EGL → GLES3 para NativeActivity.
// Responsabilidade única: display/config/context/surface + makeCurrent + swap.
#include "core/Types.h"
#include <EGL/egl.h>
#include <android/native_window.h>

namespace vv {

class EglContext {
public:
    bool init(ANativeWindow* window);   // display → config GLES3 → contexto 3 → surface → current
    void refreshSize();                 // re-consulta EGL_WIDTH/EGL_HEIGHT (APP_CMD_WINDOW_RESIZED)
    void swap();                        // eglSwapBuffers
    void shutdown();                    // destroy surface/context (mantém o display do processo)

    i32 width() const { return width_; }
    i32 height() const { return height_; }

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    i32 width_ = 0;
    i32 height_ = 0;
};

} // namespace vv
