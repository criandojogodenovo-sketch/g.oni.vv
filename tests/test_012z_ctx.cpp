// tests/test_012z_ctx.cpp — 0.12-Z FASE 0 (o DIAGNÓSTICO do contexto GL):
// os pins da VARA DE MEDIR. O F0 NÃO muda comportamento — estes pins
// aferram o que o dono pediu:
//   · os defaults dos interruptores (master OFF = render normal byte a
//     byte — o custo de estar desligado é UM bool);
//   · os CONTADORES de recursos GL (o que nasce no init morre no destroy —
//     o inventário do boot/term do engine.log);
//   · O ISOLAMENTO POR PASSE: com o mestre ON e SÓ a grelha ligada, a
//     grelha aparece SEMPRE e a geometria não desenha (o teste que deixa
//     o dono ligar um passe de cada vez até a sombra aparecer);
//   · o snapshot de ESTADO GL por passe (a forma do capture/describe).
// O caminho é o REAL do device no hospedeiro (o padrão do wiring087 /
// test_012v: platform/main.cpp inteiro, o boot INIT_WINDOW, o fb do stub).
// CLÁUSULA CALMA: zero features — só diagnóstico + testes.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::fb — o rasterizador)
#include <EGL/egl.h>     // stub (1280×720)
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>   // FAKE controlável (tests/stub é o primeiro include dir)

#include "FakeStorage.h"
#include "render/GlDebug.h"   // O SUJEITO DOS PINS

// ponte Java (o papel do "stub Java" — o padrão do test_wiring087)
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);

// ---- O CAMINHO REAL DO DEVICE (namespace anónimo = mesmo TU) ---------------
// (o android_main é do test_wiring087 — a casa: UM só TU liga o entrypoint;
// este TU precisa do frame()/globais — o main muda de nome e fica por usar)
#define android_main android_main_nao_ligado_012z
#include "platform/main.cpp"
#undef android_main

using namespace vv;
using ::test::nearEqF;

namespace {

const char* kTestLogs = "test-012z-logs";

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
        g_jni.newString("test-012z"));
    g_jni.manager_result = true;
}

