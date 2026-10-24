// tests/test_012v_viewport.cpp — 0.12-V: OS 3 BUGS DO VIEWPORT 3D DO EDITOR
// (o modelo .gmesh importado + o gizmo) — o CAMINHO REAL do device no
// hospedeiro (#include platform/main.cpp, o padrão do test_wiring087).
//
// OS SINTOMAS DO DONO (C33, landscape): (1) o modelo só APARECIA enquanto o
// gizmo era arrastado; (2) o TIC filho da câmara desenhava no sítio errado;
// (3) gizmos GIGANTES. AS CAUSAS (ficheiro:linha no relatório):
//   · o gizmo desenhava/hit-testava em tr->pos (LOCAL) como se fosse MUNDO
//     — para TIC com pai ficava fora do mesh;
//   · o arrasto escrevia o alvo do MUNDO direto no TRS LOCAL e refrescava o
//     cache com updateWorld() (world = TRS local — o mesh TELEPORTAVA para
//     o gizmo durante o drag e voltava ao largar: o "aparece só ao tocar");
//   · o comprimento do gizmo era dist_da_órbita·k (sem tan(fov/2), sem a
//     distância ATÉ ao gizmo) — gigante perto do olho, minúsculo longe.
//
// OS PINS (o resultado do dono): o modelo desenha SEM nenhum gizmo ativo;
// o gizmo ancora NO MUNDO (onde o mesh está); o arrasto compõe com o pai em
// TODOS os frames (zero teleporte); o gizmo mede o MESMO tamanho em píxeis
// a 3 distâncias e o toque tem o mínimo de 48 px.
// CLÁUSULA CALMA: zero features — só viewport/gizmo/render + testes.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::fb — o rasterizador)
#include <EGL/egl.h>     // stub (1280×720)
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>   // FAKE controlável (tests/stub é o primeiro include dir)

#include "FakeStorage.h"

// ponte Java (o papel do "stub Java" — o padrão do test_wiring087)
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);

// ---- O CAMINHO REAL DO DEVICE (namespace anónimo = mesmo TU) ---------------
// (o android_main é do test_wiring087 — a casa: UM só TU liga o entrypoint;
// este TU precisa do frame()/globais — o main muda de nome e fica por usar)
#define android_main android_main_nao_ligado_012v
#include "platform/main.cpp"
#undef android_main

using namespace vv;
using ::test::nearEqF;
using ::test::vecNearF;

namespace {

const char* kTestLogs = "test-012v-logs";

void rmrf(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xC01));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xC02));

void javaRegistersGranted() {
    g_jni.reset();
    while (vv::storage::pollResult()) {
    }
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity,
        g_jni.newString("test-012v"));
    g_jni.manager_result = true;
}

// reset do estado PARTILHADO do main.cpp entre casos (o essencial para o
// viewport; o mesmo espírito do resetEngineForTest do wiring087)
void resetViewportTest() {
    g_scene.clear();
    editor::EditorState editorFresh;
    g_editor = editorFresh;
    g_browser = FileBrowserState{};
    g_applyAsk = ApplyAskState{};
    g_primOwners.clear();
    g_primGrave.clear();
    g_catalog.meshes.clear();
    g_catalog.textures.clear();
    g_catalog.audio.clear();
    g_prevAssetMenu = 0;
    g_projectReady = false;
    g_storage.reset();
    g_gpu.releaseAll();
    g_resources.setStorage(nullptr);
    g_toast[0] = '\0';
    g_toastT = 0.0f;
    g_input.resetAll();
    g_gizmo = gizmo::GizmoState{};
    g_grab = gizmo::Grab{};
    g_gizmoBeforeValid = false;
    g_camera = Camera{};
    glstub::reset();
    glstub::fb::resetState();
}

// boot REAL (INIT_WINDOW) + fonte fixture (o device tem as do Android)
void bootEditor() {
    glResourcesRegister();   // 0.12-Z F1: o registo é do android_main — o
                             // hospedeiro regista à mão (idempotente)
    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    ASSERT(g_ready);
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        ASSERT(g_font.loadFromPaths(paths, 1, 28.0f));
    }
    g_ui.setFont(&g_font);
    g_projectReady = true;
}

