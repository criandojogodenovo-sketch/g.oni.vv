// platform/main.cpp — android_main (glue) da G.One VV — F3.
// Fluxo: glue → EGL/GLES3 (depth 24) → loop de timestep fixo
//        → TickGroups (Pre→Up→Post→Render; TransformSystem no Update)
//        → pass 3D (todos os TICs com MeshRenderer + grid, depth test)
//        → pass UI (toolbar F1 + Hierarchy/Inspector/menus F3, sem depth).
#include <android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <atomic>   // 0.8.10: progresso do import entre threads
#include <cmath>
#include <cstdio>
#include <cstdlib>   // 0.8.11: atof do settings.goni
#include <cstring>   // 0.8.11: strcmp do probe
#include <ctime>   // 0.8.11: nome do clip gravado (rec-<unix>.gi)
#include <map>   // 0.8.11: cache de clipes .gi
#include <memory>
#include <mutex>   // 0.8.11: PCM da gravação entre worker e frame
#include <thread>   // 0.8.10: import job fora do frame loop

#include "assets/AssetConverter.h"   // 0.8.10: import streaming + formatos próprios
#include "assets/ZipExtract.h"       // 0.8.10: archives (extrair ≠ importar)
#include "assets/GiFormat.h"         // 0.8.11: clips .gi (ADPCM/OGG/MP3)
#include "components/AudioPlayer.h"  // 0.8.11: o TIC de áudio
#include "core/AudioEngine.h"        // 0.8.11: misturador
#include "platform/AudioOut.h"       // 0.8.11: backend AAudio/AudioTrack
#include "ui/AudioWorkspace.h"      // 0.8.11: o workspace modo ÁUDIO
#include "assets/GOwnFormats.h"       // 0.8.10: .gmesh/.gtext/.gm
#include "assets/ObjExporter.h"
#include "assets/TextureCache.h"
#include "assets/TexturePipeline.h"
#include "components/CameraComp.h"   // 0.7.7: câmara de cena
#include "components/MeshRenderer.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "components/AnimationPlayer.h"   // 0.8.0 (F7): animação
#include "components/SkeletonComp.h"   // 0.8.2 (F7): skinning
#include "components/UiCanvas.h"   // 0.8.0: tracks de UI
#include "assets/GltfAnim.h"   // 0.8.1 (F7): clips de animação do glTF
#include "core/AnimationSystem.h"   // 0.8.0: avanço em Play
#include "core/VoniSystem.h"   // 0.9.2: a "central" da V.ONI (spec §3)
#include "voni/Voni.h"
#include "voni/VoniRegistry.h"   // 0.9.5: a referência p/ o clipboard
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
#include "platform/BuildInfo.h"   // 0.8.10: identidade (banner/dumps)
#include "platform/CrashHandler.h"
#include "platform/EglContext.h"
#include "platform/FileApi.h"
#include "platform/InputState.h"
#include "platform/StorageBridge.h"
#include "platform/StoragePerm.h"
#include "platform/ImeQueue.h"   // 0.9.1: fila do IME do sistema + orientação
#include "render/Camera.h"
#include "render/Cube.h"
#include "render/Grid.h"
#include "render/GpuAssets.h"
#include "render/Mesh.h"
#include "render/Primitives.h"   // 0.8.0 (F7): primitivas procedurais
#include "render/Renderer.h"
#include "render/ThumbPng.h"    // 0.9.0: miniatura do projeto (thumb.png)
#include "core/SceneBounds.h"   // 0.8.9: AABB da cena → far dinâmico editor+Play
#include "core/UndoStack.h"    // 0.9.0: undo/redo por snapshot
#include "ui/EditorUi.h"
#include "ui/Toolbar.h"   // 0.9.0: top bar 56 + tab bar de modo 48 (spec D)
#include "ui/ViewportChrome.h"   // 0.9.0: stack/toolbar inferior/triad (spec D)
#include "ui/BottomPanel.h"     // 0.9.0: painel de baixo + status 24dp (spec E/K)
#include "ui/SettingsPage.h"   // 0.9.0: página de Settings (spec I)
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

// ---- 0.8.10 (F8): TROCA DETERMINÍSTICA DE PRIMITIVA — SEM CACHE ----------
// História (a fonte da intermitência do C33): 0.8.0 criou um cache de
// meshes GL por assinatura (1 assinatura = 1 objeto partilhado por TODOS
// os TICs); 0.8.7 deu-lhe cap+evicção+negative-cache; 0.8.9 validou antes
// do upload. A intermitência SOBREVIVEU ("cubo funciona quase sempre,
// esfera e cilindro só às vezes") porque TODAS as trocas partilhavam O
// MESMO caminho de cache — evicção/release/rebind em qualquer ponto podia
// mexer no mesh de OUTRO TIC. DECISÃO DO DONO (0.8.10): o cache MORRE.
//
// Nova regra — um mesh POR MeshRenderer com primOn, um SÓ caminho:
//   • PEDIDO (pick do seletor, params no Inspector, load, preset):
//     mr->primOn=true + mr->prim=<assinatura> + mr->mesh=null (PENDENTE);
//   • PONTO SEGURO do frame (início, ANTES de qualquer submissão GL):
//     primFlushPending() → primUploadOne(mr) por pendente:
//       passo=gerador   → makePrimMesh (dados)
//       passo=validacao → verts>0/idx>0, coords finitas, AABB não degen.
//       passo=upload    → Mesh::create + SELF-CHECK (contagens pós-upload)
//       passo=bind      → troca atómica do ponteiro; o ANTIGO vai p/ COVA
//   • DEFERRED FREE: a cova (g_primGrave) morre no INÍCIO do frame
//     SEGUINTE (primGraveDig) — os comandos do frame corrente ainda podem
//     referenciar os buffers antigos; NUNCA se apaga no mesmo frame.
//   • FALHA em qualquer passo: o mesh ANTERIOR fica (render continua),
//     seleção intacta, primNeg=true (backoff — o rebind por frame NÃO
//     insiste: anti retry-storm), log `mesh: troca <de>→<para>
//     passo=<p> ERRO(<razão>)` + toast no ecrã.
std::vector<std::unique_ptr<Mesh>> g_primOwners;   // posse: meshes VIVOS
std::vector<std::unique_ptr<Mesh>> g_primGrave;    // deferred free (1 frame)
u32  g_primSwapOk  = 0;   // contadores — stress no CI, diagnóstico do dono
u32  g_primSwapErr = 0;

// ---- F4: física + modo Play ------------------------------------------------
phys::PhysicsSystem g_physics;       // TickGroup::Physics (só avança em Play)

// 0.9.2 — A CENTRAL V.ONI: dispatcher dos scripts (TickGroup::Update; os
// runs automáticos só em Play, os do editor vivem até stop). O hook de
// transição liga às cenas ABAIXO (só depois de g_scenes existir).
VoniSystem g_voni;
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

// 0.8.10 — SETTING "largar a fonte": false = source/<nome> é REMOVIDO
// depois do import converter com sucesso (o projeto fica só com assets/).
// Persistido por projeto em settings.goni ("keepSource=0/1").
// 0.8.11: + "audioMaster=<0..1>" (o volume geral — as DEFINIÇÕES vivem
// junto do áudio, mais abaixo, porque lêem o misturador global)
bool g_keepSource = true;
void loadProjectSettings();   // definido após o bloco de áudio (usa o mixer)
void saveProjectSettings();

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

// 0.9.0 — UNDO/REDO (scope funcional: "stack de operações ligada à stack
// da viewport"): snapshot por operação; o drag do gizmo empurra no FIM
// (press captura before, release empurra with after); picks/criar/apagar/
// renomear idem. ÁREA DE TRANSFERÊNCIA (copy/paste): snapshot do TIC.
editor::UndoStack g_undo;
editor::TicSnap   g_clipSnap;
bool              g_clipValid = false;
editor::TicSnap   g_gizmoBefore;   // captura no ARM do grab
bool              g_gizmoBeforeValid = false;
f32               g_snapValue = 0.5f;   // 0.9.0: o valor do chip [snap: N]

// 0.9.0 (spec E/G) — PAINEL DE BAIXO: tabs Ficheiros/Consola/Animação +
// drawer (240 default, pega 160..400) + estado PERSISTENTE (layout.json)
editor::bottom::BottomState g_bottom;

// 0.9.0 (spec I) — modo IMERSIVO (JNI: window flags) + mic concedido;
// PERSISTEM (layout.json / runtime check)
bool g_immersive = false;
bool g_micGranted = false;
std::string g_lastLayoutSaved;   // o layout.json da última escrita
// altura do drawer ABERTO neste frame (0 = fechado) — todos os rects do
// viewport central/painéis passam por AQUI (safe::centerRect com drawerH)
static f32 currentDrawerH() {
    return g_bottom.bottomTab > 0 ? g_bottom.drawerH : 0.0f;
}

void showToast(const char* msg);   // fwd (definido abaixo)
// 0.9.0 (spec G) — LAYOUT PERSISTENTE: layout.json na raiz do projeto
// (bottom/drawer/inspector/secções colapsadas).
//
// FASE 9 (G1-5 — o log mostrava 4 writes em ~40 s): a versão antiga chamava
// saveLayoutNow() a CADA FRAME — cada passo de 8dp do drag do painel de
// baixo gravava UM ficheiro. AGORA: DEBOUNCE — grava 1,5 s após a ÚLTIMA
// alteração (o timer RECOMEÇA a cada mudança — o drag inteiro = 1 write),
// e na saída para segundo plano (flush imediato). NÃO grava se o conteúdo
// não mudou. UMA linha de log: "layout guardado (motivo)".
static f32 g_layoutSaveTimer = 0.0f;         // >0 = pendente (contagem 1,5 s)
static char g_layoutSaveReason[48] = {0};
// sombra do estado (para o MOTIVO da linha de log — qual campo mudou)
static int g_layoutShadowTab = -1;
static int g_layoutShadowDrawer = -1;
static int g_layoutShadowInsp = -1;
static u32 g_layoutShadowCollapsed = 0xFFFFFFFFu;

static void saveLayoutNow(const char* reason) {
    if (!g_storage) {
        return;
    }
    const std::string data = editor::bottom::serializeLayout(
        g_bottom, g_editor.showInspector,
        g_editor.inspCollapsed | (g_editor.settingsCollapsed << 8));
    if (data == g_lastLayoutSaved) {
        return;   // nada mudou — NÃO grava (a regra de sempre)
    }
    g_lastLayoutSaved = data;
    g_storage->writeText("layout.json", data);
    elog::info("layout guardado (%s)",
               (reason && *reason) ? reason : "alteração");
}

// (re)agenda o save — cada alteração RECOMEÇA os 1,5 s (coalesce)
static void scheduleLayoutSave(const char* reason) {
    std::snprintf(g_layoutSaveReason, sizeof(g_layoutSaveReason), "%s",
                  reason ? reason : "");
    g_layoutSaveTimer = 1.5f;
}

// o TICK por frame: deteta a mudança (contra a SOMBRA — o último estado
// VISTO, não o último escrito: com uma mudança pendente o timer conta SEM
// parar), agenda com o MOTIVO do campo que mexeu e dispara o write quando
// o debounce vence (o write em si só corre se o conteúdo diferir do disco)
static void layoutSaveTick(f32 dt) {
    if (!g_storage) {
        return;
    }
    const int tab = g_bottom.bottomTab;
    const int drawer = static_cast<int>(g_bottom.drawerH);
    const int insp = g_editor.showInspector ? 1 : 0;
    const u32 collapsed = g_editor.inspCollapsed |
                          (g_editor.settingsCollapsed << 8);
    const bool seen = tab == g_layoutShadowTab &&
                      drawer == g_layoutShadowDrawer &&
                      insp == g_layoutShadowInsp &&
                      collapsed == g_layoutShadowCollapsed;
    if (!seen) {
        // o MOTIVO: qual campo mudou desde o último VISTO
        const char* why = "alteração";
        if (tab != g_layoutShadowTab || drawer != g_layoutShadowDrawer) {
            why = "painel de baixo";
        } else if (insp != g_layoutShadowInsp) {
            why = "inspector";
        } else {
            why = "secções recolhidas";
        }
        scheduleLayoutSave(why);
        g_layoutShadowTab = tab;
        g_layoutShadowDrawer = drawer;
        g_layoutShadowInsp = insp;
        g_layoutShadowCollapsed = collapsed;
    }
    if (g_layoutSaveTimer > 0.0f) {
        g_layoutSaveTimer -= dt;
        if (g_layoutSaveTimer <= 0.0f) {
            g_layoutSaveTimer = 0.0f;
            saveLayoutNow(g_layoutSaveReason);
        }
    }
}

// 0.9.0 (spec I/G) — carregar o layout no OPEN (defaults se ilegível)
static void loadLayoutNow() {
    g_lastLayoutSaved.clear();
    if (g_storage) {
        std::string text;
        if (g_storage->readText("layout.json", text)) {
            bool insp = true;
            u32 collapsed = 0;
            if (editor::bottom::parseLayout(text, g_bottom, insp, collapsed)) {
                g_editor.showInspector = insp;
                g_editor.inspCollapsed = collapsed & 0xFFu;
                g_editor.settingsCollapsed = (collapsed >> 8) & 0x3Fu;
                g_lastLayoutSaved = text;
                // FASE 9 (G1-5): a SOMBRA acompanha o estado carregado —
                // sem isto o arranque agendava um write espúrio (a sombra
                // nascia vazia e o 1º tick "via" mudança)
                g_layoutShadowTab = g_bottom.bottomTab;
                g_layoutShadowDrawer = static_cast<int>(g_bottom.drawerH);
                g_layoutShadowInsp = g_editor.showInspector ? 1 : 0;
                g_layoutShadowCollapsed = g_editor.inspCollapsed |
                                          (g_editor.settingsCollapsed << 8);
                LOGI("layout: carregado (tab=%d drawer=%d insp=%d)",
                     g_bottom.bottomTab, static_cast<int>(g_bottom.drawerH),
                     g_editor.showInspector ? 1 : 0);
            } else {
                LOGI("layout: ilegivel — defaults");
            }
        }
    }
}

// 0.9.0 (spec I) — IMERSIVO: esconde as barras do sistema (JNI pela
// VvActivity; sem ponte = no-op logado — o editor segue)
static void applyImmersiveMode() {
    if (storage::jniSetImmersive(g_immersive)) {
        elog::info("ui: imersivo aplicado (%s)", g_immersive ? "on" : "off");
    } else {
        elog::warn("ui: imersivo sem ponte JNI — ignorado");
    }
}

// o viewer de logs de sempre usa g_logDumps — refresh da lista
static void refreshLogDumps() {
    g_logDumps.clear();
    elog::listDumps(g_logDumps);
}

// encaminhamentos das ações de Settings (os corpos existem)
void audioProbeRun();   // definido abaixo (o probe de áudio da 0.8.11)
static void runAudioProbe() { audioProbeRun(); }
static void reconvertAllAssets() {
    // o MESMO caminho do "reconverter assets" de sempre: cada ficheiro de
    // source/ volta pelo reconvertFile (0.8.10 — a fonte é a cópia própria)
    int converted = 0, failed = 0;
    std::vector<std::string> sources;
    g_storage->listDir("source", sources);
    for (const std::string& rel : sources) {
        convert::Output out;
        convert::Stats stats;
        std::string err;
        if (convert::reconvertFile("source/" + rel, *g_storage, nullptr, out,
                                   stats, err)) {
            ++converted;
        } else {
            ++failed;
            elog::error("import: reconverter %s FALHOU — %s", rel.c_str(),
                        err.c_str());
        }
    }
    char msg[80];
    std::snprintf(msg, sizeof(msg), "reconvertido: %d (%d falhas)", converted,
                  failed);
    showToast(msg);
}

// empurra UMA operação (antes/depois) — helper de 1 linha por call-site
static void pushUndo(editor::TicSnap before, const Tic* tic) {
    if (!tic) {
        return;
    }
    g_undo.push(before, editor::snapTic(g_scene, tic->handle), tic->handle);
}

// 0.9.0 — CRIAÇÃO com undo (a op CREATE: before vazio + after = o TIC novo;
// o undo re-cria pelo snapshot — mesmo após o delete da cena)
static Handle createTicUndoable(PresetKind kind) {
    editor::TicSnap empty;
    const Handle h = createTicFromPreset(g_scene, kind, &g_cubeMesh,
                                         g_renderer.litMaterial());
    if (h.valid()) {
        g_undo.push(empty, editor::snapTic(g_scene, h), h);
    }
    return h;
}
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
        // 0.9.0 — UNDO: o drag ACABOU → empurra a operação (no-op não entra)
        if (g_gizmoBeforeValid) {
            if (const Tic* t = g_scene.get(g_editor.selected)) {
                g_undo.push(g_gizmoBefore, editor::snapTic(g_scene, t->handle),
                            t->handle);
            }
            g_gizmoBeforeValid = false;
        }
        g_grab = gizmo::Grab{};
        g_gizmo.active = gizmo::Axis::None;
        g_gizmo.dragSlot = -1;
        g_gizmo.hovered = gizmo::Axis::None;
    }
    // press edge → GRAB (hit-test com alvo GENEROSO, âncoras capturadas;
    // só se o gesto nasce no viewport central)
    const UiRect view = editor::centerRect(sw, sh, g_ui.safeArea(),
                                           currentDrawerH(),
                                           g_editor.showInspector);
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
        // 0.9.0 — UNDO: captura o BEFORE no ARM (o release empurra a op)
        g_gizmoBefore = editor::snapTic(g_scene, g_editor.selected);
        g_gizmoBeforeValid = g_scene.get(g_editor.selected) != nullptr;
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

// 0.9.0 (spec F) — MINIATURA DO PROJETO: todo o SAVE arma a captura; o FIM
// do frame DEPOIS de desenhar (antes do swap) lê a viewport central do
// backbuffer, faz crop 16:9 + downsample e escreve thumb.png na raiz do
// projeto. A tela de projetos (Java) lê-o para o card (default = logo G).
bool g_thumbPending = false;

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
void postLoadMigrateAndFixup();   // 0.8.10: migração silenciosa pós-load
void primFlushPending();               // 0.8.10: troca no ponto seguro (fwd)
void primGraveDig();
void primMeshesToGrave();              // 0.8.10: posse p/ cova (troca de cena)
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
        // 0.8.10: o preset Mesh nasce com a ESFERA PENDENTE (primOn+params
        // já vêm do preset; mesh=null sobe no PONTO SEGURO do frame seguinte
        // — zero upload fora do início de frame). As restantes fontes
        // continuam a ser o cubo procedural.
        Mesh* mesh = &g_cubeMesh;
        Handle hmesh = createTicFromPreset(g_scene, kind, mesh,
                                           g_renderer.litMaterial());
        if (kind == PresetKind::Mesh && hmesh.valid()) {
            // 0.8.10: o preset arma o PEDIDO — o mesh sobe no ponto seguro
            if (MeshRenderer* mr = g_scene.get(hmesh)->getComponent<MeshRenderer>()) {
                mr->primPending = true;
            }
        }
        return hmesh;
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
    g_thumbPending = true;   // 0.9.0: captura da viewport no próximo fim de frame
    // 0.8.10: a posse dos meshes de prim da cena ANTIGA vai para a cova
    // (deferred free no próximo frame — os draws deste frame ainda contam)
    primMeshesToGrave();
    // 3) carrega a nova (LoadCtx canônico: refs relativos re-ligam)
    const SceneSerializer::LoadCtx ctx = makeLoadCtx();
    if (g_project.loadActiveScene(*g_storage, g_scene, ctx)) {
        postLoadMigrateAndFixup();   // 0.8.10: migração silenciosa da nova
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
    std::snprintf(msg, sizeof(msg), "cena '%s' não existe", name.c_str());
    showToast(msg);
}

// nome da cena ATIVA (sem extensão/caminho) — o host da V.ONI usa para
// validar o `nomedacena.transition.for` (origem == atual)
static std::string currentSceneBaseName() {
    if (const std::string* p = g_project.activeScenePath()) {
        const size_t slash = p->rfind('/');
        std::string base = slash == std::string::npos ? *p : p->substr(slash + 1);
        const size_t dot = base.rfind('.');
        if (dot != std::string::npos) {
            base = base.substr(0, dot);
        }
        return base;
    }
    return std::string();
}

