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
#include "components/CameraComp.h"   // 0.7.7: câmara de cena
#include "components/MeshRenderer.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "components/AnimationPlayer.h"   // 0.8.0 (F7): animação
#include "components/UiCanvas.h"   // 0.8.0: tracks de UI
#include "assets/GltfAnim.h"   // 0.8.1 (F7): clips de animação do glTF
#include "core/AnimationSystem.h"   // 0.8.0: avanço em Play
#include "core/AssetPersist.h"
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
#include "core/CameraUtil.h"   // 0.7.7: uma câmara ativa por cena
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
#include "render/Primitives.h"   // 0.8.0 (F7): primitivas procedurais
#include "render/Renderer.h"
#include "ui/EditorUi.h"
#include "ui/Toolbar.h"   // 0.7.6: barra final de 5 grupos (G1..G5)
#include "ui/CamGizmo.h"   // 0.7.7: frustum/handles/câmara de jogo
#include "ui/Gizmo.h"
#include "ui/Timeline.h"   // 0.8.0 (F7): editor de timeline da animação
#include "ui/FontAtlas.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"   // 0.7.0: editor de UI dedicado (viewport 2D,
                            // teclado in-app, menu contextual, diálogos)
#include "ui/UiRuntime.h"  // 0.7.0: runtime da UI criável (draw/hit/ações)
#include "ui/SceneFx.h"   // 0.7.1: transições de cena (fade/slide)

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

// ---- 0.8.0 (F7): ANIMAÇÃO ---------------------------------------------------
// Sistema no grupo Update (ANTES do TransformSystem: escreve pos/rot/scale
// e chama updateWorld; o Transform reconfirma). Gate `enabled` como a
// física — só avança em Play; o preview do editor vive na timeline.
AnimationSystem     g_animSystem;
timeline::State     g_timeline;      // scrub/keys/preview entre frames

// cache de PRIMITIVAS PROCEDURAIS — a assinatura (tipo+parâmetros) é a
// "ref" do mesh: 1 assinatura = 1 objeto GL partilhado por todos os TICs
// que a usam. Lifecycle como o cubo: Mesh::destroy() zera ids no TERM
// (primMeshDestroy) e o próximo uso RE-GERA+re-uploda com o contexto novo
// (lazy — nunca se gera geometria sem GL corrente).
struct PrimCacheEntry {
    PrimParams            sig;
    std::unique_ptr<Mesh> mesh;
};
std::vector<PrimCacheEntry> g_primCache;

// ---- F4: física + modo Play ------------------------------------------------
phys::PhysicsSystem g_physics;       // TickGroup::Physics (só avança em Play)
// F4.2/B3: sandbox do Play — pose de editor capturada ao ENTRAR, restaurada
// ao SAIR (a simulação é descartada; a física continua a correr só no Play)
// 0.6.8: o MODO (editor↔play) vive em g_editor.playMode (EditorState) — a
// transição completa é testável na suíte; o snapshot continua aqui.
PlaySnapshot        g_playSnap;


// ---- F5-A: projeto .goni + storage -----------------------------------------
// F5.4 (Gestor de Projetos): a raiz é a PASTA ESCOLHIDA pelo utilizador no
// ecrã inicial (SafStorage sobre o URI de árvore SAF com permissão
// persistente — cada projeto na sua pasta, sem All Files Access). Sem
// projeto SAF (lançamento direto/timeout/handshake em baixo) cai no
// FsStorage app-private (getExternalFilesDir — comportamento 0.6.x).
// O I/O de FICHEIROS do utilizador (import/export) continua pelo fluxo
// All Files Access (F5.2) com File API direta — COEXISTE com o SAF.
std::unique_ptr<ProjectStorage> g_storage;
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

// 0.6.8: estado do gesto de orbit ENTRE frames — extraído para editor::
// (OrbitState puro, afervel no CI; a lógica vive em EditorUi.cpp)
editor::OrbitState g_orbit;

// ---- 0.6.9: gizmos de transformação ------------------------------------------
// Só no TIC selecionado, SÓ EM EDITOR (nunca em PLAY). O hit-test 3D corre
// ANTES do orbit (o slot que apanha o gizmo NÃO orbita); o drag escreve no
// Transform3D com âncoras (pose final = âncora + delta) e updateWorld().
// 0.7.9 — GRAB-LOCK: o press edge captura o ALVO + o RAIO (base da câmara
// no grab) + o PLANO FIXO (⟂ à câmara no grab, pela pos do TIC NO ARRANQUE)
// + as âncoras todas (hit no plano, ângulo, distância). Durante o move NÃO
// há hit-test: o delta do dedo é projetado no plano FIXO — o gizmo move-se
// com o objeto mas o drag nunca depende do dedo estar sobre ele (fim da
// oscilação/"fuga" do C33). Touch up liberta o lock.
editor::toolbar::GizmoModeState g_gizmoMode;   // 0.7.6: vive na Toolbar
gizmo::GizmoState       g_gizmo;
gizmo::Grab             g_grab;   // 0.7.9: o lock do drag em curso

// 0.7.7 — drag dos HANDLES do frustum da câmara (prioritários sobre o eixo
// do gizmo quando a câmara está selecionada): 1..4 = canto do far (fovY),
// 5 = centro do far (far). Âncoras: pose final = âncora + delta.
// 0.7.9 — GRAB-LOCK também aqui: o raio (base da câmara), a normal do
// plano e o CENTRO projetado são capturados NO GRAB — a orbit pode mexer
// com outro dedo que o delta do handle não salta.
struct CamHandleDrag {
    bool active = false;
    u32  slot = 0;
    int  handle = 0;
    f32  anchorFar = 0.0f;
    f32  anchorFov = 0.0f;
    f32  anchorDist = 0.0f;   // |dedo − olho projetado| no arranque (fov)
    Vec3 anchorHit{};         // hit raio×plano no arranque (far)
    // 0.7.9 — o lock geométrico do grab:
    gizmo::ViewBasis basis;   // base da câmara NO GRAB (o raio é fixo)
    Vec3 planeN{};            // normal do plano do drag NO GRAB (fwd do frustum)
    Vec3 planeOrigin{};       // pos da câmara NO ARRANQUE (o plano é fixo)
    f32  anchorOx = 0.0f;     // centro projetado (olho) NO GRAB (fov)
    f32  anchorOy = 0.0f;
} g_camHandle;
// âncoras do gizmo ESCALAR numa câmara (o fator escala fov/orthoSize —
// nunca o transform, que não tem significado numa câmara)
f32 g_camScaleFov = 0.0f;
f32 g_camScaleOrtho = 0.0f;

// direção do mundo de um eixo/plano do gizmo (main-side)
Vec3 axisDirLocal(gizmo::Axis a) {
    switch (a) {
        case gizmo::Axis::X: return {1.0f, 0.0f, 0.0f};
        case gizmo::Axis::Y: return {0.0f, 1.0f, 0.0f};
        case gizmo::Axis::Z: return {0.0f, 0.0f, 1.0f};
        default:             return {0.0f, 0.0f, 0.0f};
    }
}

void applyGizmoDrag(const gizmo::Grab& grab, const gizmo::ViewBasis& basis,
                    f32 sw, f32 sh, f32 px, f32 py);

// aplica o drag do handle do frustum (0.7.9: SEMPRE no plano/centro FIXOS
// do grab — nunca re-ancora na geometria atual)
void applyCamHandleDrag(const Mat4& vp, const gizmo::ViewBasis& basis,
                        f32 sw, f32 sh, f32 px, f32 py) {
    (void)vp;
    (void)basis;   // o drag usa o LOCK do grab, não a base atual
    Tic* t = g_scene.get(g_editor.selected);
    if (!t) {
        return;
    }
    CameraComp* cc = t->getComponent<CameraComp>();
    Transform3D* tr = t->getComponent<Transform3D>();
    if (!cc || !tr) {
        return;
    }
    const bool snap = g_gizmoMode.snap;
    if (g_camHandle.handle == 5) {
        // CENTRO do far: arrasto projetado no EIXO DE VISÃO da câmara — no
        // plano FIXO do grab (normal e origem capturadas no arranque)
        bool ok = false;
        const Vec3 h1 = gizmo::planeHit(g_camHandle.basis, g_camHandle.planeN,
                                        g_camHandle.planeOrigin, px, py, sw,
                                        sh, ok);
        if (!ok) {
            return;
        }
        cc->farZ = camgizmo::dragFar(g_camHandle.anchorFar,
                                     g_camHandle.anchorHit, h1,
                                     g_camHandle.planeN, snap);
    } else {
        // CANTO do far: fator radial do dedo em torno do olho projetado —
        // o CENTRO é o do GRAB (nunca o projetado atual)
        const f32 d = std::sqrt((px - g_camHandle.anchorOx) *
                                    (px - g_camHandle.anchorOx) +
                                (py - g_camHandle.anchorOy) *
                                    (py - g_camHandle.anchorOy));
        cc->fovY = camgizmo::dragFov(g_camHandle.anchorFov,
                                     g_camHandle.anchorDist, d, snap);
    }
}

u32 feedGizmo(const Mat4& vp, const Vec3& origin, f32 len, f32 sw, f32 sh,
              const gizmo::ViewBasis& basis) {
    if (!gizmo::visible(g_editor.playMode, true)) {
        g_gizmo.active = gizmo::Axis::None;
        g_gizmo.hovered = gizmo::Axis::None;
        g_gizmo.dragSlot = -1;
        g_grab = gizmo::Grab{};   // 0.7.9: sem gizmo não há lock
        g_camHandle.active = false;
        return 0;
    }
    // 0.7.7 — drag de HANDLE do frustum em curso: segue o MESMO slot
    if (g_camHandle.active) {
        const u32 slot = g_camHandle.slot;
        if (g_input.down(slot)) {
            f32 px, py;
            g_input.pos(slot, px, py);
            applyCamHandleDrag(vp, basis, sw, sh, px, py);
            return 1u << slot;
        }
        g_camHandle.active = false;   // dedo levantado — lock libertado
    }
    // 0.7.9 — drag em curso (GRAB-LOCK): segue o MESMO slot SEM hit-test;
    // o delta do dedo é projetado no plano FIXO do grab (o gizmo move-se
    // com o objeto — o drag não depende do dedo estar sobre ele)
    if (g_grab.valid()) {
        const u32 slot = static_cast<u32>(g_grab.slot);
        if (g_input.down(slot)) {
            f32 px, py;
            g_input.pos(slot, px, py);
            applyGizmoDrag(g_grab, basis, sw, sh, px, py);
            return 1u << slot;
        }
        // touch up — o lock é LIBERTADO (o próximo press volta ao hit-test)
        g_grab = gizmo::Grab{};
        g_gizmo.active = gizmo::Axis::None;
        g_gizmo.dragSlot = -1;
        g_gizmo.hovered = gizmo::Axis::None;
    }
    // press edge → GRAB (hit-test com alvo GENEROSO, âncoras capturadas;
    // só se o gesto nasce no viewport central)
    const UiRect view =
        editor::centerRect(sw, sh, g_ui.safeArea(), g_editor.showInspector);
    for (u32 slot = 0; slot < kMaxPointerSlots; ++slot) {
        if (!g_input.pressed(slot)) {
            continue;
        }
        f32 px, py;
        g_input.pos(slot, px, py);
        if (px < view.x || px >= view.x + view.w || py < view.y ||
            py >= view.y + view.h) {
            continue;   // nasceu fora do viewport (painéis/toolbar)
        }
        // 0.7.7 — PRIORIDADE ao handle do frustum: com uma CÂMARA
        // selecionada, um toque perto de um handle do far é DELE (sem
        // conflitos de drag com o eixo do gizmo)
        Tic* t = g_scene.get(g_editor.selected);
        if (t) {
            CameraComp* cc = t->getComponent<CameraComp>();
            Transform3D* tr = t->getComponent<Transform3D>();
            if (cc && tr) {
                const camgizmo::Frustum f =
                    camgizmo::computeFrustum(*tr, *cc, sw / sh);
                const int h = camgizmo::pickHandle(vp, sw, sh, f, px, py);
                if (h != 0) {
                    g_camHandle.active = true;
                    g_camHandle.slot = slot;
                    g_camHandle.handle = h;
                    g_camHandle.anchorFar = cc->farZ;
                    g_camHandle.anchorFov = cc->fovY;
                    // 0.7.9 — o LOCK do handle: raio/plano/centro do GRAB
                    g_camHandle.basis = basis;
                    g_camHandle.planeN = f.fwd;
                    g_camHandle.planeOrigin = tr->pos;
                    f32 ox = 0.0f, oy = 0.0f;
                    if (gizmo::projectPoint(vp, tr->pos, sw, sh, ox, oy)) {
                        g_camHandle.anchorOx = ox;
                        g_camHandle.anchorOy = oy;
                        g_camHandle.anchorDist =
                            std::sqrt((px - ox) * (px - ox) +
                                      (py - oy) * (py - oy));
                    } else {
                        g_camHandle.anchorOx = px;
                        g_camHandle.anchorOy = py;
                        g_camHandle.anchorDist = 0.0f;
                    }
                    bool ok = false;
                    g_camHandle.anchorHit =
                        gizmo::planeHit(g_camHandle.basis, g_camHandle.planeN,
                                        g_camHandle.planeOrigin, px, py, sw,
                                        sh, ok);
                    applyCamHandleDrag(vp, basis, sw, sh, px, py);
                    return 1u << slot;
                }
            }
        }
        // 0.7.9 — GRAB-LOCK: captura alvo (raio generoso kGrabPx) + raio +
        // plano FIXO + âncoras — tudo medido NO ARRANQUE
        const gizmo::Grab grab =
            gizmo::beginGrab(g_gizmo.mode, vp, origin, len, sw, sh, px, py,
                             static_cast<i32>(slot), basis);
        if (!grab.valid()) {
            continue;
        }
        g_grab = grab;
        g_gizmo.active = grab.target;
        g_gizmo.hovered = grab.target;
        g_gizmo.dragSlot = static_cast<i32>(slot);
        if (t) {
            if (Transform3D* tr = t->getComponent<Transform3D>()) {
                g_gizmo.anchorPos = tr->pos;
                g_gizmo.anchorRot = tr->rot;
                g_gizmo.anchorScale = tr->scale;
                // 0.7.7 — o ESCALAR numa câmara escala o FRUSTUM (fov/
                // orthoSize), não o transform: âncoras dos parâmetros
                if (CameraComp* cc = t->getComponent<CameraComp>()) {
                    if (g_gizmo.mode == gizmo::Mode::Scale) {
                        g_camScaleFov = cc->fovY;
                        g_camScaleOrtho = cc->orthoSize;
                    }
                }
            }
        }
        applyGizmoDrag(g_grab, basis, sw, sh, px, py);
        return 1u << slot;
    }
    // hover (sem press): destaque do alvo sob o dedo (feedback visual —
    // raio FINO, o alvo generoso é só do grab)
    for (u32 slot = 0; slot < kMaxPointerSlots; ++slot) {
        if (g_input.down(slot)) {
            f32 px, py;
            g_input.pos(slot, px, py);
            g_gizmo.hovered = gizmo::pickAxis(g_gizmo.mode, vp, origin, len,
                                               sw, sh, px, py);
            break;
        }
    }
    return 0;
}