// renderer + cubo (a "mesh" do TIC jogador — a geometria é irrelevante para
// os pins; o que conta é a matriz world e o gizmo)
void ensureCube() {
    static bool ready = false;
    if (ready) {
        return;
    }
    EXPECT(g_renderer.init());
    const CubeMeshData cube = makeCube(1.0f);
    EXPECT(g_cubeMesh.create(cube.vertices.data(),
                             static_cast<u32>(cube.vertices.size()),
                             cube.indices.data(),
                             static_cast<u32>(cube.indices.size())));
    ready = true;
}

// A CENA DO DONO: StaticBody3D > camera (CameraComp) > jogador (mesh,
// escala 0.096) — a hierarquia da captura do bug. Devolve o handle do
// jogador (SELECIONADO — o gizmo da casa).
Handle buildPlayerChain() {
    ensureCube();
    const Handle hBody = g_scene.create("StaticBody3D");
    EXPECT(hBody.valid());
    if (!hBody.valid()) {
        return Handle::invalid();
    }
    g_scene.get(hBody)->addComponent<Transform3D>();
    const Handle hCam =
        g_scene.create("camera", static_cast<i32>(hBody.index));
    EXPECT(hCam.valid());
    if (!hCam.valid()) {
        return Handle::invalid();
    }
    if (Transform3D* tc = g_scene.get(hCam)->addComponent<Transform3D>()) {
        tc->pos = Vec3{0.0f, 1.6f, 0.0f};   // o olho da câmara de jogo
        tc->updateWorld();
    }
    g_scene.get(hCam)->addComponent<CameraComp>();
    const Handle hJog =
        g_scene.create("jogador", static_cast<i32>(hCam.index));
    EXPECT(hJog.valid());
    if (!hJog.valid()) {
        return Handle::invalid();
    }
    if (Transform3D* tj = g_scene.get(hJog)->addComponent<Transform3D>()) {
        tj->pos = Vec3{0.0f, 0.0f, 0.0f};         // local (o mundo compõe)
        tj->scale = Vec3{0.096f, 0.096f, 0.096f}; // a escala da captura
        tj->updateWorld();
    }
    MeshRenderer* mr = g_scene.get(hJog)->addComponent<MeshRenderer>();
    if (mr == nullptr) {
        EXPECT(mr != nullptr);
        return Handle::invalid();
    }
    mr->mesh = &g_cubeMesh;
    mr->material = g_renderer.litMaterial();
    // os mundos da cadeia (o TransformSystem do tick faria isto; o teste
    // arma o estado exatamente como a cena carregada do .goni fica)
    if (Transform3D* tj = g_scene.get(hJog)->getComponent<Transform3D>()) {
        Transform3D* tc = g_scene.get(hCam)->getComponent<Transform3D>();
        tj->world = Mat4::mul(tc->world, tj->computeMatrix());
    }
    editor::selectTic(g_editor, hJog);
    return hJog;
}

// o rect + a vp do frame (AS MESMAS expressões do frame() — a fonte é a
// editor::centerRect com o estado do editor; o teste projeta com a MESMA
// câmara que o render usou)
UiRect viewRectOfFrame() {
    const f32 w = static_cast<f32>(g_egl.width());
    const f32 h = static_cast<f32>(g_egl.height());
    const bool inspRight =
        g_editor.showInspector &&
        !(g_editor.uiMode || editor::inspectorCollapsed(g_editor));
    return editor::centerRect(w, h, g_ui.safeArea(), currentDrawerH(),
                              inspRight, g_editor.hierW, g_editor.inspW,
                              !g_editor.uiMode &&
                                  editor::inspectorCollapsed(g_editor));
}
Mat4 vpOfFrame(const UiRect& vr) {
    return Mat4::mul(g_camera.proj(vr.w / vr.h), g_camera.view());
}