// 0.9.2 §9 — o hook transition.for da V.ONI: valida o destino no manifesto
// e troca com FADE (em Play) — o estilo é do engine, a LINGUAGEM só pede.
static bool voniTransitionHook(const std::string& from, const std::string& to,
                               std::string& err) {
    (void)from;   // o host JÁ validou origem==atual
    if (!g_projectReady) {
        err = "sem projeto aberto";
        return false;
    }
    const std::string rel =
        std::string(Project::kDirScenes) + "/" + to + ".goni";
    for (u32 i = 0; i < g_project.scenes.size(); ++i) {
        if (g_project.scenes[i] == rel) {
            loadSceneByName(to, g_editor.playMode ? ui::SceneSwap::Fade
                                                  : ui::SceneSwap::Instant);
            elog::info("voni: transition.for('%s') -> '%s'", from.c_str(),
                       to.c_str());
            return true;
        }
    }
    err = "cena '" + to + "' não existe";
    return false;
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
            showToast("cena já existe");
            elog::warn("cena: '%s' já existe no projeto", name.c_str());
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
    primMeshesToGrave();   // 0.8.10: posse da cena antiga p/ cova
    g_scene.clear();
    g_editor.selected = Handle::invalid();
    g_editor.selElement = -1;
    const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                    g_project.saveManifest(*g_storage);
    g_thumbPending = true;   // 0.9.0: captura da viewport no próximo fim de frame
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

// ---- 0.8.11: ÁUDIO — misturador + backend + cache de clipes -----------------
// O backend (AAudio no device; AudioTrack se o probe mandar; stub no CI)
// puxa o MISTURADOR puro no callback — a engine nunca fala com o hardware.
AudioEngine g_audioEngine;
std::unique_ptr<audioout::Backend> g_audioOut;
bool g_audioBackendReady = false;
// cache de clipes decodificados (1 ref .gi → 1 GiClip)
std::map<std::string, std::unique_ptr<GiClip>> g_audioClips;
// catálogo de áudio p/ o Inspector/workspace (refresh ao abrir)
std::vector<std::string> g_audioCatalog;
// 0.8.11: o workspace ÁUDIO (estado + preview do clip selecionado)
editor::AudioWorkspaceState g_audioWs;
i32 g_audioPreviewVoice = -1;
std::string g_audioPreviewClip;
// MASTER VOLUME (Settings — persistido em settings.goni como audioMaster)
f32 g_audioMaster = 1.0f;
void audioRecTick();
void audioPreviewTick();
void audioRecordToggle();

// ---- GRAVAÇÃO: o estado partilhado worker↔frame (a definição do worker
// vive em baixo — DEVICE: AudioRecord JNI; HOST: mic sintético) ---------
struct AudioRec {
    std::thread th;
    std::atomic<bool> on{false};
    std::atomic<int> secs{0};
    std::atomic<f32> level{0.0f};
    std::mutex mx;
    std::vector<i16> pcm;
    u32 sampleRate = 44100;
    std::chrono::steady_clock::time_point t0;
};
AudioRec g_audioRec;
#ifdef __ANDROID__
JavaVM* g_audioVm = nullptr;   // android_main registra (glue activity->vm)
#endif

const GiClip* audioClipFor(const std::string& rel) {
    if (rel.empty() || !g_storage) {
        return nullptr;
    }
    const auto it = g_audioClips.find(rel);
    if (it != g_audioClips.end()) {
        return it->second.get();   // hit (decodificado 1×)
    }
    std::vector<u8> bytes;
    if (!g_storage->readBytes(rel, bytes) || bytes.empty()) {
        return nullptr;
    }
    auto clip = std::make_unique<GiClip>();
    std::string err;
    if (!readGi(bytes.data(), bytes.size(), *clip, err)) {
        elog::error("audio: %s invalido — %s", rel.c_str(), err.c_str());
        return nullptr;
    }
    // captura os valores ANTES do move (a 1ª versão lia o unique_ptr já
    // movido — o log saía com zeros)
    const u16 ch = clip->channels;
    const u32 sr = clip->sampleRate;
    const f32 dur = clip->duration();
    const GiCodec codec = clip->codec;
    const GiClip* raw = clip.get();
    g_audioClips.emplace(rel, std::move(clip));
    elog::info("audio: load %s %uch %uHz %.2fs codec=%s", rel.c_str(), ch,
               sr, dur, giCodecName(codec));
    return raw;
}

void refreshAudioCatalog() {
    g_audioCatalog.clear();
    if (g_storage) {
        std::vector<std::string> files;
        if (g_storage->listDir("audio", files)) {
            for (const std::string& f : files) {
                if (f.size() > 3 && f.compare(f.size() - 3, 3, ".gi") == 0) {
                    g_audioCatalog.push_back(std::string("audio/") + f);
                }
            }
        }
    }
    // o catálogo do INSPECTOR (g_catalog.audio) segue o mesmo refresh — o
    // seletor de clips (assetMenu 5) lê dali
    g_catalog.audio = g_audioCatalog;
}

// settings.goni do projeto: fonte manter/largar + VOLUME GERAL (0.8.11).
// Corre no post-load (a 0.8.10 definia isto mas NUNCA o LIA — o setting
// só vivia na RAM; agora o boot do projeto aplica)
void loadProjectSettings() {
    g_keepSource = true;
    g_audioMaster = 1.0f;
    if (g_storage) {
        std::string text;
        if (g_storage->readText("settings.goni", text)) {
            g_keepSource = text.find("keepSource=0") == std::string::npos;
            const size_t p = text.find("audioMaster=");
            if (p != std::string::npos) {
                const f32 v = static_cast<f32>(std::atof(text.c_str() + p + 12));
                if (v >= 0.0f && v <= 1.0f) {
                    g_audioMaster = v;
                }
            }
        }
    }
    g_audioEngine.master = g_audioMaster;
}
void saveProjectSettings() {
    if (!g_storage) {
        return;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "keepSource=%s\naudioMaster=%.2f\n",
                  g_keepSource ? "1" : "0", g_audioMaster);
    g_storage->writeText("settings.goni", buf);
}

// ---- preview do workspace (o MESMO misturador; 1 voz, sem loop) --------
void audioPreviewToggle() {
    if (g_audioPreviewVoice >= 0) {
        g_audioEngine.stop(g_audioPreviewVoice);
        g_audioPreviewVoice = -1;
        g_audioPreviewClip.clear();
        return;
    }
    if (g_audioWs.selected >= g_audioCatalog.size()) {
        showToast("selecione um clip na lista");
        return;
    }
    const std::string& rel = g_audioCatalog[g_audioWs.selected];
    const GiClip* clip = audioClipFor(rel);
    if (!clip) {
        showToast("clip não carrega (engine.log)");
        return;
    }
    g_audioPreviewVoice = g_audioEngine.play(clip, false, 1.0f, 1.0f);
    g_audioPreviewClip = rel;
    elog::info("audio: preview '%s' (%.2fs)", rel.c_str(), clip->duration());
}
void audioPreviewStop() {
    if (g_audioPreviewVoice >= 0) {
        g_audioEngine.stop(g_audioPreviewVoice);
        g_audioPreviewVoice = -1;
        g_audioPreviewClip.clear();
    }
}

// o preview morreu sozinho (fim do clip)? limpa o estado p/ o botão voltar
// a dizer "Play" — chamado no início do frame
void audioPreviewTick() {
    if (g_audioPreviewVoice >= 0 &&
        !g_audioEngine.playing(g_audioPreviewVoice)) {
        g_audioPreviewVoice = -1;
        g_audioPreviewClip.clear();
    }
}

// picos em cache por ref (a waveform não recalcula por frame)
std::vector<f32> g_emptyPeaks;
const std::vector<f32>& audioPeaksOf(const std::string& rel) {
    static std::string lastRel;
    static std::vector<f32> lastPeaks;
    const GiClip* clip = audioClipFor(rel);
    if (!clip) {
        return g_emptyPeaks;
    }
    if (lastRel != rel || lastPeaks.empty()) {
        clip->peaks(160, lastPeaks);
        lastRel = rel;
    }
    return lastPeaks;
}

// o HOST do workspace (liga o desenho puro ao device)
editor::AudioWorkspaceHost makeAudioWorkspaceHost() {
    editor::AudioWorkspaceHost h;
    h.onImport = []() {
        // abre o navegador na raiz Music (o áudio do dono vive aí)
        browserOpen("/storage/emulated/0/Music");
        g_editor.fileBrowser = true;
        g_editor.audioMode = true;   // volta ao workspace ao fechar
    };
    h.onRecord = []() { audioRecordToggle(); };
    h.onPreviewToggle = []() { audioPreviewToggle(); };
    h.onPreviewStop = []() { audioPreviewStop(); };
    h.onDelete = [](const std::string& rel) {
        if (!g_storage) {
            return;
        }
        if (g_storage->remove(rel)) {
            if (g_audioPreviewClip == rel) {
                audioPreviewStop();
            }
            g_audioClips.erase(rel);
            refreshAudioCatalog();
            if (g_audioWs.selected >= g_audioCatalog.size() &&
                !g_audioCatalog.empty()) {
                g_audioWs.selected =
                    static_cast<u32>(g_audioCatalog.size()) - 1;
            }
            elog::info("audio: clip '%s' apagado", rel.c_str());
            showToast("clip apagado");
        }
    };
    h.onAssign = []() {
        Tic* tsel = g_scene.get(g_editor.selected);
        AudioPlayer* au = tsel ? tsel->getComponent<AudioPlayer>() : nullptr;
        if (!au) {
            showToast("selecione um TIC de Audio (Hierarchy)");
            return;
        }
        if (g_audioWs.selected >= g_audioCatalog.size()) {
            showToast("selecione um clip na lista");
            return;
        }
        au->clipPath = g_audioCatalog[g_audioWs.selected];
        char msg[96];
        std::snprintf(msg, sizeof(msg), "clip atribuido a '%s'",
                      tsel->name.c_str());
        showToast(msg);
        elog::info("audio: clip '%s' atribuido ao TIC '%s'",
                   au->clipPath.c_str(), tsel->name.c_str());
    };
    h.onRename = []() {
        // teclado in-app (propósito 7 — o MESMO do renomear de TIC; o
        // commit vive no frame(): precisa do storage, que é do main)
        if (g_audioWs.selected < g_audioCatalog.size()) {
            const std::string& rel = g_audioCatalog[g_audioWs.selected];
            const size_t slash = rel.rfind('/');
            const size_t dot = rel.rfind('.');
            editor::openTextInput(
                g_editor, 7, Handle{}, -1,
                rel.substr(slash + 1, dot - slash - 1).c_str());
        }
    };
    h.codecOf = [](const std::string& rel) -> const char* {
        const GiClip* c = audioClipFor(rel);
        return c ? giCodecName(c->codec) : "?";
    };
    h.durationOf = [](const std::string& rel) -> f32 {
        const GiClip* c = audioClipFor(rel);
        return c ? c->duration() : 0.0f;
    };
    h.peaksOf = &audioPeaksOf;
    h.previewing = g_audioPreviewVoice >= 0;
    h.previewPos = []() -> f32 {
        return g_audioEngine.voiceProgress(g_audioPreviewVoice);
    };
    h.recording = g_audioRec.on.load();
    h.recordSecs = g_audioRec.secs.load();
    h.recordLevel = g_audioRec.level.load();
    return h;
}

// Problema 3 (hotfix 0.9.3) — memória no ARRANQUE da engine: lê o
// /proc/self/status (RSS + pico) e loga. POSIX PURO (device + host do
// c33_virtual); NUNCA falha o boot (leitura best-effort, sem exceções).
static void logBootMemory(const char* where) {
    long rssKb = -1, hwmKb = -1;
    if (FILE* f = ::fopen("/proc/self/status", "r")) {
        char line[256];
        while (::fgets(line, sizeof(line), f)) {
            if (::sscanf(line, "VmRSS: %ld kB", &rssKb) == 1) { continue; }
            if (::sscanf(line, "VmHWM: %ld kB", &hwmKb) == 1) { break; }
        }
        ::fclose(f);
    }
    elog::info("boot: memoria (%s) — RSS=%ld kB pico=%ld kB", where, rssKb,
               hwmKb);
}

// arranca o backend (boot). 0.9.3 (REG-002/R-006): a CADEIA é
//   Oboe (primário — google/oboe 1.9.3: AAudio na API 27+ com fallback
//         OpenSL ES automático nos devices problemáticos como o Unisoc)
//   → AAudio direto (o fallback fixado: porta StartGate + close-no-errCb)
//   → AudioTrack (JNI — o fallback final de sempre)
//   → SEM SOM (o editor funciona; NUNCA crasha por causa do áudio).
// O arranque corre no INIT_WINDOW (a superfície já existe) — NUNCA no
// onResume (o padrão exato do tombstone da app antiga: onResume →
// startAudio direto; o RESUME desta engine só faz resume() do stream
// vivo, e o StartGate impede o arranque duplo em QUALQUER caso).
void audioBackendBoot() {
    g_audioOut.reset(audioout::createOboe());
    g_audioBackendReady = false;
    if (g_audioOut && g_audioOut->start(44100, 2)) {
        g_audioBackendReady = true;
        elog::info("audio: backend %s ATIVO (44100 Hz stereo pedidos — "
                   "o stream real manda e o misturador adapta)",
                   g_audioOut->name());
        return;
    }
    elog::warn("audio: Oboe recusou — fallback AAudio direto (porta R-006)");
    g_audioOut.reset(audioout::createAAudio());
    if (g_audioOut && g_audioOut->start(44100, 2)) {
        g_audioBackendReady = true;
        elog::info("audio: backend AAudio ATIVO (fallback 1)");
        return;
    }
    elog::warn("audio: AAudio recusou — FALLBACK AudioTrack (documentado)");
    g_audioOut.reset(audioout::createAudioTrack());
    if (g_audioOut && g_audioOut->start(44100, 2)) {
        g_audioBackendReady = true;
        elog::info("audio: backend AudioTrack ATIVO (fallback 2)");
        return;
    }
    elog::error("audio: NENHUM backend ligou (device sem áudio?) — o editor "
                "continua SEM SOM");
    g_audioOut.reset();
}

// troca o backend vivo (probe mandou fallback): para o atual, cria o novo
bool audioBackendSwitch(bool toAudioTrack) {
    if (!g_audioOut) {
        return false;
    }
    const char* from = g_audioOut->name();
    g_audioOut->stop();
    g_audioOut.reset(toAudioTrack ? audioout::createAudioTrack()
                                  : audioout::createOboe());
    if (g_audioOut && g_audioOut->start(44100, 2)) {
        g_audioBackendReady = true;
        elog::warn("audio: TROCA de backend %s -> %s (probe)", from,
                   g_audioOut->name());
        return true;
    }
    elog::error("audio: troca p/ %s FALHOU — sem som (o misturador segue)",
                toAudioTrack ? "AudioTrack" : "Oboe");
    g_audioOut.reset();
    g_audioBackendReady = false;
    return false;
}

// ---- PROBE DE ESTABILIDADE (Settings → diagnóstico): 50× start/stop +
// 10× pause/resume contra um backend FRESCO (nunca o vivo — o probe mata
// o stream de propósito); a tabela vai ao engine.log e a DECISÃO
// (shouldFallback) troca o backend vivo sozinha. 0.9.3: o probe corre
// contra o OBOE (o primário) — é a estabilidade DELE que decide a troca
// para AudioTrack ----------------------------------------------------------------
void audioProbeRun() {
    elog::info("audio: probe a correr (50 ciclos + 10 pause/resume)...");
    std::unique_ptr<audioout::Backend> probe(audioout::createOboe());
    const audioout::ProbeResult r =
        audioout::runProbe(probe.get(), 50, 10, 0);
    probe->stop();
    const std::string table = audioout::probeTable(r, "oboe");
    elog::info("%s", table.c_str());
    if (r.shouldFallback() && g_audioOut && g_audioOut->ready() &&
        std::strcmp(g_audioOut->name(), "oboe") == 0) {
        audioBackendSwitch(true);
        showToast("audio: backend instavel — AudioTrack ATIVO");
    } else if (!r.shouldFallback()) {
        showToast("audio: estavel (probe ok)");
    } else {
        showToast("audio: probe falhou — ver engine.log");
    }
}

// play/stop de um AudioPlayer pelo MISTURADOR (o caminho ÚNICO: Play,
// preview, tyker play())
void audioPlayerStart(Tic& t) {
    AudioPlayer* au = t.getComponent<AudioPlayer>();
    if (!au || !au->hasClip()) {
        return;
    }
    const GiClip* clip = audioClipFor(au->clipPath);
    if (!clip) {
        showToast("clip de áudio não encontrado");
        return;
    }
    // posicional: a posição VIVA do TIC (o listener é a câmara)
    Vec3 pos{0.0f, 0.0f, 0.0f};
    if (const Transform3D* tr = t.getComponent<Transform3D>()) {
        pos = tr->pos;
    }
    au->voiceId = g_audioEngine.play(clip, au->loop, au->volume, au->pitch);
    if (au->voiceId >= 0 && au->posicional) {
        g_audioEngine.setPosicional(au->voiceId, true, pos, au->raioInterno,
                                    au->raioExterno);
    }
    elog::info("audio: play '%s' no TIC '%s' (vol %.2f pitch %.2f%s)",
               au->clipPath.c_str(), t.name.c_str(), au->volume, au->pitch,
               au->posicional ? " posicional" : "");
}
void audioPlayerStop(Tic& t) {
    if (AudioPlayer* au = t.getComponent<AudioPlayer>()) {
        if (au->voiceId >= 0) {
            g_audioEngine.stop(au->voiceId);
            au->voiceId = -1;
        }
    }
}

// o PREVIEW do INSPECTOR (o botão "ouvir" do AudioPlayer): o Inspector
// (puro) só faz toggle do flag `previewing`; AQUI o main mapeia o flag ao
// misturador — o mesmo caminho do Play
void audioPreviewTick(Tic& t) {
    AudioPlayer* au = t.getComponent<AudioPlayer>();
    if (!au) {
        return;
    }
    if (au->previewing && au->voiceId < 0) {
        const GiClip* clip = audioClipFor(au->clipPath);
        if (!clip) {
            au->previewing = false;
            showToast("clip de áudio não encontrado");
            return;
        }
        au->voiceId = g_audioEngine.play(clip, au->loop, au->volume, au->pitch);
        if (au->voiceId >= 0 && au->posicional) {
            if (const Transform3D* tr = t.getComponent<Transform3D>()) {
                g_audioEngine.setPosicional(au->voiceId, true, tr->pos,
                                            au->raioInterno, au->raioExterno);
            }
        }
        elog::info("audio: preview no TIC '%s' ('%s')", t.name.c_str(),
                   au->clipPath.c_str());
    } else if (!au->previewing && au->voiceId >= 0) {
        g_audioEngine.stop(au->voiceId);
        au->voiceId = -1;
    } else if (au->previewing && au->voiceId >= 0 &&
               !g_audioEngine.playing(au->voiceId)) {
        // fim natural do clip (sem loop): o botão volta a "ouvir"
        au->voiceId = -1;
        au->previewing = false;
    }
}

// ENTRAR em Play: autoplay liga (a regra do componente); SAIR: tudo para
void audioEnterPlay() {
    g_scene.forEachActive([&](Tic& t) {
        AudioPlayer* au = t.getComponent<AudioPlayer>();
        if (au && au->autoplay) {
            au->previewing = false;   // o Play manda; preview não
            audioPlayerStart(t);
        }
    });
}
void audioLeavePlay() {
    g_audioEngine.stopAll();
    g_scene.forEachActive([&](Tic& t) {
        if (AudioPlayer* au = t.getComponent<AudioPlayer>()) {
            au->voiceId = -1;
        }
    });
}

// listener = olho da câmara ATIVA (posicional atenua contra isto)
void audioUpdateListener() {
    Tic* camT = g_editor.playMode ? findActiveCameraTic(g_scene) : nullptr;
    if (camT) {
        if (const Transform3D* tr = camT->getComponent<Transform3D>()) {
            g_audioEngine.setListener(tr->pos);
        }
    } else {
        g_audioEngine.setListener(g_camera.eye());
    }
    // vozes posicionais seguem a POS VIVA dos TICs
    g_scene.forEachActive([&](Tic& t) {
        AudioPlayer* au = t.getComponent<AudioPlayer>();
        if (au && au->voiceId >= 0 && au->posicional) {
            if (const Transform3D* tr = t.getComponent<Transform3D>()) {
                g_audioEngine.setPosicional(au->voiceId, true, tr->pos,
                                            au->raioInterno, au->raioExterno);
            }
        }
    });
}

// ---- 0.8.11: GRAVAÇÃO (mic → .gi ADPCM) -------------------------------------
// DEVICE: AudioRecord (JNI) numa thread própria — PCM16 mono 44100 lido por
// read(short[]) blocking; STOP → writeGi ADPCM → audio/rec-<unix>.gi.
// HOST/CI: um "mic" SINTÉTICO (senoide 440 Hz) pela MESMA máquina de
// estados (thread + mutex + t0 + level) — o teste afera o wiring inteiro
// sem hardware (o caminho JNI compila só no build Android, que o CI
// assembleRelease verifica; o padrão do StorageBridge). O guard `on` mata
// o worker; o PCM viaja sob mutex.
#ifdef __ANDROID__
// worker do DEVICE: AudioRecord real (mic). A thread NASCE desanexada — o
// attach usa a VM registada pelo audioout::setVm (android_main corre antes
// de qualquer gravação ser possível). Falha de JNI = worker sai com log;
// o toggle devolve o estado limpo ao utilizador.
static void audioRecWorker(AudioRec& rec) {
    JNIEnv* env = nullptr;
    jint rc = 0;
    if (!g_audioVm) {
        elog::error("audio: gravacao sem VM registada");
        return;
    }
    rc = g_audioVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (rc == JNI_EDETACHED) {
        JavaVMAttachArgs args {};
        args.version = JNI_VERSION_1_6;
        args.name    = "goni-rec";
        rc = g_audioVm->AttachCurrentThread(&env, &args);
    }
    if (rc != JNI_OK || !env) {
        elog::error("audio: gravacao sem JNIEnv (rc=%d)", static_cast<int>(rc));
        return;
    }
    // getMinBufferSize(44100, CHANNEL_IN_MONO=16, ENCODING_PCM_16BIT=2)
    jclass arCls = env->FindClass("android/media/AudioRecord");
    if (!arCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        elog::error("audio: AudioRecord não resolvida (RECORD_AUDIO concedida?)");
        return;
    }
    jmethodID getMin = env->GetStaticMethodID(
        arCls, "getMinBufferSize", "(III)I");
    jmethodID ctor = env->GetMethodID(
        arCls, "<init>", "(IIIII)V");
    jmethodID startRec = env->GetMethodID(arCls, "startRecording", "()V");
    jmethodID readM = env->GetMethodID(arCls, "read", "([SII)I");
    jmethodID stopM = env->GetMethodID(arCls, "stop", "()V");
    jmethodID relM = env->GetMethodID(arCls, "release", "()V");
    if (!getMin || !ctor || !startRec || !readM || !stopM || !relM) {
        env->ExceptionClear();
        elog::error("audio: métodos do AudioRecord não achados");
        return;
    }
    const jint minBuf = env->CallStaticIntMethod(
        arCls, getMin, static_cast<jint>(rec.sampleRate),
        16 /*CHANNEL_IN_MONO*/, 2 /*ENCODING_PCM_16BIT*/);
    if (env->ExceptionCheck() || minBuf <= 0) {
        env->ExceptionClear();
        elog::error("audio: getMinBufferSize devolveu %d", static_cast<int>(minBuf));
        return;
    }
    jobject rec_ = env->NewObject(arCls, ctor,
                                  1 /*MIC*/, static_cast<jint>(rec.sampleRate),
                                  16 /*MONO*/, 2 /*PCM16*/,
                                  minBuf * 4);
    if (env->ExceptionCheck() || !rec_) {
        env->ExceptionClear();
        elog::error("audio: NewObject AudioRecord FALHOU (permissao?)");
        return;
    }
    env->CallVoidMethod(rec_, startRec);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        elog::error("audio: startRecording FALHOU");
        env->CallVoidMethod(rec_, relM);
        return;
    }
    const jint kChunk = 2048;
    jshortArray arr = env->NewShortArray(kChunk);
    while (rec.on.load()) {
        const jint n = env->CallIntMethod(rec_, readM, arr, 0, kChunk);
        if (env->ExceptionCheck() || n < 0) {
            env->ExceptionClear();
            elog::error("audio: AudioRecord.read FALHOU (%d)", static_cast<int>(n));
            break;
        }
        if (n > 0) {
            jshort tmp[2048];
            env->GetShortArrayRegion(arr, 0, n, tmp);
            i16 peak = 0;
            {
                const std::lock_guard<std::mutex> lk(rec.mx);
                for (jint i = 0; i < n; ++i) {
                    rec.pcm.push_back(tmp[i]);
                    if (tmp[i] > peak) {
                        peak = tmp[i];
                    }
                }
            }
            rec.level.store(static_cast<f32>(peak) / 32768.0f);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    env->CallVoidMethod(rec_, stopM);
    env->CallVoidMethod(rec_, relM);
    env->ExceptionClear();
    env->DeleteLocalRef(arr);
    env->DeleteLocalRef(rec_);
}
#else
// worker do HOST/CI: o mic SINTÉTICO — senoide 440 Hz mono; a MESMA
// máquina de estados (append sob mutex + level do pico). Gera ~10× mais
// rápido que o tempo real para os testes serem rápidos.
static void audioRecWorker(AudioRec& rec) {
    double t = 0.0;
    const double dt = 1.0 / static_cast<double>(rec.sampleRate);
    const i16 kAmp = 12000;
    while (rec.on.load()) {
        i16 peak = 0;
        {
            const std::lock_guard<std::mutex> lk(rec.mx);
            for (int i = 0; i < 2205; ++i) {   // 50 ms de cada vez
                const double s = std::sin(t * 2.0 * 3.14159265358979 * 440.0);
                const i16 v = static_cast<i16>(s * kAmp);
                rec.pcm.push_back(v);
                if (v > peak) {
                    peak = v;
                }
                t += dt;
            }
        }
        rec.level.store(static_cast<f32>(peak) / 32768.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
#endif

// o toggle do botão GRAVAR: arranca o worker; volta = STOP → o PCM vira
// .gi ADPCM no projeto (audio/rec-<unix>.gi) + catálogo + log do rácio
#ifdef __ANDROID__
// DEVICE: o mic pede a RECORD_AUDIO runtime (VvActivity.ensureMicPermission
// — o diálogo abre na 1ª vez; o dono re-toca GRAVAR e segue). Sem a ponte =
// gravação desligada com log (o resto do áudio segue)
static bool audioMicGranted() {
    return storage::jniEnsureMicPermission();
}
#else
// HOST/CI: o mic é SINTÉTICO — sem modelo de permissões (a PONTE real é
// aferida à parte, contra o fake JNI)
static bool audioMicGranted() {
    return true;
}
#endif
void audioRecordToggle() {
    if (!g_audioRec.on.load()) {
        if (!g_storage) {
            showToast("sem projeto (gravacao guarda no projeto)");
            return;
        }
        if (!audioMicGranted()) {
            showToast("conceda o microfone e toque Gravar de novo");
            elog::info("audio: gravacao a espera da permissao do mic");
            return;
        }
        {
            const std::lock_guard<std::mutex> lk(g_audioRec.mx);
            g_audioRec.pcm.clear();
        }
        g_audioRec.on.store(true);
        g_audioRec.secs.store(0);
        g_audioRec.level.store(0.0f);
        g_audioRec.t0 = std::chrono::steady_clock::now();
        g_audioRec.th = std::thread(audioRecWorker, std::ref(g_audioRec));
        elog::info("audio: gravacao ARRANCA (mic 44100 Hz mono → ADPCM)");
        showToast("a gravar...");
        return;
    }
    // ---- STOP: PCM → .gi ---------------------------------------------------
    g_audioRec.on.store(false);
    if (g_audioRec.th.joinable()) {
        g_audioRec.th.join();
    }
    std::vector<i16> pcm;
    {
        const std::lock_guard<std::mutex> lk(g_audioRec.mx);
        pcm.swap(g_audioRec.pcm);
    }
    const double secs = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - g_audioRec.t0)
                            .count();
    g_audioRec.level.store(0.0f);
    if (pcm.size() < 256) {
        showToast("gravacao curta demais (ignorada)");
        elog::warn("audio: gravacao com %zu amostras — ignorada", pcm.size());
        return;
    }
    GiWriteIn wi;
    wi.codec = GiCodec::Adpcm;
    wi.sampleRate = g_audioRec.sampleRate;
    wi.channels = 1;
    wi.frames = pcm.size();
    wi.pcm16 = pcm.data();
    wi.name = "rec";
    std::vector<u8> gi;
    std::string err;
    if (!writeGi(wi, gi, err)) {
        showToast("gravacao falhou (engine.log)");
        elog::error("audio: writeGi da gravacao FALHOU — %s", err.c_str());
        return;
    }
    char rel[48];
    std::snprintf(rel, sizeof(rel), "audio/rec-%lld.gi",
                  static_cast<long long>(std::time(nullptr)));
    g_storage->makeDirs("audio");
    if (!g_storage->writeBytes(rel, gi.data(), gi.size())) {
        showToast("falha ao gravar o clip no projeto");
        elog::error("audio: gravacao de %s FALHOU", rel);
        return;
    }
    refreshAudioCatalog();
    // o rácio: PCM16 cru (o "custo" sem codec) vs o .gi final — 4:1 é o
    // contrato do ADPCM (a linha exigida no log)
    const double raw = static_cast<double>(pcm.size()) * 2.0;
    const double ratio = gi.size() > 0 ? raw / static_cast<double>(gi.size())
                                       : 0.0;
    elog::info("audio: gravado %.1fs (%zu amostras) → %s (%zu B, ratio=%.1fx "
               "ADPCM 4:1 vs PCM16)",
               secs, pcm.size(), rel, gi.size(), ratio);
    char msg[96];
    std::snprintf(msg, sizeof(msg), "gravado: %.1fs", secs);
    showToast(msg);
}

// o frame() alimenta o temporizador (o worker não mexe em atomics de UI)
void audioRecTick() {
    if (g_audioRec.on.load()) {
        g_audioRec.secs.store(static_cast<int>(std::chrono::duration<double>(
            std::chrono::steady_clock::now() - g_audioRec.t0).count()));
    }
}

// ---- 0.8.10: IMPORT JOB (cópia streaming + conversão em THREAD) -----------
// O import de 500 MB NUNCA mais corre no frame loop: a cópia por chunks +
// a conversão vivem numa THREAD própria; o frame() desenha o OVERLAY de
// progresso (nome, bytes/total, barra) com botão CANCELAR; o FINALIZE
// (catálogo, diálogo "aplicar ao TIC?") corre no frame() quando o job
// sinaliza done — só a thread principal toca na cena/UI/GL. A RAM de pico
// do job é um chunk (6 MB) + um range + um CompressedImage — nunca a fonte.
struct ImportJob {
    std::thread worker;
    std::atomic<bool> active{false};   // job a correr (overlay visível)
    std::atomic<bool> done{false};     // worker terminou (join no main)
    std::atomic<bool> cancel{false};   // botão cancelar (o job observa)
    std::atomic<u64> bytesDone{0};
    std::atomic<u64> bytesTotal{0};
    // resultado — SÓ o worker escreve; o main lê APÓS done==true
    convert::Output out;
    convert::Stats stats;
    zip::ExtractStats zipStats;        // modo extract
    int  mode = 0;                     // 0=import; 1=EXTRACT (archive)
    std::string destDir;               // "extracted/<nome>" (modo 1)
    std::string err;
    std::string fileName;   // p/ o overlay e o diálogo
} g_importJob;

// progresso do job (chamado na THREAD do job): atualiza os atómicos e
// devolve false quando o utilizador cancelou
bool importJobProgress(void* user, u64 doneBytes, u64 totalBytes) {
    ImportJob* job = static_cast<ImportJob*>(user);
    job->bytesDone.store(doneBytes);
    job->bytesTotal.store(totalBytes);
    return !job->cancel.load();
}

// lança o job (chamado pelo browserImportFile no toque do utilizador).
// false = já há um job a correr (não relança — o overlay é modal)
bool importJobStart(const fileapi::DirEntry& e) {
    if (g_importJob.active.load()) {
        return false;
    }
    if (!g_storage) {
        return false;
    }
    std::string safe = e.name;
    for (char& ch : safe) {
        if (ch == '/' || ch == '\\' || ch == ':') ch = '_';
    }
    g_importJob.mode = 0;
    g_importJob.fileName = e.name;   // NO GLOBAL (o local morria — o nome do
                                     // overlay e o "largar fonte" liam vazio)
    g_importJob.active.store(true);
    g_importJob.done.store(false);
    g_importJob.cancel.store(false);
    g_importJob.bytesDone.store(0);
    g_importJob.bytesTotal.store(0);
    ProjectStorage* st = g_storage.get();
    TexturePipeline* pipe = g_pipeline.get();
    const std::string srcPath = e.path;
    const std::string srcName = safe;
    g_importJob.worker = std::thread([st, pipe, srcPath, srcName]() {
        // cópia do estado do job para o LOCAL (o g_importJob fica estável
        // para o progresso em atómicos; o resultado escreve-se no fim)
        convert::Output out;
        convert::Stats stats;
        std::string err;
        convert::importFile(srcPath, srcName, *st, pipe, out, stats, err,
                            &importJobProgress, &g_importJob);
        g_importJob.out = std::move(out);
        g_importJob.stats = stats;
        g_importJob.err = std::move(err);
        g_importJob.done.store(true);
    });
    g_importJob.err.clear();
    elog::info("import: job iniciado '%s' (thread própria — progresso no "
               "overlay)", e.path.c_str());
    return true;
}

// 0.8.10 — ARCHIVE: o toque num .zip/.rar lança a EXTRAÇÃO (passo 1 de 2).
// RAR: sem unrar vendido (licença) → erro LEGÍVEL "usa .zip" (a tabela de
// decisões vive no RELATÓRIO 0.8.10). ZIP: extrai STREAMING para
// extracted/<nome>/ no projeto — nada é convertido neste passo.
void browserExtractArchive(const fileapi::DirEntry& e) {
    if (!g_storage || e.isDir) {
        return;
    }
    const size_t dot = e.name.rfind('.');
    std::string ext = dot == std::string::npos ? "" : e.name.substr(dot + 1);
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    if (ext != "zip") {
        char msg[96];
        std::snprintf(msg, sizeof(msg),
                      "rar: formato não suportado ainda — usa .zip");
        showToast(msg);
        elog::warn("archive: '%s' — RAR sem decoder (licenca unrar); usa "
                   ".zip (a decisao esta no relatorio)", e.path.c_str());
        return;
    }
    if (g_importJob.active.load()) {
        showToast("import ja em curso...");
        return;
    }
    std::string safe = e.name;
    for (char& ch : safe) {
        if (ch == '/' || ch == '\\' || ch == ':') ch = '_';
    }
    const std::string stem = convert::stemOf(safe);
    const std::string dest = std::string("extracted/") + stem;
    ProjectStorage* st = g_storage.get();
    const std::string srcPath = e.path;
    g_importJob.mode = 1;
    g_importJob.destDir = dest;
    g_importJob.fileName = e.name;
    g_importJob.active.store(true);
    g_importJob.done.store(false);
    g_importJob.cancel.store(false);
    g_importJob.bytesDone.store(0);
    g_importJob.bytesTotal.store(0);
    g_importJob.err.clear();
    g_importJob.worker = std::thread([st, srcPath, dest]() {
        zip::ExtractStats zs;
        std::string err;
        zip::extractArchive(srcPath, *st, dest, zs, err,
                            &importJobProgress, &g_importJob);
        g_importJob.zipStats = zs;
        g_importJob.err = std::move(err);
        g_importJob.done.store(true);
    });
    elog::info("archive: job de extracao iniciado '%s' → %s (thread "
               "propria; NADA converte neste passo)", e.path.c_str(),
               dest.c_str());
}

// FINALIZE no frame(): join + catálogo + diálogo aplicar/toast honesto
void importJobFinish() {
    if (g_importJob.worker.joinable()) {
        g_importJob.worker.join();
    }
    g_importJob.active.store(false);
    const bool canceled = g_importJob.stats.canceled;
    const std::string& err = g_importJob.err;
    if (canceled) {
        showToast("import cancelado");
        elog::info("import: cancelado pelo utilizador (sem estado parcial)");
        return;
    }
    if (g_importJob.mode == 1) {
        // ---- EXTRACT (passo 1 de 2): pasta com ficheiros CRUS, zero
        // conversão; o browser ABRE a pasta (o passo 2 — importar — é
        // manual, pelo fluxo de sempre)
        const zip::ExtractStats& zs = g_importJob.zipStats;
        g_importJob.mode = 0;
        if (!err.empty()) {
            showToast("extracao falhou (causa no engine.log)");
            elog::error("archive: extracao FALHOU — %s", err.c_str());
            return;
        }
        char msg[96];
        std::snprintf(msg, sizeof(msg), "extraido: %u ficheiro(s) crus",
                      zs.files);
        showToast(msg);
        elog::info("archive: extraido %u ficheiro(s) para %s (%u zip-slip "
                   "rejeitado(s), %u aninhado(s) ignorado(s))",
                   zs.files, g_importJob.destDir.c_str(), zs.rejected,
                   zs.nested);
        // o navegador abre a PASTA EXTRAÍDA (ficheiros crus visíveis)
        const std::string abs =
            joinRelPath(g_storage ? g_storage->root() : "",
                        g_importJob.destDir);
        if (!abs.empty()) {
            browserOpen(abs);
            g_editor.fileBrowser = true;
        }
        return;
    }
    if (!err.empty()) {
        showToast("import falhou (causa no engine.log)");
        elog::error("import: FALHOU — %s", err.c_str());
        return;
    }
    // sucesso: catálogo vê os convertidos; pergunta "aplicar ao TIC?"
    refreshCatalog();
    // 0.8.10 — setting "largar a fonte": o convertido está garantido em
    // assets/; a fonte sai (o botão "reconverter" reconverte quem ficar)
    if (!g_keepSource && g_storage) {
        const std::string srcRel =
            std::string("source/") + g_importJob.fileName;
        if (g_storage->remove(srcRel)) {
            elog::info("import: fonte '%s' largada (setting) — convertido "
                       "vive em assets/", srcRel.c_str());
        }
    }
    const convert::Output& out = g_importJob.out;
    char msg[96];
    std::snprintf(msg, sizeof(msg), "importado: %u mesh(es), %u tex (%llu B)",
                  out.meshes.size() + out.textures.size() > 0
                      ? static_cast<unsigned>(out.meshes.size())
                      : 0u,
                  static_cast<unsigned>(out.textures.size()),
                  static_cast<unsigned long long>(
                      g_importJob.stats.outputBytes));
    showToast(msg);
    Tic* tsel = g_scene.get(g_editor.selected);
    if (tsel && tsel->getComponent<MeshRenderer>() &&
        (!out.meshes.empty() || !out.textures.empty())) {
        g_applyAsk.open = true;
        g_applyAsk.kind = !out.meshes.empty() ? 'm' : 't';
        g_applyAsk.rel = !out.meshes.empty() ? out.meshes[0]
                                             : out.textures[0];
        g_applyAsk.fileName = g_importJob.fileName;
        g_editor.applyAsk = true;
        elog::info("import: dialogo 'aplicar ao TIC?' aberto (%s → TIC '%s')",
                   g_applyAsk.rel.c_str(), tsel->name.c_str());
    }
}

// 0.8.11 — IMPORT DE ÁUDIO pelo navegador: fonte → .gi em audio/ + a
// pergunta "aplicar ao TIC?" quando há AudioPlayer selecionado; o clip
// entra no catálogo (Inspector + workspace ÁUDIO)
void browserImportAudio(const fileapi::DirEntry& e) {
    if (!g_storage || e.isDir) {
        return;
    }
    // guarda de TAMANHO: o decode do áudio é em RAM (o streaming de 500 MB
    // é para GEOMETRIA); um "áudio" de centenas de MB é lixo/corrompido
    u64 srcBytes = 0;
    if (fileapi::fileSize(e.path, srcBytes) && srcBytes > (256ull << 20)) {
        showToast("audio demasiado grande (max 256 MB)");
        elog::error("audio: import de '%s' recusado (%llu B > 256 MB)",
                    e.path.c_str(), static_cast<unsigned long long>(srcBytes));
        return;
    }
    std::vector<u8> bytes;
    if (!fileapi::readAll(e.path.c_str(), bytes) || bytes.empty()) {
        showToast("leitura falhou (causa no engine.log)");
        elog::error("audio: leitura de '%s' FALHOU — %s", e.path.c_str(),
                    fileapi::errnoText().c_str());
        return;
    }
    GiImportOut out;
    std::string err;
    if (!importAudioToGi(bytes.data(), bytes.size(), e.name, out, err)) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "audio falhou (%s)", err.c_str());
        showToast(msg);
        elog::error("audio: import de '%s' FALHOU — %s", e.path.c_str(),
                    err.c_str());
        return;
    }
    g_storage->makeDirs("audio");
    std::string safe = convert::sanitizeName(e.name);
    const std::string rel =
        std::string("audio/") + convert::stemOf(safe) + ".gi";
    if (!g_storage->writeBytes(rel, out.gi.data(), out.gi.size())) {
        showToast("falha ao gravar o clip no projeto");
        elog::error("audio: gravacao de %s FALHOU", rel.c_str());
        return;
    }
    // o RÁCIO no log (a linha exigida pelo prompt)
    const double ratio = out.gi.size() > 0
        ? static_cast<double>(out.sourceBytes) /
          static_cast<double>(out.gi.size())
        : 0.0;
    elog::info("audio: import %s codec=%s ratio=%.2fx (%llu B -> %zu B, "
               "%uch %uHz %.2fs)",
               rel.c_str(), giCodecName(out.codec), ratio,
               static_cast<unsigned long long>(out.sourceBytes),
               out.gi.size(), out.channels, out.sampleRate, out.duration);
    refreshAudioCatalog();
    char msg[96];
    std::snprintf(msg, sizeof(msg), "clip: %s (%.1fs)",
                  convert::stemOf(safe).c_str(), out.duration);
    showToast(msg);
    // aplicar ao TIC selecionado (se tem AudioPlayer)
    Tic* tsel = g_scene.get(g_editor.selected);
    if (tsel && tsel->getComponent<AudioPlayer>()) {
        g_applyAsk.open = true;
        g_applyAsk.kind = 'a';   // áudio
        g_applyAsk.rel = rel;
        g_applyAsk.fileName = e.name;
        g_editor.applyAsk = true;
        elog::info("audio: dialogo 'aplicar ao TIC?' aberto (%s → '%s')",
                   rel.c_str(), tsel->name.c_str());
    }
}

// o toque no ficheiro escolhido: lança o JOB (streaming + conversão)
void browserImportFile(const fileapi::DirEntry& e) {
    if (!g_storage || e.isDir) {
        return;
    }
    elog::info("import: ficheiro '%s' escolhido no navegador", e.path.c_str());
    // 0.8.10 — ARCHIVE (.zip/.rar): EXTRAIR (passo 1 de 2) — utilitário
    // SEM conversão; o import de dentro da pasta extraída é que converte
    if (e.kind == 'a') {
        browserExtractArchive(e);
        return;
    }
    // 0.8.11 — ÁUDIO (.wav/.ogg/.mp3): importa → audio/<nome>.gi (ADPCM
    // 4:1 ou passthrough) — síncrono (o decode é rápido; o overlay de
    // progresso é para os 500 MB de geometria) com log do rácio
    if (e.kind == 's') {
        browserImportAudio(e);
        return;
    }
    // 0.8.5 — FORMATO NÃO SUPORTADO → ERRO CLARO (nunca silêncio)
    if (e.kind == 0) {
        const size_t dot = e.name.rfind('.');
        const std::string ext = dot == std::string::npos
                                    ? "(sem extensão)"
                                    : e.name.substr(dot);
        char msg[96];
        std::snprintf(msg, sizeof(msg), "formato não suportado ainda: %s",
                      ext.c_str());
        showToast(msg);
        elog::warn("import: '%s' — formato %s não suportado (aceites: "
                   ".obj .gltf .glb .png .zip .rar)", e.path.c_str(),
                   ext.c_str());
        return;
    }
    if (!importJobStart(e)) {
        showToast("import ja em curso...");
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

// ---- 0.9.1 — JANELA DE TEXTO PESADO (abrir/fechar COM o par obrigatório) ----
//
// ABRIR  = portrait (JNI) + IME show + log da orientação (ime::setOrientation
//          grava o estado e loga; o par de chamadas Java é o executor).
// FECHAR = landscape + IME hide + log. O par é INSEPARÁVEL — os testes do
//          device (wiring087) aferem os DOIS lados em cada transição.
void openTextWindow() {
    editor::textwin::open(g_editor.textWin);
    if (ime::setOrientation(ime::Orientation::Portrait,
                            "janela de texto aberta")) {
        storage::jniSetOrientation(true);
    }
    storage::jniImeShow();
    elog::info("texto: janela aberta — IME do sistema pedido (show)");
}

void closeTextWindow() {
    editor::textwin::close(g_editor.textWin);
    if (ime::setOrientation(ime::Orientation::Landscape,
                            "janela de texto fechada")) {
        storage::jniSetOrientation(false);
    }
    storage::jniImeHide();
    elog::info("texto: janela fechada — IME escondido, landscape reposto");
}

// ---- 0.9.2 — EDITOR DE SCRIPT + DOCS -----------------------------------------
//
// O par INSEPARÁVEL do textWin aplica-se ao editor de script (janela de
// TEXTO PESADO — spec 0.9.1 §1): abrir = portrait + IME show; fechar =
// landscape + IME hide. O buffer SALVA no ScriptComp ao fechar (§10).
// instala os hooks UMA vez no arranque do android_main (ver chamada abaixo)
static void voniInstallHooks() {
    g_voni.setSceneNameFn([]() { return currentSceneBaseName(); });
    g_voni.setTransitionHook(&voniTransitionHook);
}

void openScriptEditor(Handle tic) {
    Tic* t = g_scene.get(tic);
    if (!t) {
        return;
    }
    editor::scriptwin::open(g_editor.scriptWin, g_scene, tic);
    if (ime::setOrientation(ime::Orientation::Portrait,
                            "editor de script aberto")) {
        storage::jniSetOrientation(true);
    }
    storage::jniImeShow();
    elog::info("voni: editor de script aberto — portrait + IME");
}

void closeScriptEditor() {
    // salva o fonte no componente (persistir §10 — o back NÃO descarta).
    // FASE 9 (G0-1): o handle pode ter MORRIDO no ciclo TERM→INIT da
    // rotação portrait (o reload do INIT_WINDOW re-cria os TICs) —
    // re-valida por NOME antes de salvar (o padrão da seleção 0.8.12);
    // sem isto a fonte NUNCA era gravada (get()==null) e o Run dava
    // "TIC inválido".
    editor::scriptwin::revalidateTic(g_editor.scriptWin, g_scene);
    if (Tic* t = g_scene.get(g_editor.scriptWin.tic)) {
        if (ScriptComp* sc = t->getComponent<ScriptComp>()) {
            sc->source = g_editor.scriptWin.buf;
        } else {
            // o TIC existe mas perdeu o componente (cena trocada?) — cria
            // um novo e salva (o fonte não se perde por um ciclo)
            ScriptComp& sc2 = *t->addComponent<ScriptComp>();
            sc2.source = g_editor.scriptWin.buf;
        }
    } else {
        elog::warn("voni: fonte do editor PERDIDA no fecho (TIC '%s' não "
                   "existe na cena)",
                   g_editor.scriptWin.ticName);
    }
    editor::scriptwin::close(g_editor.scriptWin);
    if (ime::setOrientation(ime::Orientation::Landscape,
                            "editor de script fechado")) {
        storage::jniSetOrientation(false);
    }
    storage::jniImeHide();
    elog::info("voni: editor de script fechado — landscape reposto");
}

// Run do editor: (re)compila + arranca; o erro (com linha) volta para a
// barra do editor + engine.log + toast
void scriptEditorRun() {
    // FASE 9 (G0-1): o handle pode ter morrido no ciclo da rotação —
    // re-valida por NOME antes de compilar (senão "TIC inválido")
    editor::scriptwin::revalidateTic(g_editor.scriptWin, g_scene);
    voni::Error err;
    if (!g_voni.editorRestart(g_scene, g_editor.scriptWin.tic,
                              g_editor.scriptWin.buf.c_str(), err)) {
        g_editor.scriptWin.errLine = err.line;
        g_editor.scriptWin.errMsg = err.message;
        // 0.9.6 (G2-7e): o PAR do botão Substituir (o erro de COMPILE
        // também ensina — o gancho do VoniCompile preencheu o par)
        g_editor.scriptWin.fixFrom = err.fixFrom;
        g_editor.scriptWin.fixTo = err.fixTo;
        g_editor.scriptWin.running = false;
        elog::error("voni: script erro linha %u: %s", err.line,
                    err.message.c_str());
        std::snprintf(g_toast, sizeof(g_toast), "script: linha %u", err.line);
        g_toastT = 1.8f;
    } else {
        g_editor.scriptWin.errLine = 0;
        g_editor.scriptWin.errMsg.clear();
        g_editor.scriptWin.fixFrom.clear();
        g_editor.scriptWin.fixTo.clear();
        g_editor.scriptWin.running = true;
        elog::info("voni: script a correr (editor Run)");
    }
}

void scriptEditorStop() {
    g_voni.editorStop(g_editor.scriptWin.tic);
    g_editor.scriptWin.running = false;
    elog::info("voni: script parado (editor Stop)");
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
// 0.8.10 — o catálogo lista CAMINHOS COMPLETOS (as refs que o
// ResourceManager/GpuAssets resolvem): assets/*.gmesh|.gtext em PRIMEIRO
// (formatos próprios, o que o runtime carrega) + legado meshes/|textures/
// cujo convertido AINDA NÃO existe (falhou/marcou). O seletor do Inspector
// e o "Sim" do diálogo usam as entradas DIRETAMENTE.
void refreshCatalog() {
    g_catalog.meshes.clear();
    g_catalog.textures.clear();
    if (!g_storage) {
        return;
    }
    std::vector<std::string> files;
    if (g_storage->listDir(Project::kDirAssets, files)) {
        for (const std::string& f : files) {
            const std::string rel = std::string(Project::kDirAssets) + "/" + f;
            if (f.size() > 6 && f.compare(f.size() - 6, 6, ".gmesh") == 0) {
                g_catalog.meshes.push_back(rel);
            } else if (f.size() > 6 &&
                       f.compare(f.size() - 6, 6, ".gtext") == 0) {
                g_catalog.textures.push_back(rel);
            }
        }
    }
    // legado meshes/ (fontes 0.8.9-): só quem AINDA não tem convertido
    files.clear();
    if (g_storage->listDir(Project::kDirMeshes, files)) {
        for (const std::string& f : files) {
            if (fileapi::kindOfExtension(f) != 'm') {
                continue;   // 0.8.5: classificação centralizada (lowercase)
            }
            const std::string stem = convert::stemOf(f);
            bool jaConvertido = false;
            for (const std::string& m : g_catalog.meshes) {
                if (m.find(stem) != std::string::npos) {
                    jaConvertido = true;
                    break;
                }
            }
            if (!jaConvertido) {
                g_catalog.meshes.push_back(std::string("meshes/") + f);
            }
        }
    }
    files.clear();
    if (g_storage->listDir(Project::kDirTextures, files)) {
        for (const std::string& f : files) {
            if (fileapi::kindOfExtension(f) != 't') {
                continue;
            }
            const std::string stem = convert::stemOf(f);
            bool jaConvertido = false;
            for (const std::string& t : g_catalog.textures) {
                if (t.find(stem) != std::string::npos) {
                    jaConvertido = true;
                    break;
                }
            }
            if (!jaConvertido) {
                g_catalog.textures.push_back(std::string("textures/") + f);
            }
        }
    }
}

// 0.8.11 — VISUAL de editor do AudioPlayer (a técnica dos gizmos: pontos
// 3D PROJETADOS para px de ecrã + polilinhas da UI): glifo de ALTIFALANTE
// na posição do TIC + esfera WIREFRAME do raio externo (posicional). SÓ no
// editor — em Play nada desenha (só soa). Corre no pass UI.
void drawAudioGlyph(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                    const Tic& t) {
    const AudioPlayer* au = t.getComponent<AudioPlayer>();
    const Transform3D* tr = t.getComponent<Transform3D>();
    if (!au || !tr || !t.active) {
        return;
    }
    const Vec3 p = tr->pos;
    const f32 col[4] = {au->voiceId >= 0 ? 0.28f : 0.90f,
                        au->voiceId >= 0 ? 0.82f : 0.72f,
                        au->voiceId >= 0 ? 0.36f : 0.30f, 1.0f};
    auto seg = [&](const Vec3& a, const Vec3& b) {
        f32 ax, ay, bx, by;
        if (gizmo::projectPoint(vp, a, sw, sh, ax, ay) &&
            gizmo::projectPoint(vp, b, sw, sh, bx, by)) {
            ui.drawLine(ax, ay, bx, by, 2.0f, col);
        }
    };
    // caixa + cone + ondas (plano local XY — glifo pequeno, leitura visual)
    const Vec3 dx{0.11f, 0.0f, 0.0f};
    const Vec3 dy{0.0f, 0.11f, 0.0f};
    seg(p - dx - dy, p - dx + dy);
    seg(p - dx + dy, p + Vec3{-0.02f, 0.08f, 0.0f});
    seg(p + Vec3{-0.02f, 0.08f, 0.0f}, p + Vec3{-0.02f, -0.08f, 0.0f});
    seg(p + Vec3{-0.02f, -0.08f, 0.0f}, p - dx - dy);
    seg(p + Vec3{-0.02f, -0.08f, 0.0f}, p + dx - dy * 2.0f);
    seg(p + dx - dy * 2.0f, p + dx + dy * 2.0f);
    seg(p + dx + dy * 2.0f, p + Vec3{-0.02f, 0.08f, 0.0f});
    seg(p + Vec3{0.14f, -0.06f, 0.0f}, p + Vec3{0.14f, 0.06f, 0.0f});
    seg(p + Vec3{0.18f, -0.11f, 0.0f}, p + Vec3{0.18f, 0.11f, 0.0f});
    // esfera wireframe do raio EXTERNO (posicional): 8 longitudes × 4 lat
    if (au->posicional) {
        const f32 R = au->raioExterno;
        constexpr int kLon = 8;
        constexpr int kLat = 4;
        constexpr f32 kPi = 3.14159265f;
        constexpr f32 kTau = 6.28318531f;
        const f32 colR[4] = {0.47f, 0.59f, 1.0f, 0.9f};
        for (int la = 0; la <= kLat; ++la) {
            const f32 phi = static_cast<f32>(la) / kLat * kPi;
            for (int lo = 0; lo < kLon; ++lo) {
                const f32 th0 = static_cast<f32>(lo) / kLon * kTau;
                const f32 th1 = static_cast<f32>(lo + 1) / kLon * kTau;
                const Vec3 a{p.x + R * std::sin(phi) * std::cos(th0),
                             p.y + R * std::cos(phi),
                             p.z + R * std::sin(phi) * std::sin(th0)};
                const Vec3 b{p.x + R * std::sin(phi) * std::cos(th1),
                             p.y + R * std::cos(phi),
                             p.z + R * std::sin(phi) * std::sin(th1)};
                { f32 ax, ay, bx2, by2;
                  if (gizmo::projectPoint(vp, a, sw, sh, ax, ay) &&
                      gizmo::projectPoint(vp, b, sw, sh, bx2, by2)) {
                      ui.drawLine(ax, ay, bx2, by2, 1.5f, colR);
                  } }
                if (la > 0) {
                    const f32 phiP = static_cast<f32>(la - 1) / kLat * kPi;
                    const Vec3 up{p.x + R * std::sin(phiP) * std::cos(th0),
                                  p.y + R * std::cos(phiP),
                                  p.z + R * std::sin(phiP) * std::sin(th0)};
                    { f32 ux, uy, ax2, ay2;
                      if (gizmo::projectPoint(vp, up, sw, sh, ux, uy) &&
                          gizmo::projectPoint(vp, a, sw, sh, ax2, ay2)) {
                          ui.drawLine(ux, uy, ax2, ay2, 1.5f, colR);
                      } }
                }
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

// ---- 0.8.10: PRIMITIVAS — O CAMINHO ÚNICO DA TROCA ------------------------
// (substitui o cache 0.8.0→0.8.9; ver o comentário grande no topo do ficheiro)

// nome curto de UMA assinatura p/ o log (esfera/box + o raio/tam que a distingue)
static void primSigLabel(const PrimParams& p, char* out, size_t n) {
    if (p.kind == PrimKind::Box) {
        std::snprintf(out, n, "box(tam=%.2f)", p.size);
    } else {
        std::snprintf(out, n, "esfera(r=%.2f,seg=%d,an=%d)", p.radius,
                      p.segments, p.rings);
    }
}

// move a POSE do mesh antigo do MeshRenderer para a COVA (deferred free do
// frame seguinte). O cubo estático do main NÃO é nosso — ignora-se.
static void primRetire(Mesh* m) {
    if (!m || m == &g_cubeMesh) {
        return;
    }
    for (size_t i = 0; i < g_primOwners.size(); ++i) {
        if (g_primOwners[i].get() == m) {
            g_primGrave.push_back(std::move(g_primOwners[i]));
            g_primOwners.erase(g_primOwners.begin() + static_cast<long>(i));
            return;
        }
    }
}

// INÍCIO do frame: liberta os meshes que REFORMÁMOS no frame anterior —
// com o contexto GL corrente e DEPOIS de os comandos do frame anterior
// terem saído (nunca glDelete* de buffers ainda em voo).
void primGraveDig() {
    if (g_primGrave.empty()) {
        return;
    }
    const u32 n = static_cast<u32>(g_primGrave.size());
    for (std::unique_ptr<Mesh>& m : g_primGrave) {
        if (m) {
            m->destroy();
        }
    }
    g_primGrave.clear();
    elog::info("mesh: deferred free %u mesh(es) de prim (inicio do frame)", n);
}

// 0.8.10 — SELF-CHECK pós-upload: as CONTAGENS que o Mesh diz ter de
// coincidir com as contagens da GEOMETRIA GERADA (apanha upload corrompido/
// parcial — a discrepância é ERRO no passo upload e o fail-safe mantém o
// mesh anterior). Puro + afervel no CI.
bool primSelfCheck(const Mesh& m, u32 wantVerts, u32 wantIdx) {
    if (!m.ok()) {
        return false;
    }
    if (m.vertexCount() != wantVerts || m.indexCount() != wantIdx) {
        return false;
    }
    // AABB de volta ≠ zero (geometria de mentira não passa)
    return m.boundsMaxExtent() > 1e-6f;
}

// O ÚNICO caminho de troca (não há "hit de cache" nem caminho alternativo):
// gera → valida → upload → self-check → bind, com deferred free do antigo.
// Chamado SÓ do ponto seguro do frame (início, antes da submissão) pelo
// primFlushPending, e pelo boot do INIT_WINDOW p/ os pendentes do load.
// Falha: mesh anterior mantém, primNeg=true (backoff), toast + log com
// passo E razão. NUNCA crash, NUNCA desseleciona.
bool primUploadOne(MeshRenderer& mr) {
    // "de" = a FONTE do mesh ligado AGORA (a verdade do render: cube/asset/
    // prim com a assinatura anterior — primPrev, porque o pick já escreveu
    // a nova em mr.prim); "para" = a assinatura PEDIDA.
    char de[48], para[48];
    if (mr.mesh == nullptr) {
        std::snprintf(de, sizeof(de), "-");
    } else if (mr.mesh == &g_cubeMesh) {
        std::snprintf(de, sizeof(de), "cube");
    } else if (!mr.meshPath.empty()) {
        std::snprintf(de, sizeof(de), "asset");
    } else {
        primSigLabel(mr.primPrev, de, sizeof(de));
    }
    primSigLabel(mr.prim, para, sizeof(para));

    // ---- passo=gerador -----------------------------------------------------
    PrimMeshData data;
    makePrimMesh(mr.prim, data);
    if (!data.ok()) {
        ++g_primSwapErr;
        mr.primNeg = true;   // backoff: não re-tenta por frame
        elog::error("mesh: troca %s→%s passo=gerador ERRO(geometria vazia — "
                    "%u verts %u idx)", de, para,
                    static_cast<unsigned>(data.vertices.size()),
                    static_cast<unsigned>(data.indices.size()));
        showToast("mesh: troca ERRO(gerador) — mantida a anterior");
        return false;
    }
    elog::info("mesh: troca %s→%s passo=gerador ok verts=%u idx=%u", de, para,
               static_cast<unsigned>(data.vertices.size()),
               static_cast<unsigned>(data.indices.size()));

    // ---- passo=validacao ---------------------------------------------------
    for (size_t v = 0; v < data.vertices.size(); ++v) {
        const Vec3& pos = data.vertices[v].pos;
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) ||
            !std::isfinite(pos.z)) {
            ++g_primSwapErr;
            mr.primNeg = true;
            elog::error("mesh: troca %s→%s passo=validação ERRO(vert %zu "
                        "não finito)", de, para, v);
            showToast("mesh: troca ERRO(validação) — mantida a anterior");
            return false;
        }
    }
    {
        Vec3 mn, mx;
        primBounds(data, mn, mx);
        const Vec3 ext{mx.x - mn.x, mx.y - mn.y, mx.z - mn.z};
        const f32 maior = ext.x > ext.y ? (ext.x > ext.z ? ext.x : ext.z)
                                        : (ext.y > ext.z ? ext.y : ext.z);
        if (!(maior > 1e-6f)) {
            ++g_primSwapErr;
            mr.primNeg = true;
            elog::error("mesh: troca %s→%s passo=validação ERRO(AABB "
                        "degenerado — extensao %.3g)", de, para, maior);
            showToast("mesh: troca ERRO(validação) — mantida a anterior");
            return false;
        }
    }
    elog::info("mesh: troca %s→%s passo=validação ok", de, para);

    // ---- passo=upload + SELF-CHECK ----------------------------------------
    std::unique_ptr<Mesh> nm = std::make_unique<Mesh>();
    if (!nm->create(data.vertices.data(),
                    static_cast<u32>(data.vertices.size()),
                    data.indices.data(),
                    static_cast<u32>(data.indices.size())) ||
        !primSelfCheck(*nm, static_cast<u32>(data.vertices.size()),
                       static_cast<u32>(data.indices.size()))) {
        ++g_primSwapErr;
        mr.primNeg = true;
        nm.reset();   // ~Mesh com contexto corrente liberta o que subiu
        elog::error("mesh: troca %s→%s passo=upload ERRO(upload GL/self-check "
                    "— %u verts %u idx)", de, para,
                    static_cast<unsigned>(data.vertices.size()),
                    static_cast<unsigned>(data.indices.size()));
        showToast("mesh: troca ERRO(upload) — mantida a anterior");
        return false;
    }
    elog::info("mesh: troca %s→%s passo=upload ok verts=%u idx=%u "
               "(self-check ok)", de, para,
               static_cast<unsigned>(data.vertices.size()),
               static_cast<unsigned>(data.indices.size()));

    // ---- passo=bind: troca ATÓMICA + deferred free do antigo ---------------
    // o ANTIGO (que renderizou até este ponto seguro) vai para a COVA —
    // os seus buffers só são apagados no início do frame SEGUINTE
    primRetire(mr.mesh);
    mr.mesh = nm.get();
    mr.material = g_renderer.litMaterial();
    mr.primPrev = mr.prim;   // a cadeia: o "de" da PRÓXIMA troca é este
    g_primOwners.push_back(std::move(nm));
    ++g_primSwapOk;
    elog::info("mesh: troca %s→%s passo=bind ok (cova=%zu vivos=%zu)",
               de, para, g_primGrave.size(), g_primOwners.size());
    return true;
}

// PONTO SEGURO: (a) sobe os PEDIDOS pendentes de prim (pick/params/load/
// preset armaram primPending — o mesh ANTIGO renderizou até aqui e agora
// o novo substitui-o com bind atómico + deferred free); (b) transfere para
// a COVA os meshes marcados primRetire (picks none/cube/asset). Backoff:
// primNeg não insiste por frame (anti retry-storm — o pedido NOVO limpa).
void primFlushPending() {
    auto& mrs = g_scene.components().meshRenderers();
    for (u32 i = 0; i < mrs.size(); ++i) {
        MeshRenderer& mr = mrs.at(i);
        // pendente do pick/params/load OU rebind pós-TERM (mesh null com
        // primOn: o detach do lifecycle — o mesmo caminho único de sempre;
        // primNeg em falha mantém o mesh ANTIGO não-nulo, então este
        // re-rebind NUNCA vira retry-storm)
        if (mr.primOn && !mr.primNeg &&
            (mr.primPending || mr.mesh == nullptr)) {
            mr.primPending = false;   // consumido (falha = backoff, não retry)
            primUploadOne(mr);
        }
        if (mr.primRetire) {
            primRetire(mr.primRetire);
            mr.primRetire = nullptr;
        }
    }
}

// TERM_WINDOW: destrói os meshes VIVOS e os da cova (contexto AINDA corrente
// — o mesmo contrato do cubo/grid). O reload do INIT_WINDOW re-pede tudo.
void primMeshesDestroyAll() {
    const u32 vivos = static_cast<u32>(g_primOwners.size());
    const u32 cova = static_cast<u32>(g_primGrave.size());
    for (std::unique_ptr<Mesh>& m : g_primOwners) {
        if (m) {
            m->destroy();
        }
    }
    for (std::unique_ptr<Mesh>& m : g_primGrave) {
        if (m) {
            m->destroy();
        }
    }
    g_primOwners.clear();
    g_primGrave.clear();
    // novo contexto, nova sorte: o backoff morre com o contexto; o
    // pendente de load fica (o próximo frame re-sobe com o contexto novo)
    {
        auto& mrs2 = g_scene.components().meshRenderers();
        for (u32 i = 0; i < mrs2.size(); ++i) {
            mrs2.at(i).primNeg = false;
            mrs2.at(i).primRetire = nullptr;
        }
    }
    if (vivos + cova > 0) {
        elog::info("lifecycle: %u mesh(es) de prim destruidos (%u vivos + "
                   "%u na cova)", vivos + cova, vivos, cova);
    }
}

// A cena ATUAL vai ser substituída (load/switch/cena nova): toda a posse
// de meshes de prim vai para a COVA — o glDelete* corre no início do frame
// SEGUINTE (deferred free: os MeshRenderers antigos podem ainda desenhar
// neste frame; os ponteiros ficam VÁLIDOS até eles morrerem no clear()).
// Nunca se apaga nada a meio do frame — o mesmo contrato da troca.
void primMeshesToGrave() {
    if (g_primOwners.empty()) {
        return;
    }
    const u32 n = static_cast<u32>(g_primOwners.size());
    for (std::unique_ptr<Mesh>& m : g_primOwners) {
        g_primGrave.push_back(std::move(m));
    }
    g_primOwners.clear();
    elog::info("mesh: %u mesh(es) de prim p/ cova (cena substituida)", n);
}

// ---- 0.8.10: PÓS-LOAD — migração de projeto antigo + refs + .gm -----------
// Corre DEPOIS de cada loadActiveScene (boot/switch/menu):
//   1. MIGRAÇÃO silenciosa: meshes/*.obj|gltf|glb + textures/*.png legados
//      convertem para assets/*.gmesh|.gtext (+.gm) — 1× (idempotente);
//   2. FIXUP de refs: MeshRenderer.meshPath "meshes/x.obj" →
//      "assets/x.gmesh" quando o convertido existe (idem texturas); o TIC
//      aponta o formato próprio e o runtime NUNCA mais toca na fonte;
//   3. .gm IRMÃO: assets/x.gm existe → attachGAnim (clips + esqueleto) ao
//      TIC dono do mesh (o mesmo attach do import).
// NUNCA falha o load: cada passo é best-effort com log.
void postLoadMigrateAndFixup() {
    if (!g_storage || !g_projectReady) {
        return;
    }
    // 0.8.11 — settings do projeto (fonte/volume) + catálogo de áudio: o
    // load aplica (0.8.10 escrevia o settings.goni mas nunca o LIA)
    loadProjectSettings();
    loadLayoutNow();   // 0.9.0 (spec G): layout.json (bottom/drawer/inspector/secções)
    refreshAudioCatalog();
    // 1) migração (silenciosa — os assets convertem 1×)
    convert::migrateLegacyAssets(*g_storage, g_pipeline.get());
    // 2) fixup das refs da cena recém-carregada
    bool refsChanged = false;
    {
        auto& mrs = g_scene.components().meshRenderers();
        for (u32 i = 0; i < mrs.size(); ++i) {
            MeshRenderer& mr = mrs.at(i);
            if (mr.meshPath.compare(0, 7, "meshes/") == 0) {
                const std::string stem =
                    convert::stemOf(mr.meshPath.substr(7));
                const std::string want =
                    std::string("assets/") + stem + ".gmesh";
                if (g_storage->exists(want)) {
                    elog::info("asset: ref migrada '%s' -> '%s'",
                               mr.meshPath.c_str(), want.c_str());
                    mr.meshPath = want;
                    refsChanged = true;
                }
            }
            if (mr.texPath.compare(0, 9, "textures/") == 0) {
                const std::string stem =
                    convert::stemOf(mr.texPath.substr(9));
                const std::string want =
                    std::string("assets/") + stem + ".gtext";
                if (g_storage->exists(want)) {
                    mr.texPath = want;
                    refsChanged = true;
                }
            }
        }
    }
    if (refsChanged) {
        // persiste as refs novas já (o próximo save é o do dono — este é o
        // "em silêncio": abrir o projeto velho NÃO pede nada a ninguém)
        g_project.saveActiveScene(*g_storage, g_scene);
        refreshCatalog();
    }
    // 3) .gm irmão de cada ref .gmesh → clips + esqueleto
    {
        auto& mrs = g_scene.components().meshRenderers();
        for (u32 i = 0; i < mrs.size(); ++i) {
            MeshRenderer& mr = mrs.at(i);
            if (mr.meshPath.size() > 6 &&
                mr.meshPath.compare(mr.meshPath.size() - 6, 6, ".gmesh") == 0) {
                const std::string gmRel = convert::ganimSiblingOf(mr.meshPath);
                if (gmRel.empty() || !g_storage->exists(gmRel)) {
                    continue;
                }
                std::vector<u8> gmb;
                if (!g_storage->readBytes(gmRel, gmb) || gmb.empty()) {
                    continue;
                }
                GAnimFile anim;
                std::string aerr;
                if (!readGAnim(gmb.data(), gmb.size(), anim, aerr)) {
                    elog::warn("asset: %s invalido — %s (ignorado)",
                               gmRel.c_str(), aerr.c_str());
                    continue;
                }
                // o TIC dono: os componentes não guardam o dono — itera os
                // TICs ativos (a cena é pequena) e casa o PONTEIRO
                g_scene.forEachActive([&](Tic& t) {
                    if (t.getComponent<MeshRenderer>() == &mr) {
                        const u32 n = attachGAnim(g_scene, t.handle, anim);
                        if (n > 0) {
                            LOGI("asset: %u clip(s) do %s anexados ao TIC '%s'",
                                 n, gmRel.c_str(), t.name.c_str());
                        }
                    }
                });
            }
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
    // 0.8.10 — MIGRAÇÃO de prim removida (cilindro/cone/plano/triangulo/
    // torus/capsula → cube): log SEMPRE + toast UMA vez por load (a cena
    // abre, nunca crash — a prim carrega como box pendente e sobe no ponto
    // seguro do frame).
    ctx.onPrimMigrated = [](const char* removedName) {
        elog::warn("mesh: prim %s removido -> cube (0.8.10: so cubo e esfera)",
                   removedName);
        static bool toastedThisLoad = false;   // 1× por load (log = todas)
        if (!toastedThisLoad) {
            char msg[96];
            std::snprintf(msg, sizeof(msg),
                          "prim %s foi removida -> cube", removedName);
            showToast(msg);
            toastedThisLoad = true;
        }
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
    // 0.8.9: AABB do mesh COMO DADOS (o applyAssetPick é puro — nunca
    // desreferencia o Mesh; aqui sim, no device, o Mesh é REAL)
    res.meshExtent = [](const std::string& ref) -> Vec3 {
        Mesh* m = g_gpu.mesh(ref);
        return m ? m->boundsExtent() : Vec3{0.0f, 0.0f, 0.0f};
    };
    return res;
}

// 0.8.7 — o "Sim" do diálogo APLICAR-APÓS-IMPORT, EXTRAÍDO do corpo do
// frame() para função com NOME: o MESMO código corre no device (frame) e
// na suíte do hospedeiro (test_wiring087 — que inclui ESTE ficheiro). O
// import aplica ao TIC selecionado: applyAssetPick (o caminho do seletor)
// + clips/skin do glTF quando o mesh os traz.
void applyImportedAssetToSelectedTic() {
    // 0.8.11 — ÁUDIO: o "Sim" atribui o clip ao AudioPlayer do TIC
    // selecionado (e toca um PREVIEW de 1 s — o dono OUVE que importou)
    if (g_applyAsk.kind == 'a') {
        Tic* tsel = g_scene.get(g_editor.selected);
        AudioPlayer* au = tsel ? tsel->getComponent<AudioPlayer>() : nullptr;
        if (au) {
            au->clipPath = g_applyAsk.rel;
            char msg[96];
            std::snprintf(msg, sizeof(msg), "clip atribuido: %s",
                          g_applyAsk.rel.c_str());
            showToast(msg);
            elog::info("audio: clip '%s' atribuido ao TIC '%s'",
                       g_applyAsk.rel.c_str(), tsel->name.c_str());
        } else {
            showToast("selecione um TIC de Audio");
            elog::warn("audio: aplicar sem AudioPlayer no TIC selecionado");
        }
        g_applyAsk.open = false;
        g_editor.applyAsk = false;
        return;
    }
    // 0.8.10: as entradas do catálogo são CAMINHOS COMPLETOS — o match é
    // direto (a ref veio do import: assets/<x>.gmesh / .gtext)
    const std::vector<std::string>& cat =
        g_applyAsk.kind == 'm' ? g_catalog.meshes : g_catalog.textures;
    bool applied = false;
    for (size_t i = 0; i < cat.size(); ++i) {
        if (cat[i] == g_applyAsk.rel) {
            const int menuKind = g_applyAsk.kind == 'm' ? 1 : 2;
            // 0.8.12 — o pick do ficheiro i: picker de MESH tem none(1) +
            // cube(2) antes dos ficheiros (i+3); o de TEXTURA só none(1)
            // antes (i+2)
            const int pickOf = static_cast<int>(i) +
                               (g_applyAsk.kind == 'm' ? 3 : 2);
            elog::info("import: aplicando %s ao TIC selecionado (pick %d)",
                       g_applyAsk.rel.c_str(), pickOf);
            const editor::TicSnap uBefore =
                editor::snapTic(g_scene, g_editor.selected);
            const editor::AssetPickOutcome out = editor::applyAssetPick(
                g_scene, g_editor.selected, menuKind, pickOf, g_catalog,
                makeAssetResolvers());
            applied = out.applied;
            if (out.applied) {
                pushUndo(uBefore, g_scene.get(g_editor.selected));
            }
            if (out.toast[0] != '\0') {
                showToast(out.toast);
            }
            if (out.log[0] != '\0') {
                elog::info("%s", out.log);   // 0.8.9: idem (o "Sim" do import prova o fit uniforme no log)
            }
            // 0.8.7 — contagens do mesh APLICADO (o TIC tem meshes reais no
            // caminho do import; o applyAssetPick é puro — não desreferencia)
            if (out.applied) {
                if (const Tic* tNew = g_scene.get(g_editor.selected)) {
                    if (const MeshRenderer* mrNew =
                            tNew->getComponent<MeshRenderer>()) {
                        if (mrNew->mesh) {
                            elog::info("import: aplicado verts=%u idx=%u",
                                       mrNew->mesh->vertexCount(),
                                       mrNew->mesh->indexCount());
                        }
                    }
                }
            }
            // 0.8.10: ref CONVERTIDA (.gmesh) → clips+esqueleto do .gm
            // irmão (o mesmo attach do load de cenas); ref LEGADA
            // (gltf/glb) → o caminho de sempre (parse do modelo)
            if (g_applyAsk.kind == 'm') {
                const std::string gmRel = convert::ganimSiblingOf(
                    g_applyAsk.rel);
                GAnimFile anim;
                std::string aerr;
                std::vector<u8> gmb;
                if (!gmRel.empty() && g_storage && g_storage->exists(gmRel) &&
                    g_storage->readBytes(gmRel, gmb) && !gmb.empty() &&
                    readGAnim(gmb.data(), gmb.size(), anim, aerr)) {
                    const u32 nClips =
                        attachGAnim(g_scene, g_editor.selected, anim);
                    if (nClips > 0) {
                        showToast("clips importados (timeline)");
                        LOGI("editor: %u clip(s) do %s importados",
                             nClips, gmRel.c_str());
                    }
                } else {
                    std::string merr;
                    if (auto mdl = g_resources.model(g_applyAsk.rel, merr)) {
                        // 0.8.2 (F7): SKIN primeiro (os clips de JOINT
                        // só entram com o esqueleto presente)
                        const u32 nJoints =
                            gltfAttachSkin(g_scene, g_editor.selected, *mdl);
                        if (nJoints > 0) {
                            LOGI("editor: esqueleto importado (%u joints)",
                                 nJoints);
                        }
                        if (!mdl->animations.empty()) {
                            const u32 nClips = gltfAttachClips(
                                g_scene, g_editor.selected, *mdl);
                            if (nClips > 0) {
                                showToast("clips importados (timeline)");
                                LOGI("editor: %u clip(s) de animação "
                                     "importados de %s",
                                     nClips, g_applyAsk.rel.c_str());
                            }
                        }
                    }
                }
            }
            break;
        }
    }
    if (!applied) {
        // o catálogo não tem a ref (refresh falhou?): HONESTO no log — o
        // ficheiro está no projeto e aplica-se depois pelo seletor
        elog::warn("import: '%s' NÃO aplicado — fora do catálogo (aplique "
                   "pelo seletor do Inspector)",
                   g_applyAsk.rel.c_str());
    }
    g_applyAsk.open = false;
    g_editor.applyAsk = false;   // 0.8.7: as DUAS juntas — um stuck aqui é
                                 // um modal INVISÍVEL para sempre (backdrop
                                 // opaco sobre o editor = "engine travada")
}

// ---- F5.2: All Files Access — import/export por File API direta -------------

// forward: usado pelo handler de retorno e pelas tentativas
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
        // 0.8.7: retoma COMO O TOQUE original — o NAVEGADOR abre (as duas
        // flags; o mesmo caminho do attemptImport concedido, não o scan
        // antigo — o dono escolhe de ONDE for, galeria incluída)
        browserOpen(std::string(fileapi::kExternalRoot) + "/Download");
        g_editor.fileBrowser = true;
        elog::info("import: navegador ABERTO pos-concessao — retoma do "
                   "dialogo (flag fileBrowser=1)");
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

// 0.8.7: openImportScan (varredura Download/Documents + overlay de
// candidatos) foi REMOVIDO — o import é o NAVEGADOR 0.7.2 desde sempre no
// caminho concedido; a retoma pós-diálogo abre o MESMO navegador (o
// overlay de candidatos e o importCandidate ficam p/ o histórico dos
// testes do browser).

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
    elog::info("import: toque no botão (projeto=%d granted-? a verificar)",
               g_projectReady ? 1 : 0);
    if (!g_projectReady) {
        showToast("sem projeto — import indisponível");
        elog::warn("import: SEM projeto — botão não faz nada (crie/abra um)");
        return;
    }
    bool supported = false;
    if (storageGrantedNow(&supported)) {
        // 0.8.7 (fix do wiring MORTO do C33 — "o botão Import não abre
        // nada"): o browserOpen põe g_browser.open mas o OVERLAY só desenha
        // com g_editor.fileBrowser TAMBÉM a true (gate do frame:
        // `g_editor.fileBrowser && g_browser.open`). Sem a segunda flag o
        // navegador "abria" INVISÍVEL — o toque chegava ao handler, o
        // estado abria, nada aparecia. As DUAS juntas, SEMPRE (o outro
        // call-site do seletor de textura já fazia assim — era o único
        // caminho que funcionava).
        browserOpen(std::string(fileapi::kExternalRoot) + "/Download");
        g_editor.fileBrowser = true;
        elog::info("import: navegador ABERTO (Download) — flag fileBrowser=1 "
                   "(o overlay desenha neste frame)");
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
        elog::info("fileapi: import %s (%zu bytes) de %s", rel.c_str(),
                   bytes.size(), c.path.c_str());
        // 0.8.5: MESMO wiring do browserImportFile — aplicar-após-import
        // com o diálogo (as DUAS flags: g_applyAsk E g_editor.applyAsk; era
        // o 2.º caminho de import que também nunca perguntava)
        Tic* tsel = g_scene.get(g_editor.selected);
        if (tsel && tsel->getComponent<MeshRenderer>()) {
            g_applyAsk.open = true;
            g_applyAsk.kind = c.kind;
            g_applyAsk.rel = rel;
            g_applyAsk.fileName = c.name;
            g_editor.applyAsk = true;
        } else {
            char msg[96];
            std::snprintf(msg, sizeof(msg), "importado: %s", rel.c_str());
            showToast(msg);
        }
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
    g_voni.playEnabled = true;   // 0.9.2: scripts arrancam (autoPlay)
    audioEnterPlay();   // 0.8.11: autoplay dos AudioPlayers
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
    audioLeavePlay();   // 0.8.11: o sandbox de áudio também morre
    g_animSystem.enabled = false;   // 0.8.0: animação só avança em Play
    g_voni.playEnabled = false;   // 0.9.2: runs de Play morrem (as do
    g_voni.stopPlayRuns();        // editor Run continuam até o Stop)
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
            // 0.8.11: backend de áudio (AAudio; fallback AudioTrack) — o
            // misturador já vive no callback desde o arranque
            audioBackendBoot();
            // F5-A/3: recarrega a CENA ATIVA do projeto (refs relativos
            // intactos; resolvers de mesh chegam na F5-E — por agora o
            // LoadCtx liga o cubo procedural, tag "cube" das cenas antigas)
            if (g_projectReady) {
                // 0.8.12 — A SELEÇÃO SOBREVIVE ao ciclo de lifecycle: o
                // reload re-cria os TICs com handles NOVOS (o handle antigo
                // morre); guardamos o NOME do TIC selecionado ANTES do load
                // e RE-VALIDAMOS/RE-MAPEAMOS depois (o Inspector deixava a
                // seleção morrer silenciosamente — o dono re-selecionava a
                // cada fundo/recents do Android).
                char selName[64] = {0};
                if (const Tic* selTic = g_scene.get(g_editor.selected)) {
                    std::snprintf(selName, sizeof(selName), "%.60s",
                                  selTic->name.c_str());
                }
                primMeshesToGrave();   // 0.8.10: posse antiga p/ cova (boot)
                const SceneSerializer::LoadCtx ctx = makeLoadCtx();
                if (g_project.loadActiveScene(*g_storage, g_scene, ctx)) {
                    postLoadMigrateAndFixup();   // 0.8.10: migração silenciosa
                    // 0.8.12 — RE-VALIDA o handle (re-mapeia por nome se o
                    // reload re-criou os TICs; mantém se o handle vivo).
                    // O ELEMENTO de UI selecionado não sobrevive (o índice
                    // pós-reload pode apontar outro elemento — reset honesto;
                    // o TIC continua selecionado).
                    const Handle revalidated = editor::revalidateSelection(
                        g_scene, g_editor.selected, selName);
                    if (revalidated.valid() &&
                        revalidated != g_editor.selected) {
                        g_editor.selected = revalidated;
                        elog::info("lifecycle: seleção re-validada pós-INIT "
                                   "WINDOW (re-mapeada por nome '%s')",
                                   selName);
                    } else if (!revalidated.valid() &&
                               g_editor.selected.valid()) {
                        elog::info("lifecycle: seleção pós-INIT WINDOW "
                                   "dispensada (TIC '%s' não existe na cena "
                                   "recarregada)", selName);
                        g_editor.selected = Handle::invalid();
                    }
                    g_editor.selElement = -1;
                    elog::info("[boot 6/6] scene OK → editor ('%s', %u tics)",
                               g_project.activeScenePath()->c_str(), g_scene.count());
                } else {
                    elog::error("[boot 6/6] scene FALHOU ('%s') — editor arranca com cena vazia",
                                g_project.activeScenePath()->c_str());
                }
                // FASE 9 (G0-1): o EDITOR DE SCRIPT/janela de texto abertos
                // sobrevivem ao ciclo — o HANDLE do TIC dono é re-validado
                // por NOME (o reload re-criou os TICs) e o IME é RE-PEDIDO
                // (o showSoftInput do arranque foi contra a janela
                // PRÉ-rotação — a rotação é o próprio TERM→INIT; sem isto o
                // teclado não voltava e o editor parecia morto)
                if (g_editor.scriptWin.open) {
                    if (editor::scriptwin::revalidateTic(g_editor.scriptWin,
                                                         g_scene)) {
                        elog::info("lifecycle: editor de script re-validado "
                                   "pós-INIT WINDOW (TIC '%s' por nome)",
                                   g_editor.scriptWin.ticName);
                    }
                    storage::jniImeShow();
                    elog::info("lifecycle: IME re-pedido pós-INIT (editor de "
                               "script aberto)");
                }
                if (g_editor.textWin.open) {
                    storage::jniImeShow();
                }
            } else {
                elog::warn("[boot 6/6] scene SEM PROJETO — editor sem persistência");
            }
            g_ready = true;
            logBootMemory("fim do boot");
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
        case APP_CMD_PAUSE:
            // FASE 9 (G1-5): flush do debounce ao sair para segundo plano
            // (o write pendente NÃO se perde)
            if (g_layoutSaveTimer > 0.0f) {
                g_layoutSaveTimer = 0.0f;
                saveLayoutNow("saída para segundo plano");
            }
            // 0.8.11 — o áudio segue o lifecycle da activity: fundo =
            // PAUSA do stream (AAudio/AudioTrack pausam DEBAIXO da mesma
            // interface; o misturador NÃO esquece vozes — o resume continua
            // de onde estava)
            if (g_audioOut && g_audioBackendReady) {
                g_audioOut->pause();
                elog::info("audio: PAUSE (activity em fundo)");
            }
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
            // 0.8.11 — o áudio ACORDA com a activity (o pause do fundo
            // parou o stream; vozes/cursor ficaram intactos)
            if (g_audioOut && g_audioBackendReady) {
                g_audioOut->resume();
                elog::info("audio: RESUME (activity de volta)");
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
                primMeshesDestroyAll();      // 0.8.10: prims SEM cache (posse)
                g_grid.destroy();
                g_renderer.shutdown();      // programa UI + VAO/VBO + whiteTex + lit
                if (g_audioOut) {           // 0.8.11: áudio sai com o contexto
                    g_audioOut->stop();
                    g_audioBackendReady = false;
                }
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
        // 0.8.2 (F7): mesh SKINADO com esqueleto → matrizes de skin ao
        // shader (a pose vive nos joints; o TRS do TIC continua a ser o
        // model matrix — o esqueleto é RELATIVO ao TIC)
        const SkeletonComp* sk = comps.skeletons().find(mrs.owner(i));
        Mat4 bones[SkeletonComp::kMaxBones];
        u32 nBones = 0;
        if (sk && mr.mesh->skinned()) {
            nBones = computeSkinMatrices(*sk, bones, SkeletonComp::kMaxBones);
        }
        st = st + g_renderer.drawMesh(*mr.mesh, model, vp, mr.texture, mr.tint,
                                      nBones > 0 ? bones : nullptr, nBones);
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
    // 0.9.0 (spec M): BOTTOM-CENTER, ≥48dp de altura, SURFACE-2 com raio
    // 8dp, auto-3s (g_toastT já é o relógio de 3 s de sempre), text-1.
    const f32 tw = g_ui.fontWidth(toastFit);
    const f32 bw = tw + 48.0f;
    const f32 bh2 = bh < 48.0f ? 48.0f : bh;
    const f32 bx = ox + (aw - bw) * 0.5f;
    // por CIMA da tab bar do painel de baixo (spec E) — 16dp de folga
    const f32 by = oy + ah - bh2 - safe::kBottomTabH - safe::kStatusH - 16.0f;
    const f32 bg[4] = {theme::kTheme.surface2[0], theme::kTheme.surface2[1],
                       theme::kTheme.surface2[2], 0.95f * alpha};
    const f32 tx[4] = {theme::kTheme.text1[0], theme::kTheme.text1[1],
                       theme::kTheme.text1[2], alpha};
    g_ui.panelRounded(bx, by, bw, bh2, theme::kRadiusCard, bg);
    g_ui.label(bx + 24.0f, by + (bh2 - g_ui.textMetrics().block()) * 0.5f +
                                g_ui.textMetrics().ascent,
               toastFit, tx);
    (void)bh;
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

// 0.9.0 (spec F) — captura da MINIATURA no fim do frame de um save.
// Ponto SEGURO: depois do endFrame (frame completo no backbuffer, nada em
// vias de desenho) e antes do swap (o conteúdo ainda está no buffer traseiro
// — depois do swap o conteúdo é indefinido por definição EGL). Lê SÓ a
// viewport central (a área 3D entre os painéis — SEM chrome; em Play, o
// contentRect inteiro), crop 16:9 centrado, downsample box ≤480 e PNG para
// thumb.png na raiz do projeto (ProjectStorage stream — SAF no device).
static void captureThumbIfPending(f32 w, f32 h) {
    if (!g_thumbPending) {
        return;
    }
    g_thumbPending = false;
    if (!g_projectReady || !g_storage) {
        return;
    }
    const UiRect r = g_editor.playMode
        ? safe::contentRect(w, h, g_ui.safeArea())
        : safe::centerRect(w, h, g_ui.safeArea(), currentDrawerH(),
                           g_editor.showInspector);
    const u32 rw = static_cast<u32>(r.w);
    const u32 rh = static_cast<u32>(r.h);
    if (rw < 16 || rh < 16) {
        return;   // viewport degenerado (transição/lifecycle) — sem thumb
    }
    std::vector<u8> rgba(static_cast<size_t>(rw) * rh * 4u);
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT 0x0D05   // stub do CI não define (GLES3 real define)
#endif
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(static_cast<GLint>(r.x),
                 static_cast<GLint>(h - r.y - r.h),
                 static_cast<GLsizei>(rw), static_cast<GLsizei>(rh),
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    thumb::flipVerticalRgba(rgba.data(), rw, rh);   // bottom-up → top-down
    const thumb::CropRect c = thumb::crop169(rw, rh);
    const u32 tw = thumb::targetWidth(c.w);
    if (tw == 0 || c.w == 0) {
        return;
    }
    const u32 th = static_cast<u32>((u64)c.h * tw / c.w);
    if (th == 0) {
        return;
    }
    std::vector<u8> rgb(static_cast<size_t>(tw) * th * 3u);
    thumb::downsampleRgb(rgba.data() + ((size_t)c.y * rw + c.x) * 4u,
                         c.w, c.h, tw, th, rgb.data());
    const std::vector<u8> png = thumb::encodePngRgb(rgb.data(), tw, th);
    if (png.empty()) {
        elog::error("thumb: encode PNG falhou (%ux%u)", tw, th);
        return;
    }
    const int hs = g_storage->openWriteStream("thumb.png");
    if (hs <= 0) {
        elog::error("thumb: openWriteStream thumb.png falhou");
        return;
    }
    if (!g_storage->writeStreamChunk(hs, png.data(), png.size())) {
        elog::error("thumb: escrita falhou (%zu B)", png.size());
        g_storage->closeWriteStream(hs);
        return;
    }
    g_storage->closeWriteStream(hs);
    elog::info("thumb: %ux%u PNG (%zu B) — captura da viewport no save",
               tw, th, png.size());
}

void frame() {
    const f32 w = static_cast<f32>(g_egl.width());
    const f32 h = static_cast<f32>(g_egl.height());

    // 0.9.0 (spec E): o estado do drawer alimenta os rects dos painéis/
    // viewport neste frame (a fonte é o BottomState — persistente)
    g_editor.drawerH = currentDrawerH();

    // 0.8.10 — O PONTO SEGURO DO FRAME (antes de QUALQUER submissão GL):
    //   (1) COVA: liberta os meshes de prim reformados no frame ANTERIOR
    //       (deferred free — os seus comandos já saíram);
    //   (2) FLUSH: sobe os pedidos pendentes (pick/params/load/preset) pelo
    //       caminho ÚNICO gera→valida→upload→self-check→bind.
    // Aqui NADA foi desenhado ainda neste frame — um upload nunca colide
    // com draws em curso, e um glDelete* nunca apanha buffers em voo.
    primGraveDig();
    primFlushPending();

    // 0.8.10 — IMPORT JOB: o worker sinalizou done? join + finalize
    // (catálogo/diálogo) AQUI, na thread da UI — nunca no worker.
    if (g_importJob.active.load() && g_importJob.done.load()) {
        importJobFinish();
    }

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
    // 0.9.6 (G1-3): e NÃO com overlay aberto — o DRAW já era gated pelo
    // modalOpen desde 0.7.5 mas o INPUT não: com o Settings/Docs/editor
    // abertos, tocar onde o gizmo ESTARIA arrastava o TIC por trás (a
    // "cena mexe-se por trás" do relatório do dono). Input alinhado com
    // o draw — a regra da casa: não desenhado = não interativo.
    u32 gizmoClaimed = 0;
    Tic* gizmoTic = g_scene.get(g_editor.selected);
    Transform3D* gizmoTr =
        (gizmoTic && gizmoTic->active) ? gizmoTic->getComponent<Transform3D>()
                                        : nullptr;
    if (gizmo::visible(g_editor.playMode || g_editor.uiMode, gizmoTr != nullptr) &&
        !editor::anyOverlayOpen(g_editor)) {
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
    // 0.9.0: a timeline vive no DRAWER (aba Animação) — o viewport central
    // encolhe pelo drawer (currentDrawerH), não pela strip antiga
    UiRect viewRect = editor::centerRect(w, h, g_ui.safeArea(),
                                         currentDrawerH(),
                                         g_editor.showInspector);
    (void)tlVisible;

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
        g_editor.playMode || g_editor.uiMode || g_editor.audioMode ||
            // 0.9.6 (G1-3): overlay aberto = o toque pertence AO overlay —
            // o orbit da câmara por trás morria AQUI (a cena mexia-se por
            // trás do Settings/Docs/editor; o draw do chrome já era gated
            // pelo anyOverlayOpen, o input não)
            editor::anyOverlayOpen(g_editor));

    // 0.7.0 — DESSELECCIONAR: tap parado no vazio do viewport 3D limpa a
    // seleção (só em editor 3D; o modo UI desseleciona o ELEMENTO no
    // drawUiViewport, e a Hierarchy trata do seu vazio)
    // 0.7.7 — o MESMO tap pode ter acertado numa câmara: nesse caso
    // SELECIONA o TIC dela. 0.7.10 — PRIORIDADE DE OBJETOS + hit-test
    // RESTRITO: o picker testa primeiro os TICs SELECIONÁVEIS (meshes,
    // centro projetado a 44 px) e SÓ DEPOIS a câmara (CORPO/LENTE apenas
    // — tocar no cone vazio não seleciona nem bloqueia o orbit).
    // 0.8.12 — FIX DA PERDA DE SELEÇÃO DO C33: o deselect NÃO corre com
    // overlays abertos nem no workspace de ÁUDIO. O overlay dos pickers é
    // CENTRADO no viewport — o tap na LINHA do picker ou no backdrop caía
    // DENTRO do viewRect, o deselect armava no press e LIMPAVA a seleção no
    // release DO MESMO FRAME do pick (antes do dispatch, que via seleção
    // morta e logava "ERRO(sem TIC com mesh selecionado)" com de="-" — a
    // evidência exata dos logs 0.8.5/0.8.7/0.8.9/0.8.10). Os botões da UI
    // não reclamam o slot de input externo — o guard é AQUI, no chamador.
    // FASE 9 (G1-6): a seleção ANTES do tap (viewportTapClearsSelection
    // limpa DENTRO — o log do motivo precisa do valor de antes)
    const Handle selAntesTap = g_editor.selected;
    if (!g_editor.playMode && !g_editor.uiMode && !g_editor.audioMode &&
        !editor::anyOverlayOpen(g_editor)) {
        if (editor::viewportTapClearsSelection(
                g_editor, g_input, viewRect,
                claimed | gizmoClaimed)) {
            const Mat4 tapVp = Mat4::mul(g_camera.proj(w / h), g_camera.view());
            f32 px = 0.0f, py = 0.0f;
            g_input.pos(0, px, py);
            const Handle hc =
                camgizmo::pickSceneTic(g_scene, tapVp, w, h, px, py);
            if (hc.valid()) {
                if (hc != selAntesTap) {
                    const Tic* nt = g_scene.get(hc);
                    elog::info("seleção: TIC '%s' (toque no viewport)",
                               nt ? nt->name.c_str() : "?");
                }
                g_editor.selected = hc;
            } else {
                // FASE 9 (G1-6): CADA mudança de seleção LOGA o motivo
                // (selAntesTap: o handle JÁ foi limpo lá dentro)
                if (selAntesTap.valid()) {
                    const Tic* velho = g_scene.get(selAntesTap);
                    elog::info("seleção limpa: toque no vazio do viewport "
                               "(era '%s')",
                               velho ? velho->name.c_str() : "ID órfão");
                }
                g_editor.selected = Handle::invalid();
                g_editor.selElement = -1;
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
    // 0.8.11 — o coração de áudio por frame: temporizador da gravação,
    // preview do workspace (fim natural do clip), preview do INSPECTOR
    // (o flag do AudioPlayer → misturador) e o listener posicional
    audioRecTick();
    audioPreviewTick();
    if (Tic* selTic = g_scene.get(g_editor.selected)) {
        audioPreviewTick(*selTic);
    }
    audioUpdateListener();   // posicional segue a câmara/TICs vivos

    // ---- pass 3D: clear color+depth, TICs com MeshRenderer + grid com fade
    // 0.7.7 — CÂMARA DE JOGO: em Play a cena renderiza pela câmara ATIVA
    // (pose do Transform3D + parâmetros do CameraComp); sem câmara ativa o
    // fallback é a orbit de edição. O EDITOR mantém a orbit SEMPRE (o
    // frustum é que é o gizmo — nunca o render).
    // 0.8.9 (ESPAÇO SEM TETOS): near/far DINÂMICOS por frame, derivados do
    // ZOOM e do AABB da CENA (distância ao ponto mais longe + folga) — o far
    // CONTÉM sempre a cena (objetos gigantes/longe nunca mais clipam) e o
    // near segue o zoom (rácio saudável a qualquer escala). Recalculado por
    // frame = cobre edição E load por construção.
    Mat4 view;
    Mat4 proj;
    Vec3 camEye;
    f32  camFocus;
    Vec3 sceneMn, sceneMx;
    f32  sceneRadius = 0.0f;
    camerautil::sceneAABB(g_scene, sceneMn, sceneMx, sceneRadius);
    if (gameCamTr && gameCamComp) {
        // Play: far efetivo = max(farZ do dono, olho→mais-longe+10) — o
        // slider do dono é PISO, nunca teto; near com defesa de rácio
        // (≤ 100 000:1 contra z-fighting a distâncias enormes)
        CameraComp playCam = *gameCamComp;
        playCam.farZ = camerautil::playFar(
            *gameCamComp,
            camerautil::aabbFarthestDist(gameCamTr->pos, sceneMn, sceneMx));
        playCam.nearZ = camerautil::playNear(*gameCamComp, playCam.farZ);
        view = camgizmo::gameView(*gameCamTr);
        proj = camgizmo::gameProj(playCam, w / h);
        camEye = gameCamTr->pos;
        camFocus = length(gameCamTr->pos);   // fade do grid: dist. ao centro
    } else {
        f32 clipNear = Camera::kDefaultNear, clipFar = Camera::kDefaultFar;
        camerautil::editorClips(
            g_camera.dist, camerautil::sceneFarthest(sceneMn, sceneMx),
            clipNear, clipFar);
        g_camera.setClips(clipNear, clipFar);
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

    // 0.8.10 — IMPORT EM CURSO: overlay MODAL (o resto da UI não desenha —
    // o job é a única coisa que acontece; cancelar é a única ação)
    if (g_importJob.active.load()) {
        const f32 ox = g_ui.safeLeft();
        const f32 oy = g_ui.safeTop();
        const f32 aw = w - ox - g_ui.safeRight();
        const f32 ah = h - oy - g_ui.safeBottom();
        const f32 pw = 420.0f;
        const f32 ph = 172.0f;
        const f32 x = ox + (aw - pw) * 0.5f;
        const f32 y = oy + (ah - ph) * 0.5f;
        g_ui.panel(x, y, pw, ph, theme::PANEL);
        g_ui.frame(x, y, pw, ph, 2.0f, theme::ACCENT);
        const f32 th = g_ui.fontHeight();
        g_ui.label(x + 14.0f, y + 40.0f + th * 0.3f, "IMPORT...",
                   theme::TEXT);
        char fname[72];
        std::snprintf(fname, sizeof(fname), "%s",
                      g_importJob.fileName.c_str());
        g_ui.labelFitted(x + 14.0f, y + 64.0f + th * 0.3f, fname, theme::LINE,
                         pw - 28.0f);
        const u64 doneB = g_importJob.bytesDone.load();
        const u64 totalB = g_importJob.bytesTotal.load();
        char bytes[64];
        std::snprintf(bytes, sizeof(bytes), "%llu / %llu MB",
                      static_cast<unsigned long long>(doneB / (1024 * 1024)),
                      static_cast<unsigned long long>(totalB / (1024 * 1024)));
        g_ui.label(x + 14.0f, y + 88.0f + th * 0.3f, bytes, theme::TEXT);
        // barra de progresso (trilho + preenchimento ACCENT)
        const f32 bx = x + 14.0f;
        const f32 bw = pw - 28.0f;
        g_ui.panel(bx, y + 112.0f, bw, 12.0f, theme::LINE);
        const f32 frac = totalB > 0
            ? static_cast<f32>(doneB) / static_cast<f32>(totalB)
            : 0.0f;
        if (frac > 0.0f) {
            g_ui.panel(bx, y + 112.0f, bw * (frac > 1.0f ? 1.0f : frac),
                       12.0f, theme::ACCENT);
        }
        if (g_ui.button(0x81010, x + 14.0f, y + ph - 52.0f, pw - 28.0f, 40.0f,
                        "cancelar")) {
            g_importJob.cancel.store(true);
            elog::info("import: cancelamento pedido (o job para no próximo "
                       "chunk)");
        }
        drawToast();
        g_ui.endFrame();
        g_lastUiStats = g_renderer.endFrame();
        g_egl.swap();
        g_input.clearEdges();
        return;   // MODAL: nada mais desenha/processa este frame
    }

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
    // ---- 0.9.0 — CHROME DE CIMA (spec D) ---------------------------------
    // TOP BAR 56dp [Menu ≡][Cena ▾]·[pause][play][sliders]·[gear] + TAB BAR
    // 48dp [3D][UI|ÁUDIO] com underline accent. O G4 (transformação)
    // desceu para a TOOLBAR DO VIEWPORT (vpchrome — desenhada abaixo).
    if (!modalOpen) {
        const editor::toolbar::Actions ta = editor::toolbar::draw(g_ui,
                                                                  g_editor);
        if (ta.menuDropdown) {
            // Menu → dropdown (Settings/Guardar/…/Sair)
            g_editor.fileMenu = !g_editor.fileMenu;
            g_editor.plusMenu = false;
            g_editor.settingsMenu = false;
        }
        if (ta.cenaDropdown) {
            // Cena → dropdown de cenas do projeto (overlay CENAS)
            g_editor.scenesMenu = !g_editor.scenesMenu;
            g_editor.plusMenu = false;
            g_editor.settingsMenu = false;
            g_editor.fileMenu = false;
        }
        if (ta.gearPressed) {
            // 0.9.0 (spec I): o gear abre a PÁGINA de Settings
            g_editor.settingsMenu = true;
            elog::info("ui: pagina de settings aberta (gear)");
        }

        // ---- 0.9.0 — CHROME DO VIEWPORT (spec D): stack + toolbar inf. +
        // triad. SÓ em modo 3D (UI/ÁUDIO substituem o viewport).
        if (!g_editor.uiMode && !g_editor.audioMode) {
            editor::vpchrome::ChromeState cs;
            cs.canUndo = g_undo.canUndo();
            cs.canRedo = g_undo.canRedo();
            cs.canPaste = g_clipValid;
            cs.snapValue = g_snapValue;
            const editor::vpchrome::Actions va = editor::vpchrome::draw(
                g_ui, g_editor, g_gizmoMode, cs, g_camera, currentDrawerH());
            if (va.undoPressed) {
                const Handle uh = g_undo.undo(g_scene);
                if (uh.valid()) {
                    g_editor.selected = uh;
                    showToast("desfeito");
                } else {
                    showToast("nada a desfazer");
                }
            }
            if (va.redoPressed) {
                const Handle rh = g_undo.redo(g_scene);
                if (rh.valid()) {
                    g_editor.selected = rh;
                    showToast("refeito");
                } else {
                    showToast("nada a refazer");
                }
            }
            if (va.savePressed && g_projectReady) {
                // o MESMO caminho do "Guardar cena" do menu (assets+manifesto)
                const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                                g_project.saveManifest(*g_storage);
                g_thumbPending = true;
                showToast(ok ? "cena salva" : "falha ao salvar");
            }
            if (va.dupPressed) {
                // DUPLICAR o selecionado (e ARMA a área de transferência —
                // o paste cola o MESMO snapshot depois)
                if (const Tic* sel = g_scene.get(g_editor.selected)) {
                    g_clipSnap = editor::snapTic(g_scene, sel->handle);
                    g_clipValid = true;
                    const Handle dup =
                        editor::duplicateTic(g_scene, g_editor.selected);
                    if (dup.valid()) {
                        g_editor.selected = dup;
                        showToast("TIC duplicado");
                    }
                } else {
                    showToast("seleciona um TIC para duplicar");
                }
            }
            if (va.pastePressed) {
                if (g_clipValid) {
                    const editor::TicSnap before;   // vazio = criação
                    const Handle h = editor::pasteAsNew(g_scene, g_clipSnap);
                    if (h.valid()) {
                        g_undo.push(before, editor::snapTic(g_scene, h), h);
                        g_editor.selected = h;
                        showToast("colado");
                    }
                } else {
                    showToast("nada na área de transferência");
                }
            }
            if (va.addTicPressed) {
                g_editor.plusMenu = !g_editor.plusMenu;
            }
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
        // 0.8.11 — WORKSPACE ÁUDIO no MESMO sítio do viewport 2D (o rect
        // do editor de UI: entre toolbar e timeline, sem o painel direito
        // quando visível — NUNCA a área toda; painéis/toolbar ficam nos
        // seus sítios e o frame segue o fluxo normal)
        if (g_editor.audioMode) {
            // 0.9.0: o workspace encolhe pelo DRAWER aberto (a timeline na
            // aba Animação já não rouba altura ao viewport)
            UiRect audioView = editor::centerRect(w, h, g_ui.safeArea(),
                                                  currentDrawerH(),
                                                  g_editor.showInspector);
            editor::AudioWorkspaceHost host = makeAudioWorkspaceHost();
            const int pickWs = editor::drawAudioWorkspace(
                g_ui, g_input, audioView, g_audioWs, g_audioCatalog, host);
            if (pickWs > 0) {
                g_audioWs.selected = static_cast<u32>(pickWs - 1);
                audioPreviewStop();   // trocar de clip mata o preview anterior
            }
        }
    }

    // F5.1-hotfix (1.4) + F5.2: overlay Settings — Exportar logs / Ver logs /
    // 0.9.0 (spec I) — PÁGINA DE SETTINGS (full-screen, secções
    // colapsáveis, back 56dp; a seleção do editor fica INTACTA)
    if (g_editor.settingsMenu) {
        const char* modeText = g_perm.mode() == storage::Mode::Unknown
                                   ? ""
                                   : storage::modeLabel(g_perm.mode());
        char verLine[48];
        std::snprintf(verLine, sizeof(verLine), "%s (vc %u)",
                      buildinfo::g_version, buildinfo::g_versionCode);
        editor::settings::Ctx sctx;
        sctx.version = verLine;
        sctx.soSha = buildinfo::g_soSha;
        sctx.storageMode = modeText;
        sctx.keepSource = g_keepSource;
        sctx.audioMaster = g_audioMaster;
        sctx.immersive = g_immersive;
        sctx.allFilesGranted = g_perm.mode() == storage::Mode::AllFiles;
        sctx.micGranted = g_micGranted;
        sctx.dumpCount = static_cast<u32>(g_logDumps.size());
        switch (editor::settings::draw(g_ui, g_input, g_editor, sctx)) {
            case editor::settings::kResetLayout: {
                // Repor layout (spec G): defaults + PERSISTE já
                g_bottom.bottomTab = 0;
                g_bottom.drawerH = safe::kDrawerDef;
                g_editor.showInspector = true;
                g_editor.inspCollapsed = 0;
                g_editor.settingsCollapsed = 0;
                saveLayoutNow("repor layout");
                showToast("layout reposto");
                break;
            }
            case editor::settings::kToggleImmersive: {
                g_immersive = !g_immersive;
                applyImmersiveMode();
                saveLayoutNow("imersivo");
                showToast(g_immersive ? "imersivo: sim" : "imersivo: não");
                elog::info("ui: modo imersivo = %s",
                           g_immersive ? "on" : "off");
                break;
            }
            case editor::settings::kAllFilesPressed: {
                g_perm.dialogAccept();
                if (storage::jniOpenAllFilesSettings()) {
                    elog::info("storage: definicoes All Files lancadas");
                } else {
                    g_perm.dialogCancel();
                    showToast("ponte Java indisponivel (handshake)");
                }
                break;
            }
            case editor::settings::kMicPressed: {
                if (storage::jniEnsureMicPermission()) {
                    showToast("permissao de microfone pedida");
                } else {
                    showToast("ponte Java indisponivel (handshake)");
                }
                break;
            }
            case editor::settings::kViewLogs: {
                g_editor.logViewer = true;
                g_editor.logViewerJustOpened = true;
                g_logLines.clear();
                elog::readTail(g_logLines, 300);
                refreshLogDumps();
                break;
            }
            case editor::settings::kExportLogs: {
                int copied = 0;
                if (storage::jniExportLogsToDownloads(&copied) && copied >= 0) {
                    showToast("logs exportados");
                    elog::info("logs: exportados %d ficheiro(s)", copied);
                } else {
                    showToast("export falhou (sem ficheiros? API<29?)");
                }
                break;
            }
            case editor::settings::kProbeAudio: {
                runAudioProbe();
                break;
            }
            case editor::settings::kOpenTextWindow: {
                // 0.9.1 — o Settings fecha e a janela de texto fica COMO
                // modal único (sem duplo-dispatch do mesmo toque nos dois
                // botões back — o back da janela e o do settings são ambos
                // 56dp no canto superior esquerdo)
                g_editor.settingsMenu = false;
                openTextWindow();
                break;
            }
            case editor::settings::kOpenDocs: {
                // 0.9.2 §11 — Settings fecha; o ecrã de Docs fica como
                // modal único (o mesmo padrão do kOpenTextWindow)
                g_editor.settingsMenu = false;
                g_editor.docsScreen.open = true;
                g_editor.docsScreen.queryLen = 0;
                g_editor.docsScreen.query[0] = '\0';
                g_editor.docsScreen.expanded = -1;
                elog::info("voni: docs abertas");
                break;
            }
            case editor::settings::kReconvert: {
                reconvertAllAssets();
                break;
            }
            default:
                break;
        }
    }

    // F5.2: DIÁLOGO/IMPORT/LOGS desenhados DEPOIS dos painéis (ordem =
    // z-order) — ver o bloco imediatamente antes de drawToast()

    // F5-E: catálogo dos seletores — refresh quando um seletor ABRE
    // 0.8.11: idem o catálogo de ÁUDIO (o seletor de clips é o assetMenu 5)
    if (g_editor.assetMenu != 0 && g_editor.assetMenu != g_prevAssetMenu) {
        refreshCatalog();
        refreshAudioCatalog();
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
            editor::drawInspector(g_ui, g_scene, g_editor, &g_catalog,
                                  &g_voni);   // 0.9.2: +vars @+ do script
        }
        // 0.9.0 (spec E/K): a TIMELINE vive no DRAWER do painel de baixo
        // (aba Animação) — a strip de fundo morreu. Com player presente e
        // drawer fechado, a aba AUTO-ABRE (o comportamento "abre sozinha"
        // da 0.8.0 manteve-se — agora abre o drawer certo)
        if (tlVisible && g_bottom.bottomTab == 0) {
            g_bottom.bottomTab = 3;
        }
        if (g_bottom.bottomTab == 3 && tlVisible) {
            const UiRect d = editor::bottom::layout(
                w, h, g_ui.safeArea(), g_bottom).drawer;
            timeline::drawTimelineInRect(
                g_ui, g_input, g_scene, g_editor, g_timeline, g_frameDt,
                {d.x, d.y + 12.0f, d.w, d.h - 12.0f});
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
                    showToast("Câmara criada (ativa)");
                    LOGI("editor: TIC de câmara criada (ativa)");
                }
            } else if (choice == 6) {
                // 0.8.0 (F7) — TIC "Mesh": Transform+MeshRenderer com a
                // PRIMITIVA esfera default (SEM física — prototipagem pura;
                // troca-se o tipo/params no Inspector, anima-se na timeline)
                // 0.8.10: esfera default PENDENTE — o preset arma primOn+
                // params + primPending; o mesh sobe no ponto seguro
                const Handle hnew = createTicUndoable(PresetKind::Mesh);
                if (hnew.valid()) {
                    if (MeshRenderer* mr =
                            g_scene.get(hnew)->getComponent<MeshRenderer>()) {
                        mr->primPending = true;
                    }
                }
                if (hnew.valid()) {
                    g_editor.selected = hnew;
                    showToast("Mesh criado (esfera)");
                    LOGI("editor: TIC Mesh criado (primitiva esfera default)");
                }
            } else if (choice == 7) {
                // 0.8.11 — TIC "Audio": Transform + AudioPlayer (ESTRUTURA
                // primeiro — o clip vem pelo Inspector/seletor de clips;
                // glifo de altifalante no editor, som no Play)
                const Handle hnew = createTicUndoable(PresetKind::Audio);
                if (hnew.valid()) {
                    g_editor.selected = hnew;
                    showToast("Audio criado (atribua o clip no Inspector)");
                    LOGI("editor: TIC Audio criado (Transform+AudioPlayer)");
                }
            } else {
                const PresetKind kind = static_cast<PresetKind>(choice - 1);
                const Handle hnew = createTicUndoable(kind);
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
    // 0.8.12 — HINT de pick bloqueado: o Inspector seta o flag quando o
    // toque em linha de picker não tinha alvo válido; o main converte-o em
    // toast + linha de log AQUI (o toast vive no main — o Inspector é puro)
    if (g_editor.pickBlockedHint) {
        g_editor.pickBlockedHint = false;
        showToast("seleciona um TIC com mesh");
        elog::info("ui: pick bloqueado (sem seleção)");
    }

    // FASE 9 (G2-7): o LONG-PRESS no nome truncado da hierarquia pediu o
    // nome completo — o main converte em toast (o Inspector é puro)
    if (g_editor.nameTip[0]) {
        showToast(g_editor.nameTip);
        g_editor.nameTip[0] = '\0';
    }

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
            } else if (menuKind == 5) {
                // 0.8.11 — seletor de CLIPS: o "importar…" abre o navegador
                // na raiz Music (o import de áudio volta ao workspace); o
                // pick aplica clipPath ao AudioPlayer (dados puros)
                if (pick == editor::kAssetPickImport) {
                    browserOpen("/storage/emulated/0/Music");
                    g_editor.fileBrowser = true;
                    g_editor.audioMode = true;   // volta ao workspace
                    LOGI("editor: importar audio — navegador aberto (Music)");
                } else {
                    const editor::TicSnap uBefore =
                        editor::snapTic(g_scene, g_editor.selected);
                    const editor::AssetPickOutcome out =
                        editor::applyAssetPick(g_scene, g_editor.selected, 5,
                                               pick, g_catalog,
                                               makeAssetResolvers());
                    if (out.applied) {
                        pushUndo(uBefore, g_scene.get(g_editor.selected));
                        elog::info("%s", out.log);
                    } else {
                        elog::warn("audio: pick %d sem AudioPlayer no TIC "
                                   "selecionado",
                                   pick);
                    }
                    if (out.toast[0] != '\0') {
                        showToast(out.toast);
                    }
                }
            } else {
                // 0.8.12 — GUARDA DE UI no DISPATCH: os pickers de
                // mesh/prim/tex (menuKind 1/2/4) só aplicam com um TIC
                // VIVO selecionado que tenha MeshRenderer. Sem alvo (o
                // picker ficou aberto e a seleção morreu, ou o TIC é
                // câmara/áudio): HINT + log bloqueado — NUNCA o caminho
                // "ERRO(sem TIC com mesh selecionado)" que o C33 viu
                // (a linha de erro não diz ao dono o que fazer a seguir).
                if (editor::pickerGuardBlocked(g_scene, g_editor.selected)) {
                    showToast("seleciona um TIC com mesh");
                    elog::info("ui: pick bloqueado (sem seleção) — pick %d "
                               "no menu %d ignorado",
                               pick, menuKind);
                } else {
                // 0.8.7 — LOGGING EMBUTIDO da TROCA: "mesh: troca <de>→<para>
                // inicio" antes e "fim ok verts=N idx=M"/"ERRO(<razão>)"
                // depois — o log viewer do C33 mostra a linha exata se algo
                // falhar (a intermitência deixa de ser um mistério).
                const MeshRenderer* mrOld = nullptr;
                if (const Tic* tOld = g_scene.get(g_editor.selected)) {
                    mrOld = tOld->getComponent<MeshRenderer>();
                }
                char de[32] = "-";
                if (mrOld) {
                    if (mrOld->primOn) {
                        std::snprintf(de, sizeof(de), "prim %s",
                                      primName(mrOld->prim.kind));
                    } else if (mrOld->mesh == &g_cubeMesh) {
                        std::snprintf(de, sizeof(de), "cube");
                    } else if (!mrOld->meshPath.empty()) {
                        std::snprintf(de, sizeof(de), "%.24s",
                                      mrOld->meshPath.c_str());
                    } else if (mrOld->mesh) {
                        std::snprintf(de, sizeof(de), "mesh");
                    }
                }
                const char* para = "-";   // destino (menuKind 4 = PrimKind)
                char paraBuf[32];
                if (menuKind == 4) {
                    if (pick == 1) {
                        std::snprintf(paraBuf, sizeof(paraBuf), "none");
                    } else if (pick >= 2 && pick <= 9) {
                        std::snprintf(paraBuf, sizeof(paraBuf), "prim %s",
                                      primName(static_cast<PrimKind>(pick - 2)));
                    } else {
                        std::snprintf(paraBuf, sizeof(paraBuf), "pick %d", pick);
                    }
                    para = paraBuf;
                } else if (menuKind == 1) {
                    // 0.8.12: none=1, cube=2, ficheiros 3+ (labels legíveis
                    // no log — o "mesh pick N" fica p/ os ficheiros)
                    if (pick == 1) {
                        std::snprintf(paraBuf, sizeof(paraBuf), "none");
                    } else if (pick == 2) {
                        std::snprintf(paraBuf, sizeof(paraBuf), "cube");
                    } else {
                        std::snprintf(paraBuf, sizeof(paraBuf), "mesh pick %d",
                                      pick);
                    }
                    para = paraBuf;
                } else if (menuKind == 2) {
                    std::snprintf(paraBuf, sizeof(paraBuf), "tex pick %d",
                                  pick);
                    para = paraBuf;
                }
                elog::info("mesh: troca %s → %s inicio", de, para);
                const editor::TicSnap uBefore =
                    editor::snapTic(g_scene, g_editor.selected);
                const editor::AssetPickOutcome out =
                    editor::applyAssetPick(g_scene, g_editor.selected, menuKind,
                                           pick, g_catalog, makeAssetResolvers());
                if (out.applied) {
                    pushUndo(uBefore, g_scene.get(g_editor.selected));
                }
                // 0.8.7 — as CONTAGENS vêm do mesh APLICADO (o resolver do
                // device devolve meshes REAIS; o applyAssetPick é puro e
                // nunca desreferencia o Mesh — o contrato dos testes)
                const MeshRenderer* mrNew = nullptr;
                if (const Tic* tNew = g_scene.get(g_editor.selected)) {
                    mrNew = tNew->getComponent<MeshRenderer>();
                }
                const Mesh* appliedMesh = mrNew ? mrNew->mesh : nullptr;
                if (out.applied && appliedMesh) {
                    elog::info("mesh: troca %s → %s fim ok verts=%u idx=%u",
                               de, para, appliedMesh->vertexCount(),
                               appliedMesh->indexCount());
                } else if (out.applied) {
                    elog::info("mesh: troca %s → %s fim ok (sem mesh — %s)",
                               de, para,
                               (menuKind == 4 && pick == 1) ? "prim desligado"
                               : (menuKind == 1 && pick == 1)
                                   ? "slot limpo (none)"
                                   : "sem resolver");
                } else {
                    // 0.8.9 — ERRO HONESTO: distinguir "sem alvo" (o TIC
                    // selecionado não tem MeshRenderer — ex.: seleção perdida
                    // pós-restart) de "gerador/upload falhou" (o resolver
                    // devolveu null — a causa exata está nas linhas
                    // "mesh: prim … ERRO(…)" ACIMA). O C33 0.8.7 lia
                    // "gerador/upload falhou" num caso que era SÓ seleção
                    // perdida — o gerador estava bem.
                    elog::error("mesh: troca %s → %s ERRO(%s)", de, para,
                                mrOld ? "gerador/upload falhou — causa acima"
                                      : "sem TIC com mesh selecionado");
                }
                if (out.toast[0] != '\0') {
                    showToast(out.toast);
                }
                if (out.log[0] != '\0') {
                    elog::info("%s", out.log);   // 0.8.9: vai ao engine.log (a prova no log viewer do C33)
                }
                }   // 0.8.12: fim do else do GUARDA (pick aplicado com alvo válido)
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
        const editor::toolbar::TopBarLayout tbl = editor::toolbar::topbarLayout(
            w, h, g_ui.safeArea(), g_editor.uiMode, g_editor.audioMode);
        const int choice = editor::drawFileMenu(g_ui, g_input, w, h, g_editor,
                                                tbl.menu.x, tbl.menu.y + tbl.menu.h);
        if (choice == 1) {
            // 0.7.6: Settings (o item do dropdown do Menu — o botão próprio
            // deixou de existir na barra)
            g_editor.settingsMenu = true;
            elog::info("ui: menu Settings aberto (dropdown do Menu)");
        } else if (choice == 2 && g_projectReady) {
            const bool ok = g_project.saveActiveScene(*g_storage, g_scene) &&
                            g_project.saveManifest(*g_storage);
            g_thumbPending = true;   // 0.9.0: captura no próximo fim de frame
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
            primMeshesToGrave();   // 0.8.10: posse antiga p/ cova
            const SceneSerializer::LoadCtx ctx = makeLoadCtx();
            const bool ok = g_project.loadActiveScene(*g_storage, g_scene, ctx);
            if (ok) {
                postLoadMigrateAndFixup();   // 0.8.10: migração silenciosa
            }
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
                g_thumbPending = true;   // 0.9.0: captura antes de sair
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

    // 0.9.1 — JANELA DE TEXTO (topmost): consome a fila do IME ANTES do
    // desenho (o texto do frame entra no buffer deste frame) e fecha com
    // o par landscape+imeHide quando o back é tocado. Modal: qualquer
    // coisa do editor por baixo está gating pelo anyOverlayOpen.
    // 0.9.2 — PEDIDO DO EDITOR DE SCRIPT (Inspector → "Editar script"):
    // abre com o par inseparável portrait+IME (o mesmo do textWin)
    if (g_editor.requestScriptEditor) {
        g_editor.requestScriptEditor = false;
        openScriptEditor(g_editor.scriptEditorTarget);
    }

    // 0.9.2 — ERROS DA V.ONI (a central drena 1×/frame): engine.log +
    // toast + barra de erro do editor (se o script em edição é o dono)
    {
        Handle errTic{};
        voni::Error verr;
        if (g_voni.popError(errTic, verr)) {
            elog::error("voni: script erro linha %u: %s", verr.line,
                        verr.message.c_str());
            std::snprintf(g_toast, sizeof(g_toast), "script: linha %u",
                          verr.line);
            g_toastT = 1.8f;
            if (g_editor.scriptWin.open && g_editor.scriptWin.tic == errTic) {
                g_editor.scriptWin.errLine = verr.line;
                g_editor.scriptWin.errMsg = verr.message;
                g_editor.scriptWin.running = false;
                // 0.9.6 (G2-7e): o PAR do botão Substituir viaja com o
                // erro (if→exist…; o 'break' traz o do contexto da run)
                g_editor.scriptWin.fixFrom = verr.fixFrom;
                g_editor.scriptWin.fixTo = verr.fixTo;
            }
        }
    }

    if (g_editor.textWin.open) {
        ime::Event ev;
        while (ime::poll(ev)) {
            editor::textwin::applyEvent(g_editor.textWin, ev);
        }
        if (editor::textwin::draw(g_ui, g_input, g_editor.textWin,
                                  w, h, g_frameDt) == 1) {
            closeTextWindow();
        }
    }

    // 0.9.2 — DOCS (Settings → Docs; landscape, com pesquisa in-app):
    // back fecha; o campo de pesquisa abre o teclado (propósito 9).
    // FASE 9 (G0-3): também abre PELA LUPA do editor de script (result 4) —
    // as Docs desenham POR CIMA (o bloco do scriptWin abaixo fica gated).
    if (g_editor.docsScreen.open) {
        const int dr = editor::docswin::draw(g_ui, g_input,
                                             g_editor.docsScreen, w, h);
        if (dr == 1) {
            g_editor.docsScreen.open = false;
            elog::info("voni: docs fechadas");
        } else if (dr == 4) {
            editor::openTextInput(g_editor, 9, Handle{}, -1,
                                  g_editor.docsScreen.query);
        }
    }

    // 0.9.2 — EDITOR DE SCRIPT (portrait + IME — o par do textWin): a fila
    // do IME alimenta o fonte; Run/Stop pelo VoniSystem; back SALVA no
    // componente e fecha com landscape+imeHide.
    // FASE 9 (G0-1): resultados 4 (lupa → Docs POR CIMA) e 5 (toque no
    // corpo → RE-PETE o IME — sem perder foco); teclado in-app desenhado
    // pelo próprio editor (o MESMO applyEvent do IME). Com as Docs abertas
    // por cima o editor NÃO desenha (Docs é o modal corrente).
    if (g_editor.scriptWin.open && !g_editor.docsScreen.open) {
        ime::Event ev;
        while (ime::poll(ev)) {
            editor::scriptwin::applyEvent(g_editor.scriptWin, ev);
        }
        const int sr = editor::scriptwin::draw(
            g_ui, g_input, g_editor.scriptWin, w, h, g_frameDt);
        if (sr == 1) {
            closeScriptEditor();   // salva o fonte + par landscape/IME
        } else if (sr == 2) {
            scriptEditorRun();
        } else if (sr == 3) {
            scriptEditorStop();
        } else if (sr == 4) {
            // lupa: Docs por cima (pesquisa filtra + exemplo)
            g_editor.docsScreen.open = true;
            g_editor.docsScreen.queryLen = 0;
            g_editor.docsScreen.query[0] = '\0';
            g_editor.docsScreen.expanded = -1;
            elog::info("voni: docs abertas (lupa do editor de script)");
        } else if (sr == 5) {
            // toque no corpo: o IME do sistema é re-pedido (foco) — o
            // teclado próprio já CEDOU lá dentro (a política G3: os dois
            // são ALTERNATIVAS, nunca um por cima do outro)
            storage::jniImeShow();
            elog::info("voni: IME re-pedido (toque no corpo do editor)");
        } else if (sr == 7) {
            // 0.9.6 (G3): o botão do TECLADO PRÓPRIO abriu-o — o IME do
            // sistema sai do ecrã (a política de coexistência)
            storage::jniImeHide();
            elog::info("voni: teclado próprio aberto — IME do sistema "
                       "escondido");
        } else if (sr == 6) {
            // 0.9.5 · COPIAR REFERÊNCIA PARA IA: a referência V.ONI
            // COMPLETA (gerada do REGISTO — a mesma fonte das Docs) vai
            // para o clipboard como texto colável
            const std::string ref = voni::reg::fullReferenceMarkdown();
            if (storage::jniClipboardCopy(ref.c_str())) {
                std::snprintf(g_toast, sizeof(g_toast),
                              "Referência V.ONI copiada (%zu entradas)",
                              voni::reg::all().size());
                g_toastT = 1.8f;
                elog::info("voni: referência V.ONI copiada p/ o clipboard "
                           "(%zu entradas, %zu chars)",
                           voni::reg::all().size(), ref.size());
            } else {
                std::snprintf(g_toast, sizeof(g_toast),
                              "clipboard indisponível");
                g_toastT = 1.8f;
            }
        }
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
                    showToast("câmara alinhada à vista");
                    LOGI("editor: câmara '%s' alinhada à vista de edição",
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
                // 0.9.0 — UNDO do delete: before = o TIC, after = vazio
                const editor::TicSnap gone = editor::snapTic(g_scene, ct->handle);
                editor::TicSnap empty;
                g_undo.push(gone, empty, ct->handle);
                char goneName[48];
                std::snprintf(goneName, sizeof(goneName), "%.40s", ct->name.c_str());
                g_scene.destroy(g_editor.contextTic);
                if (g_editor.selected == g_editor.contextTic) {
                    g_editor.selected = Handle::invalid();
                    g_editor.selElement = -1;
                }
                showToast("TIC removido");
                LOGI("editor: TIC '%s' removido", goneName);
            }
        }
    }
    if (g_editor.textInput) {
        const char* title = g_editor.textPurpose == 0 ? "RENOMEAR TIC"
                            : g_editor.textPurpose == 1 ? "NOME DA NOVA CENA"
                            : g_editor.textPurpose == 2 ? "TEXTO DO ELEMENTO"
                            : g_editor.textPurpose == 4 ? "COR DO ELEMENTO"
                            : g_editor.textPurpose == 5 ? "COR DO TIC"
                            : g_editor.textPurpose == 7 ? "RENOMEAR CLIP"
                                                        : "ALVO DA AÇÃO";
        const int ch =
            editor::drawTextInput(g_ui, g_input, w, h, g_editor, title);
        if (ch == 1) {
            if (g_editor.textPurpose == 1) {
                // 0.7.1: NOVA CENA (o nome vem do teclado in-app)
                createSceneNamed(g_editor.textBuf);
            } else if (g_editor.textPurpose == 7) {
                // 0.8.11 — RENOMEAR CLIP: o commit precisa do STORAGE (o
                // commitTextInput é puro cena+editor); copia bytes → remove
                // o velho → catálogo (o TIC selecionado segue a NOVA ref se
                // a tinha)
                if (g_storage && g_editor.textLen > 0 &&
                    g_audioWs.selected < g_audioCatalog.size()) {
                    const std::string old =
                        g_audioCatalog[g_audioWs.selected];
                    const std::string dir =
                        old.substr(0, old.rfind('/') + 1);
                    const std::string nv = dir +
                        convert::sanitizeName(g_editor.textBuf) + ".gi";
                    if (nv != old && !g_storage->exists(nv)) {
                        std::vector<u8> b;
                        if (g_storage->readBytes(old, b) &&
                            g_storage->writeBytes(nv, b.data(), b.size()) &&
                            g_storage->remove(old)) {
                            g_audioClips.erase(old);
                            refreshAudioCatalog();
                            // TICs com a ref velha passam à nova (o clip é o
                            // MESMO — só o nome mudou)
                            g_scene.forEachActive([&](Tic& t) {
                                if (AudioPlayer* au =
                                        t.getComponent<AudioPlayer>()) {
                                    if (au->clipPath == old) {
                                        au->clipPath = nv;
                                    }
                                }
                            });
                            elog::info("audio: clip renomeado '%s' -> '%s'",
                                       old.c_str(), nv.c_str());
                            showToast("clip renomeado");
                        } else {
                            showToast("renomear falhou (engine.log)");
                            elog::error("audio: renomear %s FALHOU",
                                        old.c_str());
                        }
                    } else {
                        showToast("nome invalido/ocupado");
                    }
                }
            } else if (g_editor.textPurpose == 0) {
                // 0.9.0 — RENAME com undo (before do TIC alvo; after = pós-commit)
                const editor::TicSnap uBefore =
                    editor::snapTic(g_scene, g_editor.textTic);
                if (editor::commitTextInput(g_scene, g_editor)) {
                    pushUndo(uBefore, g_scene.get(g_editor.textTic));
                    showToast("aplicado");
                }
            } else if (editor::commitTextInput(g_scene, g_editor)) {
                showToast("aplicado");
            }
        }
    }

    // 0.7.1 — OVERLAY CENAS: lista do projeto (a ativa marcada) + nova/trocar
    // (menu de EDITOR — em Play a troca vem pela ação declarativa com
    // transição; aqui a troca é direta)
    if (g_editor.scenesMenu && g_projectReady) {
        const editor::toolbar::TopBarLayout tbl2 = editor::toolbar::topbarLayout(
            w, h, g_ui.safeArea(), g_editor.uiMode, g_editor.audioMode);
        const int pick = editor::drawScenesMenu(
            g_ui, g_input, w, h, g_editor, g_project.scenes,
            g_project.activeScene, tbl2.cena.x, tbl2.cena.y + tbl2.cena.h);
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
        } else if (pick == fileapi::kBrowserRootCount + 1) {
            browserOpen(fileapi::parentPath(g_browser.cwd));
        } else if (pick >= fileapi::kBrowserRootCount + 2) {
            const size_t i = static_cast<size_t>(
                pick - fileapi::kBrowserRootCount - 2);
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
            // 0.8.7: o corpo do "Sim" vive em applyImportedAssetToSelectedTic
            // (extraído — o MESMO código é afervel no hospedeiro)
            applyImportedAssetToSelectedTic();
        }
    }

    // 0.8.11: glifos de ALTIFALANTE (+ esfera posicional) dos AudioPlayers
    // — SÓ no editor (em Play nada desenha: só soa)
    // 0.9.6 (G1-3): e NUNCA com overlay aberto — o ícone amarelo do áudio
    // desenhava POR CIMA do Settings (o glifo corre no pass UI, DEPOIS do
    // backdrop modal); com um ecrã cheio aberto, TUDO o que desenha sobre
    // a cena sai do ecrã (a regra das camadas: cena < painéis < modais)
    if (!g_editor.playMode && !g_editor.uiMode && !g_editor.audioMode &&
        !modalOpen) {
        const Mat4 glyphVp =
            Mat4::mul(g_camera.proj(w / h), g_camera.view());
        g_scene.forEachActive([&](Tic& t) {
            drawAudioGlyph(g_ui, glyphVp, w, h, t);
        });
    }

    drawToast();

    // 0.9.0 (spec E/K) — PAINEL DE BAIXO: tabs + drawer (Ficheiros/Consola/
    // Animação) + STATUS BAR 24dp ("FPS N · TICs N" — as abreviaturas
    // morreram). Desenhado DEPOIS do viewport/painéis (o drawer cobre o
    // fundo) e ANTES dos overlays (modais por cima de tudo).
    // 0.9.6 (G1-4): ESCONDIDO em ecrãs cheios e com teclado aberto —
    // este bloco corria DEPOIS dos overlays e a barra (drawer + status)
    // desenhava POR CIMA do Settings/Docs/editor e do teclado do renomear
    // (o bug "a barra de baixo aparece com o teclado aberto"). A camada
    // fica: cena < painéis < modais < teclado — a barra pertence aos
    // painéis; com um ecrã cheio ou teclado, ela sai do ecrã.
    if (!editor::fullscreenOverlayOpen(g_editor) && !g_editor.textInput) {
        // o log da consola = o MESMO tail do engine.log do viewer (120
        // linhas chegam — a consola filra por chips)
        std::vector<std::string> logTail;
        elog::readTail(logTail, 120);
        const editor::bottom::Actions ba = editor::bottom::draw(
            g_ui, g_input, g_editor, g_bottom, g_catalog, logTail,
            static_cast<int>(g_fps + 0.5f), g_scene.count());
        if (ba.filePick > 0) {
            // card de Ficheiros → aplica ao TIC selecionado (o MESMO
            // dispatch do seletor: menuKind por tipo, pick = i+1 doss files)
            const int pick = ba.filePick;
            if (editor::pickerGuardBlocked(g_scene, g_editor.selected)) {
                g_editor.pickBlockedHint = true;
            } else {
                const editor::TicSnap uBefore =
                    editor::snapTic(g_scene, g_editor.selected);
                const editor::AssetPickOutcome out = editor::applyAssetPick(
                    g_scene, g_editor.selected, ba.filePickKind, pick,
                    g_catalog, makeAssetResolvers());
                if (out.applied) {
                    pushUndo(uBefore, g_scene.get(g_editor.selected));
                }
                if (out.toast[0] != '\0') {
                    showToast(out.toast);
                }
                if (out.log[0] != '\0') {
                    elog::info("%s", out.log);
                }
            }
        }
        if (ba.exportPressed) {
            // o MESMO Export do Settings: Downloads/GOneVV/logs (MediaStore)
            int copied = 0;
            if (storage::jniExportLogsToDownloads(&copied) && copied >= 0) {
                showToast("logs exportados");
                elog::info("logs: exportados %d ficheiro(s) para %s", copied,
                           elog::kDownloadsRelPath);
            } else {
                showToast("export falhou (sem ficheiros? API<29?)");
            }
        }
    }

    // 0.9.0 (spec G) + FASE 9 (G1-5): o layout PERSISTE por diferença com
    // DEBOUNCE de 1,5 s — o tick deteta a mudança, agenda e grava UMA vez
    // 1,5 s após a ÚLTIMA alteração (o drag inteiro do drawer = 1 write;
    // o log antigo mostrava 4 writes em ~40 s)
    layoutSaveTick(g_frameDt);

    // 0.7.1: o overlay da transição por cima de TUDO no editor também
    ui::transitionDraw(g_ui, g_sceneTrans, w, h);

    g_ui.endFrame();                       // submete solids + glyphs
    g_lastUiStats = g_renderer.endFrame(); // desenha a UI por cima do 3D

    // 0.9.0 (spec F) — a captura vem AQUI: o frame JÁ está completo no
    // backbuffer (3D + UI + overlays) e AINDA não passou ao ecrã; lê a
    // VIEWPORT CENTRAL (sem o chrome dos painéis; em Play, o contentRect),
    // crop 16:9 + downsample ≤480 e escreve thumb.png na raiz do projeto.
    captureThumbIfPending(w, h);

    g_egl.swap();
    g_input.clearEdges();   // edges já consumidas pela UI/câmara neste frame
}

} // namespace

void android_main(android_app* app) {
    // F5.1-hotfix: log DUPLO (logcat + ficheiro) desde a 1ª linha.
    // O boot ainda não tem os paths da activity? O elog usa o fallback
    // android (Android/data/vv.goni/files/logs) — JNI_OnLoad já escreveu
    // 0.8.12 — a linha de VERSÃO do arranque é o BANNER da identidade
    // (boot: G.One VV <versão> versionCode <N> sha256 <…> git <…>) — vem
    // da JNI (build_info.txt que o CI escreve em 2 passes); o banner
    // com a changelog da campanha segue-se para o dono ler no log viewer.
    elog::info("G.One VV 0.9.0 — EDITOR POLISH + DESIGN SYSTEM (spec A-M): "
               "tabela de tokens + 49 ícones outline + hierarquia com "
               "ícones de tipo/pesquisa/multi-seleção + inspector com "
               "secções colapsáveis e caixas X/Y/Z 48dp + top bar 56/tab "
               "bar 48 com underline accent + viewport com stack "
               "undo/redo/save/dup/paste e toolbar rotulada + painel de "
               "baixo com Ficheiros/Consola/Animação e drawer 240 + status "
               "24dp + MENU/CENAS ancorados com scrim + página de Settings "
               "com secções e imersivo + toasts bottom-center + undo/redo "
               "por snapshot + miniaturas de projeto PNG no save + layout "
               "persistente (layout.json)");
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

    // 0.8.11: o callback do backend puxa o MISTURADOR (instala 1× — o
    // g_mix do AudioOut aponta para o AudioEngine global)
    vv::audioout::setMixFn([](f32* out, u32 frames, u32 ch, u32 rate) {
        g_audioEngine.mix(out, frames, ch, rate);
    });
#ifdef __ANDROID__
    // a VM do glue alimenta o fallback AudioTrack (JNI) e a GRAVAÇÃO
    // (AudioRecord) — as threads de áudio nascem desanexadas
    vv::audioout::setVm(app->activity->vm);
    g_audioVm = app->activity->vm;
#endif

    // 0.8.10 — BANNER DE VERSÃO no boot log (a identidade da build —
    // versionCode/git/sha — é a 1ª linha que o log viewer mostra)
    elog::info("%s", vv::buildinfo::banner().c_str());

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
    // GCC 13.3 do runner (ubuntu-24.04) tem um ICE em gimple_add_tmp_var
    // com o temporário prvalue braced — a variável nomeada aplica os
    // mesmos NSDMIs e copia; inclui playMode = false (0.6.8) e
    // uiMode/seleção de elemento (0.7.0)
    editor::EditorState editorFresh;
    g_editor = editorFresh;
    g_timeline = timeline::State{};   // 0.8.0: scrub/keys/preview da sessão anterior não vingam
    g_animSystem.enabled = false;    // 0.8.0: idem física (gate fechado)
    g_playSnap = PlaySnapshot{};
    g_playUi = PlayUiPress{};   // 0.7.0: nenhum on-click de UI armado
    g_sceneTrans = ui::SceneTransition{};   // 0.7.1: transição morta
    g_sceneTransIdx = 0xFFFFFFFFu;
    g_browser = FileBrowserState{};   // 0.7.2: browser fechado
    g_applyAsk = ApplyAskState{};
    g_scene.clear();
    g_primOwners.clear();   // 0.8.10: TERM_WINDOW já destruiu (ids=0, no-op)
    g_primGrave.clear();
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
    g_systems.add(TickGroup::Update, &g_voni);   // 0.9.2: scripts ANTES do
    // transform cache (move() reflete no world do MESMO passo)
    voniInstallHooks();   // 0.9.2: transition.for + nome da cena atual
    g_systems.add(TickGroup::Update, &g_transformSystem);
    g_systems.add(TickGroup::Physics, &g_physics);   // entre Update e PostUpdate
    elog::info("[boot 5/6] physics OK (tickgroups Update+Physics registados; "
               "animação no Update antes do transform)");

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
