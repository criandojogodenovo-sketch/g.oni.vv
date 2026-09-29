// platform/main.cpp — android_main (glue) da G.One VV — F3.
// Fluxo: glue → EGL/GLES3 (depth 24) → loop de timestep fixo
//        → TickGroups (Pre→Up→Post→Render; TransformSystem no Update)
//        → pass 3D (todos os TICs com MeshRenderer + grid, depth test)
//        → pass UI (toolbar F1 + Hierarchy/Inspector/menus F3, sem depth).
#include <android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>
#include <memory>

#include "assets/ObjExporter.h"
#include "assets/TextureCache.h"
#include "assets/TexturePipeline.h"
#include "components/MeshRenderer.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "core/FsStorage.h"
#include "core/PlaySnapshot.h"
#include "core/Presets.h"
#include "core/Project.h"
#include "core/SafStorage.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/Tick.h"
#include "core/Time.h"
#include "core/TransformSystem.h"
#include "physics/InputSource.h"
#include "physics/PhysicsSystem.h"
#include "platform/Log.h"
#include "platform/EngineLog.h"
#include "platform/CrashHandler.h"
#include "platform/EglContext.h"
#include "platform/InputState.h"
#include "platform/SafBridge.h"
#include "render/Camera.h"
#include "render/Cube.h"
#include "render/Grid.h"
#include "render/GpuAssets.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "ui/EditorUi.h"
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
Camera g_camera;              // orbit: 1 dedo = yaw/pitch, pinch = zoom (FIXA — F8)
Mesh   g_cubeMesh;            // cubo procedural (PLACEHOLDER — F5 importa mesh)
Grid   g_grid;                // grid de chão: quad 4 vértices + shader (F3.1; F8 pode virar gizmo)

// ---- F3: editor ------------------------------------------------------------
editor::EditorState g_editor;        // seleção + overlays
TickGroups          g_systems;       // runner (não-dono)
TransformSystem     g_transformSystem;

// ---- F4: física + modo Play ------------------------------------------------
phys::PhysicsSystem g_physics;       // TickGroup::Physics (só avança em Play)
bool                g_playMode = false;   // botão Play da toolbar liga/desliga
// F4.2/B3: sandbox do Play — pose de editor capturada ao ENTRAR, restaurada
// ao SAIR (a simulação é descartada; a física continua a correr só no Play)
PlaySnapshot        g_playSnap;


// ---- F5-A: projeto .goni + storage -----------------------------------------
// Raiz = getExternalFilesDir (externalDataPath; fallback internalDataPath).
// O projeto vive nesta pasta: project.goni + scenes/ + meshes/ + textures/.
// Boot abre o projeto existente ou cria "projeto" — e recarrega a cena
// ativa (persistida no manifesto) após cada arranque.
std::unique_ptr<FsStorage> g_storage;
Project    g_project;
bool       g_projectReady = false;   // storage + projeto com cena válida

// ---- F5.1-C: SAF — pasta do projeto escolhida pelo utilizador ---------------
// Router: FsStorage (getExternalFilesDir) = primário; SafStorage (URI
// concedida) = secundário. Cancelou/URI inválida → primário (fallback).
RoutingStorage* g_router = nullptr;   // construído no boot (nunca nulo no loop)
std::unique_ptr<SafStorage> g_safStorage;
saf::SafStateMachine        g_safState;
std::vector<u8>  g_pendingExport;   // bytes do export à espera da URI
std::string      g_pendingExportName;

// ---- F5-E: assets de runtime (cache CPU → cache GPU) ------------------------
// Refs relativas ("meshes/x.obj#0", "textures/y.png") → MeshData/RawImage no
// ResourceManager (CPU, 1 parse) → Mesh*/Texture* no GpuAssets (GL, 1 upload).
// TICs que partilham a mesma ref partilham o MESMO objeto de GPU.
ResourceManager g_resources;
GpuAssets       g_gpu;

// F5.1-A: compressão + cache de texturas (o HardwareCompressor decide
// ASTC vs ETC2 conforme a extensão detectada no boot; o cache em disco
// evita re-comprimir o mesmo PNG entre arranques)
HardwareCompressor g_hwCompressor;
std::unique_ptr<TextureCache>    g_texCache;
std::unique_ptr<TexturePipeline> g_pipeline;

// F5-E: catálogo de assets p/ os seletores do Inspector (refresh ao abrir)
editor::AssetCatalog g_catalog;
int                  g_prevAssetMenu = 0;

char g_selectedName[40] = "";   // nome do TIC p/ o ficheiro de export

// estado do touch → câmara (entre frames)
bool g_orbitActive = false;
f32  g_orbitX = 0.0f;
f32  g_orbitY = 0.0f;
f32  g_pinchPrev = 0.0f;
bool g_gestureInView = false;        // F3: gesto nasce só dentro do viewport central
constexpr f32 kOrbitSens = 0.0075f;  // rad/px (~0,43° por pixel)

DrawStats g_lastUiStats;   // métricas do pass UI (disponíveis 1 frame depois)

// toast (mensagem transitória acima da status line — feedback Save/Load/criação)
char g_toast[96] = "";
f32  g_toastT = 0.0f;

void showToast(const char* msg) {
    std::snprintf(g_toast, sizeof(g_toast), "%s", msg);
    g_toastT = 1.8f;
}

