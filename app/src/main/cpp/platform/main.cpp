// platform/main.cpp — android_main (glue) da G.One VV 0.1.0 — F1.
// Fluxo: glue → EGL/GLES3 → loop de timestep fixo → UI immediate-mode → swap.
#include <android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <cstdio>

#include "core/Scene.h"
#include "core/Time.h"
#include "platform/Log.h"
#include "platform/CrashHandler.h"
#include "platform/EglContext.h"
#include "platform/InputState.h"
#include "render/Renderer.h"
#include "ui/FontAtlas.h"
#include "ui/UiContext.h"

using namespace vv;

namespace {

EglContext g_egl;
Renderer   g_renderer;
FontAtlas  g_font;
UiContext  g_ui;
InputState g_input;
Scene      g_scene;
Time       g_time;
bool       g_ready = false;

f32    g_fps = 0.0f;
double g_fpsAccum = 0.0;
u32    g_fpsFrames = 0;

void onAppCmd(android_app* app, i32 cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (!g_egl.init(app->window)) {
                LOGE("boot: EGL falhou");
                break;
            }
            if (!g_renderer.init()) {
                LOGE("boot: renderer falhou");
                break;
            }
            g_renderer.resize(g_egl.width(), g_egl.height());
            // F1 sem assets: fonte do sistema (primeira que existir vence)
            if (!g_font.loadFromPaths(kSystemFontPaths, kSystemFontPathCount, 28.0f)) {
                LOGE("boot: nenhuma fonte do sistema carregada — UI sem texto");
            }
            g_ui.init();
            g_ui.setFont(&g_font);
            g_ready = true;
            LOGI("boot: janela pronta %dx%d", (int)g_egl.width(), (int)g_egl.height());
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            g_egl.refreshSize();
            break;
        case APP_CMD_TERM_WINDOW:
            g_ready = false;
            g_renderer.shutdown();
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

void frame() {
    const f32 w = static_cast<f32>(g_egl.width());
    const f32 h = static_cast<f32>(g_egl.height());

    g_renderer.beginFrame();   // clear mono BG
    g_ui.beginFrame(&g_renderer, &g_input, w, h);

    bool clicks[3] = {false, false, false};
    g_ui.toolbar(clicks);   // exatamente 3 botões (Menu, Play, Settings)
    if (clicks[0]) { LOGI("ui: botão Menu"); }
    if (clicks[1]) { LOGI("ui: botão Play"); }
    if (clicks[2]) { LOGI("ui: botão Settings"); }

    // status line inferior: fps + contagem de TICs
    char status[64];
    std::snprintf(status, sizeof(status), "fps %d  tics %u",
                  static_cast<int>(g_fps + 0.5f), g_scene.count());
    g_ui.statusLine(status);

    g_ui.endFrame();
    g_renderer.endFrame();
    g_egl.swap();
    g_input.clearEdges();   // edges já consumidas pela UI neste frame
}

} // namespace

void android_main(android_app* app) {
    // diagnóstico: crash log em <internalDataPath>/goni_crash.log (passo 7)
    installCrashHandler(app->activity ? app->activity->internalDataPath : nullptr);

    app->onAppCmd = onAppCmd;
    app->onInputEvent = onInputEvent;
    LOGI("G.One VV 0.1.0 — F1 (passo 6: UI immediate-mode)");

    double last = nowSeconds();
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
            last = nowSeconds();
            continue;
        }

        const double nowT = nowSeconds();
        const double realDt = nowT - last;
        last = nowT;

        // Loop de timestep fixo (guard anti-spiral dentro de Time).
        const u32 steps = g_time.beginFrame(realDt);
        for (u32 s = 0; s < steps; ++s) {
            // update fixo — F1: sem componentes/sistemas (apenas consome o passo)
            g_time.endStep();
        }

        frame();

        g_fpsAccum += realDt;
        ++g_fpsFrames;
        if (g_fpsAccum >= 0.5) {
            g_fps = static_cast<f32>(g_fpsFrames / g_fpsAccum);
            g_fpsAccum = 0.0;
            g_fpsFrames = 0;
        }
    }
}