// sondas de píxel (o fb do stub é GL bottom-up; as projeções do gizmo são
// top-down) — devolve o Nº de píxeis que satisfazem o predicado dentro do
// raio `r` (px) de (sx,sy) em coords de ECRÃ
int countPixelsNear(f32 sx, f32 sy, f32 r, bool (*keep)(const u8*)) {
    const int fbW = glstub::fb::fbW_;
    const int fbH = glstub::fb::fbH_;
    if (fbW <= 0 || fbH <= 0) {
        EXPECT(fbW > 0 && fbH > 0);
        return 0;
    }
    std::vector<u8> px(static_cast<size_t>(fbW) * fbH * 4);
    glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    const int cx = static_cast<int>(sx);
    const int cyGl = fbH - 1 - static_cast<int>(sy);   // ecrã y-down → GL y-up
    int n = 0;
    const int ri = static_cast<int>(r);
    for (int dy = -ri; dy <= ri; ++dy) {
        for (int dx = -ri; dx <= ri; ++dx) {
            const int x = cx + dx;
            const int y = cyGl + dy;
            if (x < 0 || y < 0 || x >= fbW || y >= fbH) {
                continue;
            }
            if (keep(&px[(static_cast<size_t>(y) * fbW + x) * 4])) {
                ++n;
            }
        }
    }
    return n;
}

// píxeis de EIXO vermelho (#E5484D do gizmo — o conteúdo 3D da casa)
bool isAxisRed(const u8* p) {
    return p[0] > 170 && p[1] < 130 && p[2] < 130 && p[3] > 200;
}
// píxeis do MODELO LIT (o lit da casa: faces a perto de branco e faces em
// meia-luz — o clear #0B0B0D e a grelha ficam muito abaixo)
bool isLitWhite(const u8* p) {
    return p[0] > 95 && p[1] > 95 && p[2] > 95;
}

// a LARGURA (px) da caixa dos píxeis vermelhos no raio dado — o comprimento
// desenhado do eixo X do gizmo (a sonda do tamanho constante)
int redBoxWidthNear(f32 sx, f32 sy, f32 r) {
    const int fbW = glstub::fb::fbW_;
    const int fbH = glstub::fb::fbH_;
    if (fbW <= 0 || fbH <= 0) {
        EXPECT(fbW > 0 && fbH > 0);
        return 0;
    }
    std::vector<u8> px(static_cast<size_t>(fbW) * fbH * 4);
    glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    const int cx = static_cast<int>(sx);
    const int cyGl = fbH - 1 - static_cast<int>(sy);
    const int ri = static_cast<int>(r);
    int mnX = 1 << 30, mxX = -1;
    for (int dy = -ri; dy <= ri; ++dy) {
        for (int dx = -ri; dx <= ri; ++dx) {
            const int x = cx + dx;
            const int y = cyGl + dy;
            if (x < 0 || y < 0 || x >= fbW || y >= fbH) {
                continue;
            }
            const u8* p = &px[(static_cast<size_t>(y) * fbW + x) * 4];
            if (isAxisRed(p)) {
                if (x < mnX) mnX = x;
                if (x > mxX) mxX = x;
            }
        }
    }
    return mxX >= mnX ? (mxX - mnX + 1) : 0;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. O MODELO DESENHA SEM NENHUM GIZMO ATIVO (o pedido do dono — o Bug 1):
// a cena StaticBody3D > camera > jogador (escala 0.096), frames SEM toque
// nenhum: o boneco aparece no MUNDO (a translação do world composto com o
// pai). Era o sintoma «invisível; só aparecia ao tocar» (o mesh sempre
// desenhava — mas o gizmo mentia o sítio e o drag teleportava).
// ---------------------------------------------------------------------------
TEST(viewport_modelo_sem_gizmo) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetViewportTest();
    glstub::fb::enabled = true;
    javaRegistersGranted();
    bootEditor();
    const Handle hJog = buildPlayerChain();
    ASSERT(hJog.valid());
    // SEM seleção: NENHUM gizmo ativo (o cenário exato do dono — o gizmo
    // não pode «consertar» o desenho; o modelo tem de estar lá por si)
    editor::clearSelection(g_editor);

    // frames SEM toque nenhum (o dono: «o FPS é 60, o loop corre sempre»)
    frame();
    frame();
    frame();

    // o world do jogador = parentWorld ∘ local (a composição do tick) —
    // a translação é o OLHO da câmara de jogo (0, 1.6, 0)
    const Transform3D* tj =
        g_scene.get(hJog)->getComponent<Transform3D>();
    ASSERT(tj != nullptr);
    EXPECT(nearEqF(tj->world.m[12], 0.0f, 1e-4f));
    EXPECT(nearEqF(tj->world.m[13], 1.6f, 1e-4f));
    EXPECT(nearEqF(tj->world.m[14], 0.0f, 1e-4f));

    // a sonda: o centro do modelo projetado com a vp DO FRAME tem píxeis
    // lit (o modelo está desenhado — sem seleção nova, sem drag, sem nada)
    const UiRect vr = viewRectOfFrame();
    const Mat4 vp = vpOfFrame(vr);
    f32 sx = 0.0f, sy = 0.0f;
    ASSERT(gizmo::projectPoint(vp, Vec3{tj->world.m[12], tj->world.m[13],
                                        tj->world.m[14]},
                               vr.w, vr.h, sx, sy, vr.x, vr.y));
    EXPECT_MSG(sx >= vr.x && sx <= vr.x + vr.w && sy >= vr.y &&
                   sy <= vr.y + vr.h,
               "0.12-V: o modelo projetou FORA da viewport (%.0f,%.0f)",
               (double)sx, (double)sy);
    const int lit = countPixelsNear(sx, sy, 24.0f, isLitWhite);
    EXPECT_MSG(lit > 12,
               "0.12-V: o modelo NÃO desenha sem gizmo (lit=%d em "
               "(%.0f,%.0f)) — o Bug 1 do dono", lit, (double)sx, (double)sy);

    onAppCmd(nullptr, APP_CMD_TERM_WINDOW);
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}