// F4.2 (B1): converte android_app->contentRect em Insets e injeta na UI.
// Diagnóstico no device: esta linha do logcat mostra a superfície EGL, o
// contentRect bruto e os insets resultantes (nav/status bar) — a causa raiz
// do scroll morto no Inspector era a altura visível inflada pela nav bar.
void applyContentRect(android_app* app) {
    const i32 sw = g_egl.width();
    const i32 sh = g_egl.height();
    const ARect& cr = app->contentRect;
    const safe::Insets ins = safe::insetsFromContentRect(
        static_cast<f32>(sw), static_cast<f32>(sh),
        cr.left, cr.top, cr.right, cr.bottom);
    g_ui.setSafeArea(ins);
    LOGI("safearea: surface %dx%d content [%d %d %d %d] "
         "insets L%.0f T%.0f R%.0f B%.0f",
         (int)sw, (int)sh, cr.left, cr.top, cr.right, cr.bottom,
         (double)ins.left, (double)ins.top, (double)ins.right, (double)ins.bottom);
}

// F5-E: atualiza o catálogo (listDir nas pastas do projeto, filtro por ext.)
void refreshCatalog() {
    g_catalog.meshes.clear();
    g_catalog.textures.clear();
    if (!g_router) {
        return;
    }
    std::vector<std::string> files;
    if (g_router->listDir(Project::kDirMeshes, files)) {
        for (const std::string& f : files) {
            const size_t dot = f.rfind('.');
            const std::string ext = dot == std::string::npos ? "" : f.substr(dot + 1);
            if (ext == "obj" || ext == "gltf" || ext == "glb" ||
                ext == "OBJ" || ext == "glTF" || ext == "GLB") {
                g_catalog.meshes.push_back(f);
            }
        }
    }
    files.clear();
    if (g_router->listDir(Project::kDirTextures, files)) {
        for (const std::string& f : files) {
            const size_t dot = f.rfind('.');
            const std::string ext = dot == std::string::npos ? "" : f.substr(dot + 1);
            if (ext == "png" || ext == "PNG") {
                g_catalog.textures.push_back(f);
            }
        }
    }
}

// F5-E: MeshData do cubo procedural (export do TIC sem asset importado)
MeshData cubeToMeshData() {
    const CubeMeshData c = makeCube(1.0f);
    MeshData m;
    m.name = "cube";
    m.vertices.assign(c.vertices.begin(), c.vertices.end());
    m.indices.assign(c.indices.begin(), c.indices.end());
    MeshData::Group g;
    g.name = "cube";
    g.firstIndex = 0;
    g.indexCount = static_cast<u32>(m.indices.size());
    m.groups.push_back(g);
    return m;
}

// F5-E: LoadCtx canônico do device — resolvers ligam refs relativas aos
// objetos de GPU em cache (1 ref = 1 upload; memória de GPU não duplica)
SceneSerializer::LoadCtx makeLoadCtx() {
    SceneSerializer::LoadCtx ctx;
    ctx.cubeMesh = &g_cubeMesh;
    ctx.material = g_renderer.litMaterial();
    ctx.resolveMesh = [](const std::string& ref) -> Mesh* {
        return g_gpu.mesh(ref);
    };
    ctx.resolveTex = [](const std::string& ref) -> const Texture* {
        std::string warn;
        const Texture* t = g_gpu.texture(ref, &warn);
        if (!warn.empty()) {
            showToast(warn.c_str());   // gate 2K — aviso 1× por carga
        }
        return t;
    };
    return ctx;
}

// ---- F5.1-C: SAF — ativação da pasta escolhida + import/export --------------

// ativa a pasta SAF como raiz do projeto (abre/cria "projeto" lá dentro,
// persiste a URI p/ o próximo arranque e recarrega a cena ativa)
void activateSafProject(const std::string& treeUri) {
    SafBackend* backend = saf::safJniBackend();
    if (!backend || !g_router || !g_storage) {
        showToast("SAF indisponível");
        return;
    }
    g_safStorage = std::make_unique<SafStorage>(backend, treeUri);
    Project proj;
    if (!Project::openOrCreate(*g_safStorage, "projeto", proj)) {
        g_safStorage.reset();
        showToast("pasta sem projeto (URI inválida?)");
        return;
    }
    g_project = std::move(proj);
    g_router->bind(g_storage.get(), g_safStorage.get());
    g_router->useSecondary(true);
    // persiste a URI p/ reabrir no próximo arranque (ficheiro no raiz do
    // app — NÃO dentro da pasta escolhida)
    g_storage->writeText("saf_tree.txt", treeUri);
    g_projectReady = true;
    g_resources.setStorage(g_router);
    g_resources.releaseAll();
    g_gpu.releaseAll();
    refreshCatalog();
    // cena ativa do projeto na pasta escolhida
    const SceneSerializer::LoadCtx ctx = makeLoadCtx();
    if (g_project.loadActiveScene(*g_router, g_scene, ctx)) {
        LOGI("saf: projeto ativo na pasta concedida (%u tics)", g_scene.count());
    } else {
        LOGI("saf: projeto novo/sem cena carregável na pasta concedida");
    }
    g_editor.selected = Handle::invalid();
    showToast("pasta do projeto ativa");
}

