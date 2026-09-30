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
#include "platform/FileApi.h"
#include "platform/InputState.h"
#include "platform/StorageBridge.h"
#include "platform/StoragePerm.h"
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
// O projeto vive SEMPRE nesta pasta (app-private, sem permissões):
// project.goni + scenes/ + meshes/ + textures/. O I/O de FICHEIROS do
// utilizador (import/export) passa pelo fluxo All Files Access (F5.2) com
// File API direta — ver storage::PermFlow + fileapi.
std::unique_ptr<FsStorage> g_storage;
Project    g_project;
bool       g_projectReady = false;   // storage + projeto com cena válida

// ---- F5.2: All Files Access — fluxo de permissão + File API direta ---------
storage::PermFlow g_perm;                  // máquina de estado (GL-free)
std::vector<fileapi::Candidate> g_importCands;  // candidatos de Download/Documents
std::vector<std::string> g_logLines;            // tail do engine.log p/ o viewer
std::vector<std::string> g_logDumps;            // crash-*.dump p/ o viewer

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
    if (!g_storage) {
        return;
    }
    std::vector<std::string> files;
    if (g_storage->listDir(Project::kDirMeshes, files)) {
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
    if (g_storage->listDir(Project::kDirTextures, files)) {
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

// ---- F5.2: All Files Access — import/export por File API direta -------------

// forward: usados pelo handler de retorno e pelas tentativas
void openImportScan();
void beginExportToDownloads();

// VERIFICAÇÃO fresca da permissão (1 chamada JNI estática por tentativa —
// o utilizador pode ter ativado as definições sem voltar pela app)
bool storageGrantedNow(bool* outSupported) {
    bool mgr = false;
    const bool supported = storage::jniStorageApiSupported(&mgr);
    if (outSupported) {
        *outSupported = supported;
    }
    if (supported) {
        g_perm.setMode(mgr ? storage::Mode::AllFiles : storage::Mode::AppPrivate);
    }
    return supported && mgr;
}

// handler injetado — chamado pelo bridge quando a Activity volta das
// definições (kReqAllFiles). NO THREAD DA ENGINE (pollResult no loop):
// re-verifica isExternalStorageManager e retoma a ação pendente.
void onStorageResult(void* /*user*/, const saf::SafResult& r) {
    if (r.request != storage::kReqAllFiles) {
        elog::warn("storage: resultado de req=%d ignorado (esperado %d)",
                   r.request, storage::kReqAllFiles);
        return;
    }
    bool mgr = false;
    const bool supported = storage::jniStorageApiSupported(&mgr);
    const bool granted = supported && mgr;
    g_perm.onSettingsReturn(granted);
    elog::info("storage: retorno das definicoes — supported=%d mgr=%d → %s "
               "(modo %s)", supported ? 1 : 0, mgr ? 1 : 0,
               granted ? "CONCEDIDO" : "negado/sem accao",
               storage::modeLabel(g_perm.mode()));
    if (!granted) {
        showToast("acesso não ativado — modo app-private");
        return;
    }
    showToast("acesso concedido — File API direta");
    // retoma a ação que abriu o diálogo (1 import, 2 export)
    const storage::Action act = g_perm.takePendingAction();
    if (act == storage::Action::Import) {
        openImportScan();
    } else if (act == storage::Action::Export) {
        beginExportToDownloads();
    }
}

// varre Download/ e Documents/ (File API direta) e abre o overlay IMPORT
void openImportScan() {
    g_importCands.clear();
    for (int i = 0; i < fileapi::kImportDirCount; ++i) {
        const std::string dir = std::string(fileapi::kExternalRoot) + "/" +
                                fileapi::kImportDirs[i];
        std::vector<fileapi::Candidate> part;
        if (fileapi::listCandidates(dir, part)) {
            elog::info("fileapi: %s → %u candidato(s)", dir.c_str(),
                       (unsigned)part.size());
            for (const fileapi::Candidate& c : part) {
                g_importCands.push_back(c);
            }
        }
        // pasta ausente/sem acesso: o errno já foi logado pela FileApi
    }
    if (g_importCands.empty()) {
        showToast("nenhum obj/gltf/glb/png em Download/Documents");
        return;
    }
    g_editor.importMenu = true;
}

// EXPORT: mesh do TIC selecionado → /storage/emulated/0/Download/GOneVV/
// export/export_<nome>.obj (File API direta, visível no gestor de ficheiros)
void beginExportToDownloads() {
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
    std::string outName = "export_" + std::string(nameBuf) + ".obj";
    for (char& c : outName) {
        if (c == '/' || c == '\\' || c == ':' || c == '#') c = '_';
    }
    const std::string obj = exportObj(*src);
    const std::string path = std::string(fileapi::kExternalRoot) + "/" +
                             fileapi::kExportRelDir + "/" + outName;
    if (fileapi::writeAll(path, obj.data(), obj.size())) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "exportado: %s/%s",
                      fileapi::kExportRelDir, outName.c_str());
        showToast(msg);
        elog::info("fileapi: export %s (%zu bytes) — File API direta",
                   path.c_str(), obj.size());
    } else {
        showToast("falha ao exportar (causa no engine.log)");
        elog::error("fileapi: export %s FALHOU — %s", path.c_str(),
                    fileapi::errnoText().c_str());
    }
}