void resetCtxTest() {
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

void buildCubeScene() {
    ensureCube();
    const Handle h = g_scene.create("cubo");
    ASSERT(h.valid());
    if (Transform3D* tr = g_scene.get(h)->addComponent<Transform3D>()) {
        tr->pos = Vec3{0.0f, 0.5f, 0.0f};
        tr->updateWorld();
    }
    MeshRenderer* mr = g_scene.get(h)->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->mesh = &g_cubeMesh;
    mr->material = g_renderer.litMaterial();
}

// píxeis LIT (a geometria da casa — o MESMO predicado do test_012v)
bool isLitWhite(const u8* p) {
    return p[0] > 95 && p[1] > 95 && p[2] > 95;
}


int countPixelsInRect(const UiRect& r, bool (*keep)(const u8*)) {
    const int fbW = glstub::fb::fbW_;
    const int fbH = glstub::fb::fbH_;
    if (fbW <= 0 || fbH <= 0) {
        EXPECT(fbW > 0 && fbH > 0);
        return 0;
    }
    std::vector<u8> px(static_cast<size_t>(fbW) * fbH * 4);
    glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    int n = 0;
    const int x0 = static_cast<int>(r.x);
    const int y0 = static_cast<int>(r.y);
    const int x1 = static_cast<int>(r.x + r.w);
    const int y1 = static_cast<int>(r.y + r.h);
    for (int y = y0; y < y1 && y < fbH; y += 3) {
        const int gy = fbH - 1 - y;   // ecrã y-down → GL y-up
        if (gy < 0 || gy >= fbH) {
            continue;
        }
        for (int x = x0; x < x1 && x < fbW; x += 3) {
            if (x < 0) {
                continue;
            }
            if (keep(&px[(static_cast<size_t>(gy) * fbW + x) * 4])) {
                ++n;
            }
        }
    }
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. OS DEFAULTS (o custo de estar desligado é UM bool; master OFF = o
// render normal byte a byte — nenhum passe morre por engano)
// ---------------------------------------------------------------------------
TEST(gldebug_masks_default) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    gldbg::resetCounters();

    // o default É o render normal: mestre OFF e todos os passes vivos
    EXPECT(!gldbg::g_masks.master);
    EXPECT(gldbg::g_masks.sky);
    EXPECT(gldbg::g_masks.worldEnv);
    EXPECT(gldbg::g_masks.geometry);
    EXPECT(gldbg::g_masks.grid);
    EXPECT(gldbg::g_masks.gizmos);
    EXPECT(gldbg::g_masks.post);   // o no-op documentado (0 FBOs no motor)
    EXPECT(gldbg::g_masks.ui);
    EXPECT(gldbg::g_masks.uiGlass);
    EXPECT(!gldbg::g_stateLog);
    EXPECT(!gldbg::g_errorLog);

    // os contadores começam a zero (o resumo do boot conta DESDE o processo)
    for (int k = 0; k < 5; ++k) {
        EXPECT(gldbg::createdOf(static_cast<gldbg::Kind>(k)) == 0);
        EXPECT(gldbg::destroyedOf(static_cast<gldbg::Kind>(k)) == 0);
    }

    // o resumo tem a FORMA pedida (uma linha, os 5 kinds)
    char inv[160];
    gldbg::summaryLine(inv, sizeof(inv));
    EXPECT_MSG(std::strstr(inv, "prog +0/-0") != nullptr,
               "0.12-Z: o resumo não mostra prog (='%s')", inv);
    EXPECT(std::strstr(inv, "fbo +0/-0") != nullptr);
}

// ---------------------------------------------------------------------------
// 2. OS CONTADORES (o que nasce no init morre no destroy — o inventário que
// o boot/term loga; o dono compara term↔init no engine.log)
// ---------------------------------------------------------------------------
TEST(gldebug_counters_ciclo) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetCtxTest();
    gldbg::resetCounters();

    // a camada toda nasce: renderer (prog+3 vao+3 buf+3 tex+2) + grid
    // (prog+1 vao+1 buf+1)
    EXPECT(g_renderer.init());
    EXPECT(g_grid.init());
    EXPECT(nearEqF(static_cast<f32>(gldbg::createdOf(gldbg::Kind::Program)),
                   4.0f, 0.5f));
    // vao: renderer 2 + grid 1 (o lit não tem VAO) | buf: renderer 2 +
    // lit 2 (UBOs) + grid 1
    EXPECT(nearEqF(static_cast<f32>(gldbg::createdOf(gldbg::Kind::Vao)),
                   3.0f, 0.5f));
    EXPECT(nearEqF(static_cast<f32>(gldbg::createdOf(gldbg::Kind::Buffer)),
                   5.0f, 0.5f));
    EXPECT(nearEqF(static_cast<f32>(gldbg::createdOf(gldbg::Kind::Texture)),
                   2.0f, 0.5f));

    // morre EXATAMENTE o que nasceu (o 0.6.7 sem fugas)
    g_grid.destroy();
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Program) == 1);
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Vao) == 1);
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Buffer) == 1);
    g_renderer.shutdown();
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Program) == 4);
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Texture) == 2);

    // uma Mesh e uma Texture de passagem: vao+1 buf+2, tex+1, e a morte
    // devolve (o .gmesh importado e as texturas passam AQUI — o mesmo
    // caminho do GpuAssets)
    gldbg::resetCounters();
    const CubeMeshData cube = makeCube(1.0f);
    Mesh m;
    EXPECT(m.create(cube.vertices.data(),
                    static_cast<u32>(cube.vertices.size()),
                    cube.indices.data(),
                    static_cast<u32>(cube.indices.size())));
    EXPECT(gldbg::createdOf(gldbg::Kind::Vao) == 1);
    EXPECT(gldbg::createdOf(gldbg::Kind::Buffer) == 2);
    Texture t;
    const u8 px4[4] = {255, 255, 255, 255};
    EXPECT(t.createFromRGBA(px4, 1, 1));
    EXPECT(gldbg::createdOf(gldbg::Kind::Texture) == 1);
    m.destroy();
    t.destroy();
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Vao) == 1);
    EXPECT(gldbg::destroyedOf(gldbg::Kind::Texture) == 1);

    g_renderer.init();   // o estado da suíte fica utilizável para os pins 3D
}