// resultado do picker IMPORT: lê o ficheiro da URI e copia para o projeto
// (por extensão: obj/gltf/glb → meshes/, png → textures/)
void importFromSaf(const std::string& docUri) {
    if (!g_router) {
        return;
    }
    const std::string name = saf::safJniDisplayName(docUri);
    const std::vector<u8> bytes = saf::safJniReadSingle(docUri);
    if (bytes.empty() || name.empty()) {
        showToast("import falhou (ficheiro vazio?)");
        return;
    }
    std::string lower = name;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    const size_t dot = lower.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : lower.substr(dot + 1);
    const char* dir = nullptr;
    if (ext == "obj" || ext == "gltf" || ext == "glb") {
        dir = Project::kDirMeshes;
    } else if (ext == "png") {
        dir = Project::kDirTextures;
    } else {
        showToast("tipo não suportado (obj/gltf/glb/png)");
        return;
    }
    // nome de destino sanitizado
    std::string safe = lower;
    for (char& c : safe) {
        if (c == '/' || c == '\\' || c == ':') c = '_';
    }
    const std::string rel = std::string(dir) + "/" + safe;
    if (g_router->writeBytes(rel, bytes.data(), bytes.size())) {
        refreshCatalog();
        char msg[96];
        std::snprintf(msg, sizeof(msg), "importado: %s", rel.c_str());
        showToast(msg);
        LOGI("saf: import %s (%zu bytes)", rel.c_str(), bytes.size());
    } else {
        showToast("falha ao gravar no projeto");
    }
}

// resultado do picker EXPORT: grava os bytes preparados na URI escolhida
void exportToSaf(const std::string& docUri) {
    if (g_pendingExport.empty()) {
        showToast("nada para exportar");
        return;
    }
    if (saf::safJniWriteSingle(docUri, g_pendingExport.data(), g_pendingExport.size())) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "exportado: %s", g_pendingExportName.c_str());
        showToast(msg);
        LOGI("saf: export %s (%zu bytes)", g_pendingExportName.c_str(),
             g_pendingExport.size());
    } else {
        showToast("falha ao exportar (permissão?)");
    }
}

// handler injetado — chamado pelo bridge JNI quando a Activity responde
void onSafResult(void* /*user*/, const saf::SafResult& r) {
    g_safState.onResult(r);
    if (r.request == saf::kReqPickTree) {
        if (g_safState.uriUsable() && saf::flagsReadWrite(r.flags)) {
            activateSafProject(g_safState.grantedUri());
        } else {
            showToast("pasta não escolhida (fallback local)");
        }
    } else if (r.request == saf::kReqImport) {
        if (g_safState.uriUsable()) {
            importFromSaf(g_safState.grantedUri());
        } else {
            showToast("import cancelado");
        }
    } else if (r.request == saf::kReqExport) {
        if (g_safState.uriUsable()) {
            exportToSaf(g_safState.grantedUri());
        } else {
            showToast("export cancelado");
        }
    }
    g_safState.reset();
}

// prepara e lança o Export via SAF (captura o mesh AGORA — o resultado
// chega assincronamente depois)
void beginSafExport() {
    Tic* tsel = g_scene.get(g_editor.selected);
    MeshRenderer* mrs = tsel ? tsel->getComponent<MeshRenderer>() : nullptr;
    if (!mrs) {
        showToast("selecione um TIC com mesh");
        return;
    }
    std::string err;
    const MeshData* src = nullptr;
    MeshData cubeCopy;
    if (!mrs->meshPath.empty()) {
        src = g_resources.mesh(mrs->meshPath, err);
    } else if (mrs->mesh == &g_cubeMesh) {
        cubeCopy = cubeToMeshData();
        src = &cubeCopy;
    }
    if (!src || !src->ok()) {
        showToast("mesh não disponível p/ export");
        return;
    }
    char nameBuf[40];
    std::snprintf(nameBuf, sizeof(nameBuf), "%.30s", tsel->name.c_str());
    g_pendingExportName = "export_" + std::string(nameBuf) + ".obj";
    for (char& c : g_pendingExportName) {
        if (c == '/' || c == '\\' || c == ':' || c == '#') c = '_';
    }
    const std::string obj = exportObj(*src);
    g_pendingExport.assign(obj.begin(), obj.end());
    g_safState.begin(saf::kReqExport);
    if (!saf::openExportPicker(nullptr, saf::kReqExport,
                               g_pendingExportName.c_str())) {
        g_safState.reset();
        g_pendingExport.clear();
        showToast("SAF indisponível");
    }
}

// rótulo curto do formato p/ a status line (F5.1-D)
const char* shortTexFormat(CompressedFormat f) {
    switch (f) {
        case CompressedFormat::RGBA8:     return "rgba";
        case CompressedFormat::ETC2_RGB:  return "etc2";
        case CompressedFormat::ETC2_RGBA: return "eac";
        case CompressedFormat::ASTC_4x4:  return "astc4";
        case CompressedFormat::ASTC_6x6:  return "astc6";
        default:                          return "?";
    }
}

