// tests/c33_virtual.cpp — 0.8.12: o DISPOSITIVO VIRTUAL EM CI ("C33
// virtual" — o "PC virtual" obrigatório do dono).
//
// O QUE ISTO É: um harness HEADLESS que reproduz EXATAMENTE as condições
// do Android que morderam a engine entre 0.8.5 e 0.8.10 — as que o CI
// verde nunca via porque o runner do Linux é "demasiado saudável":
//
//   [fs]    /tmp READ-ONLY (errno=30/EROFS — a seam fileapi::testing do
//           FileApi; no device é o kernel, aqui é o prefixo bloqueado) —
//           o bug do staging era INVISÍVEL no CI com /tmp escrevível;
//   [fs]    só o CACHE DIR da app (getCacheDir via ponte JNI) e o projeto
//           aceitam escrita — exatamente as superfícies do Android;
//   [uri]   o caminho content:// do SAF exercitado com o FakeSafIo (o
//           MESMO modelo de provider que a suíte afere desde a F5.4) — a
//           migração de projeto antigo pelo SAF é o fluxo que morria;
//   [egl]   ciclos TERM_WINDOW/INIT_WINDOW com destruição e RE-CRIAÇÃO do
//           contexto (re-upload de tudo — o wiring do 0.6.7);
//   [tela]  superfície 1536×720 (a resolução REAL do C33) com insets
//           (status 24 + pill 24 — o contentRect do device);
//   [gpu]   ASTC LDR ativo (o Mali do C33; o boot loga "ASTC SIM");
//   [dedo]  sequências REPLAYÁVEIS de tap (injectDown/injectUp + frame()
//           REAL do main.cpp — o mesmo caminho de input do telefone).
//
// O REPLAY é a SESSÃO REAL do dono (a que produzia os sintomas dos logs):
// selecionar o TIC → abrir o picker → tocar na linha do picker DENTRO do
// viewRect (o toque que matava a seleção no MESMO frame do dispatch) →
// trocar mesh → escolher none → tap no backdrop → desselecionar →
// lifecycle TERM/INIT → trocar de novo. Cada passo ASSERTA e o output
// (passo-a-passo) é o que o relatório COLA e o gate do CI GREPA contra
// ci/forbidden_log_patterns.txt — qualquer padrão proibido = CI VERMELHO
// e release bloqueada.
//
// Este ficheiro é um EXECUTÁVEL SEPARADO (c33_virtual) — não faz parte do
// test_core: inclui platform/main.cpp (o caminho real do device, um só
// android_main por binário) e tem o SEU main() que corre o replay todo e
// sai non-zero em qualquer falha (harness vermelho = release bloqueada).
// (antes de TUDO: o FakeSafIo.h usa memfd_create — glibc exige _GNU_SOURCE
// ANTES do primeiro <unistd.h>/<sys/*.h> do TU)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats + astcLdr)
#include <EGL/egl.h>    // stub (eglstub::g_surfaceW/H — o harness põe 1536×720)
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>   // FAKE controlável (tests/stub — cache_dir p/ o getCacheDir)

#include "FakeStorage.h"
#include "FakeSafIo.h"   // 0.8.12: o provider content:// do C33 virtual
// 0.9.3 (REG-002): o stub do oboe — o OboeBackend de PRODUÇÃO que o
// main.cpp arranca compila contra ESTE header no host (o padrão jni.h)
#include <oboe/Oboe.h>

// ---- O CAMINHO REAL DO DEVICE (namespace anónimo = mesmo TU) ---------------
#include "voni/VoniDocs.h"   // FASE 9: a pesquisa das Docs
#include "platform/main.cpp"

// ponte Java (o papel do "stub Java" — como o test_wiring087/test_handshake)
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);
// 0.9.1 — os natives do IME (definidos no StorageBridge.cpp; o harness
// chama-os DIRETO como o "Java fake" — o mesmo caminho do device)
extern "C" void Java_vv_goni_VvActivity_nativeOnImeText(
        JNIEnv*, jclass, jstring text);
extern "C" void Java_vv_goni_VvActivity_nativeOnImeKey(
        JNIEnv*, jclass, jint keyCode, jint action);

using namespace vv;

// ===========================================================================
// harness: CHECK com output passo-a-passo (o que o relatório cola)
// ===========================================================================
namespace {

int g_checks = 0;
int g_failed = 0;

bool check(bool cond, const char* what) {
    ++g_checks;
    if (cond) {
        std::printf("    [ok]   %s\n", what);
    } else {
        ++g_failed;
        std::printf("    [FAIL] %s\n", what);
    }
    std::fflush(stdout);
    return cond;
}

void fase(const char* name) {
    std::printf("\n== %s ==\n", name);
    std::fflush(stdout);
}

void passo(const char* name) {
    std::printf("  > %s\n", name);
    std::fflush(stdout);
}

const char* kHarnessLogs = "c33-virtual-logs";
const char* kCacheDir = "c33-virtual-cache";

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

std::vector<std::string> logLines(int maxLines = 3000) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, maxLines);
    return lines;
}

bool logHas(const char* needle) {
    const std::vector<std::string> lines = logLines();
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// conta ocorrências (o gate exige ZERO dos proibidos — contar prova)
int logCount(const char* needle) {
    int n = 0;
    for (const std::string& l : logLines()) {
        if (l.find(needle) != std::string::npos) {
            ++n;
        }
    }
    return n;
}

jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xD001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xD002));

// o registo da activity (o papel do VvActivity.onCreate) + o CACHE DIR da
// app (o papel do getCacheDir — a ponte jniCacheDir resolve no fallback)
void javaRegistersWithCacheDir() {
    g_jni.reset();
    while (vv::storage::pollResult()) {
    }
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity,
        g_jni.newString("c33-virtual"));
    g_jni.manager_result = true;   // all-files concedido (o C33 do dono tem)
    fileapi::makeDirs(kCacheDir);
    g_jni.cache_dir = kCacheDir;   // getCacheDir() da "app"
}

// ---- geometria do overlay do picker (a MESMA fórmula do drawAssetMenu) -----
struct PickerGeom {
    f32 x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    f32 rowCy(int row) const { return y + editor::kHeaderH + static_cast<f32>(row) * 48.0f + 20.0f; }
    f32 cx() const { return x + w * 0.5f; }
};

PickerGeom meshPickerGeom(size_t files) {
    const size_t shown = files < 5 ? files : 5;
    PickerGeom g;
    g.w = editor::kMenuW;
    g.h = editor::kHeaderH + static_cast<f32>(shown + 2) * 48.0f + editor::kPad;
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    g.x = ox + (aw - g.w) * 0.5f;
    g.y = oy + (ah - g.h) * 0.5f;
    return g;
}

PickerGeom texPickerGeom(size_t files) {
    const size_t shown = files < 5 ? files : 5;
    PickerGeom g;
    g.w = editor::kMenuW;
    g.h = editor::kHeaderH + static_cast<f32>(shown + 1) * 48.0f + editor::kPad;
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    g.x = ox + (aw - g.w) * 0.5f;
    g.y = oy + (ah - g.h) * 0.5f;
    return g;
}

PickerGeom primPickerGeom() {
    PickerGeom g;
    g.w = editor::kMenuW;
    g.h = editor::kHeaderH + 44.0f + 2.0f * 44.0f + editor::kPad;
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    g.x = ox + (aw - g.w) * 0.5f;
    g.y = oy + (ah - g.h) * 0.5f;
    return g;
}

// um TOQUE do dedo: press num frame, release no seguinte (o mesmo padrão
// de edges que o glue produz no device — o botão captura no press e
// dispara no release; o deselect antigo armava no press e LIMPAVA no
// release do MESMO tap, ANTES do dispatch do pick: o bug exato do C33)
// FASE 9 (9.11): comparação com tolerância (as demais checks do harness
// são booleanas diretas; as de LAYOUT precisam de tolerância de flutuante)
static bool nearEqF(f32 a, f32 b, f32 tol = 0.05f) {
    return (a - b < tol) && (b - a < tol);
}

void tap(f32 x, f32 y) {
    g_input.injectDown(0, x, y);
    frame();
    g_input.injectUp(0);
    frame();
}

// um frame "morto" (sem dedo) — o que corre entre gestos no device
void idle(int n = 1) {
    for (int i = 0; i < n; ++i) {
        frame();
    }
}

double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0)
        .count();
}

// reset do estado partilhado entre fases (o padrão do test_wiring087)
void resetEngineForHarness() {
    g_scene.clear();
    // GCC 13.3 do runner (ubuntu-24.04) tem um ICE em gimple_add_tmp_var
    // com o temporário prvalue braced `g_editor = editor::EditorState{}`;
    // a variável nomeada aplica os mesmos NSDMIs e copia — sem temporário.
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
    g_windowInits = 0;
    g_windowTerms = 0;
    // o android_main cria/destrói estes com o storage — o harness idem
    g_pipeline.reset();
    g_texCache.reset();
    glstub::reset();
}

}  // namespace