// ---------------------------------------------------------------------------
// 3. O ISOLAMENTO POR PASSE (o pedido do dono): com o mestre ON e SÓ a
// grelha, a grelha aparece SEMPRE e a geometria some; religar a geometria
// traz o cubo de volta. É ISTO que deixa o dono ligar um passe de cada vez
// até a sombra aparecer e reportar QUAL passe a cria.
// ---------------------------------------------------------------------------
TEST(gldebug_isolam_grelha) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetCtxTest();
    glstub::fb::enabled = true;
    javaRegistersGranted();
    bootEditor();
    buildCubeScene();

    // BASELINE (mestre OFF): o render normal — o cubo está lá
    gldbg::g_masks = gldbg::Masks{};
    frame();
    frame();
    const UiRect vr = editor::centerRect(
        static_cast<f32>(g_egl.width()), static_cast<f32>(g_egl.height()),
        g_ui.safeArea(), currentDrawerH(),
        g_editor.showInspector &&
            !(g_editor.uiMode || editor::inspectorCollapsed(g_editor)),
        g_editor.hierW, g_editor.inspW,
        !g_editor.uiMode && editor::inspectorCollapsed(g_editor));
    const int litBase = countPixelsInRect(vr, isLitWhite);
    EXPECT_MSG(litBase > 12,
               "0.12-Z: o baseline sem debug não desenha o cubo (lit=%d)",
               litBase);

    // SÓ A GRELHA (o isolamento do dono): a GEOMETRIA deixa de submeter
    // (o delta dos draw calls do stub volta a zero para o pass 3D) e a
    // GRELHA continua a submeter a SUA call (o TRIANGLE_STRIP de 4 verts —
    // o raster do hospedeiro não avalia fwidth do grid; no device é a
    // captura PNG do dono que prova o píxel — AQUI prova-se a SUBMISSÃO).
    gldbg::g_masks.master = true;
    gldbg::g_masks.geometry = false;
    gldbg::g_masks.ui = false;
    {
        const int da0 = glstub::stats.drawArraysCalls;
        const int de0 = glstub::stats.drawElementsCalls;
        frame();
        const int da1 = glstub::stats.drawArraysCalls;
        const int de1 = glstub::stats.drawElementsCalls;
        // com a geometria mascarada, NENHUM drawElements (as malhas) corre
        EXPECT_MSG(de1 == de0,
                   "0.12-Z: geometria mascarada AINDA submete (%d draws)",
                   de1 - de0);
        // e a GRELHA submete a call dela (1 TRIANGLE_STRIP)
        EXPECT_MSG(da1 - da0 >= 1,
                   "0.12-Z: SÓ A GRELHA e a call dela NÃO correu — o dono "
                   "não consegue diagnosticar");
    }
    // religar a geometria volta a submeter malhas (o delta > 0)
    gldbg::g_masks.geometry = true;
    {
        const int de0 = glstub::stats.drawElementsCalls;
        frame();
        EXPECT_MSG(glstub::stats.drawElementsCalls > de0,
                   "0.12-Z: geometria re-ligada não submete malhas");
    }
    // o regresso ao normal: mestre OFF restaura TUDO byte a byte
    gldbg::g_masks = gldbg::Masks{};
    frame();
    const int litBack = countPixelsInRect(vr, isLitWhite);
    EXPECT_MSG(litBack > 12,
               "0.12-Z: mestre OFF não restaura o render normal (lit=%d)",
               litBack);

    gldbg::g_masks = gldbg::Masks{};
    onAppCmd(nullptr, APP_CMD_TERM_WINDOW);
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}

// ---------------------------------------------------------------------------
// 4. O SNAPSHOT DE ESTADO POR PASSE (a forma): o capture lê o stub, o
// describe imprime os campos, o differs deteta a mudança (o vazamento de
// estado entre passes é o que o passBoundary loga no device)
// ---------------------------------------------------------------------------
TEST(gldebug_pass_state) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));

    const gldbg::PassState a = gldbg::capture();
    char txt[128];
    gldbg::describe(a, txt, sizeof(txt));
    EXPECT_MSG(std::strstr(txt, "vp(") == txt,
               "0.12-Z: o describe não começa no viewport (='%s')", txt);
    EXPECT(std::strstr(txt, "fbo") != nullptr);

    gldbg::PassState b = a;
    EXPECT(!gldbg::differs(a, b));
    b.blend = a.blend ? GL_FALSE : GL_TRUE;
    EXPECT(gldbg::differs(a, b));

    // o checkErrors não lança (o stub devolve GL_NO_ERROR)
    gldbg::g_errorLog = true;
    gldbg::checkErrors("pin");
    gldbg::g_errorLog = false;
    EXPECT(true);
}

// ---------------------------------------------------------------------------
// 5. O GLRESOURCEMANAGER (0.12-Z F1): o registo na ORDEM da 0.6.7, o
// invalidateAll marca tudo inválido, o recreateAll repõe (e REPORTA a
// camada que falha — o lastFailed é o contrato), o recreateLayer
// individual serve o boot em passos.
// ---------------------------------------------------------------------------
// as camadas de TESTE do gestor (a ordem e a falha sob CONTROLO — o
// ambiente do hospedeiro tem fontes de sistema e não é determinístico)
namespace {
bool g_tA = false, g_tB = false, g_tC = false;   // os estados "vivos"
bool tInv(void* p) { *static_cast<bool*>(p) = false; return true; }
bool tRec(void* p) { *static_cast<bool*>(p) = true; return true; }
bool tRecFail(void*) { return false; }
} // namespace