// TENTATIVA de import: concedido → varre já; senão → DIÁLOGO (1ª vez do
// fluxo) com a ação pendente; bloqueado → mensagem HONESTA com a causa real
// (F5.3: handshake em baixo NUNCA é reportado como "sistema sem suporte")
void attemptImport() {
    if (!g_projectReady) {
        showToast("sem projeto — import indisponível");
        return;
    }
    bool supported = false;
    if (storageGrantedNow(&supported)) {
        openImportScan();
        return;
    }
    const storage::BlockReason why =
        storage::blockReason(storage::handshakeOk(), supported);
    if (why == storage::BlockReason::None) {
        if (g_perm.requestAction(storage::Action::Import, true)) {
            g_editor.storageDialog = true;   // o main desenha o diálogo
            elog::info("storage: diálogo All Files aberto (import pendente)");
        }
        return;
    }
    showToast(storage::blockMessage(why));
    elog::error("storage: import bloqueado — %s", storage::blockMessage(why));
    if (why == storage::BlockReason::Unsupported) {
        // marca o estado Unsupported no fluxo (o Settings mostra app-private)
        g_perm.requestAction(storage::Action::Import, false);
    }
}

// TENTATIVA de export (mesma forma do import — mensagens honestas)
void attemptExport() {
    bool supported = false;
    if (storageGrantedNow(&supported)) {
        beginExportToDownloads();
        return;
    }
    const storage::BlockReason why =
        storage::blockReason(storage::handshakeOk(), supported);
    if (why == storage::BlockReason::None) {
        if (g_perm.requestAction(storage::Action::Export, true)) {
            g_editor.storageDialog = true;
            elog::info("storage: diálogo All Files aberto (export pendente)");
        }
        return;
    }
    showToast(storage::blockMessage(why));
    elog::error("storage: export bloqueado — %s", storage::blockMessage(why));
    if (why == storage::BlockReason::Unsupported) {
        g_perm.requestAction(storage::Action::Export, false);
    }
}