// aplica o drag do gizmo ao Transform3D do TIC selecionado (âncoras).
// 0.7.9 — GRAB-LOCK: os hits de move/escalar-eixo vêm do PLANO FIXO do
// grab (grabHit — raio da base do GRAB contra o plano pela pos de ARRANQUE);
// rotate/escalar-uniforme medem contra o CENTRO projetado do GRAB. Nada
// aqui re-ancora na pos ATUAL do gizmo (a causa da oscilação/fuga).
void applyGizmoDrag(const gizmo::Grab& grab, const gizmo::ViewBasis& basis,
                    f32 sw, f32 sh, f32 px, f32 py) {
    Tic* t = g_scene.get(g_editor.selected);
    if (!t || !grab.valid()) {
        return;
    }
    Transform3D* tr = t->getComponent<Transform3D>();
    if (!tr) {
        return;
    }
    // 0.7.7 — CÂMARA: o ESCALAR ajusta fovY/orthoSize (o frustum escala;
    // a escala do transform NÃO tem significado numa câmara — fica intacta).
    // mover/rodar seguem o caminho normal do Transform3D (abaixo).
    // 0.7.9: o fator é medido contra o CENTRO do GRAB (nunca o atual).
    const bool snapCam = g_gizmoMode.snap;   // (antes do uso — o resto da
                                             // função declara o seu depois)
    if (g_gizmo.mode == gizmo::Mode::Scale) {
        if (CameraComp* cc = t->getComponent<CameraComp>()) {
            const f32 d = std::sqrt((px - grab.anchorOx) *
                                        (px - grab.anchorOx) +
                                    (py - grab.anchorOy) *
                                        (py - grab.anchorOy));
            const bool ortho =
                cc->projection == CameraComp::Projection::Orthographic;
            const f32 v = camgizmo::dragScaleToFov(
                g_camScaleFov, g_camScaleOrtho, ortho, grab.anchorDist, d,
                snapCam);
            if (ortho) {
                cc->orthoSize = v;
            } else {
                cc->fovY = v;
            }
            return;   // NUNCA escreve no transform da câmara
        }
    }
    const gizmo::Axis a = grab.target;
    const bool snap = g_gizmoMode.snap;

    if (g_gizmo.mode == gizmo::Mode::Move) {
        // 0.7.9 — hit AGORA no plano FIXO do grab (o raio é o do grab):
        // o delta é estável mesmo com a câmara a orbitar noutro dedo
        bool ok1 = false;
        const Vec3 h0 = grab.anchorHit;
        const Vec3 h1 = gizmo::grabHit(grab, px, py, sw, sh, ok1);
        if (!ok1) {
            return;
        }
        if (a == gizmo::Axis::X || a == gizmo::Axis::Y || a == gizmo::Axis::Z) {
            tr->pos = gizmo::dragMoveAxis(g_gizmo.anchorPos,
                                          axisDirLocal(a), h0, h1, snap);
        } else {
            // plano de drag (XY/XZ/YZ) — normal FIXA do grab
            tr->pos = gizmo::dragMovePlane(g_gizmo.anchorPos, grab.planeNormal,
                                           h0, h1, snap);
        }
    } else if (g_gizmo.mode == gizmo::Mode::Rotate) {
        const Vec3 axis = axisDirLocal(a);
        // ângulo do dedo em torno do CENTRO do GRAB (estável sob orbit)
        const f32 ang = std::atan2(py - grab.anchorOy, px - grab.anchorOx);
        tr->rot = gizmo::dragRotate(g_gizmo.anchorRot, axis, basis.fwd,
                                    grab.anchorAngle, ang, snap);
    } else {
        if (a == gizmo::Axis::Center) {
            const f32 d = std::sqrt((px - grab.anchorOx) *
                                        (px - grab.anchorOx) +
                                    (py - grab.anchorOy) *
                                        (py - grab.anchorOy));
            tr->scale = gizmo::dragScaleUniform(g_gizmo.anchorScale,
                                                grab.anchorDist, d, snap);
        } else {
            bool ok1 = false;
            const Vec3 h0 = grab.anchorHit;
            const Vec3 h1 = gizmo::grabHit(grab, px, py, sw, sh, ok1);
            if (!ok1) {
                return;
            }
            tr->scale = gizmo::dragScaleAxis(g_gizmo.anchorScale, a,
                                             axisDirLocal(a), h0, h1, snap);
        }
    }
    // escreve no Transform3D COM worldDirty: o cache é recalculado já neste
    // frame (feedback imediato) e o TransformSystem reconfirma no passo
    tr->updateWorld();
    tr->worldDirty = false;   // cache coerente com a nova pose
}

DrawStats g_lastUiStats;   // métricas do pass UI (disponíveis 1 frame depois)

// toast (mensagem transitória acima da status line — feedback Save/Load/criação)
char g_toast[96] = "";
f32  g_toastT = 0.0f;
void showToast(const char* msg);   // fwd: usada pelo feedCanvasPlay (abaixo)

// ---- 0.7.0: UI criável em PLAY (hit-test + ações declarativas) -------------
// estado do on-click: press ARMA (o slot fica reclamado — não vai à câmara
// nem aos controlos), release DENTRO do mesmo elemento DISPARA a ação (o
// gesto clássico de botão). Puro o suficiente p/ ser reproduzido no CI
// (o hitTestCanvas/applyUiAction são afervéis; aqui só a sequência).
struct PlayUiPress {
    bool armed = false;
    u32  slot = 0;
    Handle tic{};
    i32  element = -1;
    i32  menuItem = -1;
} g_playUi;

// UiActionCtx do device: spawn liga ao createTicFromPreset (cubo/lit do
// renderer); sceneExists consulta o manifesto do projeto; loadScene chega
// na 0.7.1 (transições fade/slide) — sem callback a ação é HONESTA (toast
// "0.7.1", nunca um sucesso falso).
// (fwd: definidos mais abaixo — a seção de storage/projeto/cenas)
void loadSceneByName(const std::string& name, ui::SceneSwap style);
SceneSerializer::LoadCtx makeLoadCtx();
void refreshCatalog();
Mesh* primMesh(const PrimParams& p);   // 0.8.0 (F7): cache de primitivas (fwd)
ui::UiActionCtx makeUiActionCtx() {
    ui::UiActionCtx ctx;
    ctx.sceneExists = [](const std::string& name, void*) -> bool {
        if (!g_projectReady) {
            return false;
        }
        const std::string rel =
            std::string(Project::kDirScenes) + "/" + name + ".goni";
        for (const std::string& s : g_project.scenes) {
            if (s == rel) {
                return true;
            }
        }
        return false;
    };
    // 0.7.1: Scene.Load (instantâneo) / Scene.Transition (fade/slide em
    // Play) — o estilo chega pelo callback, a ação já validou a existência
    ctx.loadScene = [](const std::string& name, ui::SceneSwap style, void*) {
        loadSceneByName(name, style);
    };
    ctx.spawnPreset = [](PresetKind kind, void*) -> Handle {
        // 0.8.0 (F7): o preset Mesh nasce com a ESFERA default do cache
        // (as restantes fontes continuam a ser o cubo procedural)
        Mesh* mesh = &g_cubeMesh;
        if (kind == PresetKind::Mesh) {
            mesh = primMesh(primDefaults(PrimKind::Sphere));
        }
        return createTicFromPreset(g_scene, kind, mesh,
                                   g_renderer.litMaterial());
    };
    return ctx;
}

// hit-test dos canvases em Play (press/release); devolve a máscara de slots
// reclamados pelos botões da UI (o dedo não vai aos TouchControls)
u32 feedCanvasPlay(f32 w, f32 h) {
    u32 claimed = 0;
    for (u32 s = 0; s < kMaxPointerSlots; ++s) {
        if (!g_input.pressed(s)) {
            continue;
        }
        f32 x = 0.0f, y = 0.0f;
        g_input.pos(s, x, y);
        const ui::CanvasHit hit =
            ui::hitTestCanvas(g_scene, x, y, w, h, g_ui.safeArea());
        if (hit.valid) {
            g_playUi.armed = true;
            g_playUi.slot = s;
            g_playUi.tic = hit.tic;
            g_playUi.element = hit.element;
            g_playUi.menuItem = hit.menuItem;
            claimed |= 1u << s;
        }
    }
    if (g_playUi.armed && g_input.released(g_playUi.slot)) {
        f32 x = 0.0f, y = 0.0f;
        g_input.pos(g_playUi.slot, x, y);
        const ui::CanvasHit hit =
            ui::hitTestCanvas(g_scene, x, y, w, h, g_ui.safeArea());
        if (hit.valid && hit.tic == g_playUi.tic &&
            hit.element == g_playUi.element) {
            Tic* t = g_scene.get(hit.tic);
            UiCanvas* c = t ? t->getComponent<UiCanvas>() : nullptr;
            if (c && hit.element >= 0 &&
                hit.element < static_cast<i32>(c->elements.size())) {
                UiElement& e = c->elements[static_cast<size_t>(hit.element)];
                const char* targetOverride = nullptr;
                std::string tgt;
                if (e.kind == UiElement::Kind::Menu && g_playUi.menuItem >= 0) {
                    std::string label;
                    if (ui::menuLineAt(e, static_cast<u32>(g_playUi.menuItem),
                                       label, tgt)) {
                        targetOverride = tgt.c_str();
                    }
                }
                const ui::UiActionResult out =
                    ui::applyUiAction(g_scene, e, makeUiActionCtx(),
                                      targetOverride);
                if (out.wantToast) {
                    showToast(out.toast);
                }
                if (out.log[0] != '\0') {
                    LOGI("%s", out.log);
                }
            }
        }
        g_playUi.armed = false;
    }
    return claimed;
}

