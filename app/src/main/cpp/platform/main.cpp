// platform/main.cpp — android_main (glue) da G.One VV — F2.
// Fluxo: glue → EGL/GLES3 (depth 24) → loop de timestep fixo →
//        pass 3D (cubo+grid, depth test) → pass UI (sem depth) → swap.
#include <android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>

#include "core/Scene.h"
#include "core/Time.h"
#include "platform/Log.h"
#include "platform/CrashHandler.h"
#include "platform/EglContext.h"
#include "platform/InputState.h"
#include "render/Camera.h"
#include "render/Cube.h"
#include "render/Grid.h"
#include "render/Mesh.h"
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

// ---- F2: viewport 3D -------------------------------------------------------
Camera g_camera;              // orbit: 1 dedo = yaw/pitch, pinch = zoom
Mesh   g_cubeMesh;            // cubo procedural (PLACEHOLDER — F5 importa mesh)
Grid   g_grid;                // grid de chão com fade (PLACEHOLDER — F8)

// estado do touch → câmara (entre frames)
bool g_orbitActive = false;
f32  g_orbitX = 0.0f;
f32  g_orbitY = 0.0f;
f32  g_pinchPrev = 0.0f;
constexpr f32 kOrbitSens = 0.0075f;   // rad/px (~0,43° por pixel)

DrawStats g_lastUiStats;   // métricas do pass UI (disponíveis 1 frame depois)

void updateCameraOrbit(const InputState& in) {
    const u32 active = in.activePointers();
    if (active >= 2) {
        // pinch: distância entre os dois primeiros dedos ativos
        f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool got0 = false, got1 = false;
        for (u32 s = 0; s < kMaxPointerSlots && !(got0 && got1); ++s) {
            if (!in.down(s)) continue;
            f32 x, y;
            in.pos(s, x, y);
            if (!got0) { x0 = x; y0 = y; got0 = true; }
            else       { x1 = x; y1 = y; got1 = true; }
        }
        if (got0 && got1) {
            const f32 d = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            if (g_pinchPrev > 0.0f && d > 1.0f) {
                g_camera.zoomBy(g_pinchPrev / d);   // dedos afastam → aproxima
            }
            g_pinchPrev = d;
        }
        g_orbitActive = false;
        return;
    }
    g_pinchPrev = 0.0f;
    if (active == 1) {
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            if (!in.down(s)) continue;
            f32 x, y;
            in.pos(s, x, y);
            if (g_orbitActive) {
                g_camera.orbit((x - g_orbitX) * kOrbitSens,
                               (y - g_orbitY) * kOrbitSens);
            }
            g_orbitX = x;
            g_orbitY = y;
            g_orbitActive = true;
            break;
        }
    } else {
        g_orbitActive = false;
    }
}
// ---------------------------------------------------------------------------

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
            // F2: geometria procedural do viewport 3D
            {
                const CubeMeshData cube = makeCube(1.0f);
                if (!g_cubeMesh.create(cube.vertices.data(),
                                       static_cast<u32>(cube.vertices.size()),
                                       cube.indices.data(),
                                       static_cast<u32>(cube.indices.size()))) {
                    LOGE("boot: mesh do cubo falhou");
                }
                if (!g_grid.init()) {
                    LOGE("boot: grid de chão falhou");
                }
            }
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
            g_renderer.resize(g_egl.width(), g_egl.height());
            break;
        case APP_CMD_TERM_WINDOW:
            g_ready = false;
            g_cubeMesh.destroy();
            g_grid.destroy();
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

    // input do frame anterior → câmara (orbit/pinch), antes de clearEdges
    updateCameraOrbit(g_input);

    // ---- pass 3D: clear color+depth, cubo assente no grid, linhas com fade
    g_renderer.beginFrame();
    const Mat4 view = g_camera.view();
    const Mat4 proj = g_camera.proj(w / h);
    const Mat4 vp = Mat4::mul(proj, view);
    const Mat4 model = Mat4::translation(0.0f, 0.5f, 0.0f);   // cubo assente no chão
    const DrawStats st3d = g_renderer.drawMesh(g_cubeMesh, model, vp);
    const DrawStats stGrid = g_grid.draw(vp, g_camera.eye());

    // ---- pass UI: immediate-mode da F1 por cima (sem depth — nunca ocluída)
    g_ui.beginFrame(&g_renderer, &g_input, w, h);

    bool clicks[3] = {false, false, false};
    g_ui.toolbar(clicks);   // exatamente 3 botões (Menu, Play, Settings)
    if (clicks[0]) { LOGI("ui: botão Menu"); }
    if (clicks[1]) { LOGI("ui: botão Play"); }
    if (clicks[2]) { LOGI("ui: botão Settings"); }

    // status line inferior: fps + TICs + vértices desenhados + draw calls
    // (parte da UI usa as métricas do frame anterior — lag de 1 frame)
    const DrawStats total = st3d + stGrid + g_lastUiStats;
    char status[96];
    std::snprintf(status, sizeof(status), "fps %d  tics %u  verts %u  dc %u",
                  static_cast<int>(g_fps + 0.5f), g_scene.count(),
                  total.vertices, total.drawCalls);
    g_ui.statusLine(status);

    g_ui.endFrame();                       // submete solids + glyphs
    g_lastUiStats = g_renderer.endFrame(); // desenha a UI por cima do 3D
    g_egl.swap();
    g_input.clearEdges();   // edges já consumidas pela UI neste frame
}

} // namespace

void android_main(android_app* app) {
    // diagnóstico: crash log em <internalDataPath>/goni_crash.log (passo 7)
    installCrashHandler(app->activity ? app->activity->internalDataPath : nullptr);

    app->onAppCmd = onAppCmd;
    app->onInputEvent = onInputEvent;
    LOGI("G.One VV 0.2.0 — F2 (passo 6: primeiro objeto 3D)");

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