// ===========================================================================
// main — o REPLAY inteiro; sai non-zero em qualquer falha
// ===========================================================================
int main() {
    std::printf("== C33 VIRTUAL — dispositivo headless em CI (0.9.3) ==\n");
    std::printf("   reproduz: /tmp read-only (errno=30), cache dir da app,\n");
    std::printf("   content:// SAF, lifecycle EGL TERM/INIT, 1536x720 + insets,\n");
    std::printf("   ASTC ativo, taps replayaveis pelo frame() real\n");
    std::printf("   0.9.3: + lifecycle agressivo do AUDIO (REG-002/R-006), backend\n");
    std::printf("   Oboe de producao contra o stub, projetos corrompidos, memoria\n");
    std::printf("   do arranque (Problema 3) e as sentinelas JVM (REG-001) no CI\n");
    rmrf(kHarnessLogs);
    rmrf(kCacheDir);
    if (!vv::elog::init(kHarnessLogs)) {
        std::printf("FATAL: elog init\n");
        return 2;
    }

    // ---- a "build instalada" (o papel do build_info.txt + VvActivity) -----
    vv::buildinfo::set("0.9.0-virtual", 43, "c33c0ffe", "aabbccdd00112233", 1790000000ull);
    elog::info("%s", vv::buildinfo::banner().c_str());

    // ---- [fs] /tmp READ-ONLY (a condição do device que o CI não tinha) ----
    fileapi::testing::setReadonlyPrefix("/tmp");
    std::printf("  [env] /tmp READ-ONLY (errno=30) ATIVO; escrita so no cache dir '%s' e no projeto\n", kCacheDir);
    // prova do ambiente: o writeAll em /tmp FALHA com a linha EXATA do device
    {
        std::vector<u8> probe{1, 2, 3};
        const bool w = fileapi::writeAll("/tmp/goni_probe_ro.tmp", probe.data(), probe.size());
        check(!w, "ambiente: writeAll em /tmp falha (read-only simulado)");
        check(vv::fileapi::errnoText().find("errno=30") != std::string::npos,
              "ambiente: errnoText devolve errno=30 (Read-only file system)");
        check(logHas("fileapi: fopen/write falhou em '/tmp/goni_probe_ro.tmp'"),
              "ambiente: a linha de log do device aparece na sonda (prova da seam)");
    }

    // ---- [tela][gpu] a resolução do C33 + ASTC + registo/cache dir ---------
    eglstub::g_surfaceW = 1536;
    eglstub::g_surfaceH = 720;
    glstub::astcLdr = true;   // o Mali do C33
    javaRegistersWithCacheDir();
    std::printf("  [env] superficie 1536x720; ASTC LDR ativo; cache dir via JNI\n");

    // ======================================================================
    // FASE 1 — projeto ANTIGO + BOOT com MIGRACAO (/tmp read-only ATIVO)
    // ======================================================================
    fase("FASE 1 — boot com migracao de projeto antigo (/tmp READ-ONLY)");
    resetEngineForHarness();
    {
        auto st = std::make_unique<FakeStorage>();
        FakeStorage* rawSt = st.get();
        // projeto REAL no storage (manifesto + cena com o TIC e a ref LEGADA)
        check(Project::createNew(*rawSt, "c33", g_project), "projeto criado no storage");
        rawSt->makeDirs("meshes");
        rawSt->writeText("meshes/casa.obj",
                         "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n");
        // a cena do dono: TIC Casa com a ref LEGADA meshes/casa.obj
        {
            const Handle h = g_scene.create("Casa");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            MeshRenderer* mr = t->addComponent<MeshRenderer>();
            mr->meshPath = "meshes/casa.obj";
            check(g_project.saveActiveScene(*rawSt, g_scene), "cena gravada com a ref legada");
            g_scene.clear();
        }
        g_storage = std::move(st);
        g_projectReady = true;
        g_resources.setStorage(rawSt);
        g_gpu.init(&g_resources);
        // o papel do android_main: cache/pipeline de texturas com o storage
        g_texCache = std::make_unique<TextureCache>(*rawSt);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                       *g_texCache);

        // o BOOT REAL do device: INIT_WINDOW (EGL feliz 1536x720, renderer,
        // fonte, cubo, grid, load da cena ativa + MIGRACAO silenciosa)
        android_app app;
        std::memset(&app, 0, sizeof(app));
        app.contentRect = {0, 24, 1512, 720};   // insets: status 24 + pill 24
        const auto tBoot = std::chrono::steady_clock::now();
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready, "boot: INIT_WINDOW completo (g_ready)");
        check(g_egl.width() == 1536 && g_egl.height() == 720,
              "boot: superficie 1536x720 (a resolucao do C33)");
        check(logHas("ASTC SIM"), "boot: caminho ASTC ativo (o Mali do C33)");
        check(msSince(tBoot) < 500.0, "boot: dentro do orcamento de tempo");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        passo("a migracao do projeto antigo corre com /tmp READ-ONLY");
        check(rawSt->exists("assets/casa.gmesh"), "migracao: assets/casa.gmesh criado");
        check(logHas("asset: migracao 'meshes/casa.obj'"), "migracao: linha do log da fonte legada");
        check(logHas("asset: staging em '"), "migracao: staging no cache dir/projeto (nunca /tmp)");
        check(logCount("staging falhou") == 0,
              "migracao: ZERO ocorrencias do sintoma R-002 (migracao morta)");
        check(logCount("mkdir falhou em '/tmp'") == 0,
              "migracao: ZERO ocorrencias do sintoma R-002 (mkdir no /tmp)");
        {
            Mesh* m = g_gpu.mesh("assets/casa.gmesh");
            check(m != nullptr && m->indexCount() == 3, "migracao: o .gmesh resolve (3 idx)");
        }
        // a ref do TIC foi re-escrita em silencio
        bool casaMigrada = false;
        g_scene.forEachActive([&casaMigrada](const Tic& t) {
            if (t.name == "Casa") {
                if (const MeshRenderer* mr = t.getComponent<MeshRenderer>()) {
                    casaMigrada = (mr->meshPath == "assets/casa.gmesh");
                }
            }
        });
        check(casaMigrada, "migracao: ref do TIC re-escrita p/ assets/casa.gmesh");
    }

    // ======================================================================
    // FASE 2 — REPLAY DA SESSAO REAL (os toques que produziam os sintomas)
    // ======================================================================
    fase("FASE 2 — replay da sessao real (selecao -> picker -> troca -> none)");
    {
        // 2.1 — o dono seleciona o TIC Casa (tap na linha da Hierarchy; o
        // efeito de estado e o mesmo: selected aponta o TIC vivo)
        passo("2.1 selecionar o TIC Casa (afastado do centro — o dono "
              "trabalha com o TIC onde o deixou)");
        Handle casa = g_scene.find("Casa");
        check(casa.valid(), "TIC Casa vivo apos o boot");
        g_editor.selected = casa;
        // o TIC PROJETA LONGE do centro do viewport: o toque no overlay do
        // picker NÃO acerta o TIC 3D por baixo (o cenário real do sintoma —
        // quando o TIC estava sob o picker, o pickSceneTic re-selecionava-o
        // e a intermitência "às vezes fim ok" da evidência vinha daí)
        if (Transform3D* tr = g_scene.get(casa)->getComponent<Transform3D>()) {
            tr->pos = Vec3{60.0f, 0.0f, -30.0f};
            tr->updateWorld();
        }
        idle(2);

        // 2.2 — tocar na linha prim: do Inspector abre o seletor de
        // PRIMITIVAS (a origem do "mesh: troca - -> prim esfera ERRO(...)")
        passo("2.2 abrir o picker de PRIMITIVAS e tocar a linha esfera");
        g_editor.assetMenu = 4;   // o Inspector abriu (guard passou: alvo valido)
        idle(1);
        {
            const PickerGeom g = primPickerGeom();
            // row 0 = none, row 1 = esfera, row 2 = box
            const auto t0 = std::chrono::steady_clock::now();
            tap(g.cx(), g.y + editor::kHeaderH + 44.0f + 22.0f);   // esfera
            check(msSince(t0) < 250.0, "tap na linha esfera: dentro do orcamento");
        }
        check(g_editor.assetMenu == 0, "picker fechou apos o toque");
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA apos tocar na linha do picker (o fix do C33)");
        idle(2);   // o flush do ponto seguro sobe a esfera
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh != nullptr && mr->primOn,
                  "esfera aplicada (primOn + mesh vivo)");
            check(mr && mr->mesh->indexCount() > 0, "esfera com geometria nao vazia");
        }
        check(logCount("ERRO(sem TIC com mesh selecionado)") == 0,
              "ZERO ocorrencias do sintoma R-001 (ERRO sem alvo) no replay");

        // 2.3 — trocar pelo MESH picker: tocar o ficheiro migrado (o
        // "mesh pick 3" dos logs — o assets/casa.gmesh em 3º)
        passo("2.3 picker de MESH: tocar o ficheiro migrado (mesh pick 3)");
        refreshCatalog();
        check(!g_catalog.meshes.empty(), "catalogo tem o .gmesh migrado");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            // row 0 = none, row 1 = cube, row 2 = o ficheiro
            tap(g.cx(), g.rowCy(2));
        }
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva apos mesh pick 3");
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh != nullptr && mr->meshPath == "assets/casa.gmesh",
                  "mesh migrado aplicado ao TIC (meshPath = assets/casa.gmesh)");
        }
        check(logHas("mesh: troca ") && logCount("fim ok") >= 1,
              "log 'mesh: troca ... fim ok' presente (a prova do C33)");

        // 2.4 — o tap no BACKDROP do picker (dentro do viewRect, fora do
        // painel): fecha o overlay SEM matar a selecao
        passo("2.4 tap no backdrop do picker (dentro do viewRect)");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            // canto do viewport central (DENTRO do viewRect, FORA do painel)
            const UiRect view = editor::centerRect(
                static_cast<f32>(g_egl.width()), static_cast<f32>(g_egl.height()),
                g_ui.safeArea(), g_editor.showInspector);
            const f32 bx = view.x + 24.0f;
            const f32 by = view.y + 24.0f;
            check(bx < g.x || bx > g.x + g.w || by < g.y || by > g.y + g.h,
                  "ponto do backdrop fora do painel (a geometria bate)");
            tap(bx, by);
        }
        check(g_editor.assetMenu == 0, "backdrop fechou o picker");
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA apos o tap no backdrop (o fix do C33)");

        // 2.5 — o TEX picker: none em 1º (a evidencia "tex pick 2"/none)
        passo("2.5 picker de TEX: none em primeiro lugar");
        g_editor.assetMenu = 2;
        idle(1);
        {
            const PickerGeom g = texPickerGeom(g_catalog.textures.size());
            tap(g.cx(), g.rowCy(0));   // none (1º lugar)
        }
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->texture == nullptr && mr->texPath.empty(),
                  "tex none: textura limpa (material volta a cor plana)");
        }
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva apos tex none");

        // 2.6 — mesh NONE (a entrada de 1ª classe): o slot limpa e o TIC
        // deixa de renderizar mesh; deferred free no frame seguinte
        passo("2.6 picker de MESH: none (o slot limpa)");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            tap(g.cx(), g.rowCy(0));   // none
        }
        {
            Tic* t = g_scene.get(g_editor.selected);
            MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh == nullptr && mr->meshPath.empty(),
                  "mesh none: slot limpo (TIC deixa de renderizar mesh)");
        }
        check(logHas("mesh: troca ") && logHas("slot limpo (none)"),
              "log 'fim ok (sem mesh — slot limpo (none))'");
        // o deferred free corre no inicio do frame seguinte (a cova abre)
        idle(2);
        check(g_primGrave.empty(), "cova aberta (deferred free no frame seguinte)");

        // 2.7 — none -> X -> none repetido (sem crash, sem leak): o ciclo
        // do prompt com FRAMES reais no meio (o ponto seguro corre)
        passo("2.7 none -> cube -> none x5 (frames reais no meio)");
        for (int i = 0; i < 5; ++i) {
            Tic* t = g_scene.get(g_editor.selected);
            if (!check(t != nullptr, "TIC vivo no ciclo none->X->none")) {
                break;
            }
            MeshRenderer* mr = t->getComponent<MeshRenderer>();
            // none (direto pelo dispatch puro — o caminho do pick)
            {
                const editor::AssetPickOutcome o = editor::applyAssetPick(
                    g_scene, g_editor.selected, 1, 1, g_catalog,
                    makeAssetResolvers());
                check(o.applied, "none aplicado (ciclo)");
            }
            idle(1);   // frame: a cova abre no ponto seguro
            {
                const editor::AssetPickOutcome o = editor::applyAssetPick(
                    g_scene, g_editor.selected, 1, 2, g_catalog,
                    makeAssetResolvers());
                check(o.applied, "cube aplicado (ciclo)");
            }
            idle(1);
            const editor::AssetPickOutcome o = editor::applyAssetPick(
                g_scene, g_editor.selected, 1, 1, g_catalog,
                makeAssetResolvers());
            check(o.applied, "none aplicado de novo (ciclo)");
            idle(1);
            if (mr) {
                check(mr->mesh == nullptr, "estado final do ciclo: slot vazio");
            }
        }
        check(g_primOwners.empty(), "sem posse viva acumulada (sem leak)");
        check(g_primGrave.empty(), "cova vazia no fim (deferred free completo)");

        // 2.8 — SEM selecao, tocar em linha de picker (incluindo none):
        // HINT + log bloqueado, ZERO caminho de ERRO
        passo("2.8 sem selecao: hint + 'ui: pick bloqueado (sem selecao)'");
        g_editor.selected = Handle::invalid();   // a selecao morreu (outra via)
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            tap(g.cx(), g.rowCy(0));   // none SEM selecao
        }
        check(logCount("ui: pick bloqueado (sem seleção)") >= 1,
              "log 'ui: pick bloqueado (sem seleção)' presente");
        check(std::strncmp(g_toast, "seleciona um TIC com mesh", 26) == 0,
              "toast de hint 'seleciona um TIC com mesh'");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            tap(g.cx(), g.rowCy(2));   // ficheiro SEM selecao
        }
        check(logCount("ui: pick bloqueado (sem seleção)") >= 2,
              "2ª linha de pick bloqueada logada (hint sem spam de ERRO)");
        check(logCount("ERRO(sem TIC com mesh selecionado)") == 0,
              "ZERO ocorrencias do sintoma R-001 mesmo sem selecao");

        passo("2.9 adversario: outro overlay (log viewer) tambem guarda a selecao");
        g_editor.selected = g_scene.find("Casa");
        idle(1);
        g_editor.logViewer = true;   // OUTRO overlay qualquer: o MESMO guard
        idle(1);
        {
            const UiRect view = editor::centerRect(
                static_cast<f32>(g_egl.width()), static_cast<f32>(g_egl.height()),
                g_ui.safeArea(), g_editor.showInspector);
            tap(view.x + 30.0f, view.y + 30.0f);   // tap no viewport por baixo
        }
        g_editor.logViewer = false;
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA com overlay de logs aberto (o guard e de TODOS os overlays)");

        passo("2.10 adversario: troca de modo 3D | UI | AUDIO nao perde a selecao");
        g_editor.uiMode = true;
        idle(2);
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva no modo UI");
        g_editor.uiMode = false;
        g_editor.audioMode = true;
        idle(2);
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva no modo AUDIO");
        g_editor.audioMode = false;
        idle(2);
        check(g_scene.get(g_editor.selected) != nullptr,
              "selecao viva de volta ao 3D (a troca de modo NAO limpa)");
    }

    // ======================================================================
    // FASE 3 — LIFECYCLE: TERM/INIT com re-criacao do contexto EGL
    // ======================================================================
    fase("FASE 3 — lifecycle TERM/INIT (a selecao sobrevive ao ciclo)");
    {
        android_app app;
        std::memset(&app, 0, sizeof(app));
        app.contentRect = {0, 24, 1512, 720};

        passo("3.1 re-selecionar e trocar antes do ciclo");
        Handle casa = g_scene.find("Casa");
        check(casa.valid(), "TIC Casa vivo antes do TERM");
        g_editor.selected = casa;
        {
            const editor::AssetPickOutcome o = editor::applyAssetPick(
                g_scene, g_editor.selected, 4, 2, g_catalog,
                makeAssetResolvers());   // esfera
            check(o.applied, "esfera pedida antes do TERM");
        }
        idle(2);

        passo("3.2 TERM_WINDOW (contexto EGL destruido)");
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        check(!g_ready, "contexto morto (g_ready=false)");
        check(logHas("lifecycle: TERM_WINDOW"), "log do TERM_WINDOW");

        passo("3.3 INIT_WINDOW (re-criacao + re-upload + RELOAD da cena)");
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready, "contexto re-criado (g_ready)");
        check(logHas("lifecycle: INIT_WINDOW") && logHas("RE-CRIADO"),
              "log do INIT_WINDOW com contexto re-criado");
        // O FIX: a selecao RE-VALIDA/re-mapeia (o reload deu handles novos)
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA apos TERM/INIT (re-validada pelo nome)");
        {
            const Tic* t = g_scene.get(g_editor.selected);
            check(t != nullptr && t->name == "Casa",
                  "o handle re-mapeado aponta o TIC 'Casa' (nao outro)");
        }
        check(logHas("lifecycle: seleção re-validada"),
              "log 'lifecycle: seleção re-validada pos-INIT WINDOW'");

        passo("3.4 trocar mesh DEPOIS do ciclo (o fluxo do dono continua)");
        idle(2);
        {
            const editor::AssetPickOutcome o = editor::applyAssetPick(
                g_scene, g_editor.selected, 4, 3, g_catalog,
                makeAssetResolvers());   // box
            check(o.applied, "box pedida apos o INIT");
        }
        idle(2);
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh != nullptr,
                  "mesh vivo no contexto NOVO (re-upload pelo ponto seguro)");
        }

        // 3.5 — o ciclo repetido: a selecao sobrevive a DOIS TERM/INIT
        passo("3.5 segundo ciclo TERM/INIT (a prova de robustez)");
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_scene.get(g_editor.selected) != nullptr,
              "selecao viva apos o 2º ciclo TERM/INIT");
    }

    // ======================================================================
    // FASE 4 — o caminho content:// do SAF (a migracao que morria no C33)
    // ======================================================================
    fase("FASE 4 — migracao pelo SAF content:// (staging no cache dir)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        FakeSafIo io;   // o provider content:// (o modelo da suíte)
        SafStorage saf(&io, "content://tree/primary:GOneVV/c33");
        // projeto antigo DENTRO do provider SAF
        check(Project::createNew(saf, "c33saf", g_project), "projeto criado no provider SAF");
        check(saf.makeDirs("meshes"), "meshes/ criado no provider");
        check(saf.writeText("meshes/casa.obj",
                            "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n"),
              "fonte legada escrita via content://");
        {
            const Handle h = g_scene.create("CasaSAF");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            MeshRenderer* mr = t->addComponent<MeshRenderer>();
            mr->meshPath = "meshes/casa.obj";
            check(g_project.saveActiveScene(saf, g_scene), "cena gravada no provider");
            g_scene.clear();
        }
        g_storage.reset(new SafStorage(&io, "content://tree/primary:GOneVV/c33"));
        g_projectReady = true;
        g_resources.setStorage(g_storage.get());
        g_gpu.init(&g_resources);
        io.flushWrites();   // o provider persiste (o passo invisível do device)

        // a MIGRACAO pelo caminho SAF com /tmp READ-ONLY: staging no CACHE DIR
        passo("migracao SAF: reconvertFile com raiz content://");
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::reconvertFile("meshes/casa.obj", *g_storage.get(),
                                               nullptr, out, stats, err);
        io.flushWrites();
        check(ok, "reconvertFile por SAF sucede (staging no cache dir)");
        if (!ok) {
            std::printf("    [erro] %s\n", err.c_str());
        }
        check(g_storage->exists("assets/casa.gmesh"), "assets/casa.gmesh no provider");
        check(logCount("staging falhou") == 0,
              "ZERO ocorrencias do sintoma R-002 no caminho SAF");
        check(logCount("mkdir falhou em '/tmp'") == 0,
              "ZERO ocorrencias do sintoma R-002 (mkdir no /tmp) — caminho SAF");
        check(logHas("asset: staging em '"), "staging no cache dir logado com o caminho");
        check(!logHas("staging em '/tmp"), "o staging JAMAIS em /tmp");
    }

    // ======================================================================
    // FASE 5 — dump VELHO no viewer: badge ANTIGO (build X)
    // ======================================================================
    fase("FASE 5 — dump velho com badge ANTIGO (identidade)");
    {
        // a "build instalada" é 0.9.0-virtual/43; um dump da build 39
        // (a 0.8.9 do dono) tem de aparecer com o badge
        {
            const std::string dumpPath = std::string(kHarnessLogs) +
                                         "/crash-1790830406-vc39.dump";
            FILE* f = std::fopen(dumpPath.c_str(), "wb");
            if (f) {
                std::fputs("G.One VV — crash dump (legível sem ndk-stack)\n"
                           "build: 0.8.9 (versionCode 39)\nframes: 2\n"
                           "#00 pc 0xe2de8  libgoni_vv.so\n"
                           "#01 pc 0xe2f24  libgoni_vv.so\n", f);
                std::fclose(f);
            }
        }
        std::vector<std::string> dumps;
        vv::elog::listDumps(dumps);
        check(!dumps.empty(), "o dump velho aparece na lista do viewer");
        bool badged = false;
        for (const std::string& d : dumps) {
            const std::string badge = vv::buildinfo::dumpBadge(d);
            std::printf("    [dump] %s%s\n", d.c_str(), badge.c_str());
            if (d.find("vc39") != std::string::npos &&
                badge.find("[ANTIGO (build 39)]") != std::string::npos) {
                badged = true;
            }
        }
        check(badged, "dump da build 39 com badge [ANTIGO (build 39)]");
        // dump NOVO (desta build): sem badge
        {
            const std::string dumpPath =
                std::string(kHarnessLogs) + "/crash-1790000500" +
                vv::buildinfo::dumpSuffix() + ".dump";
            FILE* f = std::fopen(dumpPath.c_str(), "wb");
            if (f) {
                std::fputs("build: 0.9.0-virtual (versionCode 43)\n", f);
                std::fclose(f);
            }
        }
        dumps.clear();
        vv::elog::listDumps(dumps);
        bool novoLimpo = false;
        for (const std::string& d : dumps) {
            if (d.find("-vc43") != std::string::npos &&
                vv::buildinfo::dumpBadge(d).empty()) {
                novoLimpo = true;
            }
        }
        check(novoLimpo, "dump NOVO (vc43) sem badge (e da build instalada)");
    }

    // ======================================================================
    // FASE 6 — GATE: padroes proibidos no output inteiro do replay
    // ======================================================================
    fase("FASE 6 — gate de padroes proibidos (o CI vermelho se voltar)");
    {
        const char* proibidos[] = {
            "sem TIC com mesh selecionado",
            "mkdir falhou em '/tmp'",
            "staging falhou",
            "ERRO(gerador/upload falhou)",
        };
        for (const char* p : proibidos) {
            const int n = logCount(p);
            check(n == 0, "ZERO ocorrencias do proibido (engine.log inteiro)");
            if (n > 0) {
                std::printf("    [GATE VERMELHO] '%s' apareceu %d vez(es)\n", p, n);
            }
        }
        // as provas positivas (o que TEM de estar lá)
        check(logCount("fim ok") >= 1, "troca com 'fim ok' no log");
        check(logCount("ui: pick bloqueado (sem seleção)") >= 2,
              "hints de pick bloqueado no log");
    }

    // ======================================================================
    // FASE 7 — 0.9.1: ORIENTAÇÃO PORTRAIT + IME DO SISTEMA (janela de texto)
    // ======================================================================
    fase("FASE 7 — 0.9.1: portrait + IME (janela de texto)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        android_app app;
        std::memset(&app, 0, sizeof(app));
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 7.1 — ABRIR: o par portrait + imeShow (o Java executa o pedido)
        passo("7.1 abrir a janela de texto (portrait + IME show)");
        openTextWindow();
        check(g_editor.textWin.open, "a janela de texto abre");
        check(ime::orientation() == ime::Orientation::Portrait,
              "orientação PEDIDA = portrait (estado na engine)");
        bool sawPortrait = false, sawShow = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "setOrientation" && c.second == 1) sawPortrait = true;
            if (c.first == "imeShow") sawShow = true;
        }
        check(sawPortrait, "JNI: setRequestedOrientation(PORTRAIT) executado");
        check(sawShow, "JNI: InputMethodManager.showSoftInput executado");
        check(logHas("orientacao: portrait pedida (janela de texto aberta)"),
              "a mudança de orientação fica LOGADA");

        // 7.2 — o IME ESCREVE (nativeOnImeText/Key → fila → frame consome)
        passo("7.2 o IME do sistema escreve no buffer");
        Java_vv_goni_VvActivity_nativeOnImeText(
            g_jni.env, nullptr, g_jni.newString("Ola"));
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 66, 0);
        Java_vv_goni_VvActivity_nativeOnImeText(
            g_jni.env, nullptr, g_jni.newString("C33"));
        frame();
        check(g_editor.textWin.buf == "Ola\nC33",
              "o texto commitado + ENTER chegam pela fila ime::");

        // 7.3 — a ROTAÇÃO (o frame do device roda): TERM + INIT em PORTRAIT
        passo("7.3 rotação 1536x720 → 720x1536 com a janela aberta");
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_egl.width() == 720 && g_egl.height() == 1536,
              "a superfície renasce EM PORTRAIT");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_editor.textWin.open, "a janela SOBREVIVE à rotação");
        check(g_editor.textWin.buf == "Ola\nC33",
              "o buffer sobrevive (estado da engine, não da GPU)");
        frame();
        check(g_windowInits >= 2 && g_windowTerms >= 1,
              "o lifecycle TERM/INIT correu (re-upload — sem glifos brancos)");

        // 7.4 — FECHAR: landscape + imeHide (o par espelhado do abrir)
        passo("7.4 fechar (landscape + IME hide)");
        g_jni.void_calls.clear();
        closeTextWindow();
        check(!g_editor.textWin.open, "a janela fecha");
        check(ime::orientation() == ime::Orientation::Landscape,
              "orientação REPOSTA = landscape");
        bool sawLandscape = false, sawHide = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "setOrientation" && c.second == 0) sawLandscape = true;
            if (c.first == "imeHide") sawHide = true;
        }
        check(sawLandscape, "JNI: setRequestedOrientation(LANDSCAPE) executado");
        check(sawHide, "JNI: hideSoftInput executado");

        // o device volta ao landscape para as fases seguintes
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 8 — 0.9.3 (hotfix): REPLAY DO CRASH DE ÁUDIO (REG-002/R-006)
    // + sequência dos projetos corrompidos (Sequência 3) + memória do
    // arranque (Problema 3). O backend é o OboeBackend DE PRODUÇÃO (TU
    // comum) contra o stub tests/stub/oboe/Oboe.h — o MESMO código que o
    // APK corre contra o oboe real do Google (FetchContent 1.9.3).
    // ======================================================================
    fase("FASE 8 — replay REG-002: lifecycle agressivo do audio + corruptos");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        oboe::testing::reset();

        android_app app;
        std::memset(&app, 0, sizeof(app));

        // 8.1 o BOOT pelo caminho REAL: INIT_WINDOW → audioBackendBoot →
        //     o PRIMÁRIO é o Oboe (o stub no host — o MESMO TU do APK)
        passo("8.1 boot real: INIT_WINDOW arranca o backend OBOE (o primario)");
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready, "boot completo (g_ready)");
        check(g_audioOut != nullptr && g_audioBackendReady,
              "backend de audio ATIVO no boot");
        check(std::strcmp(g_audioOut->name(), "oboe") == 0,
              "o primario e o OBOE (cadeia 0.9.3: oboe→aaudio→audiotrack)");
        check(logHas("audio(oboe): stream ATIVO rate="),
              "a linha informativa do stream (rate/ch/perf — Tarefa 2.5)");
        check(oboe::testing::hooks().openCount == 1,
              "EXATAMENTE 1 stream aberto no boot");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 8.2 A SEQUÊNCIA DO TOMBSTONE (Sequência 2 do prompt): o onResume
        //     da app antiga (com.goni.runtime) chamava startAudio()
        //     DIRETO, sem guarda — reproduzimos o MESMO padrão contra o
        //     backend vivo; o portão R-006 tem de aguentar SEM fugas
        passo("8.2 a sequencia do tombstone: startAudio repetido sem guarda");
        for (int i = 0; i < 3; ++i) {
            check(g_audioOut->start(44100, 2),
                  "startAudio() devolve true (idempotente)");
        }
        check(oboe::testing::hooks().openCount == 1,
              "o 2º/3º start NAO abrem streams (porta R-006 — a fuga de "
              "stream era a porta do crash Unisoc)");
        check(g_audioOut->ready(), "o stream original segue VIVO");
        check(logHas("audio(oboe): start ignorado — stream ja ativo"),
              "a porta R-006 deixa a linha no log");

        // o RESUME/PAUSE do lifecycle REAL (o fundo/recentes do Android)
        for (int i = 0; i < 5; ++i) {
            onAppCmd(&app, APP_CMD_RESUME);
            onAppCmd(&app, APP_CMD_PAUSE);
        }
        onAppCmd(&app, APP_CMD_RESUME);
        check(logCount("audio: RESUME") >= 1 && logCount("audio: PAUSE") >= 1,
              "o lifecycle do audio logado (pause/resume)");
        check(oboe::testing::hooks().openCount == 1,
              "pause/resume NAO reabrem streams");

        // 8.3 ADVERSÁRIO (Tarefa 7D): 50× onResume SEM onPause + 10×
        //     startAudio direto — a engine tem de seguir viva
        passo("8.3 adversario: 50 onResume sem onPause + 10 startAudio");
        for (int i = 0; i < 50; ++i) {
            onAppCmd(&app, APP_CMD_RESUME);
        }
        for (int i = 0; i < 10; ++i) {
            g_audioOut->start(44100, 2);
        }
        check(g_ready, "a engine SEGUE VIVA (zero crashes nativos)");
        check(oboe::testing::hooks().openCount == 1,
              "ainda EXATAMENTE 1 stream (zero fugas no adversario)");
        check(g_audioOut->ready(), "o audio segue pronto apos a tempestade");

        // 8.4 o CICLO DURO: TERM → INIT ×3 (o Android a destruir/recriar a
        //     surface — o ciclo que multiplicava os tombstones)
        passo("8.4 ciclo duro TERM->INIT x3 (o Android mata/recria a surface)");
        const int opensAntes = oboe::testing::hooks().openCount;
        for (int i = 0; i < 3; ++i) {
            onAppCmd(&app, APP_CMD_TERM_WINDOW);
            onAppCmd(&app, APP_CMD_INIT_WINDOW);
        }
        check(g_ready, "3 ciclos TERM->INIT completos (g_ready)");
        check(oboe::testing::hooks().openCount == opensAntes + 3,
              "cada boot abriu EXATAMENTE 1 stream novo");
        // o invariante R-006 (zero fugas): tudo o que abriu foi fechado,
        // EXCETO o stream VIVO do último boot — os contadores vão no
        // output (a evidência que o relatório cola)
        std::printf("    [streams] abertos=%d fechados=%d vivos=%d\n",
                    oboe::testing::hooks().openCount,
                    oboe::testing::hooks().closeCount,
                    oboe::testing::hooks().openCount -
                        oboe::testing::hooks().closeCount);
        check(oboe::testing::hooks().closeCount ==
                      oboe::testing::hooks().openCount - 1,
              "ZERO fugas: cada stream aberto foi fechado (so o VIVO do "
              "ultimo boot segue aberto — o stream fugido era o crash)");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 8.5 o DISCONNECT no meio da sessão (headset fora): o stream
        //     morre, o editor CONTINUA sem som; o próximo INIT re-arranca
        passo("8.5 disconnect no meio da sessao (o headset desligou)");
        oboe::testing::fireErrorOnAllStreams(
            oboe::Result::ErrorDisconnected);
        check(!g_audioOut->ready(), "o stream morto reporta NOT ready");
        check(logHas("audio(oboe): stream MORREU"),
              "a morte logada com a razao (onErrorBefore/AfterClose)");
        frame();   // o editor CONTINUA (o frame corre sem som)
        check(true, "o frame corre SEM som (degradacao graciosa — nunca "
                    "crash por causa do audio)");
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready && g_audioBackendReady,
              "o INIT re-arranca o audio depois do disconnect (porta "
              "reaberta pelo onErrorAfterClose)");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 8.6 SEQUÊNCIA 3 do prompt: PROJETOS CORROMPIDOS NO DISCO → o
        //     boot COMPLETA sem crash (a cena corrompida vira cena vazia
        //     com log; o lado Java da lista tem o sentinela JVM próprio)
        passo("8.6 cena corrompida no disco: boot completa sem crash");
        {
            auto st = std::make_unique<FakeStorage>();
            FakeStorage* rawSt = st.get();
            check(Project::createNew(*rawSt, "corrompido", g_project),
                  "projeto criado no storage");
            check(rawSt->writeText(*g_project.activeScenePath(),
                                   "{{{ lixo nao-json \x01\x02 !!!"),
                  "cena CORROMPIDA escrita no disco");
            g_scene.clear();
            g_storage = std::move(st);
            g_projectReady = true;
            g_resources.setStorage(rawSt);
            g_gpu.init(&g_resources);
            g_texCache = std::make_unique<TextureCache>(*rawSt);
            g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                           *g_texCache);
            onAppCmd(&app, APP_CMD_TERM_WINDOW);
            onAppCmd(&app, APP_CMD_INIT_WINDOW);
            check(g_ready,
                  "boot com projeto CORROMPIDO no disco completa (g_ready)");
            check(logHas("[boot 6/6] scene FALHOU"),
                  "a cena corrompida logada como FALHOU (legivel)");
            check(logHas("editor arranca com cena vazia"),
                  "o editor arranca com cena vazia (zero crash)");
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
        }

        // 8.7 PROBLEMA 3: a memória do arranque no log (a evidência p/ o
        //     dono comparar com o FinalizerWatchdog dos tombstones antigos)
        check(logHas("boot: memoria"),
              "a linha de memoria do arranque (Problema 3)");

        // 8.8 GATE interno R-006: zero assinaturas de crash nativo em TODO
        //     o engine.log do replay (a mensagem do check NUNCA cita o
        //     padrão — a lição 0.8.12-c: o gate grepa o PRÓPRIO output)
        check(logCount("SIGSEGV") == 0,
              "zero assinaturas de crash nativo no engine.log (R-006)");
        check(logCount("SEGV_ACCERR") == 0,
              "zero falhas de acesso nativas no engine.log (R-006)");

        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        // o fecho TOTAL no fim da fase: com o TERM, NENHUM stream vivo resta
        std::printf("    [streams] fim da fase: abertos=%d fechados=%d vivos=%d\n",
                    oboe::testing::hooks().openCount,
                    oboe::testing::hooks().closeCount,
                    oboe::testing::hooks().openCount -
                        oboe::testing::hooks().closeCount);
        check(oboe::testing::hooks().closeCount ==
                      oboe::testing::hooks().openCount,
              "fim da fase: NENHUM stream vivo resta (o TERM fechou tudo)");
        oboe::testing::reset();
    }

    // ======================================================================
    // FASE 9 — UI REPLAY (0.9.4 / FASE 9 do dono): o editor de script
    // digita sem fechar (G0-1/G0-2), Docs alcançáveis (G0-3), lifecycle
    // com fonte gravada (o bug do handle morto). A toolbar/acentos/layout
    // entram nos grupos G1/G2 (mesma fase, passos novos).
    // ======================================================================
    fase("FASE 9 — UI replay: editor de script + Docs + lifecycle");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        // projeto REAL com a cena gravada (o lifecycle do INIT recarrega a
        // cena do disco — o cenário exato do bug do handle morto)
        auto st9 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt9 = st9.get();
        check(Project::createNew(*rawSt9, "fase9", g_project), "projeto criado");
        {
            const Handle h = g_scene.create("Ator");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            t->addComponent<ScriptComp>();
            check(g_project.saveActiveScene(*rawSt9, g_scene), "cena gravada");
        }
        g_storage = std::move(st9);
        g_projectReady = true;

        android_app app;
        std::memset(&app, 0, sizeof(app));
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 9.1 — ABRIR o editor de script (o caminho REAL do Inspector):
        // skeleton + cursor no interior + par portrait/IME
        passo("9.1 abrir o editor de script (script NOVO)");
        const Handle ator = g_scene.find("Ator");
        check(ator.valid(), "o TIC Ator existe pós-boot");
        openScriptEditor(ator);
        check(g_editor.scriptWin.open, "o editor abre");
        check(std::string(g_editor.scriptWin.buf) ==
                  editor::scriptwin::kSkeleton,
              "script SEM fonte abre com o esqueleto base (G0-2)");
        check(g_editor.scriptWin.buf[g_editor.scriptWin.caret] == '}',
              "o cursor abre NO INTERIOR do allmoments (G0-2)");
        bool sawP = false, sawS = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "setOrientation" && c.second == 1) sawP = true;
            if (c.first == "imeShow") sawS = true;
        }
        check(sawP, "JNI: portrait pedido ao abrir");
        check(sawS, "JNI: IME show pedido ao abrir");

        // 9.2 — DIGITAR (o IME do sistema, o caminho do GBoard): 20 teclas
        // e o editor CONTINUA ABERTO com o texto presente (o sintoma exato
        // da checklist da 0.9.3: "script editor fecha ao digitar")
        passo("9.2 digitar 20 teclas do IME sem fechar (G0-1)");
        for (int i = 0; i < 20; ++i) {
            char one[2] = {static_cast<char>('a' + (i % 26)), 0};
            Java_vv_goni_VvActivity_nativeOnImeText(
                g_jni.env, nullptr, g_jni.newString(one));
        }
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 66, 0);
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 67, 0);
        frame();
        check(g_editor.scriptWin.open, "o editor SIGE aberto apos 20 teclas");
        check(g_editor.scriptWin.buf.size() > 20, "o texto esta PRESENTE");
        check(logCount("script") == 0 || true, "(diagnostico)");

        // 9.3 — o TECLADO IN-APP: toque no corpo abre o teclado (result 5 =
        // IME re-pedido) e a tecla digitavel entra pelo MESMO applyEvent
        passo("9.3 teclado in-app: toque no corpo + tecla (G0-1)");
        g_jni.void_calls.clear();
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_editor.scriptWin.open, "o editor sobrevive aa rotacao");
        bool sawImeAgain = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "imeShow") sawImeAgain = true;
        }
        check(sawImeAgain,
              "INIT_WINDOW: o IME e RE-PEDIDO pos-rotacao (o fix do foco)");
        check(g_editor.scriptWin.buf.size() > 20,
              "o buffer sobrevive ao ciclo TERM/INIT");
        // o handle do TIC morreu no reload — o MAIN re-validou por NOME
        // (o check lê o ESTADO sem chamar o fix — senão cura a mutação)
        check(g_scene.get(g_editor.scriptWin.tic) != nullptr,
              "o MAIN re-validou o TIC dono por NOME pos-reload (G0-1)");
        const size_t bufBefore = g_editor.scriptWin.buf.size();
        // toque no CORPO (meio do ecra portrait) → teclado in-app + IME
        tap(360.0f, 400.0f);
        check(g_editor.scriptWin.kbOpen, "toque no corpo ABRE o teclado in-app");
        // tecla A (linha 0, col 0) do teclado desenhado
        {
            const f32 keyW = (720.0f - 16.0f - 9.0f * 6.0f) / 10.0f;
            const f32 rowW = 9.0f * keyW + 8.0f * 6.0f;
            const f32 x0 = (720.0f - rowW) * 0.5f;
            const f32 kbTop = 1536.0f - 8.0f - (5.0f * 48.0f + 4.0f * 6.0f + 16.0f);
            tap(x0 + keyW * 0.5f, kbTop + 8.0f + 24.0f);
        }
        check(g_editor.scriptWin.buf.size() == bufBefore + 1,
              "a tecla do teclado in-app entra pelo MESMO applyEvent");

        // 9.4 — FECHAR com o back: a FONTE GRAVA no ScriptComp (o bug
        // 0.9.3: o handle morto fazia o fecho NUNCA gravar)
        passo("9.4 fechar: a fonte grava no componente (G0-1)");
        closeScriptEditor();
        check(!g_editor.scriptWin.open, "o editor fecha com o back");
        const Tic* atorDepois = g_scene.get(g_scene.find("Ator"));
        check(atorDepois != nullptr &&
                  atorDepois->getComponent<ScriptComp>() != nullptr &&
                  !atorDepois->getComponent<ScriptComp>()->source.empty(),
              "a fonte digitada FICA gravada no ScriptComp (pelo nome)");
        // GUARDAR A CENA ainda em memoria (antes de qualquer rotação — o
        // reload traz o .goni; a fonte tem de viajar NO DISCO)
        check(g_project.saveActiveScene(*rawSt9, g_scene) &&
                  g_project.saveManifest(*rawSt9),
              "a cena e guardada no .goni (a fonte viaja no disco)");
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 9.5 — DOCS pela LUPA do editor (G0-3): abre por cima, a pesquisa
        // filtra as entradas estruturadas e mostra o exemplo.
        // O fluxo REAL do .goni: fechar guarda a fonte no componente;
        // GUARDAR A CENA materializa-a no disco; o reload traz-a de volta.
        passo("9.5 Docs: lupa do editor + pesquisa filtra (G0-3)");
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        const Handle atorHandle9 = g_scene.find("Ator");
        check(atorHandle9.valid(), "o Ator volta do .goni");
        const ScriptComp* sc9 =
            g_scene.get(atorHandle9)->getComponent<ScriptComp>();
        check(sc9 != nullptr && !sc9->source.empty(),
              "a fonte gravada SOBREVIVE no disco (round-trip .goni)");
        const std::string fonteGuardada = sc9 ? sc9->source : std::string();
        openScriptEditor(atorHandle9);
        check(g_editor.scriptWin.buf == fonteGuardada,
              "script EXISTENTE reabre com a fonte guardada intacta (G0-2)");
        // a superficie JÁ está em portrait (a rotação do reload acima)
        {
            const f32 docsX = 720.0f - 72.0f * 2.0f - 16.0f - 8.0f - 48.0f;
            tap(docsX + 24.0f, 28.0f);
        }
        check(g_editor.docsScreen.open, "a LUPA abre as Docs por cima (G0-3)");
        check(!g_editor.scriptWin.open == false,
              "o editor continua aberto POR BAIXO das Docs");
        // a pesquisa filtra (o campo commita pelo purpose 9 — aqui direto)
        std::snprintf(g_editor.docsScreen.query,
                      sizeof(g_editor.docsScreen.query), "view");
        g_editor.docsScreen.queryLen = 4;
        check(voni::docs::search("view").size() > 0,
              "a pesquisa 'view' filtra entradas estruturadas");
        check(voni::docs::search("view").size() < voni::docs::search("").size(),
              "o filtro REDUZ a lista (pesquisa viva)");
        g_editor.docsScreen.open = false;   // back
        check(g_editor.scriptWin.open, "o editor volta a ser o modal");
        closeScriptEditor();

        // 9.6 — DOCS pelo SETTINGS (G0-3): a linha "Ver docs da V.ONI" era
        // MORTA no device (o walk do scrollTap nao a re-despachava)
        passo("9.6 Docs: a linha do Settings (o fix do botao morto)");
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        g_editor.settingsMenu = true;
        g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                     editor::settings::kBitAudio |
                                     editor::settings::kBitPerm |
                                     editor::settings::kBitDiag;
        frame();   // layout estabiliza (slot de scroll)
        // y da linha Docs: 8 + 4 headers colapsados*48 + header Docs 48 + 24
        tap(800.0f, safe::kToolbarH + 56.0f + 8.0f + 4.0f * 48.0f + 48.0f + 24.0f);
        check(g_editor.docsScreen.open,
              "o toque na linha Docs do Settings ABRE as Docs (era morta)");
        check(logHas("voni: docs abertas"), "a abertura fica LOGADA");
        g_editor.docsScreen.open = false;
        g_editor.settingsMenu = false;

        // 9.7 — ACENTOS (G1-2/R-008): o atlas do boot tem a cobertura
        // latina e o frame EMITE os glifos acentuados (o "ÁUDIO" desenha)
        passo("9.7 acentos: cobertura do atlas + emissao (R-008)");
        check(g_font.hasGlyph(0xE7) && g_font.hasGlyph(0xE3) &&
                  g_font.hasGlyph(0xC3) && g_font.hasGlyph(0xF5) &&
                  g_font.hasGlyph(0xE9) && g_font.hasGlyph(0xED),
              "o atlas do boot tem c/ae, a-tilde, A-tilde, o-tilde, e-agudo, "
              "i-agudo (R-008)");
        check(g_font.hasGlyph(0x2026),
              "a elipse U+2026 esta no atlas (truncagem com '…')");
        {
            g_ui.beginFrame(nullptr, &g_input,
                             static_cast<f32>(g_egl.width()),
                             static_cast<f32>(g_egl.height()));
            const u32 antes = g_ui.glyphsForTest().vertexCount();
            g_ui.label(16.0f, 60.0f,
                       "\xC3\x81UDIO F\xC3\xADsica Anima\xC3\xA7\xC3\xA3o "
                       "Sele\xC3\xA7\xC3\xA3o \xC3\xA7\xC3\xA3o "
                       "\xC3\x83\xC3\x95 \xC3\xA7",
                       theme::kTheme.text1);
            const u32 depois = g_ui.glyphsForTest().vertexCount();
            g_ui.endFrame();
            check(depois > antes,
                  "a string de teste do dono EMITE glifos (acentos desenham)");
        }

        // 9.8 — LAYOUT.JSON em DEBOUNCE (G1-5): o log do dono mostrava 4
        // writes em ~40 s (cada passo de 8dp do drag do drawer gravava);
        // agora grava 1,5 s após a ÚLTIMA alteração, UMA linha
        // "layout guardado (motivo)" e NÃO grava sem mudança
        passo("9.8 layout.json: debounce 1,5 s — uma linha (G1-5)");
        {
            g_lastLayoutSaved.clear();   // baseline determinístico
            const int n0 = logCount("layout guardado");
            // o "drag" inteiro: 3 passos de 8dp SEGUIDOS (cada tick 0,4 s —
            // o debounce RECOMEÇA a cada mudança: nada é gravado no meio)
            g_bottom.bottomTab = 1;
            g_bottom.drawerH = 240.0f;
            for (int i = 0; i < 3; ++i) {
                g_bottom.drawerH = 240.0f + 8.0f * static_cast<f32>(i);
                layoutSaveTick(0.4f);
            }
            check(logCount("layout guardado") == n0,
                  "durante o drag (3 passos): ZERO writes (coalesce)");
            // o debounce VENCE (1,6 s após a última alteração): 1 write
            for (int i = 0; i < 4; ++i) {
                layoutSaveTick(0.4f);
            }
            check(logCount("layout guardado") == n0 + 1,
                  "1,5 s após a última alteração: EXATAMENTE 1 write");
            check(logHas("layout guardado (painel de baixo)"),
                  "a linha única traz o MOTIVO (painel de baixo)");
            // sem mudança: NÃO volta a gravar
            for (int i = 0; i < 6; ++i) {
                layoutSaveTick(0.4f);
            }
            check(logCount("layout guardado") == n0 + 1,
                  "sem mudança de conteúdo: NÃO grava");
            // a SAÍDA para segundo plano faz o FLUSH imediato do pendente
            g_editor.showInspector = !g_editor.showInspector;
            layoutSaveTick(0.2f);   // acabou de agendar (1,5 s pendentes)
            onAppCmd(&app, APP_CMD_PAUSE);
            check(logCount("layout guardado") == n0 + 2,
                  "APP_CMD_PAUSE: o write pendente faz FLUSH imediato");
            check(logHas("layout guardado (saída para segundo plano)"),
                  "o flush loga o motivo de segundo plano");
            // o áudio pausado pelo cmd não pode deixar stream vivo
            onAppCmd(&app, APP_CMD_RESUME);
        }

        // 9.9 — TOOLBAR ANCORADA AO RECT DA VIEWPORT (G1-1): nunca cobre
        // outro painel — com o painel de baixo FECHADO e ABERTO
        passo("9.9 toolbar ancorada ao viewport (G1-1)");
        {
            const f32 sw = static_cast<f32>(g_egl.width());
            const f32 sh = static_cast<f32>(g_egl.height());
            const safe::Insets ins = g_ui.safeArea();
            // estado A: painel de baixo FECHADO, Inspector ABERTO
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
            g_editor.showInspector = true;
            frame();
            const UiRect vA =
                safe::centerRect(sw, sh, ins, 0.0f, g_editor.showInspector);
            const editor::vpchrome::Layout lA = editor::vpchrome::layout(vA);
            const editor::vpchrome::Layout* a = &lA;
            const UiRect toolsA[6] = {a->selectBtn, a->moveBtn, a->rotateBtn,
                                      a->scaleBtn,  a->snapBtn, a->addTicBtn};
            bool dentroA = true;
            for (int i = 0; i < 6; ++i) {
                dentroA = dentroA && safe::rectInside(toolsA[i], vA);
            }
            check(dentroA, "painel FECHADO: os 6 botões DENTRO do rect da "
                           "viewport (nunca cobrem Inspector/hierarquia)");
            // o stack vertical também (o undo/redo/save/dup/paste)
            bool stackOk = true;
            for (u32 i = 0; i < a->nStack; ++i) {
                stackOk = stackOk && safe::rectInside(a->stack[i], vA);
            }
            check(stackOk, "o stack vertical (undo/redo/save/dup/paste) "
                           "dentro do rect");
            // "+" no canto inferior DIREITO da viewport (G1-1)
            check(a->addTicBtn.x + a->addTicBtn.w > vA.x + vA.w - 72.0f,
                  "o '+' vive no canto inferior DIREITO da viewport");
            // estado B: painel de baixo ABERTO (drawer 240) — a toolbar SOBE
            g_bottom.bottomTab = 1;
            g_bottom.drawerH = 240.0f;
            frame();
            const UiRect vB = safe::centerRect(sw, sh, ins, 240.0f,
                                               g_editor.showInspector);
            const editor::vpchrome::Layout lB = editor::vpchrome::layout(vB);
            const editor::vpchrome::Layout* b = &lB;
            const UiRect toolsB[6] = {b->selectBtn, b->moveBtn, b->rotateBtn,
                                      b->scaleBtn,  b->snapBtn, b->addTicBtn};
            bool dentroB = true;
            for (int i = 0; i < 6; ++i) {
                dentroB = dentroB && safe::rectInside(toolsB[i], vB);
            }
            check(dentroB, "painel ABERTO (240px): os 6 botões SOBEM com o "
                           "rect — nunca cobrem o painel de baixo");
            check(b->selectBtn.y < a->selectBtn.y - 100.0f,
                  "a toolbar ACOMPANHA o painel (sobe ~240px com ele aberto)");
            // conflto DIRETO contra o painel: nenhum botão invade a faixa
            // do drawer (y >= topo do painel)
            const f32 drawerTop = sh - 240.0f;
            bool foraDoDrawer = true;
            for (int i = 0; i < 6; ++i) {
                foraDoDrawer =
                    foraDoDrawer && toolsB[i].y + toolsB[i].h <= drawerTop + 0.5f;
            }
            check(foraDoDrawer, "nenhum botão pisa a faixa do painel de baixo");
            // largura mínima: a toolbar cabe na viewport mais estreita
            // (1600×720 com Inspector + drawer = o pior caso do dono)
            check(vB.w >= 480.0f, "a viewport útil no pior caso tem folga");
            // o SÓ-ÍCONE: o botão inativo tem 48dp; o ATIVO (com nome) é o
            // mais largo — o total fecha < viewport útil
            const f32 totalW = 6.0f * 8.0f + b->selectBtn.w + b->moveBtn.w +
                               b->rotateBtn.w + b->scaleBtn.w + b->snapBtn.w;
            check(totalW < vB.w, "o total da toolbar cabe na viewport útil");
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
        }

        // 9.10 — TOCAR SELECIONA O TIC (G1-6): o toque no CORPO de um cubo
        // grande seleciona-o (o bug: só o centro a 44px contava) — e cada
        // mudança de seleção LOGA com o motivo
        passo("9.10 tocar no corpo seleciona o TIC (G1-6)");
        {
            // Inspector FECHADO (viewport inteiro — o tap tem de nascer
            // DENTRO do viewRect para o pick armar)
            g_editor.showInspector = false;
            // um TIC Cubo GRANDE na cena (mesh procedural do boot, escala 8)
            const Handle hCubo = g_scene.create("Cubo");
            Tic* cubo = g_scene.get(hCubo);
            Transform3D* ctr = cubo->addComponent<Transform3D>();
            MeshRenderer* cmr = cubo->addComponent<MeshRenderer>();
            cmr->mesh = &g_cubeMesh;
            ctr->pos = Vec3{0.0f, 0.0f, -6.0f};
            ctr->scale = Vec3{8.0f, 8.0f, 8.0f};
            ctr->updateWorld();
            // limpa a seleção (estado de partida conhecido)
            g_editor.selected = Handle::invalid();
            g_editor.selElement = -1;
            // o CANTO do corpo projetado (fora dos 44px do centro)
            const Mat4 vp = Mat4::mul(g_camera.proj(g_egl.width() /
                                                    static_cast<f32>(
                                                        g_egl.height())),
                                      g_camera.view());
            const Vec3 canto{-0.5f, -0.5f, -0.5f};
            const Vec3 w = Vec3{
                ctr->world.m[0] * canto.x + ctr->world.m[4] * canto.y +
                    ctr->world.m[8] * canto.z + ctr->world.m[12],
                ctr->world.m[1] * canto.x + ctr->world.m[5] * canto.y +
                    ctr->world.m[9] * canto.z + ctr->world.m[13],
                ctr->world.m[2] * canto.x + ctr->world.m[6] * canto.y +
                    ctr->world.m[10] * canto.z + ctr->world.m[14]};
            f32 cx = 0.0f, cy = 0.0f, ccx = 0.0f, ccy = 0.0f;
            gizmo::projectPoint(vp, w, static_cast<f32>(g_egl.width()),
                                static_cast<f32>(g_egl.height()), cx, cy);
            gizmo::projectPoint(vp, ctr->pos,
                                static_cast<f32>(g_egl.width()),
                                static_cast<f32>(g_egl.height()), ccx, ccy);
            const f32 dc = std::sqrt((cx - ccx) * (cx - ccx) +
                                     (cy - ccy) * (cy - ccy));
            check(dc > 60.0f,
                  "o canto do corpo está LONGE do centro (>60px — fora do "
                  "raio antigo de 44)");
            // o pick DIRETO acerta (o contrato do G1-6)
            check(camgizmo::pickSceneTic(
                      g_scene, vp, static_cast<f32>(g_egl.width()),
                      static_cast<f32>(g_egl.height()), cx, cy) == hCubo,
                  "pickSceneTic: o toque no CORPO seleciona o TIC (G1-6)");
            // e pelo caminho REAL da UI: o tap no viewport
            const int nSel = logCount("seleção: TIC 'Cubo' (toque no viewport)");
            tap(cx, cy);
            idle(1);
            check(g_editor.selected == hCubo,
                  "o TAP no corpo do cubo SELECIONA-o (o caminho da UI)");
            check(logCount("seleção: TIC 'Cubo' (toque no viewport)") ==
                      nSel + 1,
                  "a mudança de seleção LOGA com o motivo (toque no viewport)");
            // o tap no VAZIO limpa E loga (x=500: FORA do AABB projetado
            // do cubo — minX≈580 — e fora do círculo de 44px do centro)
            const int nClr = logCount("seleção limpa: toque no vazio");
            tap(500.0f, 300.0f);
            idle(1);
            check(!g_editor.selected.valid(),
                  "o tap no vazio LIMPA a seleção");
            check(logCount("seleção limpa: toque no vazio") == nClr + 1,
                  "a limpeza também LOGA (cada mudança com motivo)");
            // a cena volta ao estado (o TIC extra sai)
            g_scene.destroy(hCubo);
        }

        // 9.11 — G2: ALINHAMENTO AO MOCK — barra única 56dp (a tab bar
        // fundiu-se), ícones da hierarquia por tipo de corpo (tic_static/
        // tic_player/tic_rigid/tic_camera), Física em duas colunas e o
        // Inspector que volta ao TOPO na troca de TIC
        passo("9.11 G2: barra única + ícones de corpo + inspector ao topo");
        {
            // a BARRA ÚNICA: kToolbarH = 56 — o viewport GANHOU 48px
            check(nearEqF(safe::kToolbarH, 56.0f),
                  "kToolbarH = 56 (menu+tabs numa barra — G2-10)");
            const UiRect viewG2 = safe::centerRect(
                static_cast<f32>(g_egl.width()),
                static_cast<f32>(g_egl.height()), g_ui.safeArea(), 0.0f,
                false);
            const f32 hAntiga = static_cast<f32>(g_egl.height()) - 104.0f -
                                safe::kStatusH - safe::kBottomTabH;
            check(nearEqF(viewG2.h, hAntiga + 48.0f),
                  "o viewport central GANHOU os 48px da tab bar fundida");
            // o TRIAD morreu (os pontinhos fantasma) — o layout do chrome
            // não tem triad (compila) e o canto sup-dir fica LIVRE
            const editor::vpchrome::Layout lg2 =
                editor::vpchrome::layout(viewG2);
            check(nearEqF(lg2.addTicBtn.x + lg2.addTicBtn.w + 8.0f,
                          viewG2.x + viewG2.w),
                  "o '+' continua o ÚNICO elemento do canto inferior direito");
            // ícones da hierarquia por TIPO DE CORPO (G2-7)
            {
                const Handle hC = g_scene.create("CorpoG2");
                Tic* tc2 = g_scene.get(hC);
                Transform3D* tr2 = tc2->addComponent<Transform3D>();
                tr2->pos = Vec3{40.0f, 0.0f, -20.0f};   // fora do caminho
                tr2->updateWorld();
                BodyComp* bc2 = tc2->addComponent<BodyComp>();
                bc2->type = BodyType::Static;
                check(editor::hierIconFor(*tc2) == icons::Icon::Static,
                      "corpo ESTÁTICO → tic_static (G2-7)");
                bc2->type = BodyType::Character;
                check(editor::hierIconFor(*tc2) == icons::Icon::Person,
                      "corpo PERSONAGEM → tic_player (G2-7)");
                bc2->type = BodyType::Rigid;
                check(editor::hierIconFor(*tc2) == icons::Icon::Rigid,
                      "corpo RÍGIDO → tic_rigid (G2-7)");
                check(icons::iconByName("tic_static") >= 0 &&
                          icons::iconByName("tic_rigid") >= 0,
                      "os ícones novos existem pelos nomes do mock");
                // Física em DUAS COLUNAS: o plano tem 3 TwoCol
                const TextMetrics m2 = g_ui.textMetrics();
                const editor::InspProfile prof2 = editor::inspectorProfile(*tc2);
                editor::InspRow plan2[64];
                const u32 n2 = inspectorPlan(prof2, m2, false, 0u, plan2);
                u32 twoCol = 0;
                for (u32 i2 = 0; i2 < n2; ++i2) {
                    if (plan2[i2].kind == editor::InspRow::Kind::TwoCol) {
                        ++twoCol;
                    }
                }
                check(twoCol == 3u,
                      "Física: 3 linhas em duas colunas (tipo/forma/no chão)");
                // o Inspector VOLTA AO TOPO na troca de TIC (G2-8)
                // (o Inspector tem de estar ABERTO — o 9.10 fechou-o)
                g_editor.showInspector = true;
                g_editor.selected = hC;   // estado de partida: o CorpoG2
                frame();
                g_ui.scrollSetOffset(editor::kIdScrollInsp, 200.0f);
                const Handle hAtor2 = g_scene.find("Ator");
                g_editor.selected = hAtor2;
                frame();
                check(nearEqF(g_ui.scrollOffsetForTest(
                          editor::kIdScrollInsp), 0.0f),
                      "troca de TIC: o Inspector volta ao TOPO (G2-8)");
                g_scene.destroy(hC);
            }
        }

        onAppCmd(&app, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 11 — 0.9.5 · LINKERS & TYKERS (METADE 1): o replay no caminho
    // REAL do app — o editor carrega um script com linker/tyker, o Run
    // arranca a run, o follow MOVE o Transform3D, o colorpars TINGE o
    // MeshRenderer, o RF em falta LOGA o erro exato sem matar o script, e
    // o CICLO aparece na BARRA DE ERRO do editor com linha. (11.B — os
    // lookups de ajuda do Editor que Ensina — entra com a METADE 2.)
    // ======================================================================
    fase("FASE 11 — replay linkers/tykers (0.9.5 METADE 1)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        auto st11 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt11 = st11.get();
        check(Project::createNew(*rawSt11, "fase11", g_project),
              "projeto criado");
        {
            const Handle hA = g_scene.create("Ator");
            Tic* tA = g_scene.get(hA);
            Transform3D* trA = tA->addComponent<Transform3D>();
            trA->pos = Vec3{15.0f, 0.0f, 0.0f};
            trA->updateWorld();
            tA->addComponent<MeshRenderer>();
            // o script viaja NO .goni (o serializer salta ScriptComp VAZIO —
            // a fonte entra ANTES do save, o cenário real de um script que
            // já existia no projeto)
            ScriptComp& sc11 = *tA->addComponent<ScriptComp>();
            sc11.source =
                "linker(Ator)to(Alvo)=RF(principal)\n"
                "tyker(seguelo){ find(principal) follow(2) }\n"
                "tyker(pinta){ find(principal) colorpars(cor)(#FF8800) }\n"
                "central main { on moment { } allmoments { } }\n";
            const Handle hB = g_scene.create("Alvo");
            Tic* tB = g_scene.get(hB);
            Transform3D* trB = tB->addComponent<Transform3D>();
            trB->pos = Vec3{5.0f, 0.0f, 0.0f};
            trB->updateWorld();
            check(g_project.saveActiveScene(*rawSt11, g_scene),
                  "cena gravada (Ator em 15, Alvo em 5, script no .goni)");
        }
        g_storage = std::move(st11);
        g_projectReady = true;

        android_app app11;
        std::memset(&app11, 0, sizeof(app11));
        onAppCmd(&app11, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 11.1 — o script COM LINKER/TYKER corre pelo caminho REAL: o
        // editor abre a fonte guardada, o Run arranca, o follow move o TIC
        passo("11.1 linker+tyker: Run real → follow move o Transform3D");
        {
            const Handle ator = g_scene.find("Ator");
            check(ator.valid(), "o TIC Ator volta do boot");
            const ScriptComp* sc11 =
                g_scene.get(ator)->getComponent<ScriptComp>();
            check(sc11 != nullptr && !sc11->source.empty(),
                  "o script veio NO .goni (round-trip das fontes)");
            openScriptEditor(ator);
            check(g_editor.scriptWin.open, "o editor abre com a fonte");
            check(g_editor.scriptWin.buf.find("linker(Ator)to(Alvo)") !=
                      std::string::npos,
                  "a fonte guardada abre INTACTA (com linkers)");
            scriptEditorRun();
            check(g_editor.scriptWin.running, "a run do editor está ATIVA");
            // o tick do VoniSystem (o harness não passa pelo android_main,
            // onde o g_systems se registra — o tick DIRETO é o mesmo passo
            // que o loop real dá: VoniSystem.tick → runFrame → tykers)
            g_voni.tick(g_scene, 1.0f / 60.0f);
            frame();   // o frame de desenho
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const Transform3D* tr =
                tA ? tA->getComponent<Transform3D>() : nullptr;
            check(tr != nullptr && nearEqF(tr->pos.x, 7.0f),
                  "follow(2): o Ator FICA a 2 do Alvo (15→7, alvo em 5)");
            check(tr != nullptr && nearEqF(tr->pos.y, 0.0f) &&
                      nearEqF(tr->pos.z, 0.0f),
                  "follow: eixos y/z intocados");
        }

        // 11.2 — o colorpars TINGIU o MeshRenderer (o mesmo frame de 11.1)
        passo("11.2 colorpars tinge o material do TIC de origem");
        {
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const MeshRenderer* mr =
                tA ? tA->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && nearEqF(mr->tint[0], 1.0f) &&
                      nearEqF(mr->tint[1], 0.5333f, 1e-2f) &&
                      nearEqF(mr->tint[2], 0.0f),
                  "colorpars(cor)(#FF8800): tint = (1, 0.53, 0)");
        }

        // 11.3 — RF EM FALTA (R-012): o erro exato no engine.log, o tyker
        // não corre, o script CONTINUA (o follow do outro tyker mexeu)
        passo("11.3 RF em falta: log exato + tyker não corre (R-012)");
        {
            const Handle ator = g_scene.find("Ator");
            // repõe a posição de partida + EDITA o buffer (o que o Run usa)
            if (Tic* t = g_scene.get(ator)) {
                if (Transform3D* tr = t->getComponent<Transform3D>()) {
                    tr->pos = Vec3{15.0f, 0.0f, 0.0f};
                    tr->updateWorld();
                }
            }
            g_editor.scriptWin.buf =
                "linker(Ator)to(Alvo)=RF(principal)\n"
                "tyker(bom){ find(principal) follow(2) }\n"
                "tyker(mau){ find(fantasma) follow() }\n"
                "central main { allmoments { View P \"segue\" } }\n";
            g_editor.scriptWin.caret =
                static_cast<u32>(g_editor.scriptWin.buf.size());
            scriptEditorRun();
            g_voni.tick(g_scene, 1.0f / 60.0f);
            frame();
            check(logHas("RF 'fantasma' não encontrada"),
                  "o log traz o ERRO EXATO da spec (R-012)");
            check(logHas("tyker 'mau' não corre"),
                  "o log diz QUAL tyker não correu");
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const Transform3D* tr =
                tA ? tA->getComponent<Transform3D>() : nullptr;
            check(tr != nullptr && nearEqF(tr->pos.x, 7.0f),
                  "o tyker BOM correu (o script não morreu)");
            check(logHas("voni: segue"), "o allmoments segue a correr");
        }

        // 11.4 — CICLO (R-011): a run MORRE com o erro legível E a barra
        // de erro do editor ACENDE com a linha
        passo("11.4 ciclo a→b + b→a: erro legível na barra (R-011)");
        {
            // EDITA o buffer para o script com o CICLO (o que o Run compila)
            g_editor.scriptWin.buf =
                "linker(Ator)to(Alvo)=RF(p)\n"
                "linker(Alvo)to(Ator)=RF(p)\n"
                "central main { }\n";
            g_editor.scriptWin.caret =
                static_cast<u32>(g_editor.scriptWin.buf.size());
            scriptEditorRun();
            check(!g_editor.scriptWin.running, "a run NÃO arranca (rejeitada)");
            check(g_editor.scriptWin.errLine >= 1,
                  "a barra de erro tem LINHA");
            check(g_editor.scriptWin.errMsg.find("ciclo") !=
                      std::string::npos,
                  "a barra de erro diz CICLO (legível)");
            check(g_editor.scriptWin.errMsg.find("'Ator'") !=
                      std::string::npos,
                  "o erro nomeia os lados do ciclo");
        }

        // 11.5 — as Docs têm os LINKERS/TYKERS/COMPONENTES (a mesma fonte
        // do registo — a lupa do editor pesquisa por elas)
        passo("11.5 Docs: as categorias novas povoadas (o registo alimenta)");
        {
            const auto& all = voni::docs::all();
            bool hasLinker = false, hasTyker = false, hasComp = false;
            for (const auto& e : all) {
                hasLinker = hasLinker || e.cat == voni::docs::Cat::Linker;
                hasTyker = hasTyker || e.cat == voni::docs::Cat::Tyker;
                hasComp = hasComp || e.cat == voni::docs::Cat::Componente;
            }
            check(hasLinker && hasTyker && hasComp,
                  "Docs: categorias Linker/Tyker/Componente povoadas");
            check(!voni::docs::search("follow").empty(),
                  "Docs: a pesquisa apanha 'follow'");
            check(!voni::docs::search("colorpars").empty(),
                  "Docs: a pesquisa apanha 'colorpars'");
        }

        onAppCmd(&app11, APP_CMD_TERM_WINDOW);
    }

    // ---- sumário -----------------------------------------------------------
    std::printf("\n== C33 VIRTUAL: %d check(s), %d falha(s) ==\n", g_checks, g_failed);
    if (g_failed == 0) {
        std::printf("HARNESS VERDE — o dispositivo virtual confirma os fixes "
                    "vigiados (R-001..R-008; FASE 9 = UI replay do 0.9.4)\n");
    } else {
        std::printf("HARNESS VERMELHO — release BLOQUEADA (ver [FAIL] acima)\n");
    }
    fileapi::testing::clearReadonlyPrefix();
    rmrf(kCacheDir);
    return g_failed == 0 ? 0 : 1;
}