// desenha TODOS os canvases ativos e visíveis (UI por cima da cena)
void drawCanvasPlay(f32 w, f32 h) {
    g_scene.forEachActive([&](const Tic& t) {
        if (!t.visible) {
            return;
        }
        if (const UiCanvas* c =
                g_scene.components().uiCanvases().find(t.handle)) {
            ui::drawCanvas(g_ui, *c, w, h, g_ui.safeArea());
        }
    });
}

// ---- 0.7.1: CENAS MÚLTIPLAS + TRANSIÇÕES ---------------------------------
// Cada cena = um .goni próprio no manifesto (Project::scenes). A troca
// GUARDA a cena ATUAL no ficheiro dela, muda a ativa, persiste o manifesto
// e carrega a nova (LoadCtx canônico — refs relativos re-ligam). Em PLAY a
// troca anda dentro de uma TRANSIÇÃO (fade/slide): o swap acontece no
// PONTO MÉDIO, com o ecrã tapado — nunca se vê a troca a seco.
ui::SceneTransition g_sceneTrans;
u32                  g_sceneTransIdx = 0xFFFFFFFFu;   // índice alvo
f32                  g_frameDt = 0.0f;                // dt real do frame

// troca DIRETA de cena (sem transição — editor / Scene.Load)
void doSwitchScene(u32 idx) {
    if (!g_projectReady || !g_storage || idx >= g_project.scenes.size() ||
        idx == g_project.activeScene) {
        return;
    }
    // 1) guarda a cena ATUAL no ficheiro dela (nada se perde na troca)
    if (!g_project.saveActiveScene(*g_storage, g_scene)) {
        showToast("falha ao salvar a cena atual");
        elog::error("cena: salvar a cena ativa '%s' FALHOU antes da troca",
                    g_project.activeScenePath()
                        ? g_project.activeScenePath()->c_str()
                        : "?");
        return;   // sem gravar, sem trocar (a cena atual fica intacta)
    }
    // 2) muda a ativa + persiste o manifesto
    g_project.activeScene = idx;
    g_project.saveManifest(*g_storage);
    // 3) carrega a nova (LoadCtx canônico: refs relativos re-ligam)
    const SceneSerializer::LoadCtx ctx = makeLoadCtx();
    if (g_project.loadActiveScene(*g_storage, g_scene, ctx)) {
        char name[48];
        editor::sceneDisplayName(g_project.scenes[idx], name, sizeof(name));
        char msg[64];
        std::snprintf(msg, sizeof(msg), "cena: %s", name);
        showToast(msg);
        elog::info("cena: trocou para '%s' (%u tics)", name,
                   (unsigned)g_scene.count());
    } else {
        showToast("falha ao carregar a cena");
        elog::error("cena: load de '%s' FALHOU", g_project.scenes[idx].c_str());
    }
    g_editor.selected = Handle::invalid();   // seleção não sobrevive à troca
    g_editor.selElement = -1;
    g_playUi.armed = false;
    refreshCatalog();
}

// arranca a TRANSIÇÃO (em Play): o swap corre no ponto médio
void startSceneTransition(u32 idx, const char* name, bool slide) {
    g_sceneTrans.active = true;
    g_sceneTrans.style = slide ? ui::SceneTransition::Style::Slide
                                : ui::SceneTransition::Style::Fade;
    g_sceneTrans.t = 0.0f;
    g_sceneTrans.target = name ? name : "";
    g_sceneTransIdx = idx;
    elog::info("cena: transicao %s para '%s' iniciada",
               slide ? "slide" : "fade", g_sceneTrans.target.c_str());
}

// carrega por NOME (ação declarativa — o estilo decide instantâneo/transição)
void loadSceneByName(const std::string& name, ui::SceneSwap style) {
    if (!g_projectReady || !g_storage) {
        showToast("sem projeto — cenas indisponiveis");
        return;
    }
    const std::string rel =
        std::string(Project::kDirScenes) + "/" + name + ".goni";
    for (u32 i = 0; i < g_project.scenes.size(); ++i) {
        if (g_project.scenes[i] == rel) {
            if (style != ui::SceneSwap::Instant && g_editor.playMode) {
                startSceneTransition(i, name.c_str(),
                                     style == ui::SceneSwap::Slide);
            } else {
                doSwitchScene(i);   // editor ou Scene.Load: troca direta
            }
            return;
        }
    }
    char msg[96];
    std::snprintf(msg, sizeof(msg), "cena '%s' nao existe", name.c_str());
    showToast(msg);
}

// CRIA uma cena nova com nome (teclado in-app): guarda a atual, regista a
// nova no manifesto (ativa) e escreve o .goni VAZIO dela
void createSceneNamed(const std::string& name) {
    if (!g_projectReady || !g_storage || name.empty()) {
        return;
    }
    // 0) duplicados NUNCA passam (o addScene ATIVA a existente em vez de
    // falhar — sem este guard o .goni dela seria APAGADO pela cena vazia)
    const std::string rel =
        std::string(Project::kDirScenes) + "/" + name + ".goni";
    for (const std::string& s : g_project.scenes) {
        if (s == rel) {
            showToast("cena ja existe");
            elog::warn("cena: '%s' ja existe no projeto", name.c_str());
            return;
        }
    }
    // 1) a cena ATUAL vai para o ficheiro DELA (antes da troca)
    if (!g_project.saveActiveScene(*g_storage, g_scene)) {
        showToast("falha ao salvar a cena atual");
        return;
    }
    // 2) regista a nova e torna-a ativa
    if (!g_project.addScene(name)) {
        showToast("nome de cena invalido");
        return;
    }
    // 3) cena VAZIA em memória → escreve o .goni novo + manifesto
    g_scene.clear();
    g_editor.selected = Handle::invalid();
    g_editor.selElement = -1;
    const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                    g_project.saveManifest(*g_storage);
    char msg[64];
    std::snprintf(msg, sizeof(msg), ok ? "cena criada: %s"
                                        : "cena criada (manifesto falhou)",
                  name.c_str());
    showToast(msg);
    elog::info("cena: '%s' criada e ativa (%u cena(s) no projeto)",
               name.c_str(), (unsigned)g_project.scenes.size());
}

// ---- 0.7.2: NAVEGADOR DE FICHEIROS + APLICAR-APÓS-IMPORT --------------------
// O IMPORT robusto: navegar QUALQUER pasta do armazenamento (all-files),
// com a GALERIA (DCIM/Camera, Pictures) nas raízes, o CAMINHO visível no
// topo, e a pergunta "aplicar ao TIC?" quando há um TIC com MeshRenderer
// selecionado no fim do import.
struct FileBrowserState {
    bool open = false;
    std::string cwd;                            // onde está a procurar
    std::vector<fileapi::DirEntry> entries;     // conteúdo listado
    bool failed = false;                        // opendir falhou
} g_browser;
struct ApplyAskState {
    bool open = false;
    char kind = 0;          // 'm' mesh | 't' textura
    std::string rel;       // ref relativa importada ("textures/x.png")
    std::string fileName;  // basename (p/ o diálogo)
} g_applyAsk;

// abre/re-carrega uma pasta do navegador
void browserOpen(const std::string& path) {
    g_browser.cwd = path;
    g_browser.failed = !fileapi::listDirEntries(path, g_browser.entries);
    g_browser.open = true;
    elog::info("browser: %s (%u entrada(s)%s)", path.c_str(),
               (unsigned)g_browser.entries.size(),
               g_browser.failed ? ", opendir FALHOU" : "");
}

// copia o ficheiro escolhido para o projeto (a MESMA lógica do import
// antigo) e PERGUNTA se se aplica ao TIC selecionado
void browserImportFile(const fileapi::DirEntry& e) {
    if (!g_storage || e.isDir) {
        return;
    }
    std::vector<u8> bytes;
    if (!fileapi::readAll(e.path, bytes) || bytes.empty()) {
        showToast("leitura falhou (causa no engine.log)");
        elog::error("browser: leitura de '%s' FALHOU — %s", e.path.c_str(),
                    fileapi::errnoText().c_str());
        return;
    }
    const char* dir = e.kind == 'm' ? Project::kDirMeshes : Project::kDirTextures;
    std::string safe = e.name;
    for (char& ch : safe) {
        if (ch == '/' || ch == '\\' || ch == ':') ch = '_';
    }
    const std::string rel = std::string(dir) + "/" + safe;
    if (!g_storage->writeBytes(rel, bytes.data(), bytes.size())) {
        showToast("falha ao gravar no projeto");
        elog::error("browser: import %s — gravação no projeto falhou",
                    rel.c_str());
        return;
    }
    refreshCatalog();
    elog::info("browser: import %s (%zu bytes) de %s", rel.c_str(),
               bytes.size(), e.path.c_str());

    // APLICAR-APÓS-IMPORT: só se há TIC selecionado COM MeshRenderer
    Tic* tsel = g_scene.get(g_editor.selected);
    if (tsel && tsel->getComponent<MeshRenderer>()) {
        g_applyAsk.open = true;
        g_applyAsk.kind = e.kind;
        g_applyAsk.rel = rel;
        g_applyAsk.fileName = e.name;
    } else {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "importado: %s", rel.c_str());
        showToast(msg);
    }
}

// ---- 0.6.7: lifecycle GL -----------------------------------------------------
// Contadores de janela: distinguem o 1º arranque (boot frio) das RE-CRIAÇÕES
// do contexto EGL (voltar do fundo/recents sem matar a app — o caso que
// deixava os glifos brancos "cubinhos" no C33). NENHUM recurso GL é assumido
// vivo entre TERM_WINDOW e INIT_WINDOW: tudo é destruído no term (com o
// contexto ainda corrente) e re-criado/re-uploaded no init.
u32 g_windowInits = 0;   // APP_CMD_INIT_WINDOW vistos nesta execução
u32 g_windowTerms = 0;   // APP_CMD_TERM_WINDOW vistos (contexto morto)

void showToast(const char* msg) {
    std::snprintf(g_toast, sizeof(g_toast), "%s", msg);
    g_toastT = 1.8f;
}