// claimedMask: slots reclamados pelos TouchControls (joystick/botão) — a
// câmara de orbit ignora esses dedos (F4)
void updateCameraOrbit(const InputState& in, const UiRect& view, u32 claimedMask) {
    u32 active = 0;
    for (u32 s = 0; s < kMaxPointerSlots; ++s) {
        if (in.down(s) && !(claimedMask & (1u << s))) {
            ++active;
        }
    }
    if (active == 0) {
        g_gestureInView = false;
        g_orbitActive = false;
        g_pinchPrev = 0.0f;
        return;
    }

    // F3: o PRIMEIRO toque decide o dono do gesto. Se nasceu num painel
    // (Hierarchy/Inspector/toolbar) ou num controlo de toque (F4), a câmara
    // não orbita — mesmo que o dedo depois atravesse o viewport.
    if (!g_gestureInView) {
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            if (!in.pressed(s)) continue;
            if (claimedMask & (1u << s)) break;   // nasceu num controlo
            f32 x, y;
            in.pos(s, x, y);
            if (x >= view.x && x < view.x + view.w && y >= view.y && y < view.y + view.h) {
                g_gestureInView = true;
            }
            break;   // só o primeiro pointer com edge interessa
        }
    }
    if (!g_gestureInView) {
        return;
    }

    if (active >= 2) {
        // pinch: distância entre os dois primeiros dedos ativos (não reclamados)
        f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool got0 = false, got1 = false;
        for (u32 s = 0; s < kMaxPointerSlots && !(got0 && got1); ++s) {
            if (!in.down(s)) continue;
            if (claimedMask & (1u << s)) continue;
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
            if (claimedMask & (1u << s)) continue;
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

// F4: alimenta os TouchControls ativos (só em modo Play) e devolve a máscara
// de slots reclamados (a câmara ignora esses dedos)
u32 feedTouchControls(f32 w, f32 h, bool& outDrawn, TouchControls** outTc) {
    u32 claimed = 0;
    outDrawn = false;
    *outTc = nullptr;
    if (!g_playMode) {
        return 0;
    }
    auto& tcs = g_scene.components().touchControls();
    TouchControls* first = nullptr;
    for (u32 i = 0; i < tcs.size(); ++i) {
        Tic* t = g_scene.get(tcs.owner(i));
        if (!t || !t->active) {
            continue;
        }
        TouchControls& tc = tcs.at(i);
        if (!first) {
            first = &tc;
            *outTc = &tc;
            outDrawn = true;
        }
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            // F4.2: w/h chegam como ÁREA ÚTIL; o toque vem em coords de ecrã
            // → converte para o espaço do layout dos controlos (mesma
            // transformação que drawTouchControls usa ao desenhar)
            if (g_input.pressed(s)) {
                f32 x, y;
                g_input.pos(s, x, y);
                if (tc.touchBegin(s, x - g_ui.safeLeft(), y - g_ui.safeTop(), w, h)) {
                    claimed |= (1u << s);
                }
            } else if (g_input.down(s) && tc.ownsSlot(s)) {
                f32 x, y;
                g_input.pos(s, x, y);
                tc.touchMove(s, x - g_ui.safeLeft(), y - g_ui.safeTop());
                claimed |= (1u << s);
            }
            if (g_input.released(s)) {
                tc.touchEnd(s);
            }
        }
    }
    return claimed;
}
// ---------------------------------------------------------------------------

f32    g_fps = 0.0f;
double g_fpsAccum = 0.0;
u32    g_fpsFrames = 0;

void onAppCmd(android_app* app, i32 cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // F5.1-hotfix: boot em passos numerados — cada passo escreve
            // "[boot N/6] <nome> OK/FALHOU" no engine.log. A ordem dentro
            // do INIT_WINDOW segue a numeração (contentRect → fonts →
            // renderer → scene); storage/physics já correram no android_main.
            if (!g_egl.init(app->window)) {
                elog::error("[boot 1/6] contentRect FALHOU (EGL init)");
                break;
            }
            g_renderer.resize(g_egl.width(), g_egl.height());
            applyContentRect(app);   // F4.2: safe-area desde o primeiro frame
            elog::info("[boot 1/6] contentRect OK (surface %dx%d)",
                       (int)g_egl.width(), (int)g_egl.height());
            // F1 sem assets: fonte do sistema (primeira que existir vence)
            if (!g_font.loadFromPaths(kSystemFontPaths, kSystemFontPathCount, 28.0f)) {
                elog::error("[boot 3/6] fonts FALHOU — nenhuma fonte do sistema — UI sem texto");
            } else {
                elog::info("[boot 3/6] fonts OK (sistema, 28px)");
            }
            g_ui.init();
            g_ui.setFont(&g_font);
            // F3: geometria procedural do viewport (mesh partilhado dos presets)
            {
                const CubeMeshData cube = makeCube(1.0f);
                if (!g_cubeMesh.create(cube.vertices.data(),
                                       static_cast<u32>(cube.vertices.size()),
                                       cube.indices.data(),
                                       static_cast<u32>(cube.indices.size()))) {
                    elog::error("[boot 4/6] renderer FALHOU no passo: mesh do cubo");
                }
                if (!g_grid.init()) {
                    elog::error("[boot 4/6] renderer FALHOU no passo: grid de chão");
                }
            }
            if (!g_renderer.init()) {
                elog::error("[boot 4/6] renderer FALHOU (shaders/materiais)");
                break;
            }
            // F5-E: caches de assets ligados ao storage ATIVO (router F5.1-C)
            if (g_router) {
                g_resources.setStorage(g_router);
                g_gpu.init(&g_resources);
                if (g_pipeline) {
                    // F5.1-A: ASTC só quando a extensão KHR existe (C33/Mali
                    // tem; sem a extensão o HardwareCompressor cai p/ ETC2)
                    const bool astc = glAstcSupported();
                    g_hwCompressor.setAstcSupported(astc);
                    g_gpu.setPipeline(g_pipeline.get());
                    g_texCache->resetStats();
                    elog::info("f5.1: compressão de texturas ativa — ASTC %s (fallback ETC2), cache %s",
                               astc ? "SIM" : "não",
                               g_projectReady ? "textures/cache" : "off");
                }
            }
            elog::info("[boot 4/6] renderer OK (shaders, materiais, geometria)");
            // F5-A/3: recarrega a CENA ATIVA do projeto (refs relativos
            // intactos; resolvers de mesh chegam na F5-E — por agora o
            // LoadCtx liga o cubo procedural, tag "cube" das cenas antigas)
            if (g_projectReady) {
                const SceneSerializer::LoadCtx ctx = makeLoadCtx();
                if (g_project.loadActiveScene(*g_router, g_scene, ctx)) {
                    elog::info("[boot 6/6] scene OK → editor ('%s', %u tics)",
                               g_project.activeScenePath()->c_str(), g_scene.count());
                } else {
                    elog::error("[boot 6/6] scene FALHOU ('%s') — editor arranca com cena vazia",
                                g_project.activeScenePath()->c_str());
                }
            } else {
                elog::warn("[boot 6/6] scene SEM PROJETO — editor sem persistência");
            }
            g_ready = true;
            elog::info("boot: janela pronta %dx%d", (int)g_egl.width(), (int)g_egl.height());
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            g_egl.refreshSize();
            g_renderer.resize(g_egl.width(), g_egl.height());
            applyContentRect(app);   // F4.2: barras podem ter mudado
            break;
        case APP_CMD_CONTENT_RECT_CHANGED:
            // F4.2 (fix raiz do B1): o sistema avisou que a área desenhável
            // mudou (nav/status bar a aparecer/esconder) → reinsetar TUDO
            applyContentRect(app);
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

// pass 3D: desenha TODOS os TICs com MeshRenderer (F3) — já não é um cubo
// hardcoded: o modelo vem do Transform3D do mesmo dono (cache world).
DrawStats drawTics(const Mat4& vp) {
    DrawStats st{};
    const ComponentStore& comps = g_scene.components();
    const auto& mrs = comps.meshRenderers();
    for (u32 i = 0; i < mrs.size(); ++i) {
        const MeshRenderer& mr = mrs.at(i);
        if (!mr.mesh) {
            continue;   // sem mesh (tag "none" de cena antiga) — nada a desenhar
        }
        Mat4 model = Mat4::identity();
        if (const Transform3D* tr = comps.transforms().find(mrs.owner(i))) {
            model = tr->world;   // mantido por TransformSystem (grupo Update)
        }
        st = st + g_renderer.drawMesh(*mr.mesh, model, vp, mr.texture);
    }
    return st;
}

void drawToast() {
    if (g_toastT <= 0.0f || !g_ui.hasFont()) {
        return;
    }
    const f32 alpha = g_toastT < 1.0f ? g_toastT : 1.0f;
    const f32 bh = 44.0f;
    // F4.2: toast dentro da safe-area (acima da status line, que também vive
    // no contentRect)
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    // F4.2/B2: o texto do toast nunca excede a área útil menos as margens
    char toastFit[sizeof(g_toast)];
    textfit::ellipsize(g_toast, aw - 64.0f,
                       [](const char* s) { return g_ui.fontWidth(s); },
                       toastFit, sizeof(toastFit));
    const f32 tw = g_ui.fontWidth(toastFit);
    const f32 bw = tw + 32.0f;
    const f32 bx = ox + (aw - bw) * 0.5f;
    const f32 by = oy + ah - UiContext::kStatusH - bh - 18.0f;
    const f32 bg[4] = {theme::PANEL[0], theme::PANEL[1], theme::PANEL[2], 0.95f * alpha};
    const f32 tx[4] = {theme::TEXT[0], theme::TEXT[1], theme::TEXT[2], alpha};
    g_ui.panel(bx, by, bw, bh, bg);
    g_ui.frame(bx, by, bw, bh, 1.0f, tx);
    g_ui.label(bx + 16.0f, by + bh * 0.5f + g_ui.fontHeight() * 0.30f, toastFit, tx);
}

void frame() {
    const f32 w = static_cast<f32>(g_egl.width());
    const f32 h = static_cast<f32>(g_egl.height());

    // F4: TouchControls primeiro (só em modo Play) — os dedos que nasceram
    // nos controlos não vão para a câmara
    bool tcDrawn = false;
    TouchControls* tcDraw = nullptr;
    const f32 tcW = w - g_ui.safeLeft() - g_ui.safeRight();   // F4.2: área útil
    const f32 tcH = h - g_ui.safeTop() - g_ui.safeBottom();
    const u32 claimed = feedTouchControls(tcW, tcH, tcDrawn, &tcDraw);

    // input do frame anterior → câmara (só gestos nascidos no viewport
    // central da SAFE-AREA — gestos atrás da nav bar não orbitam, F4.2)
    updateCameraOrbit(g_input, editor::centerRect(w, h, g_ui.safeArea()), claimed);

    // F4: base de movimento do input = câmara (stick-cima afasta da câmara)
    {
        const f32 sy = std::sin(g_camera.yaw);
        const f32 cy = std::cos(g_camera.yaw);
        g_physics.frame.fwd = Vec3{-sy, 0.0f, -cy};
        g_physics.frame.right = Vec3{cy, 0.0f, -sy};
    }
    g_physics.enabled = g_playMode;   // física só avança em modo Play

    // ---- pass 3D: clear color+depth, TICs com MeshRenderer + grid com fade
    g_renderer.beginFrame();
    const Mat4 view = g_camera.view();
    const Mat4 proj = g_camera.proj(w / h);
    const Mat4 vp = Mat4::mul(proj, view);
    const DrawStats st3d = drawTics(vp);
    const DrawStats stGrid = g_grid.draw(vp, g_camera.eye(), g_camera.dist);

    // ---- pass UI: immediate-mode da F1 por cima (sem depth — nunca ocluída)
    g_ui.beginFrame(&g_renderer, &g_input, w, h);

    bool clicks[3] = {false, false, false};
    g_ui.toolbar(clicks);   // exatamente 3 botões (Menu, Play, Settings)
    if (clicks[0]) {
        g_editor.fileMenu = !g_editor.fileMenu;   // F3: Menu abre Save/Load
        g_editor.plusMenu = false;
        g_editor.settingsMenu = false;
    }
    if (clicks[1]) {
        g_playMode = !g_playMode;                 // F4: Play liga/desliga a simulação
        if (g_playMode) {
            // F4.2/B3: ENTRAR → snapshot da pose de editor
            playSnapshotCapture(g_scene, g_playSnap);
            LOGI("ui: modo play — snapshot de %u transforms / %u bodies",
                 (unsigned)g_playSnap.transforms.size(),
                 (unsigned)g_playSnap.bodies.size());
        } else {
            // F4.2/B3: SAIR → repõe a pose de editor e descarta a simulação
            playSnapshotRestore(g_scene, g_playSnap);
            LOGI("ui: modo editor — pose restaurada (%u transforms)",
                 (unsigned)g_playSnap.transforms.size());
        }
        showToast(g_playMode ? "modo play" : "modo editor");
    }
    if (clicks[2]) {
        // F5.1-hotfix: Settings abre o menu (Exportar logs)
        g_editor.settingsMenu = !g_editor.settingsMenu;
        g_editor.fileMenu = false;
        g_editor.plusMenu = false;
        elog::info("ui: menu Settings %s", g_editor.settingsMenu ? "aberto" : "fechado");
    }

    // F5.1-hotfix (parte 1.4): overlay Settings → Exportar logs
    if (g_editor.settingsMenu) {
        const int choice = editor::drawSettingsMenu(g_ui, g_input, w, h, g_editor);
        if (choice == 1) {
            int copied = 0;
            if (saf::jniExportLogsToDownloads(&copied) && copied >= 0) {
                char msg[96];
                std::snprintf(msg, sizeof(msg), "logs exportados: %d → %s",
                              copied, elog::kDownloadsRelPath);
                showToast(msg);
                elog::info("logs: exportados %d ficheiro(s) para %s",
                           copied, elog::kDownloadsRelPath);
            } else {
                showToast("export falhou (sem ficheiros? API<29?)");
                elog::warn("logs: export falhou (copied=%d)", copied);
            }
        }
    }

    // F5-E: catálogo dos seletores — refresh quando um seletor ABRE
    if (g_editor.assetMenu != 0 && g_editor.assetMenu != g_prevAssetMenu) {
        refreshCatalog();
    }
    g_prevAssetMenu = g_editor.assetMenu;

    // F3: painéis do editor (Hierarquia esquerda, Inspector direita)
    if (editor::drawHierarchy(g_ui, g_scene, g_editor)) {
        g_editor.plusMenu = true;   // "+" no cabeçalho abre os presets
        g_editor.fileMenu = false;
    }
    editor::drawInspector(g_ui, g_scene, g_editor, &g_catalog);   // sliders + seletores

    // overlay "+" → presets (cria e seleciona)
    if (g_editor.plusMenu) {
        const int choice = editor::drawPlusMenu(g_ui, g_input, w, h, g_editor);
        if (choice > 0) {
            const PresetKind kind = static_cast<PresetKind>(choice - 1);
            const Handle hnew = createTicFromPreset(g_scene, kind, &g_cubeMesh,
                                                    g_renderer.litMaterial());
            if (hnew.valid()) {
                g_editor.selected = hnew;
                char msg[64];
                std::snprintf(msg, sizeof(msg), "%s criado", presetName(kind));
                showToast(msg);
                LOGI("editor: %s criado", presetName(kind));
            }
        }
    }

    // F5-E: seletor de assets aberto → aplica no MeshRenderer selecionado
    if (g_editor.assetMenu != 0) {
        const int pick = editor::drawAssetMenu(g_ui, g_input, w, h, g_editor, g_catalog);
        if (pick > 0) {
            Tic* tsel = g_scene.get(g_editor.selected);
            MeshRenderer* mrs = tsel ? tsel->getComponent<MeshRenderer>() : nullptr;
            if (mrs && g_editor.assetMenu == 1) {
                if (pick == 1) {   // cube procedural
                    mrs->mesh = &g_cubeMesh;
                    mrs->material = g_renderer.litMaterial();
                    mrs->meshPath.clear();
                    showToast("mesh: cube");
                } else {
                    const std::string rel =
                        std::string("meshes/") + g_catalog.meshes[static_cast<size_t>(pick - 2)];
                    if (Mesh* m = g_gpu.mesh(rel)) {
                        mrs->mesh = m;
                        mrs->material = g_renderer.litMaterial();
                        mrs->meshPath = rel;
                        bool withTex = false;
                        // F5.1-B: textura embutida do glTF/GLB aplica-se logo
                        // (import sem PC — o material fica referenciado)
                        const std::string texRel = g_resources.meshTextureFor(rel);
                        if (!texRel.empty()) {
                            std::string twarn;
                            if (const Texture* tex = g_gpu.texture(texRel, &twarn)) {
                                mrs->texture = tex;
                                mrs->texPath = texRel;
                                withTex = true;
                            }
                        }
                        showToast(withTex ? "mesh aplicado (+textura)" : "mesh aplicado");
                        LOGI("editor: mesh %s aplicado%s%s", rel.c_str(),
                             withTex ? " com textura " : "",
                             withTex ? mrs->texPath.c_str() : "");
                    } else {
                        showToast("falha ao carregar mesh");
                    }
                }
            } else if (mrs && g_editor.assetMenu == 2) {
                if (pick == 1) {   // none
                    mrs->texture = nullptr;
                    mrs->texPath.clear();
                    showToast("tex: none");
                } else {
                    const std::string rel =
                        std::string("textures/") + g_catalog.textures[static_cast<size_t>(pick - 2)];
                    std::string warn;
                    if (const Texture* tex = g_gpu.texture(rel, &warn)) {
                        mrs->texture = tex;
                        mrs->texPath = rel;
                        showToast(warn.empty() ? "textura aplicada" : warn.c_str());
                        LOGI("editor: textura %s aplicada", rel.c_str());
                    } else {
                        showToast("falha ao carregar textura");
                    }
                }
            }
        }
    }

    // F4: controlos de toque por cima de tudo (só em modo Play)
    if (tcDrawn && tcDraw) {
        editor::drawTouchControls(g_ui, *tcDraw, w, h);
    }

    // overlay Menu → Save/Load cena do projeto (F5-A: caminhos relativos,
    // cena ativa guardada em scenes/<ativa>.goni + manifesto persistido)
    if (g_editor.fileMenu) {
        const int choice = editor::drawFileMenu(g_ui, g_input, w, h, g_editor);
        if (choice == 1 && g_projectReady) {
            const bool ok = g_project.saveActiveScene(*g_router, g_scene) &&
                            g_project.saveManifest(*g_router);
            char msg[64];
            std::snprintf(msg, sizeof(msg), ok ? "cena salva (%u tics)" : "falha ao salvar",
                          g_scene.count());
            showToast(msg);
            LOGI("editor: %s → %s", msg, g_project.activeScenePath()->c_str());
        } else if (choice == 2 && g_projectReady) {
            const SceneSerializer::LoadCtx ctx = makeLoadCtx();
            const bool ok = g_project.loadActiveScene(*g_router, g_scene, ctx);
            char msg[64];
            std::snprintf(msg, sizeof(msg), ok ? "cena carregada (%u tics)" : "falha ao carregar",
                          g_scene.count());
            showToast(msg);
            g_editor.selected = Handle::invalid();   // seleção antiga não sobrevive ao load
            LOGI("editor: %s ← %s", msg, g_project.activeScenePath()->c_str());
        } else if (choice == 3 && g_projectReady) {
            // F5-E: Export OBJ — mesh do TIC selecionado → meshes/export_<nome>.obj
            Tic* tsel = g_scene.get(g_editor.selected);
            MeshRenderer* mrs = tsel ? tsel->getComponent<MeshRenderer>() : nullptr;
            if (!mrs) {
                showToast("selecione um TIC com mesh");
            } else {
                std::string err;
                const MeshData* src = nullptr;
                MeshData cubeCopy;
                if (!mrs->meshPath.empty()) {
                    src = g_resources.mesh(mrs->meshPath, err);
                } else if (mrs->mesh == &g_cubeMesh) {
                    cubeCopy = cubeToMeshData();
                    src = &cubeCopy;
                }
                if (!src || !src->ok()) {
                    showToast("mesh não disponível p/ export");
                } else {
                    std::snprintf(g_selectedName, sizeof(g_selectedName), "%.30s",
                                  tsel->name.c_str());
                    std::string outName = "export_" + std::string(g_selectedName) + ".obj";
                    for (char& c : outName) {
                        if (c == '/' || c == '\\' || c == ':' || c == '#') c = '_';
                    }
                    const std::string rel =
                        std::string(Project::kDirMeshes) + "/" + outName;
                    if (g_router->writeText(rel, exportObj(*src))) {
                        char msg[96];
                        std::snprintf(msg, sizeof(msg), "export: %s", rel.c_str());
                        showToast(msg);
                        LOGI("editor: export OBJ → %s", rel.c_str());
                    } else {
                        showToast("falha ao exportar OBJ");
                    }
                }
            }
        } else if (choice == 4 && g_projectReady) {
            // F5.1-C: escolher a PASTA do projeto (SAF) — o resultado
            // chega assincronamente via onSafResult
            g_safState.begin(saf::kReqPickTree);
            if (!saf::openTreePicker(nullptr, saf::kReqPickTree)) {
                g_safState.reset();
                showToast("SAF indisponível");
            }
        } else if (choice == 5 && g_projectReady) {
            // F5.1-C: importar ficheiro (SAF) → copia p/ meshes/ ou textures/
            g_safState.begin(saf::kReqImport);
            if (!saf::openImportPicker(nullptr, saf::kReqImport)) {
                g_safState.reset();
                showToast("SAF indisponível");
            }
        } else if (choice == 6 && g_projectReady) {
            // F5.1-C: export via SAF (CREATE_DOCUMENT; mesh capturado já)
            beginSafExport();
        }
    }

    drawToast();

    // status line inferior: fps + TICs + vértices desenhados + draw calls
    // + F5.1-D: formato de textura da última carga + hits/misses do cache
    // (parte da UI usa as métricas do frame anterior — lag de 1 frame)
    const DrawStats total = st3d + stGrid + g_lastUiStats;
    char status[128];
    std::snprintf(status, sizeof(status),
                  "fps %d  tics %u  verts %u  dc %u  am %u at %u  %s c%u/%u",
                  static_cast<int>(g_fps + 0.5f), g_scene.count(),
                  total.vertices, total.drawCalls,
                  g_gpu.meshCount(), g_gpu.textureCount(),
                  shortTexFormat(g_hwCompressor.lastFormat()),
                  g_texCache ? g_texCache->hits() : 0u,
                  g_texCache ? g_texCache->misses() : 0u);
    g_ui.statusLine(status);

    g_ui.endFrame();                       // submete solids + glyphs
    g_lastUiStats = g_renderer.endFrame(); // desenha a UI por cima do 3D
    g_egl.swap();
    g_input.clearEdges();   // edges já consumidas pela UI/câmara neste frame
}

} // namespace

void android_main(android_app* app) {
    // F5.1-hotfix: log DUPLO (logcat + ficheiro) desde a 1ª linha.
    // O boot ainda não tem os paths da activity? O elog usa o fallback
    // android (Android/data/vv.goni/files/logs) — JNI_OnLoad já escreveu
    // lá (JNI_OnLoad corre ANTES do android_main).
    elog::info("G.One VV 0.6.1 — F5.1-hotfix (crash dump + logs; boot progresso por passos)");
    {
        const char* root0 = app->activity
            ? (app->activity->externalDataPath ? app->activity->externalDataPath
                                               : app->activity->internalDataPath)
            : nullptr;
        if (root0) {
            static char logsDir[512];
            std::snprintf(logsDir, sizeof(logsDir), "%s/logs", root0);
            elog::init(logsDir);
        }
    }
    // F5.1-hotfix: crash dump PERMANENTE (todas as builds) — SIGSEGV/ABRT/
    // BUS/FPE → crash-<ts>.dump legível (função+offset) no MESMO diretório
    // do engine.log; o dump também é marcado no log ("CRASH …").
    vv::crash::install(elog::dir());
    elog::info("logs: %s (ativo=%d)", elog::dir()[0] ? elog::dir() : "<só-logcat>",
               elog::active() ? 1 : 0);

    // F5-A: storage do projeto — raiz getExternalFilesDir (sem permissões
    // desde a API 19); fallback = internalDataPath. Boot abre o projeto
    // existente ou cria "projeto" (manifesto + estrutura completa).
    {
        const char* root = nullptr;
        if (app->activity) {
            root = app->activity->externalDataPath
                       ? app->activity->externalDataPath
                       : app->activity->internalDataPath;
        }
        if (root) {
            g_storage = std::make_unique<FsStorage>(root);
            // F5.1-A: cache/pipeline vivem enquanto o storage viver
            g_texCache = std::make_unique<TextureCache>(*g_storage);
            g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                           *g_texCache);
            // F5.1-C: router (primário FsStorage; SAF liga depois) + JNI/SAF
            g_router = new RoutingStorage(g_storage.get(), nullptr);
            saf::setHandler(&onSafResult, nullptr);
            if (app->activity) {
                saf::initJava(app->activity->vm, app->activity->clazz);
                if (saf::safInitJniBackend(app->activity->vm, app->activity->env,
                                          app->activity->clazz)) {
                    // reabre a última pasta SAF concedida (URI persistida)
                    std::string savedUri;
                    if (g_storage->readText("saf_tree.txt", savedUri) &&
                        !savedUri.empty()) {
                        g_safStorage = std::make_unique<SafStorage>(
                            saf::safJniBackend(), savedUri);
                        Project safProj;
                        if (Project::openOrCreate(*g_safStorage, "projeto",
                                                  safProj)) {
                            g_project = std::move(safProj);
                            g_router->bind(g_storage.get(), g_safStorage.get());
                            g_router->useSecondary(true);
                            g_projectReady = true;
                            LOGI("saf: pasta concedida reaberta: %s",
                                 savedUri.c_str());
                        } else {
                            g_safStorage.reset();
                            LOGE("saf: URI persistida sem projeto válido — fallback local");
                        }
                    }
                }
            }
            if (!g_router->usingSecondary() &&
                Project::openOrCreate(*g_storage, "projeto", g_project)) {
                g_projectReady = g_project.activeScenePath() != nullptr;
                elog::info("projeto: '%s' pronto em %s (%u cena(s), ativa=%s)",
                           g_project.name.c_str(), root,
                           (unsigned)g_project.scenes.size(),
                           g_projectReady ? g_project.activeScenePath()->c_str() : "-");
            } else {
                elog::error("projeto: storage inutilizável em %s — editor sem persistência", root);
            }
        } else {
            elog::error("projeto: sem externalDataPath/internalDataPath — editor sem persistência");
        }
        // [boot 2/6] storage — passo crítico do arranque (ficheiro legível
        // no device: se o boot morrer aqui, o dono vê exatamente onde)
        elog::info("[boot 2/6] storage %s (raiz=%s, saf=%s)",
                   g_storage ? "OK" : "FALHOU",
                   app->activity && app->activity->externalDataPath
                       ? "external" : "-",
                   g_router && g_router->usingSecondary() ? "saf" : "local");
    }

    // F3/F4: systems do engine (ordem interna ao grupo = registo)
    g_systems.add(TickGroup::Update, &g_transformSystem);
    g_systems.add(TickGroup::Physics, &g_physics);   // entre Update e PostUpdate
    elog::info("[boot 5/6] physics OK (tickgroups Update+Physics registados)");

    app->onAppCmd = onAppCmd;
    app->onInputEvent = onInputEvent;

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
            // F3: cada passo fixo roda os TickGroups na ordem
            // PreUpdate → Update → PostUpdate → Render
            g_systems.run(g_scene, static_cast<f32>(g_time.fixedDt));
            g_time.endStep();
        }

        frame();

        if (g_toastT > 0.0f) {
            g_toastT -= static_cast<f32>(realDt);
        }

        g_fpsAccum += realDt;
        ++g_fpsFrames;
        if (g_fpsAccum >= 0.5) {
            g_fps = static_cast<f32>(g_fpsFrames / g_fpsAccum);
            g_fpsAccum = 0.0;
            g_fpsFrames = 0;
        }
    }
}