TEST(glresources_ciclo) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetCtxTest();
    glstub::fb::enabled = false;
    javaRegistersGranted();
    bootEditor();

    // o arranque registou as 8 camadas do produto
    EXPECT_MSG(g_glResources.count() == 8,
               "0.12-Z F1: o gestor tem %u camadas (esperado 8)",
               (unsigned)g_glResources.count());

    // ---- o CICLO com camadas de TESTE (a ordem + a falha sob controlo) --
    {
        GLResourceManager m;
        m.registerLayer("a", &g_tA, tInv, tRec);
        m.registerLayer("b", &g_tB, tInv, tRecFail);   // o recreate MORRE
        m.registerLayer("c", &g_tC, tInv, tRec);
        g_tA = g_tB = g_tC = true;

        // o invalidate pela ORDEM e tudo inválido
        m.invalidateAll();
        EXPECT_MSG(!g_tA && !g_tB && !g_tC,
                   "0.12-Z F1: o invalidate não correu nas 3 camadas");

        // o recreateAll para no 1.º falhado (a camada b) e REPORTA
        g_tA = g_tB = g_tC = false;
        EXPECT_MSG(!m.recreateAll(),
                   "0.12-Z F1: o recreateAll devia falhar na camada b");
        EXPECT_MSG(std::strcmp(m.lastFailed(), "b") == 0,
                   "0.12-Z F1: o lastFailed é '%s' (esperado b)",
                   m.lastFailed());
        EXPECT_MSG(g_tA, "0.12-Z F1: a camada a não recriou antes do "
                         "falhar da b (a ordem do dono)");
        EXPECT_MSG(!g_tC, "0.12-Z F1: a camada c recriou DEPOIS do "
                          "falhar da b (o ciclo devia ter parado)");

        // o term duplo: só o que está VIVO morre — após o recreateAll
        // falhar na b, SÓ a camada "a" está viva (a 1.ª invalidate mata-a;
        // a 2.ª é um no-op)
        const u32 inv0 = m.invalidations();
        m.invalidateAll();
        m.invalidateAll();
        EXPECT_MSG(m.invalidations() == inv0 + 1,
                   "0.12-Z F1: o term duplo não é idempotente (%u de 1)",
                   (unsigned)(m.invalidations() - inv0));
    }

    // ---- as camadas GL do PRODUTO recriam individualmente (o boot em
    // passos do dono) — no hospedeiro SEM as fontes do sistema, as camadas
    // GL puras (sem texto) são o contrato testável
    EXPECT(g_glResources.recreateLayer("gpu-cube"));
    EXPECT(g_cubeMesh.ok());
    EXPECT(g_glResources.recreateLayer("grid"));
    EXPECT(g_glResources.recreateLayer("sky"));
    EXPECT(g_glResources.recreateLayer("particles"));
    EXPECT(g_glResources.recreateLayer("renderer"));
    EXPECT(g_glResources.isValid("grid"));
    EXPECT(g_glResources.isValid("renderer"));
}

// ---------------------------------------------------------------------------
// 6. A CÂMARA SÃ (0.12-Z F1): NaN/fora dos limites → o PADRÃO (a cena
// fica); câmara sã → SÓ os clamps de sempre (byte a byte).
// ---------------------------------------------------------------------------
TEST(camera_sana) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));

    // a câmara com NaN (o estado degenerado que um term a meio do gesto
    // pode deixar) repõe o padrão
    const f32 nan = std::numeric_limits<f32>::quiet_NaN();
    g_camera.target = Vec3{nan, 0.0f, 0.0f};
    cameraEnsureSane();
    EXPECT_MSG(nearEqF(g_camera.target.x, 0.0f, 1e-6f) &&
                   nearEqF(g_camera.dist, 6.0f, 1e-5f),
               "0.12-Z F1: a câmara com NaN não repôs o padrão "
               "(target.x=%f dist=%f)", (double)g_camera.target.x,
               (double)g_camera.dist);
    EXPECT(nearEqF(g_camera.fovY, 1.0472f, 1e-3f));

    // a distância fora dos limites clampa (sem reset — o zoom excessivo é
    // clamped, não destruído)
    g_camera.dist = 1e9f;
    cameraEnsureSane();
    EXPECT_MSG(g_camera.dist < 1e8f,
               "0.12-Z F1: a distância gigante não clampou (%f)",
               (double)g_camera.dist);

    // a câmara sã fica byte a byte (os clamps de sempre não mudam nada)
    const Camera sane;
    g_camera.target = sane.target;
    g_camera.yaw = sane.yaw;
    g_camera.pitch = sane.pitch;
    g_camera.dist = sane.dist;
    g_camera.fovY = sane.fovY;
    const f32 yaw0 = g_camera.yaw;
    cameraEnsureSane();
    EXPECT(nearEqF(g_camera.yaw, yaw0, 1e-7f));
    EXPECT(nearEqF(g_camera.dist, 6.0f, 1e-5f));
}