// 0.6.7: desliga os MeshRenderers dos objetos de GPU que vão morrer.
// O g_gpu.releaseAll() APAGA os Mesh*/Texture* (unique_ptr + glDelete*) — os
// ponteiros não-donos dos componentes ficariam PENDENTES (use-after-free no
// próximo drawTics). Política:
//   • &g_cubeMesh é um objeto ESTÁTICO re-criado NO SITIO no próximo init —
//     o ponteiro continua válido (Mesh::destroy só zera os ids);
//   • meshes/texturas do GpuAssets são APAGADOS → mesh/texture = nullptr;
//     o reload da cena no INIT_WINDOW re-resolve via g_gpu.mesh/texture();
//   • material aponta para o LitMaterial DO RENDERER (objeto estático,
//     re-init no sitio) — mantém-se.
// As refs RELATIVAS (meshPath/texPath) ficam intactas — são a fonte da
// verdade para o re-bind (serializer).
u32 detachRenderersFromGpu() {
    u32 detached = 0;
    auto& mrs = g_scene.components().meshRenderers();
    for (u32 i = 0; i < mrs.size(); ++i) {
        MeshRenderer& mr = mrs.at(i);
        bool drop = false;
        if (mr.mesh && mr.mesh != &g_cubeMesh) {
            mr.mesh = nullptr;
            drop = true;
        }
        if (mr.texture) {
            mr.texture = nullptr;
            drop = true;
        }
        if (drop) {
            ++detached;
        }
    }
    return detached;
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
    // 0.7.8 (defensivo): o CONTENT_RECT_CHANGED pode chegar SEM um
    // WINDOW_RESIZED (barras do sistema a esconder/mostrar mudam a
    // superfície em alguns OEMs) — re-sincroniza o tamanho do EGL e o
    // viewport do renderer quando a superfície mudou, senão o pass de UI
    // desenharia com viewport/ortográfica STALE (a causa raiz da UI
    // gigante/cortada no Play do C33 — ver RELATORIO-0.7.8, secção 3).
    g_egl.refreshSize();
    if (g_egl.width() != sw || g_egl.height() != sh) {
        g_renderer.resize(g_egl.width(), g_egl.height());
    }
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

// ---- 0.8.0 (F7): PRIMITIVAS PROCEDURAIS — cache por assinatura ------------

// resolve (ou gera+uploda na 1ª vez) o mesh de UMA primitiva. Chamado com
// o contexto GL CORRENTE (load de cena, seletor, rebind por frame). O
// clamp acontece DENTRO do gerador — a assinatura guardada é a clampada
// (chaves de cache estáveis: raio 100 e raio 64 fazem o MESMO mesh).
Mesh* primMesh(const PrimParams& pIn) {
    PrimParams p = pIn;
    primClamp(p);
    for (const PrimCacheEntry& e : g_primCache) {
        const PrimParams& s = e.sig;
        if (s.kind == p.kind && s.radius == p.radius && s.height == p.height &&
            s.radius2 == p.radius2 && s.segments == p.segments &&
            s.rings == p.rings && s.size == p.size) {
            return e.mesh.get();
        }
    }
    PrimMeshData data;
    makePrimMesh(p, data);
    if (!data.ok()) {
        return nullptr;   // defesa (nunca: geradores são totais)
    }
    PrimCacheEntry e;
    e.sig = p;
    e.mesh = std::make_unique<Mesh>();
    if (!e.mesh->create(data.vertices.data(),
                        static_cast<u32>(data.vertices.size()),
                        data.indices.data(),
                        static_cast<u32>(data.indices.size()))) {
        elog::error("render: primitiva %s FALHOU no upload (%u verts)",
                    primName(p.kind),
                    static_cast<unsigned>(data.vertices.size()));
        return nullptr;
    }
    g_primCache.push_back(std::move(e));
    return g_primCache.back().mesh.get();
}

// TERM_WINDOW: glDelete* das primitivas em cache (MeshRenderers já foram
// desligados pelo detachRenderersFromGpu — os ponteiros ≠ &g_cubeMesh são
// anulados lá; o rebind é lazy no próximo frame com contexto novo)
void primMeshDestroy() {
    const u32 n = static_cast<u32>(g_primCache.size());
    for (PrimCacheEntry& e : g_primCache) {
        e.mesh->destroy();
    }
    g_primCache.clear();
    if (n > 0) {
        elog::info("lifecycle: %u primitiva(s) procedural(is) destruída(s)", n);
    }
}

// rebind lazy: MeshRenderers com primOn e mesh null (params editados no
// Inspector ou pós-TERM) voltam a apontar para o mesh do cache
void rebindPrimMeshes() {
    auto& mrs = g_scene.components().meshRenderers();
    for (u32 i = 0; i < mrs.size(); ++i) {
        MeshRenderer& mr = mrs.at(i);
        if (mr.primOn && mr.mesh == nullptr) {
            mr.mesh = primMesh(mr.prim);
            mr.material = mr.mesh ? g_renderer.litMaterial() : nullptr;
        }
    }
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
    // 0.8.0 (F7): "mesh":"prim" → cache de primitivas (lazy: gera no load)
    ctx.resolvePrim = [](const PrimParams& p) -> Mesh* {
        return primMesh(p);
    };
    return ctx;
}

// F6: resolvers do seletor de assets (injetados no applyAssetPick — a
// lógica de aplicação é PURA em ui/EditorUi e afervel no CI; aqui só se
// liga aos objetos reais de runtime: GpuAssets/ResourceManager/cubo/lit)
editor::AssetResolvers makeAssetResolvers() {
    editor::AssetResolvers res;
    res.mesh = [](const std::string& ref) -> Mesh* {
        return g_gpu.mesh(ref);
    };
    res.texture = [](const std::string& ref, std::string* warn) -> const Texture* {
        return g_gpu.texture(ref, warn);
    };
    res.meshTextureFor = [](const std::string& ref) -> std::string {
        return g_resources.meshTextureFor(ref);   // textura embutida glTF/GLB
    };
    res.cubeMesh = &g_cubeMesh;
    res.material = g_renderer.litMaterial();
    res.prim = [](const PrimParams& p) -> Mesh* {   // 0.8.0 (F7)
        return primMesh(p);
    };
    return res;
}

// ---- F5.2: All Files Access — import/export por File API direta -------------

// forward: usados pelo handler de retorno e pelas tentativas
void openImportScan();
void beginExportToDownloads();

// F5.5 — LOG DE TRANSIÇÃO do all-files ("storage: all-files granted=1/0" —
// visível no log viewer do C33). force=true nos pontos onde TODO o valor
// observado deve sair (boot, retorno das definições, re-verificação no
// resume); nas tentativas de import/export só quando MUDA desde a última
// linha (sem spam — a mesma verificação corre a cada tentativa).
int g_grantedLogged = -1;   // último granted=N logado (-1 = nunca)

void logAllFilesGranted(bool granted, const char* where, bool force = false) {
    const int g = granted ? 1 : 0;
    if (!force && g == g_grantedLogged) {
        return;
    }
    g_grantedLogged = g;
    elog::info("storage: all-files granted=%d (%s)", g, where);
}

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
    // F5.5: transição detetada numa TENTATIVA (ex.: o utilizador concedeu/
    // revogou nas definições do sistema e voltou sem passar pelo fluxo da
    // app) — só loga quando o valor é DIFERENTE da última linha.
    logAllFilesGranted(supported && mgr, "verificacao por tentativa");
    return supported && mgr;
}

// F5.5 — CAUDA do retorno das definições (toast claro + ação retomada 1×),
// SEM transição de estado: o chamador já fez onSettingsReturn (o
// onStorageResult diretamente, ou o resumeRecheck do APP_CMD_RESUME).
// A recusa NUNCA relança as definições (nenhum loop — a próxima tentativa
// é SEMPRE iniciada pelo utilizador).
void resumePendingAfterReturn(bool granted) {
    if (!granted) {
        showToast("acesso não ativado — modo app-private");
        return;
    }
    showToast("acesso concedido — File API direta");
    // retoma a ação que abriu o diálogo (1 import, 2 export; None = foi
    // aberto pelo Settings "Acesso a ficheiros…" — nada a retomar)
    const storage::Action act = g_perm.takePendingAction();
    if (act == storage::Action::Import) {
        openImportScan();
    } else if (act == storage::Action::Export) {
        beginExportToDownloads();
    }
}

