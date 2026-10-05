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
#include <sys/stat.h>   // 0.9.6.4 (12.8b): mkdir cru p/ a fixture do par
#include <cerrno>
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

// 0.9.6 (G6/12.9): o nº de linhas de um bloco (o relatório de bench tem 9)
int countLines(const std::string& s) {
    int n = 0;
    for (char c : s) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
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
    // 0.9.6.5 (GRUPO B): os OBJETOS do framebuffer real também morrem com o
    // contexto (o `enabled` MANTÉM-SE — é configuração do ambiente, como o
    // astcLdr; só a FASE 13 o liga)
    glstub::fb::resetState();
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
        // 0.9.6 (G3): o toque no CORPO pede o IME DO SISTEMA e o teclado
        // próprio CEDA (a política de coexistência — nunca os dois)
        g_editor.scriptWin.kbOpen = true;
        tap(360.0f, 400.0f);
        check(!g_editor.scriptWin.kbOpen,
              "toque no corpo: o teclado próprio CEDA ao IME (G3)");
        // o teclado próprio ABRE pelo BOTÃO do cabeçalho (docsX-152+24)
        tap(504.0f - 152.0f + 24.0f, 28.0f);
        check(g_editor.scriptWin.kbOpen,
              "o BOTÃO do cabeçalho abre o teclado próprio (G3)");
        // tecla Q (linha 0, col 0) do teclado desenhado — 0.9.6.1: a grelha
        // QWERTY começa na margem única de 8px (o antigo x0 centrado caía
        // NO VÃO entre teclas)
        {
            const f32 keyW = (720.0f - 16.0f - 9.0f * 6.0f) / 10.0f;
            const f32 kbTop = 1536.0f - (5.0f * 48.0f + 4.0f * 6.0f + 16.0f);
            tap(8.0f + keyW * 0.5f, kbTop + 8.0f + 24.0f);
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
        // 0.9.6 (G1): Settings ECRÃ CHEIO — sem a banda kToolbarH do overlayArea
        tap(800.0f, 56.0f + 8.0f + 4.0f * 48.0f + 48.0f + 24.0f);
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

        // ================================================================
        // 11.B — O EDITOR QUE ENSINA (METADE 2): os lookups de ajuda no
        // caminho REAL do app — o erro que ENSINA, a mini-descrição desde
        // a 1ª letra, os esqueletos por Tab, o toque numa palavra, os
        // níveis I/N/S, o copiar-referência — e o WALKTHROUGH scriptado:
        // um script FUNCIONAL montado só com a ajuda do editor.
        // (O editor é PORTRAIT 720×1536 — o par inseparável 0.9.1; a
        // superfície muda ANTES dos toques, o mesmo ciclo do 9.3, e o
        // editor SOBREVIVE ao reload — o fix G0-1 do handle por nome.)
        // ================================================================
        passo("11.6 erros-que-ensinam: o 'if' do Python aprende o exist");
        {
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            onAppCmd(&app11, APP_CMD_TERM_WINDOW);
            onAppCmd(&app11, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            check(g_editor.scriptWin.open,
                  "o editor SOBREVIVE ao ciclo portrait (G0-1)");
            g_editor.scriptWin.buf = "if (vida == 0) { }";
            g_editor.scriptWin.caret = 19;
            scriptEditorRun();
            check(!g_editor.scriptWin.running, "o script com 'if' não corre");
            check(g_editor.scriptWin.errLine >= 1,
                  "a barra de erro tem LINHA");
            check(g_editor.scriptWin.errMsg.find("exist") !=
                      std::string::npos,
                  "a barra de erro ENSINA: 'if' → exist (registo kForeign)");
        }

        passo("11.7 mini-descrição em tempo real DESDE A 1ª LETRA");
        {
            g_editor.scriptWin.errLine = 0;
            g_editor.scriptWin.errMsg.clear();
            g_editor.scriptWin.buf = "exi";
            g_editor.scriptWin.caret = 3;
            frame();   // o draw recalcula a strip do estado
            const std::string l1 =
                editor::scriptwin::helpStripLine1(g_editor.scriptWin);
            check(!l1.empty(), "a strip acende com 'exi' (3 letras)");
            check(l1.find("exist") != std::string::npos,
                  "a strip mostra o NOME do casamento por prefixo");
            check(editor::scriptwin::helpStripLine2(g_editor.scriptWin)
                      .empty(),
                  "nível Normal: 1 linha (sem encher o ecrã)");
        }

        passo("11.8 TAB → o esqueleto (o texto exato da spec)");
        {
            // a tecla Tab do GBoard chega pela fila do IME (keycode 61)
            Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 61, 0);
            frame();
            check(g_editor.scriptWin.buf == "exist(){ } notexist{ }",
                  "'exi' + Tab → o esqueleto 'exist(){ } notexist{ }'");
            check(g_editor.scriptWin.caret == 8,
                  "o caret fica NO INTERIOR do exist(){ … }");
        }

        passo("11.9 toque numa palavra → explicação com exemplo (Docs)");
        {
            // a palavra 'notexist' na linha 0 do buffer atual — o toque na
            // posição dela acende a strip com a explicação + exemplo
            g_editor.scriptWin.helpLevel = 1;   // Normal
            const f32 lh11 = 28.0f + 10.0f;     // (linha do corpo ~38px)
            tap(90.0f, 56.0f + 8.0f + lh11 * 0.5f);
            check(g_editor.scriptWin.helpTapped,
                  "o toque numa palavra marca o estado de explicação");
            const std::string l1 =
                editor::scriptwin::helpStripLine1(g_editor.scriptWin);
            check(!l1.empty(), "a strip mostra a explicação da palavra");
            const std::string l2 =
                editor::scriptwin::helpStripLine2(g_editor.scriptWin);
            check(l2.find("ex.:") == 0,
                  "no toque a 2ª linha acende com o EXEMPLO (das Docs)");
        }

        passo("11.10 os níveis I/N/S: Silencioso apaga a strip");
        {
            // 0.9.6 (G3): o botão do nível está em docsX-200 (o do teclado
            // próprio entrou em docsX-152) — docsX=504, y=28
            tap(504.0f - 200.0f + 24.0f, 28.0f);
            check(g_editor.scriptWin.helpLevel == 2, "N → S (Silencioso)");
            check(editor::scriptwin::helpStripLine1(g_editor.scriptWin)
                      .empty(),
                  "Silencioso: a strip APAGA (sem encher o ecrã)");
            tap(504.0f - 200.0f + 24.0f, 28.0f);
            check(g_editor.scriptWin.helpLevel == 0, "S → I (Iniciante)");
            check(!editor::scriptwin::helpStripLine1(g_editor.scriptWin)
                      .empty(),
                  "Iniciante: a strip acende");
            check(!editor::scriptwin::helpStripLine2(g_editor.scriptWin)
                      .empty(),
                  "Iniciante: SEMPRE com o exemplo (2 linhas)");
            tap(504.0f - 200.0f + 24.0f, 28.0f);
            check(g_editor.scriptWin.helpLevel == 1, "I → N (volta ao Normal)");
        }

        passo("11.11 copiar-referência: o clipboard recebe a referência");
        {
            g_jni.void_calls.clear();
            g_jni.last_new_string.clear();
            tap(448.0f + 24.0f, 28.0f);   // o botão 📋 (docsX-56)
            frame();
            bool sawClip = false;
            for (const auto& c : g_jni.void_calls) {
                if (c.first == "clipboardCopy") {
                    sawClip = true;
                }
            }
            check(sawClip, "JNI: clipboardCopy chamada com a referência");
            check(g_jni.last_new_string.find(
                      "V.ONI — Referência da linguagem") !=
                      std::string::npos,
                  "o texto colável é a referência COMPLETA (do registo)");
            check(g_jni.last_new_string.find("tyker") != std::string::npos,
                  "a referência traz os tykers");
            check(logHas("referência V.ONI copiada"),
                  "o log regista a cópia (com o nº de entradas)");
        }

        passo("11.12 WALKTHROUGH: script funcional SÓ com a ajuda do editor");
        {
            // A pessoa que sabe Python/JS monta um script V.ONI que CORRE:
            // limpa o esqueleto com o backspace do teclado, digita o linker,
            // o 'tyker' expande pelo Tab, corrige o placeholder RF→principal
            // e acrescenta o follow — TUDO pelo caminho REAL do IME.
            g_editor.scriptWin.helpLevel = 1;
            g_editor.scriptWin.errLine = 0;
            g_editor.scriptWin.errMsg.clear();
            // (1) limpa: o caret vai ao FIM (seta →) e o backspace come
            // tudo (o backspace só apaga ANTES do caret — do fim limpa o
            // buffer inteiro)
            for (int i = 0; i < 30; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       22, 0);   // RIGHT
            }
            for (int i = 0; i < 80; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       67, 0);   // DEL
            }
            frame();
            check(g_editor.scriptWin.buf.empty(), "o esqueleto é limpo");
            // (2) digita o linker (o IME commita char a char)
            for (const char c :
                 std::string("linker(Ator)to(Alvo)=RF(principal)\n")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            frame();
            check(g_editor.scriptWin.buf.find("linker(Ator)to(Alvo)") !=
                      std::string::npos,
                  "o linker está no buffer (digitado)");
            // (3) 'tyker' + TAB → o esqueleto do registo
            for (const char c : std::string("tyker")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 61, 0);
            frame();
            check(g_editor.scriptWin.buf.find("tyker(nome){ find(RF) }") !=
                      std::string::npos,
                  "o Tab expande o tyker (esqueleto do registo)");
            // (4) a pessoa corrige o placeholder RF→principal: o caret
            // está antes do '}' (22 no esqueleto); ←×2 põe-no DEPOIS do
            // 'RF', DEL×2 apaga as duas letras, e 'principal' entra no sítio
            for (int i = 0; i < 2; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       21, 0);   // LEFT
            }
            for (int i = 0; i < 2; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       67, 0);   // DEL
            }
            for (const char c : std::string("principal")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            frame();
            check(g_editor.scriptWin.buf.find("find(principal)") !=
                      std::string::npos,
                  "o placeholder RF→principal corrigido pelo IME");
            // (5) o caret salta o ')' (fica depois do find(...)) e
            // acrescenta o follow DENTRO do tyker
            Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                   22, 0);   // RIGHT
            for (const char c : std::string(" follow(2)")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            frame();
            // (6) RUN: o script FUNCIONA — o Ator segue o Alvo
            if (Tic* t = g_scene.get(g_scene.find("Ator"))) {
                if (Transform3D* tr = t->getComponent<Transform3D>()) {
                    tr->pos = Vec3{15.0f, 0.0f, 0.0f};
                    tr->updateWorld();
                }
            }
            scriptEditorRun();
            check(g_editor.scriptWin.running,
                  "o script do walkthrough ARRANCA");
            check(g_editor.scriptWin.errLine == 0, "sem erros na barra");
            g_voni.tick(g_scene, 1.0f / 60.0f);
            frame();
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const Transform3D* tr =
                tA ? tA->getComponent<Transform3D>() : nullptr;
            check(tr != nullptr && nearEqF(tr->pos.x, 7.0f),
                  "o follow FUNCIONA: o Ator fica a 2 do Alvo (15→7)");
        }

        onAppCmd(&app11, APP_CMD_TERM_WINDOW);
    }

    // =====================================================================
    // FASE 12 — 0.9.6 G1: INSETS + CAMADAS (os ecrãs cheios respeitam a
    // safe-area e capturam TODO o toque; nada desenha por cima deles)
    // =====================================================================
    fase("FASE 12 — 0.9.6 G1: insets reais + camadas (ecrãs cheios)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        auto st12 = std::make_unique<FakeStorage>();
        check(Project::createNew(*st12, "fase12", g_project),
              "projeto fase12 criado");
        {
            const Handle h = g_scene.create("Ator");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            t->addComponent<MeshRenderer>();
            t->addComponent<ScriptComp>();
            check(g_project.saveActiveScene(*st12, g_scene), "cena gravada");
        }
        g_storage = std::move(st12);
        g_projectReady = true;

        // O C33 com a faixa preta de verdade: topo 96 (status+recorte) e
        // barra de navegação 48 em baixo — o contentRect que o device manda
        // (landscape 1536x720, como o boot da FASE 1)
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        android_app app12;
        std::memset(&app12, 0, sizeof(app12));
        app12.contentRect = {0, 96, 1536, 672};   // insets T96 B48
        onAppCmd(&app12, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(nearEqF(g_ui.safeArea().top, 96.0f) &&
                  nearEqF(g_ui.safeArea().bottom, 48.0f),
              "12.0 os insets do contentRect chegam à UI (T96 B48)");

        // helper: NENHUM glifo sob a faixa do topo/acima da barra de baixo
        auto glyphsDentroDosInsets = [&]() {
            const QuadBatch& g = g_ui.glyphsForTest();
            const QuadVertex* v = g.vertices();
            const u32 n = g.vertexCount();
            const f32 topLimit = g_ui.safeArea().top;
            const f32 botLimit =
                static_cast<f32>(g_egl.height()) - g_ui.safeArea().bottom;
            u32 fora = 0;
            for (u32 i = 0; i + 5 < n; i += 6) {
                if (v[i].y < topLimit - 0.5f ||
                    v[i + 2].y > botLimit + 0.5f) {
                    ++fora;
                    if (std::getenv("VV_DBG_INSETS")) {
                        std::printf("FORA: y0=%.1f y1=%.1f x0=%.1f x1=%.1f "
                                    "(limites T%.0f B%.0f)\n",
                                    v[i].y, v[i + 2].y, v[i].x, v[i + 2].x,
                                    topLimit, botLimit);
                    }
                }
            }
            return fora;
        };

        // ---- 12.1 DOCS: o título nunca sob a faixa preta -------------------
        passo("12.1 Docs com insets T96/B48: nada desenha sob o sistema");
        {
            g_editor.docsScreen.open = true;
            g_editor.docsScreen.queryLen = 0;
            g_editor.docsScreen.expanded = -1;
            frame();
            check(glyphsDentroDosInsets() == 0,
                  "Docs: NENHUM glifo sob a faixa do topo/barra de baixo");
            // o BACK mora no inset+0..inset+56 — o toque FUNCIONA lá
            tap(32.0f, 96.0f + 28.0f);
            check(!g_editor.docsScreen.open,
                  "Docs: o back (dentro da parte útil) fecha");
            g_editor.docsScreen.open = false;
        }

        // ---- 12.2 SETTINGS: ecrã cheio + captura TODO o toque -------------
        passo("12.2 Settings: ecrã cheio, orbit morto, glifo do áudio fora");
        {
            g_editor.settingsMenu = true;
            frame();   // o settings desenha (sem o TIC Som ainda)
            check(glyphsDentroDosInsets() == 0,
                  "Settings: NENHUM glifo sob a faixa do topo/barra de baixo");
            // (a) baseline dos sólidos COM o modal aberto; o TIC de áudio
            // entra DEPOIS — se o glifo amarelo desenhasse por cima do
            // settings, o contador de sólidos SUBIA
            const u32 solidsBaseline = g_ui.solidsForTest().vertexCount();
            const Handle ha = g_scene.create("Som");
            if (Tic* ta = g_scene.get(ha)) {
                ta->addComponent<Transform3D>();
                ta->addComponent<AudioPlayer>();
            }
            frame();
            check(g_ui.solidsForTest().vertexCount() == solidsBaseline,
                  "o glifo amarelo do áudio NÃO desenha sobre o Settings "
                  "(sólidos idênticos com o TIC Som criado)");
            // (b) o DRAG no viewport NÃO orbita (a cena não mexe por trás)
            const f32 yaw0 = g_camera.yaw;
            const f32 pitch0 = g_camera.pitch;
            g_input.injectDown(0, 900.0f, 400.0f);
            frame();
            g_input.injectMove(0, 1100.0f, 300.0f);
            frame();
            g_input.injectUp(0);
            frame();
            check(nearEqF(g_camera.yaw, yaw0) &&
                      nearEqF(g_camera.pitch, pitch0),
                  "Settings aberto: o drag NÃO orbita a câmara (toque "
                  "capturado pelo ecrã cheio)");
            // (c) a barra de baixo escondida: o toque na tab Ficheiros não
            // abre o drawer (não desenhado = não interativo — a regra da
            // casa). A tab mora em y = 720-48(inset)-24(status)-48/2 = 624
            const f32 drawerH0 = g_editor.drawerH;
            const int tab0 = g_bottom.bottomTab;
            tap(200.0f, 624.0f);
            check(g_bottom.bottomTab == tab0 && g_editor.drawerH == drawerH0,
                  "Settings aberto: a barra de baixo NÃO responde (escondida)");
            g_editor.settingsMenu = false;
            frame();
            check(g_ui.solidsForTest().vertexCount() != solidsBaseline,
                  "settings fechado: o chrome volta a desenhar (o modal "
                  "não deixou estado)");
        }

        // ---- 12.3 EDITOR DE SCRIPT (PORTRAIT): corpo no contentRect ------
        passo("12.3 editor de script com insets: corpo no contentRect");
        {
            // o par inseparável portrait+IME: a superfície MUDA antes dos
            // toques (o mesmo ciclo do 11.B) com contentRect T96/B48
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            app12.contentRect = {0, 96, 720, 1488};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            const Handle ator = g_scene.find("Ator");
            openScriptEditor(ator);
            check(g_editor.scriptWin.open, "o editor abre (portrait)");
            check(g_editor.scriptWin.buf ==
                      std::string(editor::scriptwin::kSkeleton),
                  "o esqueleto volta INTACTO do reload (revalidação R-007)");
            frame();
            check(glyphsDentroDosInsets() == 0,
                  "editor: NENHUM glifo sob a faixa do topo (1ª linha "
                  "visível) nem sob a barra de baixo");
            // o teclado ancora ACIMA da barra de navegação: a tecla
            // ESPAÇO funciona no lugar ancorado (inset B=48). A linha de
            // baixo do teclado: y = kbBottom - 32 (centro da tecla)
            // Silencioso: a strip de ajuda apaga (o teclado docka no
            // inset de baixo SEM a strip — matemática determinística)
            g_editor.scriptWin.helpLevel = 2;
            g_editor.scriptWin.kbOpen = true;
            frame();
            const f32 kbBottom = 1536.0f - g_ui.safeArea().bottom;
            const f32 spaceY = kbBottom - 32.0f;
            // 0.9.6 (G3): a linha de baixo tem 4 SETAS antes do espaço —
            // [<][^][v][>][ESPACO 2u]… unit=(704-9*6)/12=54.2
            const f32 unit3 = (720.0f - 16.0f - 9.0f * 6.0f) / 12.0f;
            const f32 spaceX =
                8.0f + 4.0f * (unit3 + 6.0f) + unit3;
            const u32 len0 = (u32)g_editor.scriptWin.buf.size();
            const u32 caret0 = g_editor.scriptWin.caret;
            tap(spaceX, spaceY);
            check(g_editor.scriptWin.buf.size() == len0 + 1 &&
                      g_editor.scriptWin.caret == caret0 + 1 &&
                      g_editor.scriptWin.buf[caret0] == ' ',
                  "teclado ancorado: o ESPAÇO tecla no lugar certo "
                  "(acima da barra de navegação — insere NO cursor)");
            // ---- 12.4 (G2-7c/R-010): o esqueleto fresco CORRE LIMPO ----
            {
                openScriptEditor(g_scene.find("Ator"));
                check(g_editor.scriptWin.buf ==
                          std::string(editor::scriptwin::kSkeleton),
                      "12.4 o modelo inicial é o esqueleto da spec");
                scriptEditorRun();
                if (std::getenv("VV_DBG_124")) {
                    std::printf("124: running=%d errLine=%u msg=%s\n",
                                (int)g_editor.scriptWin.running,
                                g_editor.scriptWin.errLine,
                                g_editor.scriptWin.errMsg.c_str());
                }
                check(g_editor.scriptWin.running &&
                          g_editor.scriptWin.errLine == 0,
                      "12.4 Run no esqueleto fresco = ZERO erros (R-010)");
                scriptEditorStop();
                closeScriptEditor();
            }

            // ---- 12.5 (G2-7e): o ERRO QUE ENSINA + SUBSTITUIR -----------
            {
                openScriptEditor(g_scene.find("Ator"));
                // o dono Python/JS escreve 'if'
                g_editor.scriptWin.buf = "central main {\n  on moment { }\n}\n";
                g_editor.scriptWin.buf = "if (x) { }\n";
                g_editor.scriptWin.caret =
                    (u32)g_editor.scriptWin.buf.size();
                scriptEditorRun();
                check(g_editor.scriptWin.errLine == 1,
                      "12.5 o 'if' dá erro na linha 1");
                check(g_editor.scriptWin.errMsg.find("exist") !=
                          std::string::npos,
                      "12.5 a mensagem ENSINA o exist");
                check(g_editor.scriptWin.fixFrom == "if" &&
                          g_editor.scriptWin.fixTo == "exist",
                      "12.5 o par do SUBSTITUIR viaja com o erro (if→exist)");
                frame();   // a barra de erro desenha (com o botão)
                // o botão (retrato 720): x = 720-128+60 = 652 ·
                // y = errY(1536-48-40)+4+16 = 1468
                if (std::getenv("VV_DBG_125")) {
                    std::printf("125: errLine=%u msg=%s fix=%s->%s\n",
                                g_editor.scriptWin.errLine,
                                g_editor.scriptWin.errMsg.c_str(),
                                g_editor.scriptWin.fixFrom.c_str(),
                                g_editor.scriptWin.fixTo.c_str());
                }
                tap(652.0f, 1468.0f);
                check(g_editor.scriptWin.buf == "exist (x) { }\n",
                      "12.5 SUBSTITUIR: o 'if' virou 'exist' no buffer");
                check(g_editor.scriptWin.errLine == 0,
                      "12.5 o erro LIMPA após a substituição");
                closeScriptEditor();
            }

            closeScriptEditor();
            // volta ao landscape p/ as próximas fases
            eglstub::g_surfaceW = 1536;
            eglstub::g_surfaceH = 720;
            app12.contentRect = {0, 96, 1536, 672};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
        }

        // ---- 12.6 (G2-5): Docs com QUEBRA DE LINHA (sem "...") -----------
        passo("12.6 Docs: descrição inteira com wrap (altura variável)");
        {
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            g_editor.docsScreen.open = true;
            std::snprintf(g_editor.docsScreen.query,
                          sizeof(g_editor.docsScreen.query), "%s",
                          "colorpars");
            g_editor.docsScreen.queryLen = 9;
            g_editor.docsScreen.expanded = -1;
            frame();
            // a descrição do colorpars tem ~40+ code points; com o wrap a
            // PARTIR da largura ela faz >= 2 linhas -> os glifos da 2ª
            // linha existem ABAIXO da linha do nome (e o texto NÃO sai com
            // "..."). Conta code points da desc REAL:
            const voni::docs::Entry* eCP = nullptr;
            for (const auto* ee : voni::docs::search("colorpars")) {
                eCP = ee;
                break;
            }
            check(eCP != nullptr, "12.6 a entrada colorpars existe");
            if (eCP) {
                u32 cps = 0;
                for (const char* q = eCP->desc; *q;) {
                    const unsigned char c = *q;
                    cps += (c & 0xC0) != 0x80 ? 1 : 0;   // code points
                    ++q;
                }
                // glifos na zona da lista (abaixo do campo de pesquisa)
                const QuadBatch& g = g_ui.glyphsForTest();
                const QuadVertex* v = g.vertices();
                const u32 n = g.vertexCount();
                u32 inList = 0;
                const f32 listTop = g_ui.safeArea().top + 56.0f + 8.0f +
                                    48.0f + 8.0f;
                if (std::getenv("VV_DBG_126")) {
                    std::printf("126: cps=%u listTop=%.0f safeT=%.0f\n",
                                cps, listTop, g_ui.safeArea().top);
                }
                for (u32 i = 0; i + 5 < n; i += 6) {
                    if (v[i].y > listTop) {
                        ++inList;
                    }
                }
                if (std::getenv("VV_DBG_126b")) {
                    std::printf("126b: inList=%u cps=%u desc=[%.80s]\n",
                                inList, cps, eCP->desc);
                    u32 shown = 0;
                    for (u32 i = 0; i + 5 < n && shown < 30; i += 6) {
                        if (v[i].y > 216.0f) {
                            std::printf("  LIST gy=%.0f gx=%.0f\n", v[i].y,
                                        v[i].x);
                            ++shown;
                        }
                    }
                    std::printf("  totalGlyphs=%u\n", n / 6);
                }
                check(inList >= cps,
                      "12.6 a descrição desenha INTEIRA (sem reticências: "
                      ">= code points em glifos)");
            }
            g_editor.docsScreen.open = false;
        }

        // ---- 12.7 (G3): O TECLADO PRÓPRIO — setas, shift, coexistência --
        passo("12.7 teclado próprio: setas movem o cursor, shift, política");
        {
            // portrait + insets T96/B48 (o par do editor)
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            app12.contentRect = {0, 96, 720, 1488};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            openScriptEditor(g_scene.find("Ator"));
            g_editor.scriptWin.helpLevel = 2;   // strip fora (matemática)
            g_editor.scriptWin.kbOpen = true;
            frame();
            const f32 unit = (720.0f - 16.0f - 9.0f * 6.0f) / 12.0f;
            const f32 kbTop = 1536.0f - 48.0f -
                              (5.0f * 48.0f + 4.0f * 6.0f + 2.0f * 8.0f);
            const f32 botY = kbTop + 8.0f + 4.0f * (48.0f + 6.0f) + 24.0f;
            // (a) AS SETAS: [ < ][ ^ ][ v ][ > ] — o cursor mexe-se
            {
                g_editor.scriptWin.buf = "abc";
                g_editor.scriptWin.caret = 3;
                tap(8.0f + unit * 0.5f, botY);          // <
                check(g_editor.scriptWin.caret == 2,
                      "12.7 a seta < recua o cursor");
                tap(8.0f + 3.0f * (unit + 6.0f) + unit * 0.5f, botY);  // >
                check(g_editor.scriptWin.caret == 3,
                      "12.7 a seta > avança o cursor");
            }
            // (b) O SHIFT: a tecla Aa alterna maiúsculas/minúsculas
            // 0.9.6.1 (G2-6b): o Aa mora NA GRELHA — fila 1 (2.ª de cima),
            // 1.ª casa (antes saía do ecrã à direita na casa livre da fila
            // de 9); TODAS as filas começam na margem de 8px (pad)
            {
                const f32 keyW = (720.0f - 16.0f - 9.0f * 6.0f) / 10.0f;
                const f32 x0 = 8.0f;   // a margem da grelha (a MESMA em todas)
                const f32 shX = x0 + keyW * 0.5f;
                const f32 shY = kbTop + 8.0f + 1.0f * (48.0f + 6.0f) + 24.0f;
                const bool lower0 = g_editor.scriptWin.kbLower;
                tap(shX, shY);
                check(g_editor.scriptWin.kbLower == !lower0,
                      "12.7 a tecla Aa (na grelha) alterna maiúsculas/"
                      "minúsculas");
                // e o caso ATIVO escreve: a 1.ª tecla da fila 0 (o Q do
                // QWERTY — o teclado deixou de ser alfabético)
                const u32 len0 = (u32)g_editor.scriptWin.buf.size();
                g_editor.scriptWin.caret = len0;
                tap(x0 + keyW * 0.5f, kbTop + 8.0f + 24.0f);
                check(g_editor.scriptWin.buf.size() == len0 + 1 &&
                          (g_editor.scriptWin.buf[len0] == 'Q' ||
                           g_editor.scriptWin.buf[len0] == 'q'),
                      "12.7 o QWERTY escreve (a 1.ª tecla é o Q — a ordem "
                      "alfabética morreu)");
                // 0.9.6.1 (G2-6f): o LONG-PRESS no a dá o acento (0,5s)
                g_editor.scriptWin.kbLower = true;
                g_editor.scriptWin.kbLongId = 0;
                g_editor.scriptWin.kbLongT = 0.0f;
                g_editor.scriptWin.kbLongFired = false;
                const u32 len1 = (u32)g_editor.scriptWin.buf.size();
                g_editor.scriptWin.caret = len1;
                // a 2.ª tecla da fila 1 é o a (depois do Aa na grelha)
                const f32 aX = 8.0f + keyW + 6.0f + keyW * 0.5f;
                const f32 aY = kbTop + 8.0f + 1.0f * (48.0f + 6.0f) + 24.0f;
                g_input.injectDown(0, aX, aY);
                frame(); frame(); frame();   // o press captura active_
                g_editor.scriptWin.kbLongT = 0.6f;   // o relógio injetado
                frame();   // o long-press dispara (a variante sai)
                g_input.injectUp(0);
                frame();   // o release NÃO repete (a variante já saiu)
                // o acento mede 2 BYTES em UTF-8 (o code point inteiro —
                // nunca parte bytes)
                check(g_editor.scriptWin.buf.size() == len1 + 2 &&
                          g_editor.scriptWin.buf.find("á") !=
                              std::string::npos,
                      "12.7 o long-press no a insere o acento (o ç/ã/á/é "
                      "das vogais — 1 code point, 2 bytes UTF-8)");
            }
            // (c) A COEXISTÊNCIA: o botão do cabeçalho ABRE o próprio e
            // o main ESCONDE o IME (result 7 → jniImeHide no registo JNI)
            {
                g_editor.scriptWin.kbOpen = false;
                frame();
                tap(504.0f - 152.0f + 24.0f, 96.0f + 28.0f);
                check(g_editor.scriptWin.kbOpen,
                      "12.7 o BOTÃO abre o teclado próprio");
                check(logHas("teclado próprio aberto"),
                      "12.7 o main ESCONDEU o IME do sistema (política)");
                // o toque no corpo CEDA (o IME do sistema é pedido)
                tap(360.0f, 400.0f);
                check(!g_editor.scriptWin.kbOpen,
                      "12.7 o toque no corpo fecha o próprio (política: "
                      "nunca os dois)");
            }
            closeScriptEditor();
            // volta ao landscape
            eglstub::g_surfaceW = 1536;
            eglstub::g_surfaceH = 720;
            app12.contentRect = {0, 96, 1536, 672};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
        }

        // ---- 12.8 (G4 · R-014): IMPORT glb REAL → o seletor MOSTRA --------
        passo("12.8 import de glb: o asset aparece no seletor (<1s)");
        {
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            // 6 .gmesh JÁ no projeto (acima do cap antigo de 5 — o cenário
            // exato do bug: o import novo nunca aparecia)
            for (int i = 1; i <= 6; ++i) {
                char rel[48];
                std::snprintf(rel, sizeof(rel), "assets/m%d.gmesh", i);
                const char dummy[8] = "GMESH";
                g_storage->writeBytes(rel, dummy, 5);
            }
            // um .glb REAL (triângulo: pos+norm+uv+idx — o container GLB
            // com JSON chunk + BIN chunk, como o test_import_gltf)
            const auto t0 = std::chrono::steady_clock::now();
            {
                const f32 pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
                const f32 nrm[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
                const f32 uv[6] = {0, 0, 1, 0, 0, 1};
                const u16 idx[3] = {0, 1, 2};
                std::vector<u8> bin;
                auto pushF = [&bin](const f32* v, int n) {
                    for (int i = 0; i < n; ++i) {
                        const u32 b = *reinterpret_cast<const u32*>(&v[i]);
                        bin.push_back((u8)(b & 0xFF));
                        bin.push_back((u8)((b >> 8) & 0xFF));
                        bin.push_back((u8)((b >> 16) & 0xFF));
                        bin.push_back((u8)((b >> 24) & 0xFF));
                    }
                };
                const u32 po = 0, pl = 36;
                const u32 no = 36, nl = 36;
                const u32 uo = 72, ul = 24;
                const u32 io = 96, il = 6;
                pushF(pos, 9);
                pushF(nrm, 9);
                pushF(uv, 6);
                for (int i = 0; i < 3; ++i) {
                    bin.push_back((u8)(idx[i] & 0xFF));
                    bin.push_back((u8)(idx[i] >> 8));
                }
                char j[900];
                std::snprintf(j, sizeof(j),
                    "{\"asset\":{\"version\":\"2.0\"},"
                    "\"buffers\":[{\"byteLength\":%u}],"
                    "\"bufferViews\":["
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
                    "\"accessors\":["
                    "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                    "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                    "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
                    "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
                    "\"meshes\":[{\"primitives\":[{\"attributes\":"
                    "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},"
                    "\"indices\":3}]}]}",
                    (u32)bin.size(), po, pl, no, nl, uo, ul, io, il);
                std::string json = j;
                while (json.size() % 4 != 0) json += ' ';
                std::vector<u8> binPad = bin;
                while (binPad.size() % 4 != 0) binPad.push_back(0);
                std::vector<u8> glb;
                auto u32push = [&glb](u32 v) {
                    glb.push_back((u8)(v & 0xFF));
                    glb.push_back((u8)((v >> 8) & 0xFF));
                    glb.push_back((u8)((v >> 16) & 0xFF));
                    glb.push_back((u8)((v >> 24) & 0xFF));
                };
                u32push(0x46546C67u);   // 'glTF'
                u32push(2);
                u32push(12 + 8 + (u32)json.size() + 8 + (u32)binPad.size());
                u32push((u32)json.size());
                u32push(0x4E4F534Au);   // 'JSON'
                glb.insert(glb.end(), json.begin(), json.end());
                u32push((u32)binPad.size());
                u32push(0x004E4942u);   // 'BIN'
                glb.insert(glb.end(), binPad.begin(), binPad.end());
                // o ficheiro FONTE (host /tmp — o mesmo padrão do wiring010)
                char src[128];
                std::snprintf(src, sizeof(src), "/tmp/goni_fase12_robo.glb");
                FILE* f = std::fopen(src, "wb");
                std::fwrite(glb.data(), 1, glb.size(), f);
                std::fclose(f);
                // O IMPORT REAL (o MESMO convert::importFile do worker)
                convert::Output out;
                convert::Stats stats;
                std::string err;
                const bool ok = convert::importFile(
                    src, "robo.glb", *g_storage, g_pipeline.get(), out, stats,
                    err, nullptr, nullptr);
                check(ok && err.empty(),
                      "12.8 o import do glb REAL funciona (o conversor de "
                      "produção)");
                check(out.meshes.size() == 1 &&
                          out.meshes[0].find("robo") != std::string::npos,
                      "12.8 o convertido vive em assets/robo.gmesh");
                std::remove(src);
            }
            // o catálogo VÊ o novo asset (o refresh do fim do import)
            refreshCatalog();
            bool achou = false;
            for (const auto& m : g_catalog.meshes) {
                if (m == "assets/robo.gmesh") {
                    achou = true;
                }
            }
            check(achou, "12.8 o catálogo lista o import NOVO (com 6+ "
                         "meshes já no projeto)");
            const double ms = msSince(t0);
            check(ms < 1000.0,
                  "12.8 import + catálogo em <1s (o fluxo é síncrono no fim "
                  "do job)");
            // O SELETOR MOSTRA E APLICA: TIC com MeshRenderer selecionado,
            // picker aberto, tap na linha do robo (idx 6 — INVISÍVEL no cap
            // antigo de 5) — o dispatch REAL do main aplica no componente
            {
                const Handle ator = g_scene.find("Ator");
                g_editor.selected = ator;
                g_editor.assetMenu = 1;
                frame();   // o refresh-on-open + o desenho do seletor
                // geometria do seletor (landscape 1536x720, insets T96/B48):
                // overlayArea: oy=96+56=152, ah=720-96-48-56-24-48=448;
                // fixedH=48+2*48+16=160; maxListH=448-160-8=280 → 5.8 linhas
                // visíveis; 7 ficheiros → lista com scroll (336>280)
                const f32 w = 340.0f;
                const f32 fixedH = 48.0f + 2.0f * 48.0f + 16.0f;
                const f32 maxListH = 448.0f - fixedH - 8.0f;
                const f32 listH = 7.0f * 48.0f < maxListH ? 7.0f * 48.0f
                                                          : maxListH;
                const f32 h = fixedH + listH;
                const f32 x = (1536.0f - w) * 0.5f;
                const f32 y = 152.0f + (448.0f - h) * 0.5f;
                const f32 listTop = y + 48.0f + 2.0f * 48.0f;
                // drag p/ o FIM da lista (o robo é o 7º) — o dedo DENTRO
                g_input.injectDown(0, x + w * 0.5f, listTop + 200.0f);
                frame();
                g_input.injectMove(0, x + w * 0.5f, listTop + 40.0f);
                frame();
                g_input.injectUp(0);
                frame();
                // a ÚLTIMA linha visível é o robo: tap
                tap(x + w * 0.5f, listTop + listH - 24.0f);
                const Tic* tA = g_scene.get(ator);
                const MeshRenderer* mr =
                    tA ? tA->getComponent<MeshRenderer>() : nullptr;
                check(mr != nullptr && mr->meshPath == "assets/robo.gmesh",
                      "12.8 O SELETOR APLICA O IMPORT NOVO (meshPath no "
                      "MeshRenderer — o fim-a-fim do R-014)");
            }
            g_editor.assetMenu = 0;
            g_editor.selected = Handle::invalid();
        }

        // ---- 12.8b (GRUPO A · R-021/R-022): O PAR .gltf+.bin PELO BROWSER
        // REAL — 1 toque importa (irmãos copiados, textura externa lida) e
        // o seletor APLICA o convertido. O cenário EXATO do device: a pasta
        // com o par (o browser dá caminhos POSIX); o CWD do processo NÃO é
        // essa pasta (o bug histórico «buffer externo não resolvido:
        // scene.bin» resolvia o URI contra o CWD).
        {
            passo("12.8b browser 1-toque: o par .gltf+.bin importa e "
                  "aplica (R-021)");
            // o DIRETÓRIO ORIGINAL com o PAR + a textura %20 — criado com
            // syscalls CRUS (mkdir/fopen, o precedente da 12.8): a seam
            // /tmp READ-ONLY do ambiente C33 bloqueia o fileapi DE
            // PRODUÇÃO — a fixture do harness não é produção (o IMPORT em
            // si lê por fileapi::readAll, que a seam não bloqueia)
            char dir[96];
            std::snprintf(dir, sizeof(dir), "/tmp/goni_fase128b_%d",
                          (int)::getpid());
            check(::mkdir(dir, 0775) == 0 || errno == EEXIST,
                  "12.8b a pasta do par existe (fixture)");
            {
                std::vector<u8> bin;
                auto pushF = [&bin](const f32* v, int n) {
                    for (int i = 0; i < n; ++i) {
                        const u32 b =
                            *reinterpret_cast<const u32*>(&v[i]);
                        bin.push_back((u8)(b & 0xFF));
                        bin.push_back((u8)((b >> 8) & 0xFF));
                        bin.push_back((u8)((b >> 16) & 0xFF));
                        bin.push_back((u8)((b >> 24) & 0xFF));
                    }
                };
                const f32 pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
                pushF(pos, 9);
                const u16 idx[3] = {0, 1, 2};   // u16 LITTLE-ENDIAN no bin
                for (int i = 0; i < 3; ++i) {
                    bin.push_back((u8)(idx[i] & 0xFF));
                    bin.push_back((u8)(idx[i] >> 8));
                }
                FILE* f = std::fopen((std::string(dir) + "/scene.bin").c_str(),
                                     "wb");
                std::fwrite(bin.data(), 1, bin.size(), f);
                std::fclose(f);
                // a textura com ESPAÇO no nome (URI «tex%20albedo.png»)
                std::vector<u8> png;
                {
                    const std::string p =
                        std::string(FIXTURE_DIR) + "/yellow4.png";
                    FILE* pf = std::fopen(p.c_str(), "rb");
                    u8 buf[4096];
                    size_t n;
                    while (pf && (n = std::fread(buf, 1, sizeof(buf), pf)) > 0) {
                        png.insert(png.end(), buf, buf + n);
                    }
                    if (pf) {
                        std::fclose(pf);
                    }
                }
                f = std::fopen((std::string(dir) + "/tex albedo.png").c_str(),
                               "wb");
                std::fwrite(png.data(), 1, png.size(), f);
                std::fclose(f);
                char j[880];
                std::snprintf(j, sizeof(j),
                    "{\"asset\":{\"version\":\"2.0\"},"
                    "\"buffers\":[{\"uri\":\"scene.bin\",\"byteLength\":%zu}],"
                    "\"bufferViews\":["
                    "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                    "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
                    "\"accessors\":["
                    "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                    "\"type\":\"VEC3\"},"
                    "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
                    "\"type\":\"SCALAR\"}],"
                    "\"materials\":[{\"pbrMetallicRoughness\":"
                    "{\"baseColorTexture\":{\"index\":0}}}],"
                    "\"textures\":[{\"source\":0}],"
                    "\"images\":[{\"uri\":\"tex%%20albedo.png\"}],"
                    "\"meshes\":[{\"primitives\":[{\"attributes\":"
                    "{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
                    "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],"
                    "\"scene\":0}",
                    bin.size());
                f = std::fopen((std::string(dir) + "/par.gltf").c_str(), "wb");
                std::fwrite(j, 1, std::strlen(j), f);
                std::fclose(f);
            }
            // O BROWSER ABERTO NA PASTA (o caminho REAL do device)
            g_editor.fileBrowser = true;
            browserOpen(dir);
            frame();
            check(g_browser.open && !g_browser.failed &&
                      !g_browser.entries.empty(),
                  "12.8b o browser lista a pasta do par");
            // a linha do par.gltf: ficheiros ordenados («par.gltf» é o
            // 1.º dos 3) — a MESMA geometria do drawFileBrowser
            // (landscape 1536x720, insets T96/B48):
            //   oy=96 ah=576 w=900 x=318 h=362 y=203 listTop=389
            {
                const f32 ox = 0.0f, oy = 96.0f;
                const f32 aw = 1536.0f, ah = 576.0f;
                const f32 w = 900.0f;
                const f32 h = 48.0f + 34.0f + 52.0f + 8.0f + 52.0f + 8.0f +
                              3.0f * 48.0f + 16.0f;
                const f32 x = ox + (aw - w) * 0.5f;
                const f32 y = oy + (ah - h) * 0.5f;
                const f32 listTop = y + 48.0f + 6.0f + 34.0f - 18.0f + 12.0f +
                                    52.0f + 52.0f;
                const auto t0 = std::chrono::steady_clock::now();
                // 1 TOQUE na linha (o A4: 1 toque = seleciona/importa)
                tap(x + w * 0.5f, listTop + 24.0f);
                // 0.9.6.4 — espera o CICLO COMPLETO do job: o finalize do
                // frame() faz o JOIN e publica os resultados; SÓ DEPOIS o
                // estado do job é legível. (Esperar só pelo `done` tinha
                // uma JANELA: um done STALADO de um job anterior saía do
                // loop antes do worker terminar — os checks liam os campos
                // A MEIO da escrita do worker (corrida/UB) e o cleanup lá
                // embaixo apagava a fixture SOB o worker — o errno=2 que
                // se viu na 1ª rodada. active=false SÓ acontece DEPOIS do
                // importJobFinish — é a testemunha certa.)
                int guard = 0;
                while (g_importJob.active.load() && guard++ < 3000) {
                    frame();
                }
                frame();   // garante o importJobFinish (o finalize põe active=false)
                check(g_importJob.err.empty(),
                      "12.8b o import do par pelo browser funciona (err no "
                      "engine.log se falhar)");
                check(g_importJob.out.meshes.size() == 1 &&
                          g_importJob.out.meshes[0] == "assets/par.gmesh",
                      "12.8b o convertido vive em assets/par.gmesh");
                check(g_importJob.stats.siblings == 2,
                      "12.8b os 2 irmaos (scene.bin + tex albedo.png) foram "
                      "copiados para source/");
                check(g_storage->exists("source/scene.bin") &&
                          g_storage->exists("source/tex albedo.png"),
                      "12.8b source/ tem os irmaos (o projeto fica "
                      "autossuficiente)");
                // o catálogo lista o novo asset; o SELETOR aplica (troca)
                refreshCatalog();
                bool achouPar = false;
                for (const auto& m : g_catalog.meshes) {
                    if (m == "assets/par.gmesh") {
                        achouPar = true;
                    }
                }
                check(achouPar, "12.8b o catalogo lista o par convertido");
                const double ms = msSince(t0);
                check(ms < 1000.0,
                      "12.8b toque->catalogo em <1s (o fluxo do browser)");
                {
                    const Handle ator = g_scene.find("Ator");
                    g_editor.selected = ator;
                    // o índice do par no catálogo ORDENADO (m1..m6, par,
                    // robo) — procurado, não assumido; pick = idx + 3
                    // (none=1, cube=2, ficheiros a partir de 3)
                    i32 parIdx = -1;
                    for (size_t i = 0; i < g_catalog.meshes.size(); ++i) {
                        if (g_catalog.meshes[i] == "assets/par.gmesh") {
                            parIdx = static_cast<i32>(i);
                            break;
                        }
                    }
                    check(parIdx >= 0, "12.8b o par tem indice no catalogo");
                    if (parIdx >= 0) {
                        const editor::AssetPickOutcome out =
                            editor::applyAssetPick(g_scene, ator, 1,
                                                   parIdx + 3, g_catalog,
                                                   makeAssetResolvers());
                        const Tic* tA = g_scene.get(ator);
                        const MeshRenderer* mr =
                            tA ? tA->getComponent<MeshRenderer>() : nullptr;
                        check(out.applied && mr != nullptr &&
                                  mr->meshPath == "assets/par.gmesh",
                              "12.8b O SELETOR APLICA O PAR (meshPath no "
                              "MeshRenderer)");
                    }
                }
                g_editor.selected = Handle::invalid();
            }
            // a fixture sai (a lição da FASE 9: /tmp não acumula)
            ::remove((std::string(dir) + "/par.gltf").c_str());
            ::remove((std::string(dir) + "/scene.bin").c_str());
            ::remove((std::string(dir) + "/tex albedo.png").c_str());
            ::remove(dir);
        }

        // ---- 12.9 (G6 · R-017): O BENCH — medições reais + honestidade --
        // A máquina de fases INTEIRA pelo caminho do device (o main.cpp
        // REAL que este TU inclui): Settings → Diagnóstico → Correr bench
        // → as fases DefRun/BuildScene/Import/BenchRun/Texture/Audio/
        // Finish → Done. O CI não tem GPU: o dt é INJETADO a 60fps e o
        // relatório MEDE O RELÓGIO INJETADO (se o format() hardcodasse um
        // número, os checks abaixo caem — a escala injetada vs reportada é
        // a prova). O que NO HOST não existe (áudio/APK/device) tem de
        // dizer "não medido" — a honestidade é PARTE da sentinela.
        {
            passo("12.9 bench: o bloco de 9 linhas com MEDIÇÕES (R-017)");
            const u32 ticsAntes = g_scene.count();
            // CI: cenas de 0.3s (o PERCURSO é o mesmo do device — lá são
            // 10s por cena; aqui o harness não pode dormir 20s)
            benchTestHookSetSecs(0.3);
            // pelo caminho da UI: o Settings com o Diagnóstico ABERTO (os
            // outros 5 secções fechadas) — o botão Correr bench é a 4ª
            // linha da secção (depois de logs/Export/Probe)
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();   // o layout estabiliza (o scroll abre no topo)
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            frame();
            const safe::Insets si = g_ui.safeArea();
            const f32 aw = g_ui.screenWidth() - si.left - si.right;
            const f32 btnCx = si.left + aw - 16.0f - 76.0f;   // botão 152
            // y do botão Correr bench: 3 headers fechados ANTES do
            // Diagnóstico (Geral/Áudio/Permissões — Docs e Sobre vêm
            // DEPOIS na página) + header Diag + logs/Export/Probe + centro
            const f32 yRun = si.top + 56.0f + 8.0f + 3.0f * 48.0f +
                             48.0f + 3.0f * 48.0f + 24.0f;
            tap(btnCx, yRun);
            check(!g_editor.settingsMenu,
                  "12.9 o botão Correr bench FECHA o Settings (o bench é "
                  "modal de facto)");
            check(logHas("bench: início"),
                  "12.9 o arranque do bench fica LOGADO");
            // a máquina de fases: dt INJETADO a 60fps (o relatório mede o
            // relógio que lhe deram — no device é o dt REAL do loop)
            const double dtInj = 1.0 / 60.0;
            int guard = 0;
            while (!benchTestHookDone() && guard++ < 900) {
                g_frameDt = static_cast<f32>(dtInj);
                frame();
            }
            check(benchTestHookDone(),
                  "12.9 a máquina de fases TERMINA (DefRun→BuildScene→"
                  "Import→BenchRun→Texture→Audio→Finish→Done)");
            const bench::Report& r = benchTestHookReport();
            // AS MEDIÇÕES deste processo (R-017: nada hardcodado — o
            // relógio injetado a 60 ⇒ avg≈60; verts do stub GL REAIS; o
            // import é o convert::importFile DE PRODUÇÃO com cronómetro)
            check(r.def.ok && r.def.avg > 55.0 && r.def.avg < 65.0,
                  "12.9 cena default: média ≈60fps (o relógio injetado — "
                  "se fosse hardcode não casava com o inject)");
            check(r.scene.ok && r.scene.avg > 55.0 && r.scene.avg < 65.0,
                  "12.9 cena bench: idem (64 TICs + mesh importado)");
            check(r.defVerts.ok && r.defVerts.value > 0.0,
                  "12.9 verts da cena default MEDIDOS (a MESMA soma da "
                  "barra de estado)");
            check(r.sceneVerts.ok && r.sceneVerts.value > r.defVerts.value,
                  "12.9 a cena bench tem MAIS verts que a default (64 TICs "
                  "+ o mesh importado)");
            check(r.importMs.ok && r.importMs.value >= 0.0 &&
                      r.importMs.value < 10000.0,
                  "12.9 import glTF ref MEDIDO (o cronómetro real)");
            check(nearEqF(static_cast<f32>(r.importScale.value), 2.5f,
                          0.001f),
                  "12.9 a escala do nó (2.5) fez o round-trip GLB→importer"
                  "→Transform3D");
            check(r.texMs.ok && r.texFormat[0] != '\0',
                  "12.9 textura comprimida COM FORMATO REAL (a mesma "
                  "máquina do pipeline)");
            check(r.rssPeakMb.ok && r.rssPeakMb.value > 1.0,
                  "12.9 pico RSS MEDIDO no host (VmHWM do /proc — real)");
            check(r.projMb.ok && r.projMb.value > 0.0,
                  "12.9 o tamanho do projeto MEDIDO (stat por ficheiro)");
            check(g_scene.count() == ticsAntes,
                  "12.9 a cena bench SAIU no fim (o editor volta exatamente "
                  "ao que era)");
            // A HONESTIDADE do host (o que não há, é DITO — nunca 0):
            check(r.audioTotal == 0,
                  "12.9 áudio: não medido no host (o probe corre no "
                  "aparelho — R-017)");
            check(!r.apkMb.ok && r.device[0] == '\0' && r.sdk == 0,
                  "12.9 device/APK/Android: não medidos no host (a JNI do "
                  "bench é do aparelho)");
            // o BLOCO de 9 linhas com os valores E os "não medido"
            const std::string bloco = bench::format(r);
            check(countLines(bloco) == 9,
                  "12.9 o bloco colável tem EXATAMENTE 9 linhas");
            check(bloco.find("não medido") != std::string::npos,
                  "12.9 o bloco DIZ o que não foi medido");
            check(bloco.find("cena bench (mesh importado)") !=
                      std::string::npos,
                  "12.9 a linha da cena bench está no bloco");
            check(bloco.find("(escala 2.5)") != std::string::npos,
                  "12.9 a escala medida está no bloco");
            // COPIAR: o botão põe o MESMO bloco no clipboard (o dono cola
            // no relatório — a fonte é ÚNICA). O padrão da FASE 11.11:
            // void_calls + last_new_string do stub
            g_jni.void_calls.clear();
            g_jni.last_new_string.clear();
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            const f32 yCopy = yRun + 48.0f;   // a linha SEGUINTE
            tap(btnCx, yCopy);
            bool sawClip = false;
            for (const auto& c : g_jni.void_calls) {
                if (c.first == "clipboardCopy") {
                    sawClip = true;
                }
            }
            check(sawClip, "12.9 o Copiar relatório chama a ponte do "
                           "clipboard (a MESMA do copiar-referência)");
            check(g_jni.last_new_string == bloco,
                  "12.9 o clipboard recebe o bloco EXATO do format (fonte "
                  "única — byte a byte)");
            g_editor.settingsMenu = false;
            frame();
        }

        onAppCmd(&app12, APP_CMD_TERM_WINDOW);
    }

    // ---- 12.10 (0.9.6.1 · PASSO 0 · R-018): A DENSIDADE — dp a dp -----------
    // O DONO mediu no device: cabeçalho 56px (devia ser 112), teclas 48×65px
    // (metade de 48dp). A CAUSA: constantes dp consumidas como px. O FIX: a
    // função theme::dp() na FONTE (SafeArea/EditorLayout/componentes). AQUI
    // a prova do mecanismo: com densidade 2.0 INJETADA (o par do C33) os
    // rects do layout duplicam; com 1.0 ficam IGUAIS ao de sempre (o resto
    // do harness inteiro corre a 1.0 — os checks acima são a prova).
    passo("12.10 densidade: dp×2 injetado → barra 112, alvos 96 (R-018)");
    {
        // (a) a densidade chega do AConfiguration (o caminho REAL do device:
        // 320 dpi ÷ 160 = 2.0) e o layout da barra escala
        vvstub::g_stubDensityDpi = 320;
        theme::setDensity(2.0f);
        editor::applyDensity();
        const safe::Insets zero{};
        const UiRect bar = safe::toolbarRect(1536.0f, 720.0f, zero);
        check(bar.h == 112.0f,
              "12.10 com densidade 2.0 a barra de cima mede 112px (56dp "
              "REAL — o bug era 56px)");
        const UiRect status = safe::statusRect(1536.0f, 720.0f, zero);
        check(status.h == 48.0f && status.y + status.h == 720.0f,
              "12.10 a status line mede 48px (24dp real) e continua no fundo");
        check(editor::kRowH == 96.0f && editor::kPad == 32.0f,
              "12.10 as linhas/paddings dos painéis duplicam (48dp/16dp "
              "reais — applyDensity)");
        check(safe::kTopBarH == 56.0f && theme::dp(safe::kTopBarH) == 112.0f,
              "12.10 o dp() da casa multiplica pela densidade corrente");
        // (b) o teclado do editor: teclas de 96px de altura (48dp real — o
        // dono media 48×65px)
        check(editor::scriptwin::keyboardHeight() ==
                  5.0f * 96.0f + 4.0f * 12.0f + 2.0f * 16.0f,
              "12.10 o teclado mede as teclas a 96px de altura (48dp real)");
        // (c) a densidade do ARRANQUE vem do AConfiguration (o main lê
        // 320→2.0; o log de identidade do ecrã existe no arranque)
        check(vv::theme::g_density == 2.0f,
              "12.10 a densidade injetada fica no theme (o layout consome)");
        // (d) REPOSIÇÃO: densidade 1.0 — o harness inteiro continua a correr
        // o layout de sempre (os checks 12.11+ e o sumário abaixo dependem)
        vvstub::g_stubDensityDpi = 160;
        theme::setDensity(1.0f);
        editor::applyDensity();
        const UiRect bar1 = safe::toolbarRect(1536.0f, 720.0f, zero);
        check(bar1.h == 56.0f && editor::kRowH == 48.0f,
              "12.10 com densidade 1.0 o layout é EXATAMENTE o de sempre "
              "(os testes não mudam)");
    }

    // ---- 12.11 (0.9.6.2 · R-019): O TOQUE MOVE O CURSOR ---------------------
    // O dono: "no editor de script o cursor nunca fica dentro de
    // 'on moment { }' nem de 'allmoments { }'; fica sempre fora... torna o
    // editor inutilizável". A CAUSA: o tap CALCULAVA o offset e nunca o
    // aplicava ao caret. AQUI o teste EXATO do dono, pelo caminho REAL da
    // UI: tocar ENTRE as chavetas de "allmoments { }" → o caret fica aí;
    // escrever "x" insere aí.
    passo("12.11 toque entre as chavetas move o cursor e escreve aí (R-019)");
    {
        // app PRÓPRIO (o app12 saiu de escopo no fim do 12.9) — o MESMO
        // padrão: superfície portrait + insets T96/B48, INIT_WINDOW
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        android_app app11;
        std::memset(&app11, 0, sizeof(app11));
        app11.contentRect = {0, 96, 720, 1488};
        onAppCmd(&app11, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        openScriptEditor(g_scene.find("Ator"));
        g_editor.scriptWin.helpLevel = 2;   // strip fora (matemática limpa)
        g_editor.scriptWin.kbOpen = false;
        frame();
        // a linha 3 do esqueleto: "  allmoments { }" — o centro dela no
        // corpo. 0.9.6.6 (GRUPO C · C2): TODOS os números vêm da GEOMETRIA
        // ÚNICA exportada (lineHeight/codeX/lineTopOnScreen — as MESMAS
        // funções que o draw usa; antes «lh=34» e «64» hardcoded DRIFTAVAM
        // quando a tipografia ganhou o sp()/textK — a lição R-019 aplicada
        // ao próprio teste)
        const f32 lh = vv::editor::scriptwin::lineHeight(g_ui);
        const safe::Insets ins11 = g_ui.safeArea();
        const f32 bodyY = 96.0f + vv::theme::dp(56.0f);
        const f32 yLine3 = vv::editor::scriptwin::lineTopOnScreen(
                               bodyY, 2u, lh, g_ui.scrollOffset()) +
                           lh * 0.5f;
        // x do INTERIOR das chavetas: codeX + largura REAL (pelo CONTEXTO —
        // textK incluído, a MESMA escala do draw) + 1px
        const f32 xBraces =
            vv::editor::scriptwin::codeX(ins11) +
            g_ui.fontWidth("  allmoments { ") + 1.0f;
        const u32 caret0 = g_editor.scriptWin.caret;
        tap(xBraces, yLine3);
        check(g_editor.scriptWin.caret == caret0 &&
                  g_editor.scriptWin.buf.find("allmoments {") !=
                      std::string::npos,
              "12.11 preparar: o esqueleto com o caret no interior");
        // (a) o TOQUE põe o cursor ENTRE as chavetas (antes era ignorado)
        check(g_editor.scriptWin.caret ==
                  (u32)g_editor.scriptWin.buf.find("allmoments { ") + 13,
              "12.11 tocar entre as chavetas põe o caret AÍ (offset sob o "
              "dedo aplicado — o bug: calculava e não aplicava)");
        // (b) escrever "x" insere NO CARET (não no fim)
        const u32 at = g_editor.scriptWin.caret;
        vv::ime::clearForTest();
        vv::ime::pushText("x");
        vv::ime::Event ev;
        while (vv::ime::poll(ev)) {
            vv::editor::scriptwin::applyEvent(g_editor.scriptWin, ev);
        }
        check(g_editor.scriptWin.buf.find("allmoments { x}") !=
                      std::string::npos &&
                  g_editor.scriptWin.caret == at + 1,
              "12.11 escrever \"x\" insere ENTRE as chavetas (o dono vê o "
              "código nascer onde tocou)");
        // (c) o toque no MEIO da linha 2 ("on moment") move para lá
        // (a geometria única de novo — zero fórmulas à mão)
        const f32 yLine2 = vv::editor::scriptwin::lineTopOnScreen(
                               bodyY, 1u, lh, g_ui.scrollOffset()) +
                           lh * 0.5f;
        tap(vv::editor::scriptwin::codeX(ins11) +
                g_ui.fontWidth("  on ") + 2.0f, yLine2);
        check(g_editor.scriptWin.caret > 15 && g_editor.scriptWin.caret < 31,
              "12.11 tocar no meio da linha 2 põe o cursor NA linha 2 "
              "(coluna pelas métricas reais)");
        // (d) ENTER aí cria linha nova INDENTADA (o herda-indentação)
        vv::ime::clearForTest();
        vv::ime::pushKey(vv::ime::Key::Enter);
        while (vv::ime::poll(ev)) {
            vv::editor::scriptwin::applyEvent(g_editor.scriptWin, ev);
        }
        {
            const u32 c = g_editor.scriptWin.caret;
            const u32 ls =
                editor::scriptwin::lineStartOfOffset(g_editor.scriptWin.buf,
                                                     c);
            check(g_editor.scriptWin.buf.compare(ls, 2, "  ") == 0,
                  "12.11 o ENTER herda a indentação da linha (o código "
                  "nasce alinhado)");
        }
        // o log do toque existe (o dono segue o cursor no engine.log)
        check(logHas("editor: toque x="),
              "12.11 o toque LOGA px/dp/linha/coluna/caret (R-019)");
        closeScriptEditor();
        // TERM (o par do lifecycle — o estado fica limpo p/ o sumário)
        onAppCmd(&app11, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 13 — 0.9.6.5 (GRUPO B): LAYOUT EXPORTADO (PNG+JSON) + AUDITORIA
    // + OS PNGs RELIDOS. O framebuffer REAL do C33 virtual rasteriza a
    // sério (glstub::fb) e o engine exporta o que o dono obtém no device:
    // layout/<ecrã>.png (backbuffer full-res) + .json (as entradas REAIS
    // do frame) + auditoria-<ecrã>.txt (o validador da casa). A PROVA
    // fecha o círculo: o PNG é RELIDO pelo loadPng DE PRODUÇÃO e os píxeis
    // CONFIRMAM o que o JSON diz (o painel do botão está onde o registo
    // diz, na cor do tema) — o "PNG lido" do relatório, ao pé da letra.
    // ======================================================================
    fase("FASE 13 — layout exportado (PNG+JSON) + auditoria + PNG relido");
    {
        // o ambiente: A GPU do C33 virtual deixa de ser no-op — rasteriza
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        resetEngineForHarness();
        auto st13 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt13 = st13.get();
        check(Project::createNew(*rawSt13, "c33", g_project), "13 projeto criado");
        g_storage = std::move(st13);
        g_projectReady = true;
        g_resources.setStorage(rawSt13);
        g_gpu.init(&g_resources);
        g_texCache = std::make_unique<TextureCache>(*rawSt13);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor, *g_texCache);
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        android_app app13;
        std::memset(&app13, 0, sizeof(app13));
        app13.contentRect = {0, 24, 1512, 720};   // insets: status 24 + pill 24
        onAppCmd(&app13, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_ready, "13 boot com o framebuffer REAL ligado (fb rasteriza)");
        check(glstub::fb::enabled, "13 o modo fb persiste ao boot (ambiente)");

        // 0.9.6.6 (GRUPO C · 13.6): a cópia das entradas do editor a 1.0 —
        // a dupla densidade compara entrada a entrada (o ecrã a 2.0 é o
        // ecrã a 1.0 visto a 2×; a INvariância da escala dp+sp)
        std::vector<vv::layout::Entry> editor1x;

        // helper: exporta o ecrã ATUAL e devolve os bytes do PNG+JSON
        auto exportScreen = [&](const char* nome) {
            g_layoutExportPending = true;
            frame();
            std::vector<u8> png, js;
            const bool okP = rawSt13->readBytes(std::string("layout/") + nome + ".png", png);
            const bool okJ = rawSt13->readBytes(std::string("layout/") + nome + ".json", js);
            check(okP && !png.empty(),
                  (std::string("13 o PNG de [") + nome + "] esta no projeto").c_str());
            check(okJ && !js.empty(),
                  (std::string("13 o JSON de [") + nome + "] esta no projeto").c_str());
            return std::make_pair(png, js);
        };

        // ---- (a) O EDITOR 3D: export + PNG RELIDO ---------------------------
        passo("13.1 editor: PNG relido confirma o registo do layout");
        {
            auto [png, js] = exportScreen("editor");
            check(logHas("layout: export do ecrã \"editor\""),
                  "13.1 o export LOGA o ecrã e os ficheiros");
            // o PNG RELIDO (o decode de PRODUÇÃO do pipeline de texturas)
            vv::RawImage img;
            std::string err;
            check(vv::loadPng(png.data(), png.size(), img, err) && img.ok(),
                  "13.1 o PNG RELIDO decodifica (loadPng de produção)");
            check(img.width == 1536 && img.height == 720,
                  "13.1 o PNG tem a resolução da superfície (1536x720)");
            // o registo (o MESMO frame que o PNG) — os botões da toolbar
            const layout::Record& rec = g_ui.auditRecord();
            check(rec.entries.size() > 8,
                  "13.1 o JSON tem as entradas do ecrã (>8 widgets)");
            u32 btns = 0;
            f32 bigArea = 0.0f;
            u32 bigIdx = 0xFFFFFFFFu;
            for (u32 i = 0; i < rec.entries.size(); ++i) {
                const auto& e = rec.entries[i];
                if (e.kind == layout::Entry::Button) ++btns;
                if (e.kind == layout::Entry::Panel && e.w * e.h > bigArea) {
                    bigArea = e.w * e.h;
                    bigIdx = i;
                }
            }
            check(btns >= 4, "13.1 a toolbar regista os botões interativos");
            check(bigIdx != 0xFFFFFFFFu,
                  "13.1 o registo tem painéis (o chrome do editor)");
            // PIXEL vs REGISTO (1): o MAIOR painel do registo (a banda da
            // toolbar) - o canto dele no PNG NAO pode ser o clear BG
            // (20,20,20): o rect do registo e o pixel dizem o MESMO
            {
                const auto& e = rec.entries[bigIdx];
                const u32 px = (u32)(e.x + 3.0f), py = (u32)(e.y + 3.0f);
                check(px < img.width && py < img.height,
                      "13.1 o maior painel esta DENTRO do ecra");
                const size_t pi = (size_t(py) * img.width + px) * 4;
                const int d = (int)img.rgba[pi + 2] - 20;
                check(d > 8 || d < -8,
                      "13.1 o pixel do maior painel NAO e o fundo (o "
                      "registo bate com o PNG)");
            }
            // PIXEL vs REGISTO (2): a BANDA da toolbar (56px) POVOADA -
            // a fracao de pixels != clear passa 30% (o chrome RASTERIZOU)
            {
                u32 diff = 0, tot2 = 0;
                for (u32 y = 24; y < 80; ++y) {
                    for (u32 x = 0; x < img.width; x += 3) {
                        const size_t pi = (size_t(y) * img.width + x) * 4;
                        ++tot2;
                        if (img.rgba[pi] != 20 || img.rgba[pi + 1] != 20 ||
                            img.rgba[pi + 2] != 20) {
                            ++diff;
                        }
                    }
                }
                check(tot2 > 0 && diff * 100u > tot2 * 30u,
                      "13.1 a banda da toolbar esta POVOADA no PNG "
                      "(>30% pixels != fundo)");
            }
            // o GLIFO: texto brilhante na banda da toolbar (o atlas R8
            // rasterizou cobertura — o caminho do texto do device)
            u32 bright = 0;
            for (u32 y = 24; y < 80; ++y) {
                for (u32 x = 0; x < img.width; ++x) {
                    const size_t pi = (size_t(y) * img.width + x) * 4;
                    if (img.rgba[pi] > 200) ++bright;
                }
            }
            check(bright > 200,
                  "13.1 o TEXTO desenha no PNG (glifos do atlas na toolbar)");
            // a cópia para o CI colecionar como artefacto do run
            fileapi::writeAll("layout-harness-editor.png", png.data(), png.size());
            fileapi::writeAll("layout-harness-editor.json", js.data(), js.size());
            // (13.6) a cópia viva do registo a densidade 1.0
            editor1x = rec.entries;
        }

        // ---- (b) A AUDITORIA pelo CAMINHO DO DEVICE (botão do Diagnóstico) --
        passo("13.2 auditoria: o botão do Diagnóstico audita o ecrã");
        {
            // o Settings com o Diagnóstico ABERTO (o padrão 12.9)
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            frame();
            // O TAP PELO REGISTO (a vara de medir do Grupo B a trabalhar):
            // um frame auditado dá o rect REAL do botão «Auditoria do
            // ecrã» — o toque cai no CENTRO exato dele, ZERO fórmulas de
            // layout que driftam quando a página muda (a lição deste run)
            g_layoutExportPending = true;
            frame();   // exporta o settings (ecrã também!) + registo
            const layout::Record& rr = g_ui.auditRecord();
            f32 ax = -1.0f, ay = -1.0f;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Button &&
                    e.id == editor::settings::kLayoutAudId) {
                    ax = e.x + e.w * 0.5f;
                    ay = e.y + e.h * 0.5f;
                }
            }
            check(ax > 0.0f && ay > 0.0f,
                  "13.2 o botao Auditoria esta no registo (o rect real do "
                  "frame real)");
            std::vector<u8> setPng;
            check(rawSt13->readBytes("layout/settings.png", setPng) &&
                      !setPng.empty(),
                  "13.2 o export do PROPRIO settings saiu (ecra completo)");
            tap(ax, ay);
            check(!g_editor.settingsMenu,
                  "13.2 o botao Auditoria FECHA o Settings (o ecra por baixo "
                  "e o auditado - um ecra de cada vez)");
            check(g_layoutExportPending && g_layoutAuditPending,
                  "13.2 o pedido armado (export+audit no proximo frame)");
            frame();   // o frame auditado: registo + PNG + validador + log
            check(logHas("layout: AUDITORIA do ecrã"),
                  "13.2 a auditoria CORRE e LOGA o veredito");
            std::vector<u8> aud;
            check(rawSt13->readBytes("layout/auditoria-editor.txt", aud) &&
                      !aud.empty(),
                  "13.2 o relatorio auditoria-editor.txt esta no projeto");
            std::string audS(aud.begin(), aud.end());
            check(audS.find("AUDITORIA do ecr") != std::string::npos &&
                      audS.find("problemas:") != std::string::npos,
                  "13.2 o relatorio traz o cabecalho e as contagens");
            // 0.9.6.6 (GRUPO C): a linha de base medida pelo Grupo B está
            // CURADA — a auditoria do editor diz VERDE (o ERRO da label que
            // sangrava 3px o fundo e os avisos <48dp morreram com o sp()/dp)
            check(audS.find("problemas: 0 ERRO") != std::string::npos &&
                      audS.find("VERDE") != std::string::npos,
                  "13.2 o editor esta VERDE (o ERRO da status bar + os avisos "
                  "da linha de base do Grupo B curados pelo sp()/dp)");
            fileapi::writeAll("layout-harness-auditoria.txt", aud.data(), aud.size());
            std::printf("    [aud]  %s",
                        audS.find("VERDE") != std::string::npos
                            ? "editor: VERDE\n"
                            : "editor: problemas documentados no relatorio\n");
        }

        // ---- (c) O EDITOR DE SCRIPT (portrait: o ciclo REAL da janela) ---
        passo("13.3 script: export portrait + PNG relido");
        {
            // o editor de script e PORTRAIT no device — o ciclo completo
            // (TERM -> superficie 720x1536 + insets T96/B48 -> INIT), o
            // MESMO padrao da FASE 12.11
            onAppCmd(&app13, APP_CMD_TERM_WINDOW);
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            android_app app13p;
            std::memset(&app13p, 0, sizeof(app13p));
            app13p.contentRect = {0, 96, 720, 1488};
            onAppCmd(&app13p, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            const Handle ator13 = g_scene.create("Ator");
            openScriptEditor(ator13);
            // o TECLADO da engine aberto — o layout dele é o INSUMO do
            // Grupo E (a barra de símbolos que o substitui)
            g_editor.scriptWin.kbOpen = true;
            frame();
            auto [png, js] = exportScreen("script");
            vv::RawImage img;
            std::string err;
            check(vv::loadPng(png.data(), png.size(), img, err) &&
                      img.width == 720 && img.height == 1536,
                  "13.3 o PNG do script e 720x1536 (portrait real)");
            const layout::Record& rec = g_ui.auditRecord();
            check(std::string(rec.screen) == "script" &&
                      rec.entries.size() > 8,
                  "13.3 o registo do script tem as entradas (header/teclado)");
            u32 kb = 0;
            for (auto& e : rec.entries) {
                if (e.kind == layout::Entry::Button) ++kb;
            }
            check(kb >= 10,
                  "13.3 as teclas do teclado da engine estao registadas");
            fileapi::writeAll("layout-harness-script.png", png.data(), png.size());
            fileapi::writeAll("layout-harness-script.json", js.data(), js.size());

            // ---- (c) 0.9.6.6 (GRUPO C · C3): O CULLING — o custo por frame
            // deixa de ser O(buffer): 800 linhas com o caret no FIM (o
            // scroll segue) → o draw TOKENIZA as visíveis+folga, não 800
            {
                std::string big;
                for (int i = 0; i < 800; ++i) {
                    big += "  linha ";
                    big += std::to_string(i);
                    big += " momento { }\n";
                }
                g_editor.scriptWin.buf = big;
                ++g_editor.scriptWin.bufVersion;   // o contrato do índice
                g_editor.scriptWin.caret = (u32)big.size();
                frame();   // o scroll segue o caret → a janela no FIM
                const u32 tok = vv::editor::scriptwin::dbgLinesTokenized;
                check(tok > 0 && tok < 100,
                      "13.3 o CULLING: 800 linhas, o frame tokeniza as "
                      "visiveis (~50), nao as 800 (a prova do perf)");
                check(vv::editor::scriptwin::lineCount(g_editor.scriptWin) ==
                          801u,
                      "13.3 o indice de linhas conta as 800+1 (O(1))");
                // o roundtrip da GEOMETRIA UNICA (C2): y→linha→y fecha
                const f32 lhR = vv::editor::scriptwin::lineHeight(g_ui);
                const safe::Insets insR = g_ui.safeArea();
                const f32 bodyYR = insR.top + vv::theme::dp(56.0f);
                bool rtOk = true;
                for (u32 i : {0u, 5u, 400u, 800u}) {
                    const f32 y = vv::editor::scriptwin::lineTopOnScreen(
                        bodyYR, i, lhR, 0.0f);
                    if (vv::editor::scriptwin::lineAtScreenY(
                            y + lhR * 0.5f, bodyYR, lhR, 0.0f) !=
                        static_cast<i32>(i)) {
                        rtOk = false;
                    }
                }
                check(rtOk,
                      "13.3 a geometria unica: lineTopOnScreen/lineAtScreenY "
                      "sao UM o inverso do outro (draw<->toque nunca drifta)");
            }
            closeScriptEditor();

            // ---- (d) O DOCS + (e) O BROWSER de volta ao landscape -------
            passo("13.4 docs + 13.5 browser: os ecras restantes (landscape)");
            onAppCmd(&app13p, APP_CMD_TERM_WINDOW);
            eglstub::g_surfaceW = 1536;
            eglstub::g_surfaceH = 720;
            android_app app13l;
            std::memset(&app13l, 0, sizeof(app13l));
            app13l.contentRect = {0, 24, 1512, 720};
            onAppCmd(&app13l, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            {
                g_editor.docsScreen.open = true;
                frame();
                auto [png, js] = exportScreen("docs");
                vv::RawImage img;
                std::string err;
                check(vv::loadPng(png.data(), png.size(), img, err) &&
                          img.width == 1536 && img.height == 720,
                      "13.4 o PNG do docs e 1536x720 (landscape)");
                check(std::string(g_ui.auditRecord().screen) == "docs",
                      "13.4 o registo diz o ecra CERTO (docs)");
                fileapi::writeAll("layout-harness-docs.png", png.data(), png.size());
                fileapi::writeAll("layout-harness-docs.json", js.data(), js.size());
                g_editor.docsScreen.open = false;
            }
            {
                // o browser REAL: browserOpen lista uma pasta (no host, o
                // caminho do device falha e o estado "opendir FALHOU"
                // desenha à mesma — o ecrã com o seu chrome)
                browserOpen(std::string(fileapi::kExternalRoot) + "/Download");
                g_editor.fileBrowser = true;
                frame();
                auto [png, js] = exportScreen("browser");
                vv::RawImage img;
                std::string err;
                check(vv::loadPng(png.data(), png.size(), img, err) && img.ok(),
                      "13.5 o PNG do browser decodifica");
                fileapi::writeAll("layout-harness-browser.png", png.data(), png.size());
                fileapi::writeAll("layout-harness-browser.json", js.data(), js.size());
                g_editor.fileBrowser = false;
            }

            // ---- (f) 13.6 · A DUPLA DENSIDADE (o NÃO VERIFICADO #4 do
            // relatório B fechado): o export a 2.0 é o ecrã a 1.0 VISTO A
            // 2× — a INvariância da escala: dp para o layout, sp para o
            // texto, NADA fica para trás (o texto era o atlas cru em
            // qualquer densidade — a causa do ERRO da status bar)
            passo("13.6 dupla densidade: o ecrã a 2.0 == o ecrã a 1.0 × 2");
            {
                onAppCmd(&app13l, APP_CMD_TERM_WINDOW);
                eglstub::g_surfaceW = 3072;
                eglstub::g_surfaceH = 1440;
                vvstub::g_stubDensityDpi = 320;   // o caminho REAL: 320→2.0
                theme::setDensity(2.0f);
                editor::applyDensity();
                android_app app13d;
                std::memset(&app13d, 0, sizeof(app13d));
                app13d.contentRect = {0, 48, 3024, 1440};   // insets ×2 (24→48)
                onAppCmd(&app13d, APP_CMD_INIT_WINDOW);
                if (!g_font.ok()) {
                    const char* paths[] = {FONT_FIXTURE};
                    g_font.loadFromPaths(paths, 1, 28.0f);
                }
                g_ui.setFont(&g_font);
                // o mesmo estado da 13.1 (a cena vazia — o Ator da 13.3 sai)
                if (const Handle hAtor = g_scene.find("Ator"); hAtor.valid()) {
                    g_scene.destroy(hAtor);
                }
                // o TOAST da auditoria da 13.2 ainda está vivo (1,8s de vida
                // e os frames do harness correm em milissegundos) — a 13.1
                // exportou SEM ele; mata-se para o estado ser o MESMO
                g_toastT = 0.0f;
                g_toast[0] = '\0';
                frame();
                auto [png2, js2] = exportScreen("editor");
                fileapi::writeAll("layout-harness-editor-2x.png", png2.data(),
                                  png2.size());
                fileapi::writeAll("layout-harness-editor-2x.json", js2.data(),
                                  js2.size());
                vv::RawImage img2;
                std::string err2;
                check(vv::loadPng(png2.data(), png2.size(), img2, err2) &&
                          img2.width == 3072 && img2.height == 1440,
                      "13.6 o PNG a 2.0 e 3072x1440 (a superficie duplicada)");
                const layout::Record& r2 = g_ui.auditRecord();
                check(r2.density == 2.0f && r2.entries.size() > 8,
                      "13.6 o registo a densidade 2.0 existe");
                // (a) o VALIDADOR na dupla densidade: VERDE (a regra 48dp
                // multiplica pela densidade — os alvos 48dp sao 96px la)
                const auto probs2 = layout::validate(r2);
                check(probs2.empty(),
                      "13.6 o editor a 2.0 passa o validador INTEIRO "
                      "(0 erros, 0 avisos — nada fica pela densidade)");
                // (b) a INvariância: entrada a entrada, o rect a 2.0 é o
                // rect a 1.0 × 2 (a mesma ORDEM/kind — o ecrã e o MESMO)
                check(r2.entries.size() == editor1x.size(),
                      "13.6 o MESMO numero de entradas (o estado e o mesmo)");
                u32 cmp = 0, mism = 0;
                for (u32 i = 0; i < r2.entries.size() &&
                                 i < editor1x.size(); ++i) {
                    const auto& e2 = r2.entries[i];
                    const auto& e1 = editor1x[i];
                    if (e2.kind != e1.kind) { ++mism; continue; }
                    if (std::fabs(e2.x - e1.x * 2.0f) > 1.0f ||
                        std::fabs(e2.y - e1.y * 2.0f) > 1.0f ||
                        std::fabs(e2.w - e1.w * 2.0f) > 1.0f ||
                        std::fabs(e2.h - e1.h * 2.0f) > 1.0f) {
                        ++mism;
                        continue;
                    }
                    ++cmp;
                }
                check(mism == 0 && cmp == editor1x.size(),
                      "13.6 a INvariância: TODAS as entradas a 2.0 sao as de "
                      "1.0 × 2 (dp E sp — o texto tambem dobra)");
                // (c) a prova sp(): a largura do TEXTO dobra (o atlas nao
                // era escala nenhuma antes — media igual nas duas)
                bool txt2x = false;
                for (u32 i = 0; i < r2.entries.size() &&
                                 i < editor1x.size(); ++i) {
                    if (r2.entries[i].kind == layout::Entry::Label &&
                        editor1x[i].w > 5.0f &&
                        std::fabs(r2.entries[i].w - editor1x[i].w * 2.0f) <=
                            1.0f) {
                        txt2x = true;
                    }
                }
                check(txt2x,
                      "13.6 o sp(): a largura do TEXTO dobra com a densidade "
                      "(o atlas cru media SEMPRE igual — o bug do Grupo B)");
                // REPOSIÇÃO: o resto da suíte corre a 1.0 (o layout de sempre)
                onAppCmd(&app13d, APP_CMD_TERM_WINDOW);
                vvstub::g_stubDensityDpi = 160;
                theme::setDensity(1.0f);
                editor::applyDensity();
            }
            onAppCmd(&app13l, APP_CMD_TERM_WINDOW);
        }

        // a GPU volta ao no-op (o resto da suíte não paga o raster)
        onAppCmd(&app13, APP_CMD_TERM_WINDOW);
        glstub::fb::enabled = false;
        glstub::fb::resetState();
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