// ---------------------------------------------------------------------------
// 2. O GIZMO ANCORADO NO MUNDO (o Bug 2e): o gizmo desenha NO MESMO SÍTIO
// do mesh (a translação do world) — NUNCA no tr->pos local-as-world. Com o
// local (0,0,0) e o mundo (0,1.6,0) os dois sítios são distintos: vermelho
// no mundo, ZERO vermelho no local.
// ---------------------------------------------------------------------------
TEST(viewport_gizmo_no_mundo) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetViewportTest();
    glstub::fb::enabled = true;
    javaRegistersGranted();
    bootEditor();
    const Handle hJog = buildPlayerChain();
    ASSERT(hJog.valid());

    frame();
    frame();

    const Transform3D* tj =
        g_scene.get(hJog)->getComponent<Transform3D>();
    const UiRect vr = viewRectOfFrame();
    const Mat4 vp = vpOfFrame(vr);
    f32 wx = 0.0f, wy = 0.0f, lx = 0.0f, ly = 0.0f;
    const Vec3 worldPos{tj->world.m[12], tj->world.m[13], tj->world.m[14]};
    ASSERT(gizmo::projectPoint(vp, worldPos, vr.w, vr.h, wx, wy, vr.x, vr.y));
    ASSERT(gizmo::projectPoint(vp, tj->pos, vr.w, vr.h, lx, ly, vr.x, vr.y));
    EXPECT(std::fabs(wx - lx) + std::fabs(wy - ly) > 8.0f);   // sítios distintos

    // o gizmo está NO MUNDO (o mesmo sítio do mesh)
    const int redWorld = countPixelsNear(wx, wy, 26.0f, isAxisRed);
    EXPECT_MSG(redWorld > 10,
               "0.12-V: o gizmo NÃO está no mundo do mesh (red=%d em "
               "(%.0f,%.0f)) — o Bug 2e do dono", redWorld, (double)wx,
               (double)wy);
    // e NÃO está no local-as-world (a origem do mundo, neste caso)
    const int redLocal = countPixelsNear(lx, ly, 26.0f, isAxisRed);
    EXPECT_MSG(redLocal == 0,
               "0.12-V: o gizmo ficou no LOCAL-as-world (red=%d em "
               "(%.0f,%.0f)) — o Bug 2e do dono", redLocal, (double)lx,
               (double)ly);

    onAppCmd(nullptr, APP_CMD_TERM_WINDOW);
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}