// escolha do DIÁLOGO ("Permitir"/"Cancelar") — processada no frame em que
// drawStorageDialog devolve != 0. O diálogo só existe pós-handshake, mas a
// Activity pode ter sido recriada entretanto — a falha do lançamento é
// diagnosticada com a causa REAL (handshake vs intent).
void onStorageDialogChoice(int choice) {
    if (choice == 1) {
        g_perm.dialogAccept();
        if (g_perm.consumeOpenSettings()) {
            if (storage::jniOpenAllFilesSettings()) {
                elog::info("storage: %s lançada — à espera do retorno",
                           storage::kSettingsAction);
            } else {
                g_perm.dialogCancel();
                if (!storage::handshakeOk()) {
                    showToast("ponte Java indisponível (handshake)");
                    elog::error("storage: lançamento de %s FALHOU — ponte Java "
                                "indisponível (handshake)",
                                storage::kSettingsAction);
                } else {
                    showToast("não consegui abrir as definições");
                    elog::error("storage: lançamento de %s FALHOU (handshake "
                                "OK — intent sem activity no aparelho?)",
                                storage::kSettingsAction);
                }
            }
        }
    } else if (choice == 2) {
        g_perm.dialogCancel();
        showToast("sem acesso — modo app-private");
        elog::info("storage: utilizador recusou (modo app-private)");
    }
}

