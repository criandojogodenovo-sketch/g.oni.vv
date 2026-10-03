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

// ---- O CAMINHO REAL DO DEVICE (namespace anónimo = mesmo TU) ---------------
#include "platform/main.cpp"

// ponte Java (o papel do "stub Java" — como o test_wiring087/test_handshake)
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);

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
    g_editor = editor::EditorState{};
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
    std::printf("== C33 VIRTUAL — dispositivo headless em CI (0.9.0) ==\n");
    std::printf("   reproduz: /tmp read-only (errno=30), cache dir da app,\n");
    std::printf("   content:// SAF, lifecycle EGL TERM/INIT, 1536x720 + insets,\n");
    std::printf("   ASTC ativo, taps replayaveis pelo frame() real\n");
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
        check(logHas("lifecycle: selecao re-validada"),
              "log 'lifecycle: selecao re-validada pos-INIT WINDOW'");

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

    // ---- sumário -----------------------------------------------------------
    std::printf("\n== C33 VIRTUAL: %d check(s), %d falha(s) ==\n", g_checks, g_failed);
    if (g_failed == 0) {
        std::printf("HARNESS VERDE — o dispositivo virtual confirma os 5 fixes\n");
    } else {
        std::printf("HARNESS VERMELHO — release BLOQUEADA (ver [FAIL] acima)\n");
    }
    fileapi::testing::clearReadonlyPrefix();
    rmrf(kCacheDir);
    return g_failed == 0 ? 0 : 1;
}
