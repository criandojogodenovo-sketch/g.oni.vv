// platform/main.cpp — android_main (glue) da G.One VV 0.1.0 — F1.
// Passo 3 (platform): EGL→GLES3, clear cinza mono, AInputQueue→InputState.
// O loop de timestep fixo entra no passo 4 (core); render/UI nos passos 5-6.
#include <android_native_app_glue.h>
#include <GLES3/gl3.h>

#include "platform/Log.h"
#include "platform/EglContext.h"
#include "platform/InputState.h"

using namespace vv;

namespace {

EglContext g_egl;
InputState g_input;
bool g_ready = false;

void onAppCmd(android_app* app, i32 cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            g_ready = g_egl.init(app->window);
            if (!g_ready) {
                LOGE("boot: EGL falhou");
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            g_egl.refreshSize();
            break;
        case APP_CMD_TERM_WINDOW:
            g_ready = false;
            g_egl.shutdown();
            break;
        case APP_CMD_DESTROY:
            g_ready = false;
            break;
        default:
            break;
    }
}

i32 onInputEvent(android_app* /*app*/, AInputEvent* event) {
    return g_input.process(event) ? 1 : 0;
}

} // namespace

void android_main(android_app* app) {
    app->onAppCmd = onAppCmd;
    app->onInputEvent = onInputEvent;
    LOGI("G.One VV 0.1.0 — F1 (passo 3: platform)");

    while (true) {
        int ident = 0, events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollAll(g_ready ? 0 : -1, nullptr, &events,
                                        reinterpret_cast<void**>(&source))) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                LOGI("loop: destroyRequested — fim");
                return;
            }
        }
        if (!g_ready) {
            continue;
        }
        glClearColor(0.0784314f, 0.0784314f, 0.0784314f, 1.0f);   // BG #141414
        glClear(GL_COLOR_BUFFER_BIT);
        g_egl.swap();
        g_input.clearEdges();   // edges já consumidas neste frame
    }
}