// ---------------------------------------------------------------------------
// 3. O ARRASTO COMPÕE COM O PAI EM TODOS OS FRAMES (o Bug 1 mecânico): o
// drag escreve o alvo do MUNDO convertido a LOCAL e o cache do frame é
// world = parentWorld ∘ TRS — o mesh NUNCA teleporta para o gizmo durante
// o arrasto nem volta ao largar.
// ---------------------------------------------------------------------------
TEST(viewport_drag_sem_teleporte) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetViewportTest();
    glstub::fb::enabled = true;
    javaRegistersGranted();
    bootEditor();
    const Handle hJog = buildPlayerChain();
    ASSERT(hJog.valid());
    frame();

    Transform3D* tj = g_scene.get(hJog)->getComponent<Transform3D>();
    const Handle hCam = g_scene.find("camera");
    ASSERT(hCam.valid());
    Transform3D* tc = g_scene.get(hCam)->getComponent<Transform3D>();
    ASSERT(tj != nullptr && tc != nullptr);
    const Mat4 worldBefore = tj->world;

    // o press NO MEIO do eixo Y do gizmo (projetado com a vp do frame)
    const UiRect vr = viewRectOfFrame();
    const Mat4 vp = vpOfFrame(vr);
    f32 len = 0.0f;
    const Vec3 anchor = gizmoAnchor(*tj, std::tan(g_camera.fovY * 0.5f), len);
    f32 mx = 0.0f, my = 0.0f;
    ASSERT(gizmo::projectPoint(vp, anchor + Vec3{0.0f, 1.0f, 0.0f} * (len * 0.5f),
                               vr.w, vr.h, mx, my, vr.x, vr.y));
    g_input.injectDown(0, mx, my);
    frame();   // o grab (delta 0 — a âncora captura)

    // (a) o PRIMEIRO frame do drag já respeita o contrato do tick
    Mat4 expect = Mat4::mul(tc->world, tj->computeMatrix());
    EXPECT_MSG(test::matNearF(tj->world, expect, 1e-4f),
               "0.12-V: o cache no GRAB deixou de compor com o pai "
               "(world = local-as-world — o teleporte do Bug 1)");
    EXPECT(vecNearF(Vec3{tj->world.m[12], tj->world.m[13], tj->world.m[14]},
                    Vec3{worldBefore.m[12], worldBefore.m[13],
                         worldBefore.m[14]},
                    1e-4f));   // dedo parado = pose parada

    // (b) o arrasto: o dedo sobe 40 px → o MUNDO sobe (o mesh segue o dedo)
    g_input.injectMove(0, mx, my - 40.0f);
    frame();
    const f32 worldYBefore = worldBefore.m[13];
    const f32 worldYAfter = tj->world.m[13];
    EXPECT_MSG(worldYAfter > worldYBefore + 0.15f,
               "0.12-V: o arrasto no eixo Y não moveu o MUNDO (%.3f → "
               "%.3f)", (double)worldYBefore, (double)worldYAfter);
    // (c) o contrato DURANTE o arrasto: world = parentWorld ∘ TRS(local)
    expect = Mat4::mul(tc->world, tj->computeMatrix());
    EXPECT_MSG(test::matNearF(tj->world, expect, 1e-4f),
               "0.12-V: DURANTE o arrasto o cache não compõe com o pai "
               "(o mesh teleporta para o gizmo — o Bug 1 do dono)");
    // (d) o TRS LOCAL ficou coerente com o alvo do mundo (a conversão):
    // recompor o mundo a partir do local TEM de reproduzir a translação
    EXPECT(nearEqF(expect.m[12], tj->world.m[12], 1e-4f));
    EXPECT(nearEqF(expect.m[13], tj->world.m[13], 1e-4f));

    // (e) largar: sem salto (o mundo fica onde o drag deixou — composto)
    g_input.injectUp(0);
    frame();
    expect = Mat4::mul(tc->world, tj->computeMatrix());
    EXPECT_MSG(test::matNearF(tj->world, expect, 1e-4f),
               "0.12-V: ao LARGAR o cache descompôs (o mesh volta ao sítio "
               "antigo — o «invisível» do Bug 1)");
    EXPECT(nearEqF(tj->world.m[13], worldYAfter, 1e-4f));

    onAppCmd(nullptr, APP_CMD_TERM_WINDOW);
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}