// F5.5 — transição COMPLETA do retorno das definições (concedido ou não),
// pelo onActivityResult (onStorageResult): estado + toast + ação retomada.
void finishStorageReturn(bool granted) {
    g_perm.onSettingsReturn(granted);
    resumePendingAfterReturn(granted);
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
    logAllFilesGranted(granted, "retorno das definicoes", /*force=*/true);
    // F5.5: a transição inteira (estado + toast + ação retomada) vive em
    // finishStorageReturn — partilhada com a re-verificação do RESUME
    finishStorageReturn(granted);
    elog::info("storage: retorno das definicoes — supported=%d mgr=%d → %s "
               "(modo %s)", supported ? 1 : 0, mgr ? 1 : 0,
               granted ? "CONCEDIDO" : "negado/sem accao",
               storage::modeLabel(g_perm.mode()));
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

// TENTATIVA de import: concedido → abre o NAVEGADOR (0.7.2: qualquer pasta
// + galeria, com o caminho visível); senão → DIÁLOGO (1ª vez do fluxo) com a
// ação pendente; bloqueado → mensagem HONESTA com a causa real
void attemptImport() {
    if (!g_projectReady) {
        showToast("sem projeto — import indisponível");
        return;
    }
    bool supported = false;
    if (storageGrantedNow(&supported)) {
        browserOpen(std::string(fileapi::kExternalRoot) + "/Download");
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

// 0.6.8: ENTRA no play — snapshot da pose + menus fechados (em play não há
// edição; ao parar, os PAINÉIS voltam exatamente — scroll/seleção vivem
// fora das flags de overlay e ficam intactos)
void enterPlayMode() {
    g_editor.playMode = true;
    editor::closeAllOverlays(g_editor);
    g_playUi.armed = false;   // 0.7.0: nenhum on-click armado atravessa a transição
    // 0.8.0 (F7): o preview da timeline MORRE na transição (restaura a pose
    // de editor ANTES do snapshot — o play de jogo captura a pose limpa)
    timeline::stopPreview(g_scene, g_timeline.previewTic, g_timeline);
    playSnapshotCapture(g_scene, g_playSnap);
    // 0.8.0 (F7): TODOS os players arrancam do zero em Play (o critério de
    // aceitação: "em Play objeto/UI anima")
    auto& players = g_scene.components().animators();
    for (u32 i = 0; i < players.size(); ++i) {
        AnimationPlayer& pl = players.at(i);
        const Tic* ownerTic = g_scene.get(players.owner(i));
        pl.playing = ownerTic && ownerTic->active;
        pl.time = 0.0f;
        pl.resetDir();
    }
    g_animSystem.enabled = true;
    LOGI("ui: modo play — snapshot de %u transforms / %u bodies / %u ui-elems; "
         "%u animation player(s) a tocar",
         (unsigned)g_playSnap.transforms.size(),
         (unsigned)g_playSnap.bodies.size(),
         (unsigned)g_playSnap.uiElems.size(),
         (unsigned)players.size());
}

// 0.6.8: SAI do play — repõe a pose de editor (PlaySnapshot intacto) e a UI
// de EDITOR volta com os painéis nos seus sítios exatos
void leavePlayMode() {
    g_editor.playMode = false;
    g_animSystem.enabled = false;   // 0.8.0: animação só avança em Play
    playSnapshotRestore(g_scene, g_playSnap);
    // 0.8.0 (F7): players PARADOS no zero (o Play é sandbox — o estado de
    // edição nunca herda "a meio de um clip")
    auto& players = g_scene.components().animators();
    for (u32 i = 0; i < players.size(); ++i) {
        AnimationPlayer& pl = players.at(i);
        pl.playing = false;
        pl.time = 0.0f;
        pl.resetDir();
    }
    LOGI("ui: modo editor — pose restaurada (%u transforms, %u ui-elems)",
         (unsigned)g_playSnap.transforms.size(),
         (unsigned)g_playSnap.uiElems.size());
}

// F4: alimenta os TouchControls ativos (só em modo Play) e devolve a máscara
// de slots reclamados (a câmara ignora esses dedos)
u32 feedTouchControls(f32 w, f32 h, bool& outDrawn, TouchControls** outTc) {
    u32 claimed = 0;
    outDrawn = false;
    *outTc = nullptr;
    if (!g_editor.playMode) {   // 0.6.8: modo de UI no EditorState
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
        case APP_CMD_INIT_WINDOW: {
            // 0.6.7 — diagnóstico do lifecycle: o 2º (e seguintes)
            // INIT_WINDOW nesta execução são RE-CRIAÇÕES do contexto EGL
            // (voltar do fundo/recents/reentrar no editor sem matar a app).
            // Neste caso NENHUM recurso GL do ciclo anterior sobrevive — o
            // TERM_WINDOW destruiu-os todos — e o boot abaixo re-cria e
            // re-upe TUDO (fonte, cubo, grid, renderer, assets do g_gpu).
            const bool contextRecreated = (g_windowTerms > 0);
            ++g_windowInits;
            if (contextRecreated) {
                elog::warn("lifecycle: INIT_WINDOW #%u — contexto EGL "
                           "RE-CRIADO (janela re-aberta sem matar a app; "
                           "recursos GL do ciclo anterior foram libertos "
                           "no TERM_WINDOW #%u — re-upload de tudo)",
                           g_windowInits, g_windowTerms);
            }
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
            // F1 sem assets: fonte do sistema (primeira que existir vence).
            // 0.6.7: após TERM_WINDOW o atlas está destruído (tex_ == 0) →
            // isto é um RE-BAKE + RE-UPLOAD no contexto novo. O guard do
            // loadFromPaths só evita o upload duplicado no MESMO contexto.
            if (!g_font.loadFromPaths(kSystemFontPaths, kSystemFontPathCount, 28.0f)) {
                elog::error("[boot 3/6] fonts FALHOU — nenhuma fonte do sistema — UI sem texto");
            } else {
                elog::info("[boot 3/6] fonts OK (sistema, 28px)%s",
                           contextRecreated
                               ? " — RE-UPLOAD no contexto novo (lifecycle)"
                               : "");
            }
            g_ui.init();
            g_ui.setFont(&g_font);
            // 0.7.0: resolver de texturas da UI criável (elemento Image — ref
            // relativa → Texture do GpuAssets; o mesmo cache do material)
            g_ui.setImageResolver([](const std::string& ref) -> const Texture* {
                std::string warn;
                return g_gpu.texture(ref, &warn);
            });
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
        }
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
        case APP_CMD_RESUME: {
            // F5.5 — All Files Access: RE-VERIFICAÇÃO NO RETORNO (o
            // "re-verificar no onResume" do fluxo). O caminho normal do
            // retorno das definições é o onActivityResult (o Java corre-o
            // ANTES do onResume), que enfileira p/ ESTE thread — a fila é
            // drenada AQUI primeiro para que o retorno seja tratado pelo
            // onStorageResult (a ordem natural; o drain é a mesma chamada
            // do loop da engine, fila mutex-guarded). Se DEPOIS do drain o
            // fluxo continua PendingSettings (ecrã de settings OEM que
            // termina SEM setResult / volta pelos recents), a verificação
            // FRESCA de isExternalStorageManager decide AGORA via
            // resumeRecheck (política pura afervel no CI):
            //   concedido → Granted + ação pendente RETOMADA (o import
            //              prossegue sem re-pedir — o caminho feliz);
            //   recusado → Idle + toast claro "acesso não ativado" SEM
            //              relançar as definições (nenhum loop).
            // Nada corre sem fluxo pendente (arranque frio e voltas do
            // fundo incluídas — o guard é o estado do fluxo, não o cmd).
            // NOTA: SEM setMode aqui antes do recheck — setMode(AllFiles)
            // põe o estado em Granted e o recheck tem de ver PendingSettings.
            while (storage::pollResult()) {
            }
            if (g_perm.state() == storage::FlowState::PendingSettings) {
                bool mgr = false;
                const bool supported = storage::jniStorageApiSupported(&mgr);
                if (storage::resumeRecheck(g_perm, supported, mgr)) {
                    // concedido ⟺ modo ficou AllFiles (PendingSettings
                    // implica que NÃO era AllFiles antes do recheck)
                    const bool granted =
                        g_perm.mode() == storage::Mode::AllFiles;
                    logAllFilesGranted(granted, "re-verificacao no resume",
                                       /*force=*/true);
                    elog::info("storage: resume sem onActivityResult — "
                               "decisao aqui (supported=%d mgr=%d → modo %s)",
                               supported ? 1 : 0, mgr ? 1 : 0,
                               storage::modeLabel(g_perm.mode()));
                    resumePendingAfterReturn(granted);
                }
            }
            break;
        }
        case APP_CMD_TERM_WINDOW:
            // 0.6.7 (fix dos "cubinhos"): o EglContext::shutdown destrói a
            // SURFACE E O CONTEXTO — TODOS os ids GL ficam órfãos. O código
            // antigo só libertava cubo/grid/renderer: o atlas da fonte
            // (tex_ != 0 stale → o guard saltava o re-upload) e os mapas do
            // GpuAssets (Mesh*/Texture* com handles mortos) sobreviviam ao
            // term e o próximo INIT_WINDOW usava-os → texto branco em quads
            // e draws contra ids inválidos.
            //
            // Ordem obrigatória (tudo com o contexto AINDA corrente, o
            // g_egl.shutdown é o ÚLTIMO a correr):
            //   1. detach dos MeshRenderers (ponteiros de GPU vão morrer);
            //   2. g_gpu.releaseAll() — glDelete* dos meshes/texturas;
            //   3. g_font.destroy() — glDeleteTextures do atlas + reset;
            //   4. cubo/grid/renderer (como antes);
            //   5. EGL por fim.
            // NENHUM recurso GL é assumido vivo entre term/init — o boot do
            // INIT_WINDOW re-cria e re-upe tudo (fonte incluída).
            g_ready = false;
            {
                const u32 detached = detachRenderersFromGpu();
                const u32 gpuMeshes = g_gpu.meshCount();
                const u32 gpuTextures = g_gpu.textureCount();
                g_gpu.releaseAll();          // unique_ptr → ~Mesh/~Texture → glDelete*
                g_font.destroy();            // atlas: glDeleteTextures + reset das métricas
                g_cubeMesh.destroy();
                primMeshDestroy();           // 0.8.0: primitivas procedurais
                g_grid.destroy();
                g_renderer.shutdown();      // programa UI + VAO/VBO + whiteTex + lit
                g_egl.shutdown();            // POR FIM: surface + contexto morrem
                ++g_windowTerms;
                elog::info("lifecycle: TERM_WINDOW #%u — contexto EGL destruído; "
                           "libertados: %u mesh(es) gpu, %u textura(s) gpu, "
                           "atlas da fonte (re-bake no próximo init), %u "
                           "MeshRenderer(es) desligados; NENHUM recurso GL "
                           "assumido vivo no próximo INIT_WINDOW",
                           g_windowTerms, gpuMeshes, gpuTextures, detached);
            }
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
// 0.7.0: TIC INVISÍVEL não desenha (gestão de TICs — física/lógica
// continuam; só o render salta) e o TINT do MeshRenderer (cor por TIC,
// sliders R/G/B do Inspector) vai ao drawMesh.
DrawStats drawTics(const Mat4& vp) {
    DrawStats st{};
    const ComponentStore& comps = g_scene.components();
    const auto& mrs = comps.meshRenderers();
    for (u32 i = 0; i < mrs.size(); ++i) {
        const MeshRenderer& mr = mrs.at(i);
        if (!mr.mesh) {
            continue;   // sem mesh (tag "none" de cena antiga) — nada a desenhar
        }
        const Tic* owner = g_scene.get(mrs.owner(i));
        if (owner && !owner->visible) {
            continue;   // 0.7.0: invisível — o editor e o play NÃO desenham
        }
        Mat4 model = Mat4::identity();
        if (const Transform3D* tr = comps.transforms().find(mrs.owner(i))) {
            model = tr->world;   // mantido por TransformSystem (grupo Update)
        }
        st = st + g_renderer.drawMesh(*mr.mesh, model, vp, mr.texture, mr.tint);
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

// 0.6.8: status line inferior partilhada pelos DOIS modos (fps/tics/verts/
// dc/formato/cache — em play também é útil e não é painel de edição)
void statusLine(const DrawStats& st3d, const DrawStats& stGrid) {
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
}

void frame() {
    const f32 w = static_cast<f32>(g_egl.width());
    const f32 h = static_cast<f32>(g_egl.height());

    // 0.7.1 — TRANSIÇÃO DE CENA: avança o relógio e faz o SWAP no ponto
    // médio (com o ecrã tapado — doSwitchScene guarda/carrega as cenas)
    if (ui::transitionStep(g_sceneTrans, g_frameDt)) {
        if (g_sceneTransIdx < g_project.scenes.size()) {
            doSwitchScene(g_sceneTransIdx);
        }
        g_sceneTransIdx = 0xFFFFFFFFu;
    }

    // F4: TouchControls primeiro (só em modo Play) — os dedos que nasceram
    // nos controlos não vão para a câmara
    bool tcDrawn = false;
    TouchControls* tcDraw = nullptr;
    const f32 tcW = w - g_ui.safeLeft() - g_ui.safeRight();   // F4.2: área útil
    const f32 tcH = h - g_ui.safeTop() - g_ui.safeBottom();
    u32 claimed = feedTouchControls(tcW, tcH, tcDrawn, &tcDraw);

    // 0.7.0: UI criável em PLAY — hit-test por cima dos controlos (press
    // armado; o slot reclamado não vai à câmara); o DRAW acontece no bloco
    // do play (depois da play bar, antes dos TouchControls)
    u32 canvasClaimed = 0;
    if (g_editor.playMode) {
        canvasClaimed = feedCanvasPlay(w, h);
    } else {
        g_playUi.armed = false;
    }

    // 0.6.9: gizmos — vp/base da câmara ANTES do orbit (o hit-test precisa
    // delas para reclamar o slot que apanha o gizmo). Só em EDITOR com TIC
    // selecionado que tenha Transform3D; o drag escreve no componente via
    // âncoras (feedGizmo) e o slot reclamado NÃO orbita.
    // 0.7.0: também NÃO em modo UI (o viewport central é o editor 2D — os
    // gizmos são 3D; o orbit também fica desligado no modo UI)
    u32 gizmoClaimed = 0;
    Tic* gizmoTic = g_scene.get(g_editor.selected);
    Transform3D* gizmoTr =
        (gizmoTic && gizmoTic->active) ? gizmoTic->getComponent<Transform3D>()
                                        : nullptr;
    if (gizmo::visible(g_editor.playMode || g_editor.uiMode, gizmoTr != nullptr)) {
        const Mat4 gview = g_camera.view();
        const Mat4 gproj = g_camera.proj(w / h);
        const Mat4 gvp = Mat4::mul(gproj, gview);
        const gizmo::ViewBasis gbasis = gizmo::viewBasis(g_camera, w / h);
        const f32 glen = gizmo::gizmoLength(g_camera.dist);
        // sincroniza o modo/snap da toolbar com o estado do gizmo
        g_gizmo.mode = static_cast<gizmo::Mode>(g_gizmoMode.mode);
        g_gizmo.snap = g_gizmoMode.snap;
        gizmoClaimed = feedGizmo(gvp, gizmoTr->pos, glen, w, h, gbasis);
    } else {
        g_gizmo.active = gizmo::Axis::None;
        g_gizmo.hovered = gizmo::Axis::None;
    }

    // 0.8.0 (F7): o orbit e o tap-de-seleção nascem só na área do viewport
    // ACIMA da timeline (quando visível) — a strip não interfere com gestos
    // 3D nem com os painéis (centerRect intacto para hierarquia/inspector)
    const bool tlVisible = timeline::visible(g_editor.playMode, g_editor.uiMode,
                                              g_scene, g_editor.selected);
    UiRect viewRect = editor::centerRect(w, h, g_ui.safeArea(),
                                         g_editor.showInspector);
    if (tlVisible) {
        viewRect.h -= timeline::kTimelineH;
    }

    // input do frame anterior → câmara (só gestos nascidos no viewport
    // central da SAFE-AREA — gestos atrás da nav bar não orbitam, F4.2)
    // 0.6.8: orbit DESATIVADO em play (guard playMode dentro — 1 dedo =
    // controlos); lógica extraída p/ editor:: (afervel no CI)
    // 0.6.9: claimed | gizmoClaimed — drag em gizmo NÃO orbita
    // 0.7.0: orbit também DESATIVADO em modo UI (o viewport central é o
    // editor 2D da UI — arrastar elementos não pode orbitar por baixo)
    editor::updateCameraOrbit(
        g_camera, g_orbit, g_input, viewRect,
        claimed | gizmoClaimed | canvasClaimed,
        g_editor.playMode || g_editor.uiMode);

    // 0.7.0 — DESSELECCIONAR: tap parado no vazio do viewport 3D limpa a
    // seleção (só em editor 3D; o modo UI desseleciona o ELEMENTO no
    // drawUiViewport, e a Hierarchy trata do seu vazio)
    // 0.7.7 — o MESMO tap pode ter acertado numa câmara: nesse caso
    // SELECIONA o TIC dela. 0.7.10 — PRIORIDADE DE OBJETOS + hit-test
    // RESTRITO: o picker testa primeiro os TICs SELECIONÁVEIS (meshes,
    // centro projetado a 44 px) e SÓ DEPOIS a câmara (CORPO/LENTE apenas
    // — tocar no cone vazio não seleciona nem bloqueia o orbit).
    if (!g_editor.playMode && !g_editor.uiMode) {
        if (editor::viewportTapClearsSelection(
                g_editor, g_input, viewRect,
                claimed | gizmoClaimed)) {
            const Mat4 tapVp = Mat4::mul(g_camera.proj(w / h), g_camera.view());
            f32 px = 0.0f, py = 0.0f;
            g_input.pos(0, px, py);
            const Handle hc =
                camgizmo::pickSceneTic(g_scene, tapVp, w, h, px, py);
            if (hc.valid()) {
                g_editor.selected = hc;
                LOGI("editor: tic selecionado pelo toque no viewport");
            }
        }
    }

    // F4: base de movimento do input = câmara (stick-cima afasta da câmara).
    // 0.7.7: em PLAY com câmara de jogo ATIVA, a base vem da POSE DELA (o
    // jogador move-se em relação ao que VÊ); sem câmara ativa, a orbit.
    Tic* gameCamTic =
        g_editor.playMode ? findActiveCameraTic(g_scene) : nullptr;
    Transform3D* gameCamTr =
        gameCamTic ? gameCamTic->getComponent<Transform3D>() : nullptr;
    CameraComp* gameCamComp =
        gameCamTic ? gameCamTic->getComponent<CameraComp>() : nullptr;
    if (gameCamTr && gameCamComp) {
        Vec3 fwd = camgizmo::gameForward(*gameCamTr);
        Vec3 right = gameCamTr->rot.rotate(Vec3{1.0f, 0.0f, 0.0f});
        fwd.y = 0.0f;
        right.y = 0.0f;
        fwd = normalized(fwd);
        right = normalized(right);
        if (length(fwd) > 0.5f) {
            g_physics.frame.fwd = fwd;
            g_physics.frame.right = right;
        } else {
            // câmara a olhar para a vertical: fallback da orbit
            const f32 sy = std::sin(g_camera.yaw);
            const f32 cy = std::cos(g_camera.yaw);
            g_physics.frame.fwd = Vec3{-sy, 0.0f, -cy};
            g_physics.frame.right = Vec3{cy, 0.0f, -sy};
        }
    } else {
        const f32 sy = std::sin(g_camera.yaw);
        const f32 cy = std::cos(g_camera.yaw);
        g_physics.frame.fwd = Vec3{-sy, 0.0f, -cy};
        g_physics.frame.right = Vec3{cy, 0.0f, -sy};
    }
    g_physics.enabled = g_editor.playMode;   // física só avança em modo Play
    g_animSystem.enabled = g_editor.playMode;  // 0.8.0: animação idem (F7)

    // 0.8.0 (F7): rebind LAZY das primitivas (params editados no Inspector
    // ou pós-TERM_WINDOW — o mesh null volta ao cache neste frame)
    rebindPrimMeshes();

    // ---- pass 3D: clear color+depth, TICs com MeshRenderer + grid com fade
    // 0.7.7 — CÂMARA DE JOGO: em Play a cena renderiza pela câmara ATIVA
    // (pose do Transform3D + parâmetros do CameraComp); sem câmara ativa o
    // fallback é a orbit de edição. O EDITOR mantém a orbit SEMPRE (o
    // frustum é que é o gizmo — nunca o render).
    Mat4 view;
    Mat4 proj;
    Vec3 camEye;
    f32  camFocus;
    if (gameCamTr && gameCamComp) {
        view = camgizmo::gameView(*gameCamTr);
        proj = camgizmo::gameProj(*gameCamComp, w / h);
        camEye = gameCamTr->pos;
        camFocus = length(gameCamTr->pos);   // fade do grid: dist. ao centro
    } else {
        view = g_camera.view();
        proj = g_camera.proj(w / h);
        camEye = g_camera.eye();
        camFocus = g_camera.dist;
    }
    g_renderer.beginFrame();
    const Mat4 vp = Mat4::mul(proj, view);
    const DrawStats st3d = drawTics(vp);
    const DrawStats stGrid = g_grid.draw(vp, camEye, camFocus);

    // 0.7.8 — FRONTEIRA EXPLÍCITA 3D→UI: repor o estado GL que o pass 3D
    // deixou (LitMaterial liga depth/cull com a proj da câmara de jogo; o
    // grid alterna blend/depthmask) ANTES de qualquer widget: viewport
    // CHEIO com o tamanho ATUAL, scissor off, depth off p/ a UI. O pass de
    // UI NUNCA herda o estado do pass 3D — a UI de jogo volta ao tamanho/
    // posição do resolver (coords de ecrã + safe-area), por cima da cena.
    g_renderer.beginUiPass(static_cast<i32>(w), static_cast<i32>(h));

    // ---- pass UI: immediate-mode da F1 por cima (sem depth — nunca ocluída)
    g_ui.beginFrame(&g_renderer, &g_input, w, h);

    if (g_editor.playMode) {
        // ---- 0.6.8: PLAY — janela própria ----------------------------------
        // Viewport fullscreen (painéis/toolbar escondidos) + UI CRIÁVEL por
        // cima da cena (0.7.0 — canvases dos TICs ativos/visíveis) +
        // TouchControls (ancorados na safe-area) + barra superior mínima.
        // ORBIT DESATIVADO (o guard playMode do editor::updateCameraOrbit já
        // correu acima — 1 dedo = controlos/botões da UI).
        const bool stop = editor::drawPlayBar(g_ui, g_input, w, h,
                                              static_cast<int>(g_fps + 0.5f));
        if (stop) {
            // Stop → EDITOR: pose restaurada (PlaySnapshot) + painéis
            // repostos EXATAMENTE (os scrolls/seleção nunca foram tocados)
            leavePlayMode();
            showToast("modo editor");
        }
        // 0.7.0: UI criável POR CIMA da cena (antes dos controlos — os
        // botões da UI ficam por baixo do joystick/jump quando sobrepõem)
        drawCanvasPlay(w, h);
        // TouchControls por cima de tudo (só existem em play — o feed no
        // início do frame já devolveu 0 claimed e tcDrawn=false no editor)
        if (tcDrawn && tcDraw) {
            editor::drawTouchControls(g_ui, *tcDraw, w, h);
        }
        drawToast();
        statusLine(st3d, stGrid);
        // 0.7.1: o overlay da transição por cima de TUDO (a troca ao escuro)
        ui::transitionDraw(g_ui, g_sceneTrans, w, h);
        g_ui.endFrame();                       // submete solids + glyphs
        g_lastUiStats = g_renderer.endFrame(); // UI por cima do 3D
        g_egl.swap();
        g_input.clearEdges();
        return;
    }

    // ---- 0.6.8: EDITOR — toolbar + painéis + menus (como sempre) ----------
    // 0.6.9: gizmo PRIMEIRO (as linhas ficam POR BAIXO dos painéis —
    // z-order correto de editor) e só com seleção em EDITOR
    // 0.7.0: gizmos só no modo 3D (o modo UI desenha o viewport 2D no
    // MESMO sítio — nunca os dois)
    // 0.7.5 — OVERLAYS MODAIS: com um modal aberto o CHROME DO EDITOR não
    // se desenha (nada de toolbar/painéis/canvas UI por trás/à mista com o
    // overlay — o C33 via o texto do canvas ATRAVÉS do MENU) e no seu lugar
    // desenha-se um BACKDROP OPACO que tapa o ecrã TODO. Os widgets são
    // immediate-mode: não desenhados = não interativos (os toques só
    // pertencem ao modal, que fecha com toque fora como sempre).
    const bool modalOpen = editor::anyOverlayOpen(g_editor);
    if (modalOpen) {
        editor::drawModalBackdrop(g_ui, w, h);
    }
    // 0.7.7 — FRUSTUMS das câmaras da cena (wireframe na cor de marca; a
    // selecionada ganha os handles do far). SÓ no editor 3D — nunca em
    // Play (como os gizmos)
    if (!modalOpen && camgizmo::visible(g_editor.playMode, g_editor.uiMode)) {
        camgizmo::drawAll(g_ui, g_scene, vp, w, h, g_editor.selected);
    }
    if (!modalOpen && gizmo::visible(g_editor.playMode || g_editor.uiMode,
                                     gizmoTr != nullptr)) {
        gizmo::drawGizmo(g_ui, vp, gizmoTr->pos,
                         gizmo::gizmoLength(g_camera.dist), g_gizmo.mode,
                         g_gizmo.hovered);
    }
    // ---- 0.7.6 — BARRA FINAL DE 5 GRUPOS ---------------------------------
    // G1 [Menu ▾][Cena ▾] · G2 [pause][play] · G3 [3D|UI] · G4 transformação
    // (SÓ com seleção) · G5 [inspector]. Ícones vetoriais no line batch,
    // ativos com fundo de marca (ui/Toolbar.cpp).
    if (!modalOpen) {
        Tic* selTic = g_scene.get(g_editor.selected);
        const editor::toolbar::Actions ta = editor::toolbar::draw(
            g_ui, g_editor, g_gizmoMode, selTic && selTic->active);
        if (ta.menuDropdown) {
            // G1 Menu → dropdown (Settings/Guardar/…/Sair)
            g_editor.fileMenu = !g_editor.fileMenu;
            g_editor.plusMenu = false;
            g_editor.settingsMenu = false;
        }
        if (ta.cenaDropdown) {
            // G1 Cena → dropdown de cenas do projeto (overlay CENAS de
            // sempre — lista + nova + trocar)
            g_editor.scenesMenu = !g_editor.scenesMenu;
            g_editor.plusMenu = false;
            g_editor.settingsMenu = false;
            g_editor.fileMenu = false;
        }
        if (ta.playPressed && !g_editor.playMode) {
            // G2 play: a JANELA PLAY (sem painéis, orbit off); o Stop da
            // play bar (ou o pause) volta ao EDITOR com a pose restaurada.
            enterPlayMode();
            showToast("modo play");
        }
        if (ta.pausePressed && g_editor.playMode) {
            // defensivo: em play a barra não se desenha (o Stop da play bar
            // é o caminho normal) — se algum dia se desenhar, pausa = sair
            leavePlayMode();
            showToast("modo editor");
        }
        // viewport 2D no modo UI (antes dos painéis — z-order de editor)
        if (g_editor.uiMode) {
            editor::drawUiViewport(g_ui, g_scene, g_editor, g_input, w, h);
        }
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

    // F3: painéis do editor (Hierarquia esquerda, Inspector direita).
    // 0.7.0: no modo UI com ELEMENTO selecionado o painel direito mostra o
    // INSPECTOR DE UI (pos/size/cor/texto/visivel/âncoras/ação); sem elemento
    // (ou em 3D) o Inspector de TICs de sempre.
    // 0.7.5: com um MODAL aberto os painéis NÃO se desenham (o backdrop
    // tapa o editor; nada de texto à mista — e sem desenho não há gesto)
    if (!modalOpen) {
        if (editor::drawHierarchy(g_ui, g_scene, g_editor)) {
            g_editor.plusMenu = true;   // "+" no cabeçalho abre os presets
            g_editor.fileMenu = false;
        }
        const bool uiInsp =
            g_editor.uiMode && (g_editor.selElement >= 0 || g_editor.selJoystick);
        if (uiInsp) {
            editor::drawUiInspector(g_ui, g_scene, g_editor, g_input);
        } else if (g_editor.showInspector) {
            // 0.7.6: o G5 da toolbar pode ter escondido o painel direito
            editor::drawInspector(g_ui, g_scene, g_editor, &g_catalog);   // sliders + seletores
        }
        // 0.8.0 (F7): TIMELINE do AnimationPlayer do TIC selecionado — strip
        // no FUNDO do viewport central (nada sobrepõe os painéis; o orbit já
        // nasce só na área acima dela via viewRect). Com modal aberto fica
        // tapada pelo backdrop (como os painéis — sem desenho não há gesto).
        if (tlVisible) {
            timeline::drawTimeline(g_ui, g_input, g_scene, g_editor, g_timeline,
                                   g_frameDt);
        }
    }

    // overlay "+" → presets de TIC (3D) OU elementos de UI (modo UI, 0.7.0)
    if (g_editor.plusMenu) {
        const int choice = editor::drawPlusMenu(g_ui, g_input, w, h, g_editor);
        if (choice > 0) {
            if (g_editor.uiMode) {
                // 0.7.4 — o mapa escolha→Kind vive em uiPlusChoiceKind
                // (1..7 Panel..Article; 8 Joystick; 9/10 VBox/HBox)
                const int kind = editor::uiPlusChoiceKind(choice);
                const bool hadTic = g_scene.get(g_editor.selected) != nullptr;
                if (kind < 0) {
                    // 0.7.3 — JOYSTICK: widget de TouchControls EDITÁVEL no
                    // TIC selecionado (a UI do Player passa a ser esta
                    // instância); seleciona-o no viewport 2D
                    Tic* tic = g_scene.get(g_editor.selected);
                    if (!tic) {
                        showToast("selecione um TIC na Hierarchy");
                    } else {
                        TouchControls* tc = tic->getComponent<TouchControls>();
                        if (!tc) {
                            tc = tic->addComponent<TouchControls>();
                        }
                        if (tc) {
                            g_editor.selJoystick = true;
                            g_editor.selElement = -1;
                            showToast("joystick adicionado");
                            LOGI("editor: joystick (TouchControls) adicionado");
                        }
                    }
                } else if (editor::uiAddElement(g_scene, g_editor,
                                               static_cast<u32>(kind), w, h)) {
                    // 0.7.0: cria o elemento no canvas do TIC selecionado
                    // (cria o canvas à primeira) e seleciona-o — WYSIWYG.
                    // 0.7.4: com container selecionado nasce FILHO dele.
                    // 0.7.5: SEM TIC selecionado o uiAddElement assegura/
                    // cria o TIC DE UI ("UI", só com UiCanvas) — criar UI
                    // nunca obrigou a um TIC 3D.
                    if (!hadTic) {
                        showToast("TIC 'UI' criado + elemento");
                        LOGI("editor: TIC de UI criado (canvas hospedeiro)");
                    } else {
                        showToast("elemento UI criado");
                    }
                    LOGI("editor: elemento UI criado (kind %d)", kind);
                } else {
                    showToast("selecione um TIC na Hierarchy");
                }
            } else if (choice == 5) {
                // 0.7.7 — TIC de CÂMARA: Transform3D (pose) + CameraComp
                // (perspetiva). Nasce A ATIVA da cena (uma só — CameraUtil)
                const Handle hnew = g_scene.create("Camera");
                Tic* ct = g_scene.get(hnew);
                if (ct) {
                    ct->addComponent<Transform3D>();
                    if (ct->addComponent<CameraComp>()) {
                        setOnlyActiveCamera(g_scene, hnew);
                    }
                    g_editor.selected = hnew;
                    showToast("Camera criada (ativa)");
                    LOGI("editor: TIC de camera criado (ativo)");
                }
            } else if (choice == 6) {
                // 0.8.0 (F7) — TIC "Mesh": Transform+MeshRenderer com a
                // PRIMITIVA esfera default (SEM física — prototipagem pura;
                // troca-se o tipo/params no Inspector, anima-se na timeline)
                const PrimParams sph = primDefaults(PrimKind::Sphere);
                const Handle hnew = createTicFromPreset(
                    g_scene, PresetKind::Mesh, primMesh(sph),
                    g_renderer.litMaterial());
                if (hnew.valid()) {
                    g_editor.selected = hnew;
                    showToast("Mesh criado (esfera)");
                    LOGI("editor: TIC Mesh criado (primitiva esfera default)");
                }
            } else {
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
    }

    // F5-E/F6: seletor de assets aberto → aplica no MeshRenderer selecionado.
    // F6 (fix do C33 0.6.9): o menuKind é capturado ANTES do drawAssetMenu —
    // o seletor FECHA a si próprio no clique (st.assetMenu = 0 dentro do
    // draw) e o dispatch antigo lia g_editor.assetMenu DEPOIS da chamada
    // (sempre 0 → bloco morto desde a F5-E: a escolha nunca chegava ao
    // componente; o C33 via "tex: none" eterno, cubo cinzento e NENHUMA
    // linha no engine.log). A lógica vive agora em editor::applyAssetPick
    // (pura, afervel no CI) e o resultado traz o toast + a linha de log.
    // 0.7.4: menuKind 3 = textura de ELEMENTO de UI (Inspector de UI, linha
    // tex:) → applyUiTexPick (escreve a ref; a render resolve por frame) +
    // "importar…" abre o NAVEGADOR 0.7.2 (o ficheiro cai em textures/).
    if (g_editor.assetMenu != 0) {
        const int menuKind = g_editor.assetMenu;   // ANTES do draw (o pick fecha)
        const int pick = editor::drawAssetMenu(g_ui, g_input, w, h, g_editor,
                                               g_catalog, menuKind == 3);
        if (pick > 0) {
            if (menuKind == 3) {
                if (pick == editor::kAssetPickImport) {
                    // "importar…" → NAVEGADOR (0.7.2): o dono escolhe a
                    // textura de onde for; o ficheiro importa para textures/
                    // e fica no seletor (tex: de novo)
                    browserOpen(fileapi::kBrowserRoots[0].path);
                    g_editor.fileBrowser = true;
                    LOGI("editor: importar textura — navegador aberto");
                } else {
                    const editor::UiTexPickOutcome out = editor::applyUiTexPick(
                        g_scene, g_editor.selected, g_editor.selElement, pick,
                        g_catalog);
                    if (out.toast[0] != '\0') {
                        showToast(out.toast);
                    }
                    if (out.log[0] != '\0') {
                        LOGI("%s", out.log);
                    }
                }
            } else {
                const editor::AssetPickOutcome out =
                    editor::applyAssetPick(g_scene, g_editor.selected, menuKind,
                                           pick, g_catalog, makeAssetResolvers());
                if (out.toast[0] != '\0') {
                    showToast(out.toast);
                }
                if (out.log[0] != '\0') {
                    LOGI("%s", out.log);
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
        if (choice == 1) {
            // 0.7.6: Settings (o item do dropdown do Menu — o botão próprio
            // deixou de existir na barra)
            g_editor.settingsMenu = true;
            elog::info("ui: menu Settings aberto (dropdown do Menu)");
        } else if (choice == 2 && g_projectReady) {
            const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                            g_project.saveManifest(*g_storage);
            // F5.4-hotfix: Salvar materializa os assets que só existem em
            // runtime (cubo procedural → meshes/cube.obj, formato OBJ já
            // definido). Cada tipo de asset fica na SUBPASTA certa — a
            // escrita em si loga "saf: write meshes/cube.obj — N bytes"
            // (visível no "Ver logs"). Falha do asset NÃO desfaz a cena
            // salva — o erro fica logado com a causa real.
            std::vector<std::string> matWritten;
            std::string matErr;
            if (!persistSceneAssets(*g_storage, g_scene, matWritten, matErr)) {
                elog::error("editor: salvar assets — %s", matErr.c_str());
            }
            for (const std::string& rel : matWritten) {
                LOGI("editor: asset materializado no Salvar → %s", rel.c_str());
            }
            char msg[64];
            std::snprintf(msg, sizeof(msg), ok ? "cena salva (%u tics)" : "falha ao salvar",
                          g_scene.count());
            showToast(msg);
            LOGI("editor: %s → %s", msg, g_project.activeScenePath()->c_str());
        } else if (choice == 3 && g_projectReady) {
            const SceneSerializer::LoadCtx ctx = makeLoadCtx();
            const bool ok = g_project.loadActiveScene(*g_storage, g_scene, ctx);
            char msg[64];
            std::snprintf(msg, sizeof(msg), ok ? "cena carregada (%u tics)" : "falha ao carregar",
                          g_scene.count());
            showToast(msg);
            g_editor.selected = Handle::invalid();   // seleção antiga não sobrevive ao load
            LOGI("editor: %s ← %s", msg, g_project.activeScenePath()->c_str());
        } else if (choice == 4 && g_projectReady) {
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
        } else if (choice == 5) {
            // F5.2: IMPORTAR — All Files Access → varre Download/Documents →
            // overlay de escolha → cópia para meshes/ ou textures/
            attemptImport();
        } else if (choice == 6) {
            // F5.2: EXPORT DOWNLOADS — All Files Access → Download/GOneVV/export
            attemptExport();
        } else if (choice == 7) {
            // 0.6.7: SAIR PARA PROJETOS — auto-save da cena + volta ao
            // gestor SEM matar a app. A activity termina-se (finish() pela
            // ponte Java — o gestor está na back stack); o APP_CMD_TERM_WINDOW
            // que se segue liberta TODOS os recursos GL (lifecycle 0.6.7-a);
            // reentrar arranca um novo android_main com contexto novo.
            if (g_projectReady && g_storage) {
                const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                                g_project.saveManifest(*g_storage);
                std::vector<std::string> matWritten;
                std::string matErr;
                persistSceneAssets(*g_storage, g_scene, matWritten, matErr);
                for (const std::string& rel : matWritten) {
                    LOGI("editor: asset materializado ao sair → %s", rel.c_str());
                }
                showToast(ok ? "cena salva — a sair…" : "save falhou — a sair…");
                elog::info("editor: sair p/ projetos — auto-save %s (%u tics, "
                           "%u asset(s) materializado(s))",
                           ok ? "OK" : "FALHOU", g_scene.count(),
                           (unsigned)matWritten.size());
            } else {
                elog::warn("editor: sair p/ projetos SEM projeto — nada a "
                           "auto-salvar");
            }
            if (storage::jniFinishToLauncher()) {
                elog::info("editor: finish() pedido à activity — o gestor "
                           "retoma (app viva)");
            } else {
                showToast("não consegui sair (ponte Java — ver logs)");
                elog::error("editor: jniFinishToLauncher FALHOU — fica no editor");
            }
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

    // 0.7.0 — GESTÃO DE TICs: menu contextual (⋮) → Renomear/Remover/
    // Duplicar/Visibilidade; diálogo de remoção COM CONFIRMAÇÃO; teclado
    // in-app (renomear/texto de elemento/alvo de ação). Por cima de tudo —
    // a ordem de desenho é o z-order.
    if (g_editor.contextMenu) {
        Tic* ct = g_scene.get(g_editor.contextTic);
        if (!ct) {
            g_editor.contextMenu = false;   // o TIC morreu entretanto
        } else {
            const int ch = editor::drawContextMenu(
                g_ui, g_input, w, h, g_editor, ct->name.c_str(), ct->visible,
                ct->getComponent<CameraComp>() != nullptr);   // 0.7.7
            if (ch == 1) {
                // RENOMEAR: teclado in-app (zero IME de sistema)
                editor::openTextInput(g_editor, 0, g_editor.contextTic, -1,
                                      ct->name.c_str());
            } else if (ch == 2) {
                // REMOVER: com confirmação (substitui o apagar sem aviso)
                g_editor.removeDialog = true;
            } else if (ch == 3) {
                // DUPLICAR: todos os componentes por valor + nome único
                const Handle dup =
                    editor::duplicateTic(g_scene, g_editor.contextTic);
                if (dup.valid()) {
                    g_editor.selected = dup;
                    showToast("TIC duplicado");
                    if (const Tic* nd = g_scene.get(dup)) {
                        LOGI("editor: TIC duplicado → '%s'", nd->name.c_str());
                    }
                }
            } else if (ch == 4) {
                // VISIBILIDADE: toggle (o olho da Hierarchy atalha o mesmo)
                ct->visible = !ct->visible;
            } else if (ch == 5) {
                // 0.7.7 — ALINHAR À VISTA: copia a pose da orbit de edição
                // para o transform da câmara (posição + orientação)
                if (Transform3D* tr = ct->getComponent<Transform3D>()) {
                    camgizmo::alignToView(*tr, g_camera);
                    showToast("camera alinhada a vista");
                    LOGI("editor: camera '%s' alinhada a vista de edicao",
                         ct->name.c_str());
                }
            }
        }
    }
    if (g_editor.removeDialog) {
        Tic* ct = g_scene.get(g_editor.contextTic);
        if (!ct) {
            g_editor.removeDialog = false;
        } else {
            const int ch =
                editor::drawRemoveDialog(g_ui, g_input, w, h, g_editor,
                                         ct->name.c_str());
            if (ch == 1) {
                char gone[48];
                std::snprintf(gone, sizeof(gone), "%.40s", ct->name.c_str());
                g_scene.destroy(g_editor.contextTic);
                if (g_editor.selected == g_editor.contextTic) {
                    g_editor.selected = Handle::invalid();
                    g_editor.selElement = -1;
                }
                showToast("TIC removido");
                LOGI("editor: TIC '%s' removido", gone);
            }
        }
    }
    if (g_editor.textInput) {
        const char* title = g_editor.textPurpose == 0 ? "RENOMEAR TIC"
                            : g_editor.textPurpose == 1 ? "NOME DA NOVA CENA"
                            : g_editor.textPurpose == 2 ? "TEXTO DO ELEMENTO"
                                                        : "ALVO DA ACAO";
        const int ch =
            editor::drawTextInput(g_ui, g_input, w, h, g_editor, title);
        if (ch == 1) {
            if (g_editor.textPurpose == 1) {
                // 0.7.1: NOVA CENA (o nome vem do teclado in-app)
                createSceneNamed(g_editor.textBuf);
            } else if (editor::commitTextInput(g_scene, g_editor)) {
                showToast("aplicado");
            }
        }
    }

    // 0.7.1 — OVERLAY CENAS: lista do projeto (a ativa marcada) + nova/trocar
    // (menu de EDITOR — em Play a troca vem pela ação declarativa com
    // transição; aqui a troca é direta)
    if (g_editor.scenesMenu && g_projectReady) {
        const int pick = editor::drawScenesMenu(
            g_ui, g_input, w, h, g_editor, g_project.scenes,
            g_project.activeScene);
        if (pick == 1) {
            // nova cena: o TECLADO in-app pede o nome (propósito 1)
            editor::openTextInput(g_editor, 1, Handle{}, -1, "");
        } else if (pick >= 2) {
            const u32 idx = static_cast<u32>(pick - 2);
            if (idx != g_project.activeScene) {
                doSwitchScene(idx);
            }
        }
    }

    // 0.7.2 — NAVEGADOR de ficheiros: raízes/subir/lista (o caminho vive no
    // topo do overlay); diretorias navegam, ficheiros IMPORTAM
    if (g_editor.fileBrowser && g_browser.open) {
        const int pick = editor::drawFileBrowser(
            g_ui, g_input, w, h, g_editor, g_browser.cwd, g_browser.entries,
            g_browser.failed);
        if (pick >= 1 && pick <= fileapi::kBrowserRootCount) {
            browserOpen(fileapi::kBrowserRoots[pick - 1].path);
        } else if (pick == 6) {
            browserOpen(fileapi::parentPath(g_browser.cwd));
        } else if (pick >= 7) {
            const size_t i = static_cast<size_t>(pick - 7);
            if (i < g_browser.entries.size()) {
                if (g_browser.entries[i].isDir) {
                    browserOpen(g_browser.entries[i].path);
                } else {
                    browserImportFile(g_browser.entries[i]);
                    g_browser.open = false;   // fecha ao importar
                    g_editor.fileBrowser = false;
                }
            }
        }
    }

    // 0.7.2 — APLICAR-APÓS-IMPORT: textura/mesh importada + TIC com
    // MeshRenderer selecionado → "aplicar ao TIC?" (Sim aplica já)
    if (g_editor.applyAsk && g_applyAsk.open) {
        Tic* tsel = g_scene.get(g_editor.selected);
        const char* ticName = tsel ? tsel->name.c_str() : "?";
        const int ch = editor::drawApplyDialog(g_ui, g_input, w, h, g_editor,
                                               g_applyAsk.fileName.c_str(),
                                               ticName);
        if (ch == 1) {
            // Sim: aplica via applyAssetPick (o MESMO caminho do seletor —
            // ref do projeto + resolvers de GPU + toast/log honestos)
            const std::vector<std::string>& cat =
                g_applyAsk.kind == 'm' ? g_catalog.meshes : g_catalog.textures;
            const std::string base = g_applyAsk.kind == 'm'
                                         ? std::string("meshes/")
                                         : std::string("textures/");
            for (size_t i = 0; i < cat.size(); ++i) {
                if (base + cat[i] == g_applyAsk.rel) {
                    const int menuKind = g_applyAsk.kind == 'm' ? 1 : 2;
                    const editor::AssetPickOutcome out = editor::applyAssetPick(
                        g_scene, g_editor.selected, menuKind,
                        static_cast<int>(i) + 2, g_catalog,
                        makeAssetResolvers());
                    if (out.toast[0] != '\0') {
                        showToast(out.toast);
                    }
                    if (out.log[0] != '\0') {
                        LOGI("%s", out.log);
                    }
                    // 0.8.1 (F7): mesh gltf/glb aplicado → importa os CLIPS
                    // de animação para o AnimationPlayer do TIC (o cache do
                    // ResourceManager garante 1 parse; o nó alvo é o RAIZ —
                    // o TIC inteiro; joints ficam para a 0.8.2)
                    if (g_applyAsk.kind == 'm') {
                        std::string merr;
                        if (auto mdl = g_resources.model(g_applyAsk.rel, merr)) {
                            if (!mdl->animations.empty()) {
                                const u32 nClips = gltfAttachClips(
                                    g_scene, g_editor.selected, *mdl);
                                if (nClips > 0) {
                                    showToast("clips importados (timeline)");
                                    LOGI("editor: %u clip(s) de animacao "
                                         "importados de %s",
                                         nClips, g_applyAsk.rel.c_str());
                                }
                            }
                        }
                    }
                    break;
                }
            }
            g_applyAsk.open = false;
        }
    }

    drawToast();
    statusLine(st3d, stGrid);

    // 0.7.1: o overlay da transição por cima de TUDO no editor também
    ui::transitionDraw(g_ui, g_sceneTrans, w, h);

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
    elog::info("G.One VV 0.7.5 — overlays modais com backdrop opaco (o "
               "canvas nunca se desenha à mista com o MENU/teclado/CENAS/"
               "navegador) + TIC de UI próprio ('UI', só com UiCanvas — "
               "criar UI sem TIC 3D selecionado) + teclado com MINÚSCULAS "
               "(toggle abc/ABC; fix do 'Z' em falta na linha S..Z) — fix "
               "das falhas de UX do C33 0.7.4)");
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

    // 0.6.7 — REENTRADA do android_main: a lib goni_vv fica CARREGADA no
    // processo (o static init NÃO volta a correr) e uma NOVA VvActivity
    // (reentrar no editor depois de "Sair para projetos") arranca ESTE
    // android_main PELA 2ª VEZ — os globais estáticos trazem o estado da
    // sessão anterior. Reset de TODO o estado de editor/cena/play/input:
    //   • os recursos GL já foram destruídos no TERM_WINDOW (0.6.7-a) —
    //     nada aqui toca em GL;
    //   • os storages/projeto são remontados ABAIXO (slot do gestor ou
    //     fallback app-private) — os unique_ptr substituem os antigos;
    //   • TickGroups É esvaziado: sem isto, a física seria registada 2× e
    //     daria DOIS passos por frame;
    //   • g_windowInits/g_windowTerms NÃO são resetados — são contadores
    //     do PROCESSO (o INIT_WINDOW da reentrada loga "contexto
    //     RE-CRIADO", que é a verdade).
    if (g_windowInits > 0 || g_windowTerms > 0) {
        elog::info("lifecycle: REENTRADA do android_main (janelas anteriores: "
                   "%u init(s), %u term(s)) — reset do estado de editor",
                   g_windowInits, g_windowTerms);
    }
    g_systems.clear();
    g_editor = editor::EditorState{};   // inclui playMode = false (0.6.8) e
                                        // uiMode/seleção de elemento (0.7.0)
    g_timeline = timeline::State{};   // 0.8.0: scrub/keys/preview da sessão anterior não vingam
    g_animSystem.enabled = false;    // 0.8.0: idem física (gate fechado)
    g_playSnap = PlaySnapshot{};
    g_playUi = PlayUiPress{};   // 0.7.0: nenhum on-click de UI armado
    g_sceneTrans = ui::SceneTransition{};   // 0.7.1: transição morta
    g_sceneTransIdx = 0xFFFFFFFFu;
    g_browser = FileBrowserState{};   // 0.7.2: browser fechado
    g_applyAsk = ApplyAskState{};
    g_scene.clear();
    g_input.resetAll();
    g_project = Project{};
    g_projectReady = false;
    g_prevAssetMenu = 0;
    g_catalog.meshes.clear();
    g_catalog.textures.clear();
    g_importCands.clear();
    g_logLines.clear();
    g_logDumps.clear();
    g_toast[0] = '\0';
    g_toastT = 0.0f;
    g_lastUiStats = DrawStats{};
    g_orbit = editor::OrbitState{};
    g_texCache.reset();       // referenciam o storage antigo — libertados
    g_pipeline.reset();        // ANTES dele (remontados quando houver storage)
    g_storage.reset();   // o antigo é destruído; remontado abaixo

    // F5-A: storage do projeto — F5.4: GESTOR DE PROJETOS. O arranque
    // ESPERE (até 3s) pelo projeto escolhido no ecrã inicial: a VvActivity
    // empurra em nativeOpenProject (extras do Intent) → ProjectSlot.
    // Chegou + handshake OK → SafStorage sobre a pasta SAF (a estrutura
    // project.goni/scenes/meshes/textures é criada por openOrCreate).
    // Sem projeto SAF → fallback app-private (comportamento 0.6.x).
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
        // falhado ficam no engine.log ANTES de qualquer I/O pesado.
        fileapi::logStorageSelfCheck(root, isExternal);

        elog::info("storage: à espera do projeto do gestor (ProjectSlot, "
                   "timeout 3000ms)");
        storage::ProjectRequest req;
        bool safReady = false;
        if (storage::projectSlot().waitFor(&req, 3000)) {
            if (storage::handshakeOk()) {
                g_storage = std::make_unique<SafStorage>(storage::jniSafIo(),
                                                         req.treeUri);
                if (Project::openOrCreate(*g_storage, req.name, g_project)) {
                    safReady = true;
                    g_projectReady = g_project.activeScenePath() != nullptr;
                    elog::info("projeto: '%s' pronto (SAF) — %u cena(s), "
                               "ativa=%s",
                               g_project.name.c_str(),
                               (unsigned)g_project.scenes.size(),
                               g_projectReady
                                   ? g_project.activeScenePath()->c_str()
                                   : "-");
                } else {
                    elog::error("projeto: '%s' SAF inutilizável — fallback "
                                "app-private",
                                req.name.c_str());
                    g_storage.reset();
                }
            } else {
                elog::error("storage: projeto '%s' recebido mas ponte Java "
                            "indisponível (handshake) — modo app-private",
                            req.name.c_str());
            }
        } else {
            elog::info("storage: sem projeto SAF no arranque (timeout ou "
                       "lançamento direto) — modo app-private");
        }

        if (!safReady) {
            if (root) {
                g_storage = std::make_unique<FsStorage>(root);
                // F5.2: ponte Java (janela de permissões + retorno + export logs)
                storage::setHandler(&onStorageResult, nullptr);
                if (Project::openOrCreate(*g_storage, "projeto", g_project)) {
                    g_projectReady = g_project.activeScenePath() != nullptr;
                    elog::info("projeto: '%s' pronto em %s (%u cena(s), "
                               "ativa=%s)",
                               g_project.name.c_str(), root,
                               (unsigned)g_project.scenes.size(),
                               g_projectReady
                                   ? g_project.activeScenePath()->c_str()
                                   : "-");
                } else {
                    elog::error("projeto: storage inutilizável em %s — editor "
                                "sem persistência", root);
                }
            } else {
                elog::error("projeto: sem externalDataPath/internalDataPath — "
                            "editor sem persistência");
            }
        } else {
            // F5.2: ponte Java (janela de permissões + retorno + export logs)
            storage::setHandler(&onStorageResult, nullptr);
        }
        if (g_storage) {
            // F5.1-A: cache/pipeline vivem enquanto o storage viver
            g_texCache = std::make_unique<TextureCache>(*g_storage);
            g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                           *g_texCache);
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
            // F5.5: granted=1/0 da transição de BOOT (a 1ª linha do
            // all-files que o log viewer mostra num arranque frio)
            logAllFilesGranted(supported && mgr, "boot", /*force=*/true);
        }
        // [boot 2/6] storage — passo crítico do arranque (ficheiro legível
        // no device: se o boot morrer aqui, o dono vê exatamente onde)
        elog::info("[boot 2/6] storage %s (origem=%s, modo=%s)",
                   g_storage ? "OK" : "FALHOU",
                   safReady ? "SAF (pasta escolhida)"
                            : (app->activity && app->activity->externalDataPath
                                   ? "app-private (external)"
                                   : "app-private (internal)"),
                   storage::modeLabel(g_perm.mode()));
    }

    // F3/F4: systems do engine (ordem interna ao grupo = registo)
    // 0.8.0 (F7): a ANIMAÇÃO registra ANTES do TransformSystem — escreve
    // pos/rot/scale (updateWorld já no apply); o Transform reconfirma o
    // cache no mesmo passo (nunca vê dados meio-escritos)
    g_systems.add(TickGroup::Update, &g_animSystem);
    g_systems.add(TickGroup::Update, &g_transformSystem);
    g_systems.add(TickGroup::Physics, &g_physics);   // entre Update e PostUpdate
    elog::info("[boot 5/6] physics OK (tickgroups Update+Physics registados; "
               "animacao no Update antes do transform)");

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
        g_frameDt = static_cast<f32>(realDt);   // 0.7.1: relógio da transição

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