// IMPORT: copia o candidato escolhido para o projeto (por extensão:
// obj/gltf/glb → meshes/, png → textures/) — File API lê, FsStorage grava
void importCandidate(size_t idx) {
    if (idx >= g_importCands.size() || !g_storage) {
        return;
    }
    const fileapi::Candidate& c = g_importCands[idx];
    std::vector<u8> bytes;
    if (!fileapi::readAll(c.path, bytes) || bytes.empty()) {
        showToast("leitura falhou (causa no engine.log)");
        return;
    }
    const char* dir = (c.kind == 'm') ? Project::kDirMeshes
                                      : Project::kDirTextures;
    // nome de destino sanitizado
    std::string safe = c.name;
    for (char& ch : safe) {
        if (ch == '/' || ch == '\\' || ch == ':') ch = '_';
    }
    const std::string rel = std::string(dir) + "/" + safe;
    if (g_storage->writeBytes(rel, bytes.data(), bytes.size())) {
        refreshCatalog();
        char msg[96];
        std::snprintf(msg, sizeof(msg), "importado: %s", rel.c_str());
        showToast(msg);
        elog::info("fileapi: import %s (%zu bytes) de %s", rel.c_str(),
                   bytes.size(), c.path.c_str());
    } else {
        showToast("falha ao gravar no projeto");
        elog::error("fileapi: import %s — gravação no projeto falhou",
                    rel.c_str());
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
            // F5-E: caches de assets ligados ao storage do projeto (F5.2:
            // app-private — o SAF router foi removido)
            if (g_storage) {
                g_resources.setStorage(g_storage.get());
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
                if (g_project.loadActiveScene(*g_storage, g_scene, ctx)) {
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
        // F5.1-hotfix/F5.2: Settings abre o menu (logs + armazenamento)
        g_editor.settingsMenu = !g_editor.settingsMenu;
        g_editor.fileMenu = false;
        g_editor.plusMenu = false;
        elog::info("ui: menu Settings %s", g_editor.settingsMenu ? "aberto" : "fechado");
    }

    // F5.1-hotfix (1.4) + F5.2: overlay Settings — Exportar logs / Ver logs /
    // Acesso a ficheiros… + linha do modo de armazenamento ativo
    if (g_editor.settingsMenu) {
        const char* modeText = g_perm.mode() == storage::Mode::Unknown
                                   ? ""
                                   : storage::modeLabel(g_perm.mode());
        const int choice = editor::drawSettingsMenu(g_ui, g_input, w, h, g_editor,
                                                    modeText);
        if (choice == 1) {
            // export para Downloads/GOneVV/logs (MediaStore — sem permissões)
            int copied = 0;
            if (storage::jniExportLogsToDownloads(&copied) && copied >= 0) {
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
        } else if (choice == 2) {
            // F5.2: VER LOGS in-app — tail do engine.log + crash dumps
            g_logLines.clear();
            g_logDumps.clear();
            elog::readTail(g_logLines, 300);
            elog::listDumps(g_logDumps);
            g_editor.logViewer = true;
            g_editor.logViewerJustOpened = true;
            elog::info("logs: viewer aberto (%u linhas, %u dump(s))",
                       (unsigned)g_logLines.size(), (unsigned)g_logDumps.size());
        } else if (choice == 3) {
            // F5.2: "Acesso a ficheiros…" — mesmo fluxo do diálogo (sem ação
            // pendente: se conceder, o próximo import/export funciona direto)
            // F5.3: mensagens honestas (handshake ≠ sistema sem suporte)
            bool supported = false;
            if (storageGrantedNow(&supported)) {
                showToast("acesso já concedido (all files)");
            } else {
                const storage::BlockReason why =
                    storage::blockReason(storage::handshakeOk(), supported);
                if (why == storage::BlockReason::None) {
                    if (g_perm.requestAction(storage::Action::None, true)) {
                        g_editor.storageDialog = true;
                    }
                } else {
                    showToast(storage::blockMessage(why));
                    elog::error("storage: acesso a ficheiros bloqueado — %s",
                                storage::blockMessage(why));
                    if (why == storage::BlockReason::Unsupported) {
                        g_perm.requestAction(storage::Action::None, false);
                    }
                }
            }
        }
    }

    // F5.2: DIÁLOGO/IMPORT/LOGS desenhados DEPOIS dos painéis (ordem =
    // z-order) — ver o bloco imediatamente antes de drawToast()

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
    // F5.2: menu de ficheiro com 5 itens — Save/Load/Export OBJ/Importar…/
    // Export Downloads (o "Pasta (SAF)" foi REMOVIDO com o fluxo SAF)
    if (g_editor.fileMenu) {
        const int choice = editor::drawFileMenu(g_ui, g_input, w, h, g_editor);
        if (choice == 1 && g_projectReady) {
            const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                            g_project.saveManifest(*g_storage);
            char msg[64];
            std::snprintf(msg, sizeof(msg), ok ? "cena salva (%u tics)" : "falha ao salvar",
                          g_scene.count());
            showToast(msg);
            LOGI("editor: %s → %s", msg, g_project.activeScenePath()->c_str());
        } else if (choice == 2 && g_projectReady) {
            const SceneSerializer::LoadCtx ctx = makeLoadCtx();
            const bool ok = g_project.loadActiveScene(*g_storage, g_scene, ctx);
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
                    if (g_storage->writeText(rel, exportObj(*src))) {
                        char msg[96];
                        std::snprintf(msg, sizeof(msg), "export: %s", rel.c_str());
                        showToast(msg);
                        LOGI("editor: export OBJ → %s", rel.c_str());
                    } else {
                        showToast("falha ao exportar OBJ");
                    }
                }
            }
        } else if (choice == 4) {
            // F5.2: IMPORTAR — All Files Access → varre Download/Documents →
            // overlay de escolha → cópia para meshes/ ou textures/
            attemptImport();
        } else if (choice == 5) {
            // F5.2: EXPORT DOWNLOADS — All Files Access → Download/GOneVV/export
            attemptExport();
        }
    }

    // F5.2: overlays de armazenamento — DEPOIS dos painéis (ordem = z-order):
    // diálogo de permissão → import → viewer de logs (por cima de tudo)
    if (g_editor.storageDialog) {
        const int ch = editor::drawStorageDialog(g_ui, g_input, w, h, g_editor);
        if (ch != 0) {
            onStorageDialogChoice(ch);
        }
    }
    if (g_editor.importMenu) {
        const int pick = editor::drawImportMenu(g_ui, g_input, w, h, g_editor,
                                                g_importCands);
        if (pick > 0) {
            importCandidate(static_cast<size_t>(pick - 1));
        }
    }
    if (g_editor.logViewer) {
        editor::drawLogViewer(g_ui, g_input, w, h, g_editor,
                              g_logLines, g_logDumps);
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
    elog::info("G.One VV 0.6.2 — F5.2 (All Files Access + File API direta + log viewer)");
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
    // F5.2: SEM SAF router — o projeto vive SEMPRE app-private; o I/O de
    // ficheiros do utilizador passa pelo fluxo All Files Access.
    {
        const char* root = nullptr;
        const bool isExternal = app->activity &&
                                app->activity->externalDataPath != nullptr;
        if (app->activity) {
            root = app->activity->externalDataPath
                       ? app->activity->externalDataPath
                       : app->activity->internalDataPath;
        }
        // F5.2 (item 5): BOOT SELF-CHECK — o resultado do mapeamento
        // (getExternalFilesDir null?) e o errno de cada fopen/opendir
        // falhado ficam no engine.log ANTES de qualquer I/O pesado. Se algo
        // falhar mais tarde, a CAUSA do storage já está registrada aqui.
        fileapi::logStorageSelfCheck(root, isExternal);
        if (root) {
            g_storage = std::make_unique<FsStorage>(root);
            // F5.1-A: cache/pipeline vivem enquanto o storage viver
            g_texCache = std::make_unique<TextureCache>(*g_storage);
            g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                           *g_texCache);
            // F5.2: ponte Java (janela de permissões + retorno + export logs)
            storage::setHandler(&onStorageResult, nullptr);
            // F5.3 — HANDSHAKE INVERTIDO (docs/HANDSHAKE_AUDIT.md): o native
            // NÃO tenta descobrir a activity sozinho. O android_main corre no
            // thread do glue (pthread) que NÃO está anexado à VM — GetEnv
            // devolvia JNI_EDETACHED e a ponte morria com "env/activity
            // indisponíveis" (causa única de todas as features Java-dependentes
            // falharem desde a 0.6.0). É a VvActivity (thread da UI, sempre
            // anexado) que se registra: onCreate → nativeRegisterActivity,
            // onResume reforça. Pendente aqui é NORMAL — o android_main corre
            // antes de super.onCreate terminar; o refresco vem com o onResume.
            elog::info("jni: handshake invertido — à espera de "
                       "java: onCreate → nativeRegisterActivity "
                       "(onResume reforça)");
            if (Project::openOrCreate(*g_storage, "projeto", g_project)) {
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
        // F5.2: estado da permissão NO ARRANQUE (o Settings mostra o modo;
        // API < 30 → sem suporte → app-private sem nunca pedir).
        // F5.3: o estado do HANDSHAKE entra na mesma linha — se a ponte
        // Java estiver morta, o log diz "handshake=0" (a causa real) em vez
        // de sugerir que o sistema é que não suporta All Files Access.
        {
            bool mgr = false;
            const bool supported = storage::jniStorageApiSupported(&mgr);
            g_perm.setMode(storage::resolveMode(supported, mgr));
            elog::info("storage: All Files Access — handshake=%d supported=%d "
                       "manager=%d → modo %s",
                       storage::handshakeOk() ? 1 : 0,
                       supported ? 1 : 0, mgr ? 1 : 0,
                       storage::modeLabel(g_perm.mode()));
            if (!storage::handshakeOk()) {
                // pendente no boot é NORMAL (android_main corre dentro do
                // onCreate); se persistir no primeiro import/export, a
                // mensagem certa é "ponte Java indisponível (handshake)"
                elog::warn("jni: handshake pendente no boot — onResume deve "
                           "registar (java: onCreate → nativeRegisterActivity)");
            }
        }
        // [boot 2/6] storage — passo crítico do arranque (ficheiro legível
        // no device: se o boot morrer aqui, o dono vê exatamente onde)
        elog::info("[boot 2/6] storage %s (raiz=%s, modo=%s)",
                   g_storage ? "OK" : "FALHOU",
                   app->activity && app->activity->externalDataPath
                       ? "external" : "-",
                   storage::modeLabel(g_perm.mode()));
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

        // F5.1-hotfix (auditoria JNI) / F5.2: resultados da Activity (retorno
        // das definições de permissão) chegam do thread da UI e são
        // processados AQUI (thread da engine, EGL corrente) — o handler toca
        // em storage/scene/GPU e não pode correr no thread Java (crash
        // garantido sem contexto GL). Se a janela não está pronta, o
        // resultado fica na fila e é consumido mais tarde.
        while (storage::pollResult()) {
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