// ---------------------------------------------------------------------------
// 4. O TAMANHO DO GIZMO EM PÍXEL É CONSTANTE (o Bug 3, a fórmula do dono):
// o mesmo gizmo medido a 3 distâncias (perto ~1.7, médio ~5.5, longe ~20.7)
// ocupa a MESMA largura em píxeis (±20% — a perspectiva do eixo obliquo
// cabe); o toque usa o MESMO tamanho (kGrabPx ≥ 48, o mínimo do dono).
// ---------------------------------------------------------------------------
TEST(viewport_gizmo_px_constante) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetViewportTest();
    glstub::fb::enabled = true;
    javaRegistersGranted();
    bootEditor();
    ensureCube();

    // um TIC RAIZ (o tamanho do gizmo não depende de hierarquia) com o cubo
    const Handle hAlvo = g_scene.create("alvo");
    ASSERT(hAlvo.valid());
    Transform3D* ta = g_scene.get(hAlvo)->addComponent<Transform3D>();
    ASSERT(ta != nullptr);
    MeshRenderer* mr = g_scene.get(hAlvo)->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->mesh = &g_cubeMesh;
    mr->material = g_renderer.litMaterial();
    ta->updateWorld();
    editor::selectTic(g_editor, hAlvo);

    const UiRect vr = viewRectOfFrame();
    const Mat4 vp = vpOfFrame(vr);
    const Vec3 olho = g_camera.eye();
    const Vec3 posicoes[3] = {
        Vec3{2.0f, 2.2f, 3.2f},    // perto (~1.7 do olho)
        Vec3{0.0f, 1.6f, 0.0f},    // médio (~5.5)
        Vec3{-8.0f, -4.0f, -12.0f} // longe (~20.7)
    };
    int larguras[3] = {0, 0, 0};
    f32 dists[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        ta->pos = posicoes[i];
        TransformSystem::composeWorld(*ta, nullptr);
        g_input.resetAll();
        frame();
        dists[i] = length(olho - posicoes[i]);
        f32 sx = 0.0f, sy = 0.0f;
        ASSERT(gizmo::projectPoint(vp, posicoes[i], vr.w, vr.h, sx, sy,
                                   vr.x, vr.y));
        larguras[i] = redBoxWidthNear(sx, sy, 90.0f);
        EXPECT_MSG(larguras[i] > 10,
                   "0.12-V: o eixo X do gizmo não desenhou a %s (w=%d em "
                   "(%.0f,%.0f))", i == 0 ? "perto" : i == 1 ? "médio"
                   : "longe", larguras[i], (double)sx, (double)sy);
    }
    // a CONSTÂNCIA: as 3 larguras dentro de ±20% da média (a fórmula do
    // dono: len = dist·tan(fov/2)·k → px = k/2·altura — distância/FOV/
    // zoom não entram). A fórmula velha (dist_da_órbita·k) dava 1:3.8.
    const f32 media = static_cast<f32>(larguras[0] + larguras[1] +
                                       larguras[2]) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        EXPECT_MSG(std::fabs(larguras[i] - media) <= media * 0.20f,
                   "0.12-V: o gizmo NÃO tem tamanho constante em píxel "
                   "(%s: %d px, média %.1f, dist %.2f) — o Bug 3 do dono",
                   i == 0 ? "perto" : i == 1 ? "médio" : "longe", larguras[i],
                   (double)media, (double)dists[i]);
    }
    // o MESMO tamanho no hit-test: o raio do grab é o mínimo do dono
    EXPECT_MSG(gizmo::kGrabPx >= 48.0f,
               "0.12-V: a área de toque do gizmo ficou abaixo do mínimo "
               "(kGrabPx=%.0f < 48)", (double)gizmo::kGrabPx);
    // e a fórmula é a do dono (dist ATÉ ao gizmo · tan(fov/2) · k único)
    const f32 tanHalf = std::tan(g_camera.fovY * 0.5f);
    EXPECT(nearEqF(gizmo::gizmoLength(10.0f, tanHalf),
                   10.0f * tanHalf * gizmo::kScreenLen, 1e-5f));

    onAppCmd(nullptr, APP_CMD_TERM_WINDOW);
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}
