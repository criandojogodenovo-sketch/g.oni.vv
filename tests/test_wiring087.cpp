// tests/test_wiring087.cpp — 0.8.7: HOTFIX CIRÚRGICO (IMPORT + TROCA DE MESH).
//
// O QUE ESTE TU TEM DE DIFERENTE: #include "platform/main.cpp". O caminho
// REAL do device — primMesh (cache de primitivas), attemptImport,
// browserImportFile, rebindPrimMeshes, applyImportedAssetToSelectedTic, o
// BOOT do INIT_WINDOW e o frame() — vivia SÓ no main.cpp (device-only,
// FORA da suíte: os testes 0.8.5 usavam resolvers PRÓPRIOS e stubs que
// nunca viam o cache). Era exatamente aí que moravam os bugs do C33:
//
//   1. "o botão Import não abre nada" — attemptImport() chamava
//      browserOpen() (g_browser.open=true) mas NUNCA setava
//      g_editor.fileBrowser — o gate do overlay no frame() exige as DUAS;
//      o navegador "abria" invisível. Este teste seria VERMELHO no código
//      de antes (o fix 0.8.7 seta as duas juntas).
//
//   2. "a troca de mesh trava intermitentemente ou dá erro" — o cache de
//      primitivas era SEM LIMITE (slider = assinatura nova = mesh GL novo
//      para sempre → exaustão de GPU → falhas INTERMITENTES de upload =
//      "falha ao gerar primitiva" + driver engasgado) e o rebind tentava
//      RE-GERAR+re-uplodar A CADA FRAME quando um upload falhava (o
//      retry-storm = freeze). O fix 0.8.7: CAP + EVICÇÃO de não-usados +
//      NEGATIVE-cache (resposta estável por assinatura). O stress test
//      troca 600× em ordens variadas COM GUARDA DE TEMPO — hang = falha.
//
//   3. "susppeita de que as primitivas nem existem no build" — a auditoria
//      confirma no CMake da app (render/Primitives.cpp está lá desde a
//      0.8.0) e o CI ganhou o gate de símbolos (makePrimMesh/primDefaults/
//      primName/primClamp/applyAssetPick no .dynsym do APK); AQUI o teste
//      apanha a outra ponta: cada primitiva gera geometria NÃO-VAZIA pelo
//      caminho REAL e o engine.log ganha a linha "mesh: prim <tipo>
//      verts=N idx=M" (a prova que o log viewer do C33 vai mostrar).
//
//   4. GESTO ÓRFÃO: um widget que desaparece a meio do gesto deixava o
//      active_ do UiContext preso PARA SEMPRE — a UI inteira morria ("a
//      engine trava": render corre, nada responde). O fix mata o active_
//      órfão no FIM de cada frame sem dedo. O teste simula o cenário e
//      afera que um botão NOVO volta a capturar (impossível antes).
//
// Estilo da casa: stub GLES3/EGL/JNI + FakeStorage; cada caso é o FLUXO
// completo, não funções isoladas. CLÁUSULA CALMA: zero features.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats + failNextGenObjects)
#include <EGL/egl.h>    // stub (0.8.7: init feliz + 1280×720)
#include <dirent.h>     // rmrf do diretório de logs do teste
#include <sys/types.h>
#include <unistd.h>   // 0.8.12: getpid do cache dir de teste
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>   // FAKE controlável (tests/stub é o primeiro include dir)

#include "FakeStorage.h"

// ---- O CAMINHO REAL DO DEVICE (namespace anónimo = mesmo TU) ---------------
#include "platform/main.cpp"

// ponte Java (papel do "stub Java" — como o test_handshake)
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);
// 0.9.1 — os natives do IME (definidos no StorageBridge.cpp; os testes
// chamam-nos DIRETO — o mesmo caminho que o InputConnection do device)
extern "C" void Java_vv_goni_VvActivity_nativeOnImeText(
        JNIEnv*, jclass, jstring text);
extern "C" void Java_vv_goni_VvActivity_nativeOnImeKey(
        JNIEnv*, jclass, jint keyCode, jint action);

using namespace vv;
using ::test::nearEqF;

namespace {

const char* kTestLogs = "test-wiring087-logs";

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

std::vector<std::string> logLines(int maxLines = 800) {
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

jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xB001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xB002));

void drainQueue() {
    while (vv::storage::pollResult()) {
    }
}

// registo simulado (o papel do VvActivity.onCreate) + permissão concedida
void javaRegistersGranted() {
    g_jni.reset();
    drainQueue();
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity,
        g_jni.newString("test-wiring087"));
    g_jni.manager_result = true;   // isExternalStorageManager() == true
}

// 0.8.12 — o CACHE DIR da app pela ponte JNI fake (o papel do
// VvActivity.cacheDirPath → getCacheDir): o STAGING da reconversão NÃO
// usa /tmp (read-only no Android — a causa da migração morta no C33) e o
// FakeStorage tem raiz /fake (não escrevível): o fallback em cascata cai
// no cache dir, EXATAMENTE como o device no caminho SAF.
void enableDeviceCacheDir() {
    javaRegistersGranted();
    char cdir[64];
    std::snprintf(cdir, sizeof(cdir), "test-wiring087-cache-%d",
                  static_cast<int>(::getpid()));
    fileapi::makeDirs(cdir);
    g_jni.cache_dir = cdir;
}

void rmrfCacheDir() {
    char cdir[64];
    std::snprintf(cdir, sizeof(cdir), "test-wiring087-cache-%d",
                  static_cast<int>(::getpid()));
    rmrf(cdir);
}

// reset do estado PARTILHADO do main.cpp entre casos (o namespace anónimo
// é DESTE TU — os globais do device são acessíveis diretamente)
void resetEngineForTest() {
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
    g_catalog.audio.clear();   // 0.8.11
    g_prevAssetMenu = 0;
    g_projectReady = false;
    g_storage.reset();
    g_gpu.releaseAll();
    g_resources.setStorage(nullptr);
    g_toast[0] = '\0';
    g_toastT = 0.0f;
    g_input.resetAll();
    g_timeline = timeline::State{};
    g_playSnap = PlaySnapshot{};
    // 0.8.11 — o estado de ÁUDIO também reseta (cache/catálogo/preview/
    // gravação pendente/misturador; o backend é gerido pelos casos que o
    // usam — o boot é por INIT_WINDOW)
    g_audioClips.clear();
    g_audioCatalog.clear();
    g_audioWs = editor::AudioWorkspaceState{};
    g_audioPreviewVoice = -1;
    g_audioPreviewClip.clear();
    g_audioEngine.reset();
    g_audioEngine.master = 1.0f;
    g_audioMaster = 1.0f;
    g_keepSource = true;
    if (g_audioRec.on.load()) {   // gravação abandonada a meio? mata o worker
        g_audioRec.on.store(false);
        if (g_audioRec.th.joinable()) {
            g_audioRec.th.join();
        }
    }
    glstub::reset();
}

// renderer/cubo/cena mínimos p/ os resolvers do device (uma vez)
bool g_engineReady = false;
void ensureEngineReady() {
    if (g_engineReady) {
        return;
    }
    if (!g_renderer.init()) {
        return;
    }
    const CubeMeshData cube = makeCube(1.0f);
    g_cubeMesh.create(cube.vertices.data(),
                      static_cast<u32>(cube.vertices.size()),
                      cube.indices.data(),
                      static_cast<u32>(cube.indices.size()));
    g_engineReady = true;
}

// TIC "Mesh" (Transform+MeshRenderer) com primitiva — o alvo das trocas
Handle addMeshTic(const char* name) {
    ensureEngineReady();
    const Handle h = createTicFromPreset(g_scene, PresetKind::Mesh, nullptr,
                                         nullptr);
    if (!h.valid()) {
        return h;
    }
    Tic* t = g_scene.get(h);
    t->name = name;
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    if (mr) {
        // 0.8.10: o caminho REAL — pedido PENDENTE + flush (ponto seguro)
        mr->primOn = true;
        mr->prim = primDefaults(PrimKind::Sphere);
        mr->primPending = true;
        primFlushPending();
        if (mr->mesh) {
            mr->material = g_renderer.litMaterial();
        }
    }
    g_editor.selected = h;
    return h;
}

// escreve um .obj mínimo NO DISCO (o browserImportFile lê por File API)
std::string writeTempObj(const char* path) {
    const char* obj = "o tri\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        return "";
    }
    std::fwrite(obj, 1, std::strlen(obj), f);
    std::fclose(f);
    return path;
}

// guarda de tempo do stress test (freeze = falha, não timeout do CI)
double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0)
        .count();
}

// 0.8.10 — espera o IMPORT JOB terminar e FINALIZA como o frame() faria
// (join + catálogo + diálogo). Guarda de tempo: hang do worker = falha.
void pumpImportJob(double maxMs = 15000.0) {
    const auto t0 = std::chrono::steady_clock::now();
    while (!g_importJob.done.load()) {
        if (msSince(t0) > maxMs) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT(g_importJob.done.load());   // hang = falha explícita
    if (g_importJob.active.load() && g_importJob.done.load()) {
        importJobFinish();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 1. IMPORT — o botão ABRE o navegador (as DUAS flags; seria VERMELHO antes)
// ---------------------------------------------------------------------------
TEST(wiring087_import_toque_abre_o_navegador) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    ensureEngineReady();
    g_projectReady = true;   // projeto aberto

    // o toque no botão Import (Menu → Importar…): o caminho concedido
    attemptImport();

    // O FIX: o overlay do navegador desenha com `g_editor.fileBrowser &&
    // g_browser.open` — SEM a flag do EditorState o browser "abre" invisível
    // (o bug do C33 "o botão Import não abre nada")
    EXPECT(g_browser.open);
    EXPECT(g_editor.fileBrowser);
    // o navegador abriu no Download (raiz navegável, all-files)
    EXPECT(g_browser.cwd.find("Download") != std::string::npos);
    // LOGGING EMBUTIDO: a linha que o log viewer do C33 mostra
    EXPECT(logHas("import: navegador ABERTO"));

    // o overlay é MODAL: com ele aberto o anyOverlayOpen fecha o chrome
    EXPECT(editor::anyOverlayOpen(g_editor));
}

TEST(wiring087_import_sem_projeto_toast_honesto) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    g_projectReady = false;   // SEM projeto: o botão diz porquê

    attemptImport();
    EXPECT(!g_browser.open);
    EXPECT(!g_editor.fileBrowser);
    EXPECT(std::strcmp(g_toast, "sem projeto — import indisponível") == 0);
    EXPECT(logHas("import: SEM projeto"));
}

TEST(wiring087_import_pos_concessao_retoma_pelo_navegador) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    g_projectReady = true;

    // o fluxo DIALOGO: toque no Import SEM permissão → pedido pendente
    g_jni.manager_result = false;   // ainda não concedida
    attemptImport();                 // abre o diálogo (ação Import pendente)
    EXPECT(g_editor.storageDialog);
    g_jni.manager_result = true;    // o dono concedeu nas definições

    // o retorno concedido retoma a ação: o MESMO navegador
    resumePendingAfterReturn(true);
    EXPECT(g_browser.open);
    EXPECT(g_editor.fileBrowser);
    EXPECT(logHas("import: navegador ABERTO pos-concessao"));
}

// ---------------------------------------------------------------------------
// 2. IMPORT — o navegador importa .obj e APLICA ao TIC ("Sim" do diálogo)
// ---------------------------------------------------------------------------
TEST(wiring087_browser_importa_obj_e_aplica_ao_tic) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    ensureEngineReady();

    // projeto com storage fake + GPU ligada ao ResourceManager REAL
    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    rawSt->makeDirs("meshes");   // o FsStorage real cria a pasta ao gravar
    g_storage = std::move(st);
    g_resources.setStorage(rawSt);
    g_gpu.init(&g_resources);
    g_projectReady = true;
    refreshCatalog();

    const Handle h = addMeshTic("Alvo");
    ASSERT(h.valid());
    MeshRenderer* mr = g_scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    const u32 idxBefore = mr->mesh ? mr->mesh->indexCount() : 0;

    // ficheiro REAL no disco + entrada do navegador (o browser lista o FS)
    const std::string tmp = writeTempObj("goni_w087_import.obj");
    ASSERT(!tmp.empty());
    fileapi::DirEntry e;
    e.name = "goni_w087_import.obj";
    e.path = tmp;
    e.isDir = false;
    e.kind = 'm';

    // 0.8.10: o toque lança o JOB (thread própria); o frame desenharia o
    // overlay — aqui esperamos e finalizamos pelo MESMO caminho do frame
    browserImportFile(e);
    EXPECT(g_importJob.active.load());      // o overlay estaria visível
    pumpImportJob();                        // frame() faria o finalize

    // passo-a-passo no engine.log (o logging embutido da 0.8.10)
    EXPECT(logHas("import: job iniciado"));
    EXPECT(logHas("import: fonte copiada"));
    EXPECT(logHas("asset: convert"));
    // fonte copiada em chunks + CONVERTIDO no projeto (assets/)
    EXPECT(rawSt->exists("source/goni_w087_import.obj"));
    EXPECT(rawSt->exists("assets/goni_w087_import.gmesh"));
    refreshCatalog();
    ASSERT(!g_catalog.meshes.empty());
    EXPECT(g_catalog.meshes[0] == "assets/goni_w087_import.gmesh");
    // a pergunta "aplicar ao TIC?" abriu (o finalize abre as DUAS flags)
    EXPECT(g_applyAsk.open);
    EXPECT(g_editor.applyAsk);
    EXPECT(g_applyAsk.rel == "assets/goni_w087_import.gmesh");

    // o "Sim": o MESMO código do frame() (extraído — afervel aqui)
    applyImportedAssetToSelectedTic();
    EXPECT(!g_applyAsk.open);
    EXPECT(!editor::anyOverlayOpen(g_editor));   // NENHUM modal preso
    EXPECT(logHas("import: aplicando"));
    // o mesh CONVERTIDO está no MeshRenderer (GpuAssets: readGMesh + upload)
    // e DIFERE da primitiva anterior
    ASSERT(mr->mesh != nullptr);
    EXPECT(mr->meshPath == "assets/goni_w087_import.gmesh");
    EXPECT(mr->mesh->indexCount() > 0);
    EXPECT(mr->mesh->indexCount() != idxBefore || !mr->primOn);
    EXPECT(!mr->primOn);   // asset limpa o prim (uma fonte de cada vez)
    EXPECT(logHas("import: aplicado verts="));
    // o loader próprio loga os tempos (a linha exigida pelo prompt)
    EXPECT(logHas("asset: load assets/goni_w087_import.gmesh verts="));

    // regressão: o TIC continua desenhável (draw no stub GL)
    const u32 draws = static_cast<u32>(glstub::stats.drawElementsCalls);
    g_renderer.beginFrame();
    const Mat4 vp = Mat4::identity();
    (void)g_renderer.drawMesh(*mr->mesh, Mat4::identity(), vp);
    EXPECT(static_cast<u32>(glstub::stats.drawElementsCalls) == draws + 1);

    std::remove(tmp.c_str());
}

TEST(wiring087_browser_formato_nao_suportado_erro_claro) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    ensureEngineReady();
    g_storage = std::make_unique<FakeStorage>();
    g_projectReady = true;
    addMeshTic("Alvo");

    fileapi::DirEntry e;
    e.name = "coisa.fbx";
    e.path = "/fake/coisa.fbx";
    e.isDir = false;
    e.kind = 0;   // fora de obj/gltf/glb/png
    browserImportFile(e);

    EXPECT(std::strcmp(g_toast, "formato não suportado ainda: .fbx") == 0);
    EXPECT(logHas("import: '/fake/coisa.fbx' — formato .fbx não suportado"));
    EXPECT(!g_applyAsk.open);   // nada importado, nada perguntado
}

// ---------------------------------------------------------------------------
// 3. AUDITORIA DE EXISTÊNCIA — cada primitiva gera geometria pelo caminho REAL
// ---------------------------------------------------------------------------
TEST(wiring087_troca_prim_por_tipo_geometria_nao_vazia_e_log_da_prova) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Troca");
    ASSERT(h.valid());
    MeshRenderer* mr = g_scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);

    // 0.8.10: as DUAS formas PELO DISPATCH REAL (pick = pedido pendente →
    // primFlushPending = o ponto seguro do frame) — passo a passo no log
    for (int k = 0; k < 2; ++k) {
        const PrimKind kind = static_cast<PrimKind>(k);
        const editor::AssetPickOutcome out = editor::applyAssetPick(
            g_scene, h, 4, k + 2, g_catalog, makeAssetResolvers());
        EXPECT(out.applied);
        EXPECT(mr->primOn);
        EXPECT(mr->prim.kind == kind);
        EXPECT(mr->primPending);          // o pedido está ARMADO
        primFlushPending();               // ← o ponto seguro do frame
        EXPECT(!mr->primPending);         // consumido
        ASSERT(mr->mesh != nullptr);
        EXPECT(mr->mesh->vertexCount() > 0);   // geometria EXISTE
        EXPECT(mr->mesh->indexCount() >= 3);
        // a PROVA no log: o passo-a-passo que o log viewer do C33 mostra
        // (de/para trazem os params — o needle afera o FORMATO exigido)
        EXPECT(logHas("mesh: troca "));
        EXPECT(logHas(" passo=gerador ok verts="));
        EXPECT(logHas("passo=validação ok"));
        EXPECT(logHas("passo=upload ok"));
        EXPECT(logHas("passo=bind ok"));
        EXPECT(logHas("passo=validação ok"));
        EXPECT(logHas("passo=upload ok"));
        EXPECT(logHas("passo=bind ok"));
    }

    // o seletor LISTA a partir dos dados do gerador (não uma lista à parte):
    // 2 rótulos = 2 PrimKind — o catálogo do seletor é o próprio gerador
    u32 labels = 0;
    for (int k = 0; k < 2; ++k) {
        if (primLabel(static_cast<PrimKind>(k)) != nullptr &&
            primLabel(static_cast<PrimKind>(k))[0] != '\0') {
            ++labels;
        }
    }
    EXPECT(labels == 2u);
}

// ---------------------------------------------------------------------------
// 4. STRESS ANTI-FREEZE — 600 trocas variadas COM GUARDA DE TEMPO
// ---------------------------------------------------------------------------
TEST(wiring087_troca_stress_antifreeze_com_guarda_de_tempo) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Stress");
    ASSERT(h.valid());
    MeshRenderer* mr = g_scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);

    // 0.8.10 SEM CACHE: cada troca é gerar+valida+upload+bind (nunca "hit")
    // com deferred free do antigo. Sequências do critério: ciclos de ordens,
    // A→B→A, primitiva↔cube, primitiva↔importado, params de slider.
    FakeStorage st;
    st.makeDirs("meshes");
    st.writeText("meshes/imp.obj", "o tri\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    g_storage.reset(new FakeStorage(st));
    g_resources.setStorage(g_storage.get());
    g_gpu.init(&g_resources);
    g_projectReady = true;
    refreshCatalog();

    const auto t0 = std::chrono::steady_clock::now();
    const int N = 600;
    u32 ok = 0, err = 0;
    for (int i = 0; i < N; ++i) {
        const auto ti = std::chrono::steady_clock::now();
        // (1) a COVA do frame anterior abre ANTES do flush (a ordem do main)
        primGraveDig();
        if (i % 6 == 5) {
            // params de SLIDER: assinatura NOVA — o caminho REAL do Inspector
            PrimParams p = primDefaults(PrimKind::Sphere);
            p.radius = 0.1f + 0.01f * static_cast<f32>(i % 40);
            mr->primOn = true;
            mr->prim = p;
            mr->primPending = true;
            primFlushPending();   // ← o ponto seguro
            if (mr->mesh != nullptr) { ++ok; } else { ++err; }
        } else {
            int pick;
            int menuKind = 4;
            switch (i % 6) {
                case 0:  // ciclo sequencial pelas 2 formas
                    pick = (i / 6) % 2 + 2;
                    break;
                case 1:  // A→B→A
                    pick = 2 + (i % 2);
                    break;
                case 2:  // ordem REVERSA
                    pick = 3 - (i % 2);
                    break;
                case 3:  // primitiva ↔ cube (menuKind 1, pick 2 — 0.8.12:
                         // none ocupa o 1º lugar, cube é o 2º)
                    menuKind = 1;
                    pick = 2;
                    break;
                default:  // primitiva ↔ IMPORTADO (ficheiro do catálogo em 3)
                    menuKind = 1;
                    pick = 3;
                    break;
            }
            const editor::AssetPickOutcome out = editor::applyAssetPick(
                g_scene, h, menuKind, pick, g_catalog, makeAssetResolvers());
            EXPECT(out.applied);
            primFlushPending();   // o rebind por frame do main (ponto seguro)
            if (mr->mesh != nullptr) { ++ok; } else { ++err; }
        }
        // GUARDA DE TEMPO: freeze = iteração que não volta em 250 ms
        const double ms = msSince(ti);
        if (ms > 250.0) {
            std::printf("  iteracao %d demorou %.1f ms (freeze?)\n", i, ms);
        }
        EXPECT(ms < 250.0);
        ASSERT(mr->mesh != nullptr);
        // a posse NUNCA cresce: 1 mesh vivo por TIC com prim (sem cache!)
        EXPECT(g_primOwners.size() <= 2u);
    }
    const double totalMs = msSince(t0);
    const u32 okFinal = g_primSwapOk, errFinal = g_primSwapErr;
    std::printf("  stress: %d trocas em %.0f ms (media %.2f ms) — trocas ok "
                "%u, ERRO %u, vivos=%zu cova=%zu\n", N, totalMs,
                totalMs / static_cast<double>(N), okFinal, errFinal,
                g_primOwners.size(), g_primGrave.size());
    EXPECT(totalMs < 30000.0);   // guarda total (hang infinito = falha)
    EXPECT(ok == static_cast<u32>(N));   // 100% de sucesso no stub feliz
    EXPECT(err == 0u);
    EXPECT(g_primSwapErr == 0u);

    // estado final SAUDÁVEL: mesh válido, desenha, posse contida
    EXPECT(mr->mesh->ok() || mr->mesh->indexCount() > 0);
    EXPECT(g_primOwners.size() <= 2u);   // 1-2 TICs com prim (cap natural)
    g_renderer.beginFrame();
    const Mat4 vp = Mat4::identity();
    (void)g_renderer.drawMesh(*mr->mesh, Mat4::identity(), vp);
    EXPECT(glstub::stats.drawElementsCalls > 0);
}

// ---------------------------------------------------------------------------
// 5. DEFERRED FREE — o mesh antigo NÃO é libertado no MESMO frame da troca;
// morre no INÍCIO do frame seguinte (a cova) — os comandos em voo ficam
// sempre com buffers válidos. (substitui o teste de cap/evicção do cache —
// o cache MORREU na 0.8.10; a posse é 1 mesh por TIC, cap natural)
// ---------------------------------------------------------------------------
TEST(wiring087_deferred_free_antigo_nao_morre_no_mesmo_frame) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Cova");
    ASSERT(h.valid());
    MeshRenderer* mr = g_scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    ASSERT(mr->mesh != nullptr);            // esfera (do addMeshTic)
    Mesh* const antigo = mr->mesh;
    const u32 idxAntigo = antigo->indexCount();

    // TROCA esfera→box no "frame N": pick (pendente) + flush
    const int delVaoAntes = glstub::stats.deleteVertexArrays;
    const editor::AssetPickOutcome out = editor::applyAssetPick(
        g_scene, h, 4, 3, g_catalog, makeAssetResolvers());   // 3 = box
    EXPECT(out.applied);
    primFlushPending();
    ASSERT(mr->mesh != nullptr);
    EXPECT(mr->mesh != antigo);             // o NOVO está ligado
    EXPECT(mr->mesh->indexCount() == 36);   // box 24/36
    EXPECT(mr->prim.kind == PrimKind::Box);
    // o ANTIGO ainda NÃO foi apagado (deferred free!): zero glDelete* novos
    EXPECT(glstub::stats.deleteVertexArrays == delVaoAntes);
    // ...e o antigo AINDA DESENHA neste frame (buffer vivo na cova)
    g_renderer.beginFrame();
    const u32 draws = static_cast<u32>(glstub::stats.drawElementsCalls);
    (void)g_renderer.drawMesh(*antigo, Mat4::identity(), Mat4::identity());
    EXPECT(static_cast<u32>(glstub::stats.drawElementsCalls) == draws + 1);
    EXPECT(antigo->indexCount() == idxAntigo);

    // "frame N+1" (início): a COVA abre — SÓ AGORA o antigo morre
    EXPECT(!g_primGrave.empty());
    primGraveDig();
    EXPECT(g_primGrave.empty());
    EXPECT(glstub::stats.deleteVertexArrays > delVaoAntes);   // glDelete* correu
    // o NOVO continua perfeito depois da cova
    EXPECT(mr->mesh->indexCount() == 36);
    g_renderer.beginFrame();
    (void)g_renderer.drawMesh(*mr->mesh, Mat4::identity(), Mat4::identity());
    EXPECT(glstub::stats.drawElementsCalls > 0u);

    // posse contida: 1 mesh vivo (o box), zero na cova
    EXPECT(g_primOwners.size() == 1u);
}

// ---------------------------------------------------------------------------
// 6. BACKOFF pós-falha (primNeg) — o upload GL falhou: o mesh ANTERIOR
// fica (render continua), ZERO retry por frame; pedido NOVO ou contexto
// NOVO voltam a tentar. (o anti-storm da 0.8.7, agora SEM cache)
// ---------------------------------------------------------------------------
TEST(wiring087_upload_falhou_backoff_sem_retry_storm) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Falha");
    ASSERT(h.valid());
    MeshRenderer* mr = g_scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    Mesh* const esfera = mr->mesh;          // o mesh ANTIGO (fica!)
    ASSERT(esfera != nullptr);

    // o "device doente": glGen* devolve 0 → o upload da troca falha
    glstub::failNextGenObjects = true;
    const editor::AssetPickOutcome out = editor::applyAssetPick(
        g_scene, h, 4, 3, g_catalog, makeAssetResolvers());   // box
    EXPECT(out.applied);                    // o PEDIDO armou (é pendente)
    primFlushPending();                     // ← a TROCA falha AQUI
    glstub::failNextGenObjects = false;
    EXPECT(mr->prim.kind == PrimKind::Box); // o pedido ficou registado
    EXPECT(mr->primNeg);                    // backoff ARMADO
    // FAIL-SAFE: o mesh ANTERIOR fica intacto e a renderizar
    EXPECT(mr->mesh == esfera);
    EXPECT(mr->mesh->indexCount() > 0);
    // ERRO legível com PASSO e RAZÃO + toast no ecrã
    EXPECT(logHas(" passo=upload ERRO("));   // de/para com params no meio
    EXPECT(std::strstr(g_toast, "ERRO(upload)") != nullptr);

    // o RETRY-STORM morreu: 60 frames de flush NÃO regeneram nem uploda
    const int gen = glstub::stats.genVertexArrays;
    const int bufdata = glstub::stats.bufferData;
    for (int f = 0; f < 60; ++f) {
        primGraveDig();
        primFlushPending();
        EXPECT(mr->mesh == esfera);   // estável — sem mil tentativas
    }
    EXPECT(glstub::stats.genVertexArrays == gen);
    EXPECT(glstub::stats.bufferData == bufdata);

    // pedido NOVO (assinala de novo o MESMO tipo — primNeg limpa): funciona
    const editor::AssetPickOutcome out2 = editor::applyAssetPick(
        g_scene, h, 4, 3, g_catalog, makeAssetResolvers());
    EXPECT(out2.applied);
    primFlushPending();
    ASSERT(mr->mesh != nullptr);
    EXPECT(mr->mesh->indexCount() == 36);   // box subiu
    EXPECT(mr->mesh != esfera);

    // o esfera antigo: estava na posse (a falha não o retirou) — a TROCA
    // bem-sucedida é que o pôs na cova; a cova abre sem pressa
    primGraveDig();
    EXPECT(g_primOwners.size() == 1u);      // só o box vivo
}

// ---------------------------------------------------------------------------
// 7. LIFECYCLE — destroy/recreate determinístico (TERM_WINDOW ↔ INIT_WINDOW)
// ---------------------------------------------------------------------------
TEST(wiring087_lifecycle_destroy_recreate_deterministico) {
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Vida");
    ASSERT(h.valid());
    MeshRenderer* mr = g_scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);

    const PrimKind seq[2] = {PrimKind::Sphere, PrimKind::Box};
    for (int cycle = 0; cycle < 5; ++cycle) {
        // troca pelo caminho real (pick + flush)
        const PrimKind kind = seq[cycle % 2];
        const editor::AssetPickOutcome out = editor::applyAssetPick(
            g_scene, h, 4, static_cast<int>(kind) + 2, g_catalog,
            makeAssetResolvers());
        EXPECT(out.applied);
        primFlushPending();
        ASSERT(mr->mesh != nullptr);
        const u32 idx = mr->mesh->indexCount();
        const u32 vao = 0;   // (ids do stub não distinguem — o ESTADO afera)

        // TERM_WINDOW: buffers GL mortos + renderers desligados
        detachRenderersFromGpu();
        primGraveDig();
        primMeshesDestroyAll();
        EXPECT(mr->mesh == nullptr);
        EXPECT(g_primOwners.empty());
        EXPECT(g_primGrave.empty());
        EXPECT(!mr->primNeg);   // novo contexto, nova sorte

        // INIT_WINDOW: o flush do próximo frame RE-GERA (primPending ficou)
        primFlushPending();
        ASSERT(mr->mesh != nullptr);
        EXPECT(mr->mesh->indexCount() == idx);   // MESMA geometria
        EXPECT(mr->prim.kind == kind);           // o .goni/params é a verdade

        // desenha no contexto novo
        g_renderer.beginFrame();
        const Mat4 vp = Mat4::identity();
        const u32 draws = static_cast<u32>(glstub::stats.drawElementsCalls);
        (void)g_renderer.drawMesh(*mr->mesh, Mat4::identity(), vp);
        EXPECT(static_cast<u32>(glstub::stats.drawElementsCalls) == draws + 1);
        (void)vao;
    }
}

// ---------------------------------------------------------------------------
// 8. GESTO ÓRFÃO — widget desapareceu a meio do gesto: a UI NÃO morre
// ---------------------------------------------------------------------------
TEST(wiring087_gesto_orfao_nao_trava_a_ui) {
    resetEngineForTest();
    Renderer r;
    ASSERT(r.init());
    UiContext ui;
    ui.init();
    InputState in;

    const f32 sw = 1280.0f, sh = 720.0f;

    // frame 1: o dedo AGARRA o botão A (overlay aberto)
    ui.beginFrame(&r, &in, sw, sh);
    in.injectDown(0, 500.0f, 300.0f);   // dentro do botão A
    (void)ui.widgetHit(0x1001, 480.0f, 280.0f, 120.0f, 60.0f);
    ui.endFrame();
    in.clearEdges();

    // frame 2: o OVERLAY FECHOU (o botão A desapareceu) ANTES do release —
    // o active_ ficaria ÓRFÃO. Dedo levanta; ninguém o consome.
    ui.beginFrame(&r, &in, sw, sh);
    in.injectUp(0);
    ui.endFrame();   // ← o fix 0.8.7 mata o active_ órfão AQUI
    in.clearEdges();

    // frame 3: um botão NOVO (B) tem de conseguir capturar + fired —
    // com o active_ preso isto era IMPOSSÍVEL ("a engine trava")
    ui.beginFrame(&r, &in, sw, sh);
    in.injectDown(0, 700.0f, 300.0f);
    (void)ui.widgetHit(0x2002, 680.0f, 280.0f, 120.0f, 60.0f);
    in.injectUp(0);
    const bool clicked = ui.widgetHit(0x2002, 680.0f, 280.0f, 120.0f, 60.0f);
    EXPECT(clicked);   // ANTES do fix: false — a UI inteira estava morta
    ui.endFrame();
    in.clearEdges();

    // regressão do gesto normal: press → drag fora → release fora = SEM
    // clique (o active_ limpa no release de quem o desenhou, como sempre)
    ui.beginFrame(&r, &in, sw, sh);
    in.injectDown(0, 700.0f, 300.0f);
    (void)ui.widgetHit(0x3003, 680.0f, 280.0f, 120.0f, 60.0f);
    in.injectMove(0, 900.0f, 500.0f);   // sai do botão
    (void)ui.widgetHit(0x3003, 680.0f, 280.0f, 120.0f, 60.0f);
    in.injectUp(0);
    const bool clickedOutside = ui.widgetHit(0x3003, 680.0f, 280.0f, 120.0f, 60.0f);
    EXPECT(!clickedOutside);
    ui.endFrame();
    in.clearEdges();
    r.shutdown();
}

// ---------------------------------------------------------------------------
// 9. REGRESSÃO — a animação continua a funcionar com trocas a meio
// ---------------------------------------------------------------------------
TEST(wiring087_anim_intacta_sob_trocas_de_mesh) {
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Anim");
    ASSERT(h.valid());
    Tic* t = g_scene.get(h);
    Transform3D* tr = t->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    const Vec3 p0 = tr->pos;

    AnimationPlayer* pl = t->addComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    AnimTrack* track = pl->addTrack(AnimTarget::TicPos, "");
    ASSERT(track != nullptr);
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[0] = p0.x; k0.v[1] = p0.y; k0.v[2] = p0.z;
    track->keys.push_back(k0);
    AnimKey k1;
    k1.t = 2.0f;
    k1.v[0] = p0.x + 4.0f; k1.v[1] = p0.y; k1.v[2] = p0.z;
    track->keys.push_back(k1);
    track->sortKeys();

    // play com TROCAS de mesh a meio (o cenário do C33: trocar com a
    // timeline viva) — a pose é do Transform, o mesh é só a casca
    pl->mode = AnimationPlayer::Mode::Once;
    pl->time = 0.0f;
    pl->playing = true;
    int swaps = 0;
    for (u32 s = 0; s < 130; ++s) {   // 2.17 s > 2 s (folga do épsilon fp)
        pl->advance(1.0f / 60.0f);
        pl->apply(g_scene, h);
        if (s % 20 == 10) {   // troca a meio do clip, 6×
            const PrimKind kind = static_cast<PrimKind>(swaps % 2);
            const editor::AssetPickOutcome out = editor::applyAssetPick(
                g_scene, h, 4, static_cast<int>(kind) + 2, g_catalog,
                makeAssetResolvers());
            EXPECT(out.applied);
            primFlushPending();   // o ponto seguro do frame
            ++swaps;
        }
    }
    EXPECT(swaps == 6);
    EXPECT(!pl->playing);   // Once: parou no fim
    EXPECT(nearEqF(tr->pos.x, p0.x + 4.0f));
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    ASSERT(mr->mesh != nullptr);
    EXPECT(mr->mesh->indexCount() > 0);   // o mesh final é válido e desenha
}

// ---------------------------------------------------------------------------
// 10. BOOT + FRAME — o caminho do device inteiro no hospedeiro (stub EGL feliz)
// ---------------------------------------------------------------------------
TEST(wiring087_boot_e_frame_smoke_com_browser_aberto) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();

    // o arranque REAL: INIT_WINDOW (EGL feliz no stub → 1280×720, renderer,
    // cubo, grid, ready) — como o C33 faz
    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    EXPECT(g_ready);
    EXPECT(g_egl.width() == 1280);
    EXPECT(g_egl.height() == 720);

    // sem fonte de sistema no hospedeiro: carrega a fixture (o device tem
    // as fontes do Android — kSystemFontPaths)
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        EXPECT(g_font.loadFromPaths(paths, 1, 28.0f));
    }
    g_ui.setFont(&g_font);

    // um TIC de mesh + o IMPORT (navegador aberto pelo caminho concedido)
    g_projectReady = true;
    const Handle h = addMeshTic("Smoke");
    ASSERT(h.valid());
    attemptImport();
    EXPECT(g_browser.open);
    EXPECT(g_editor.fileBrowser);

    // FRAMES com o navegador aberto (modal) + com o seletor de primitivas
    // aberto (modal) + um frame limpo: todos completam dentro do orçamento
    const auto t0 = std::chrono::steady_clock::now();
    frame();                                   // browser aberto (desenha)
    EXPECT(msSince(t0) < 500.0);

    const auto t1 = std::chrono::steady_clock::now();
    g_editor.fileBrowser = false;
    g_browser.open = false;
    g_editor.assetMenu = 4;                    // seletor de primitivas (modal)
    frame();
    EXPECT(msSince(t1) < 500.0);

    const auto t2 = std::chrono::steady_clock::now();
    g_editor.assetMenu = 0;
    frame();                                   // editor limpo
    EXPECT(msSince(t2) < 500.0);

    // o pass UI submeteu (o overlay desenha de verdade no stub)
    EXPECT(glstub::stats.drawArraysCalls > 0);

    // TERM_WINDOW: desliga TUDO (contexto morto) — sem crash, estado pronto
    // para o próximo INIT
    onAppCmd(&app, APP_CMD_TERM_WINDOW);
    EXPECT(!g_ready);
    EXPECT(g_primOwners.empty());
    EXPECT(g_primGrave.empty());
}

// ===========================================================================
// 0.8.9 — CRASH-PROOF: casos do CAMINHO DO DEVICE (este TU inclui o
// platform/main.cpp — primMesh/applyImportedAssetToSelectedTic/globais).
// A parte PURA (placeAt/Json/grid/far/normalização/campos numéricos/
// geradores extremos) vive em test_wiring089.cpp.
// ===========================================================================

// ---------------------------------------------------------------------------
// 0.8.9-1. A SEQUÊNCIA EXATA DO DEVICE (esfera→box→cápsula, origem "-")
// termina SEM crash — incluindo o estado de origem "-" (TIC cubo sem prim,
// o que o C33 via após o restart) e a cápsula que "gerador/upload falhou"
// (era seleção perdida + upload; nunca mais dessceleciona).
// ---------------------------------------------------------------------------
TEST(wiring089_device_sequencia_do_dump_termina_sem_crash) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    ensureEngineReady();

    // TIC CUBO (sem prim — a origem "-" do dump do C33)
    const Handle h = createTicFromPreset(g_scene, PresetKind::Mesh, nullptr,
                                         nullptr);
    ASSERT(h.valid());
    Tic* t = g_scene.get(h);
    t->name = "Cubo";
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->primOn = false;
    mr->mesh = &g_cubeMesh;
    mr->material = g_renderer.litMaterial();
    g_editor.selected = h;

    // a sequência do log do C33 (0.8.10: cápsula não existe — o análogo é
    // "-" → esfera → box → esfera com o flush de cada frame)
    const PrimKind seq[3] = {PrimKind::Sphere, PrimKind::Box,
                             PrimKind::Sphere};
    for (const PrimKind kind : seq) {
        const editor::AssetPickOutcome out = editor::applyAssetPick(
            g_scene, g_editor.selected, 4,
            static_cast<int>(kind) + 2, g_catalog, makeAssetResolvers());
        EXPECT(out.applied);                        // TODAS aplicam
        EXPECT(g_editor.selected == h);             // seleção NUNCA muda
        primFlushPending();                         // o ponto seguro do frame
        ASSERT(mr->mesh != nullptr);
        EXPECT(mr->mesh->vertexCount() > 0);
        EXPECT(mr->mesh->indexCount() >= 3);
        primGraveDig();                             // início do frame seguinte
    }
    // o estado final é a ESFERA (o pedido do dono fica mesmo onde ele pôs)
    EXPECT(mr->primOn);
    EXPECT(mr->prim.kind == PrimKind::Sphere);
    EXPECT(logHas("mesh: troca"));                  // a prova no log
    EXPECT(logHas("passo=bind ok"));

    // PICK FORA DO RANGE (o antigo "9 = cápsula"): não aplica, não crasha
    const editor::AssetPickOutcome outInv = editor::applyAssetPick(
        g_scene, h, 4, 9, g_catalog, makeAssetResolvers());
    EXPECT(!outInv.applied);                        // 9 já não existe
    EXPECT(g_editor.selected == h);

    // E o caminho com SELEÇÃO PERDIDA (a origem "-" verdadeira do dump: TIC
    // sem MeshRenderer): falha GRACIOSA, sem crash, sem tocar na seleção
    const Handle plain = g_scene.create("semMesh");
    g_editor.selected = plain;
    const editor::AssetPickOutcome out2 = editor::applyAssetPick(
        g_scene, plain, 4, 3, g_catalog, makeAssetResolvers());
    EXPECT(!out2.applied);                          // sem alvo: não aplica
    EXPECT(g_editor.selected == plain);             // seleção intacta
    // o TIC de cubo CONTINUA esfera (o estado anterior é sagrado)
    EXPECT(g_scene.get(h)->getComponent<MeshRenderer>()->prim.kind ==
           PrimKind::Sphere);
}

// ---------------------------------------------------------------------------
// 0.8.9-2. FALHA DE GERADOR/UPLOAD: mantém o mesh ANTERIOR e a seleção.
// (o prompt: "falha simulada de gerador mantém mesh anterior E seleção")
// ---------------------------------------------------------------------------
TEST(wiring089_device_falha_gl_mantem_mesh_anterior_e_selecao) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Falha89");   // esfera no mesh
    ASSERT(h.valid());
    Tic* t = g_scene.get(h);
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    Mesh* const esferaMesh = mr->mesh;
    ASSERT(esferaMesh != nullptr);
    const u32 idxBefore = esferaMesh->indexCount();

    // o "device doente": glGen* devolve 0 → o UPLOAD da troca falha
    glstub::failNextGenObjects = true;
    const editor::AssetPickOutcome out = editor::applyAssetPick(
        g_scene, h, 4, 3, g_catalog, makeAssetResolvers());   // box
    primFlushPending();   // ← a falha acontece NO ponto seguro
    glstub::failNextGenObjects = false;
    // FAIL-SAFE: o mesh ANTERIOR fica intacto (nunca fica sem mesh)
    EXPECT(mr->mesh == esferaMesh);
    EXPECT(mr->mesh->indexCount() == idxBefore);
    EXPECT(mr->primNeg);                       // backoff armado
    // seleção NUNCA muda (o "desseleciona e continua cubo" era o crash)
    EXPECT(g_editor.selected == h);
    // a RAZÃO no log (erro legível, não um crash dump)
    EXPECT(logHas(" passo=upload ERRO("));   // de/para com params no meio
    // e o mesh antigo CONTINUA desenhando neste frame
    const u32 draws = static_cast<u32>(glstub::stats.drawElementsCalls);
    g_renderer.beginFrame();
    (void)g_renderer.drawMesh(*esferaMesh, Mat4::identity(),
                              Mat4::identity());
    EXPECT(static_cast<u32>(glstub::stats.drawElementsCalls) == draws + 1);

    // RECUPERAÇÃO: contexto NOVO (TERM/INIT limpa o backoff) e o MESMO
    // pedido sobe com sucesso
    primGraveDig();
    primMeshesDestroyAll();      // TERM: posse morta, primNeg limpo
    detachRenderersFromGpu();    // mesh null (vai re-subir)
    EXPECT(mr->mesh == nullptr);
    EXPECT(!mr->primNeg);
    primFlushPending();          // INIT: re-sobe (o pedido persistiu)
    ASSERT(mr->mesh != nullptr);
    EXPECT(mr->mesh->vertexCount() > 0);
    EXPECT(mr->prim.kind == PrimKind::Box);
    EXPECT(logHas("passo=bind ok"));
}

// ---------------------------------------------------------------------------
// 0.8.9-3. AS 8 PRIMITIVAS × 3 ORDENS variadas — sem crash, sem perda de
// seleção, TODAS desenham (stub GL grava draw calls; o material lit afirma
// o cull — backface culling contra winding errado).
// ---------------------------------------------------------------------------
TEST(wiring089_device_duas_prims_tres_ordens_todas_renderizam) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    const Handle h = addMeshTic("Ordens");
    ASSERT(h.valid());
    Tic* t = g_scene.get(h);
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);

    // 0.8.10: DUAS formas × 3 ordens (direta, inversa, intercalada) ×
    // REPETIÇÃO (A→B→A) — cada troca com o FLUSH (fronteira de frame)
    const int ordA[4] = {0, 1, 0, 1};
    const int ordB[4] = {1, 0, 1, 0};
    const int ordC[4] = {0, 0, 1, 1};
    const int* ords[3] = {ordA, ordB, ordC};
    for (int o = 0; o < 3; ++o) {
        for (int i = 0; i < 4; ++i) {
            const PrimKind kind = static_cast<PrimKind>(ords[o][i]);
            const editor::AssetPickOutcome out = editor::applyAssetPick(
                g_scene, h, 4, static_cast<int>(kind) + 2, g_catalog,
                makeAssetResolvers());
            EXPECT(out.applied);
            EXPECT(g_editor.selected == h);          // SEM perda de seleção
            primGraveDig();                          // início do frame
            primFlushPending();                      // ponto seguro
            ASSERT(mr->mesh != nullptr);
            EXPECT(mr->mesh->vertexCount() > 0);
            // TODAS renderizam: draw no stub GL (1 draw a mais por troca)
            const u32 draws = static_cast<u32>(glstub::stats.drawElementsCalls);
            g_renderer.beginFrame();
            (void)g_renderer.drawMesh(*mr->mesh, Mat4::identity(),
                                      Mat4::identity());
            EXPECT(static_cast<u32>(glstub::stats.drawElementsCalls) ==
                   draws + 1);
            // culling: o lit material liga GL_CULL_FACE (winding CCW do
            // gerador passa — a aferição por triângulo vive no test_prims)
            EXPECT(glstub::stats.cullEnabled);
        }
    }
    // SEM CACHE: a posse é contida (1 mesh por TIC com prim), a cova limpa
    primGraveDig();   // o último flush deixou 1 na cova — o frame seguinte
    EXPECT(g_primOwners.size() == 1u);
    EXPECT(g_primGrave.empty());
}

// ---------------------------------------------------------------------------
// 0.8.9-4. IMPORT GIGANTE e2e (o caminho do device: browser → "Sim" →
// applyImportedAssetToSelectedTic): fator ÚNICO, proporções preservadas,
// linha "import: dims=… uniform scale=…", re-aplicar não mexe na escala.
// ---------------------------------------------------------------------------
TEST(wiring089_device_import_gigante_uniforme_sem_espalmar) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    ensureEngineReady();

    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    rawSt->makeDirs("meshes");
    g_storage = std::move(st);
    g_resources.setStorage(rawSt);
    g_gpu.init(&g_resources);
    g_projectReady = true;
    refreshCatalog();

    const Handle h = addMeshTic("Gigante");
    ASSERT(h.valid());
    Tic* t = g_scene.get(h);
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    Transform3D* tr = t->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    tr->scale = Vec3{1.0f, 1.0f, 1.0f};

    // OBJ GIGANTE 1000×500×250 (o "modelo do dono" que entrava inutilizável)
    const char* giant =
        "o gigante\nv 0 0 0\nv 1000 0 0\nv 0 500 250\nf 1 2 3\n";
    const std::string tmp = "goni_w089_giant.obj";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(giant, 1, std::strlen(giant), f);
    std::fclose(f);
    fileapi::DirEntry e;
    e.name = "goni_w089_giant.obj";
    e.path = tmp;
    e.isDir = false;
    e.kind = 'm';

    browserImportFile(e);
    pumpImportJob();   // 0.8.10: o job converte → assets/goni_w089_giant.gmesh
    applyImportedAssetToSelectedTic();
    ASSERT(mr->mesh != nullptr);
    EXPECT(mr->meshPath == "assets/goni_w089_giant.gmesh");

    // AABB ORIGINAL do mesh (1000×500×250) — a geometria fica INTACTA
    // (0.8.10: quantização 16-bit do .gmesh — erro ≤ 0.02 em 1000: tolerância)
    const Vec3 ext = mr->mesh->boundsExtent();
    EXPECT(::test::nearEqF(ext.x, 1000.0f, 0.1f));
    EXPECT(::test::nearEqF(ext.y, 500.0f, 0.1f));
    EXPECT(::test::nearEqF(ext.z, 250.0f, 0.1f));

    // FATOR ÚNICO: s = 2/1000 ≈ 0.002 nos TRÊS eixos (nunca espalmado;
    // a quantização mexe ~1e-5 no fator)
    EXPECT(::test::nearEqF(tr->scale.x, 0.002f, 1e-4f));
    EXPECT(::test::nearEqF(tr->scale.y, 0.002f, 1e-4f));
    EXPECT(::test::nearEqF(tr->scale.z, 0.002f, 1e-4f));
    // AABB RENDER ≤ alvo com PROPORÇÕES IGUAIS (x:y:z antes == depois)
    const Vec3 scaled{ext.x * tr->scale.x, ext.y * tr->scale.y,
                      ext.z * tr->scale.z};
    EXPECT(scaled.x <= editor::kImportTargetSize * 1.001f);
    EXPECT(::test::nearEqF(scaled.x / scaled.y, ext.x / ext.y, 1e-3f));
    EXPECT(::test::nearEqF(scaled.y / scaled.z, ext.y / ext.z, 1e-3f));

    // a linha exigida pelo prompt no engine.log
    EXPECT(logHas("import: dims="));
    EXPECT(logHas("uniform scale="));

    // "escala original": repõe {1,1,1} — o mesh volta ao tamanho REAL
    tr->scale = Vec3{1.0f, 1.0f, 1.0f};
    EXPECT(::test::nearEqF(mr->mesh->boundsMaxExtent(), 1000.0f, 0.5f));

    // re-aplicar o MESMO ref NÃO re-normaliza (a escala afinada é sagrada)
    tr->scale = Vec3{3.0f, 3.0f, 3.0f};
    const editor::AssetPickOutcome out = editor::applyAssetPick(
        g_scene, h, 1, 2, g_catalog, makeAssetResolvers());
    EXPECT(out.applied);
    EXPECT(::test::nearEqF(tr->scale.x, 3.0f, 1e-5f));

    std::remove(tmp.c_str());
}

// ---------------------------------------------------------------------------
// 0.8.9-5. ESPAÇO SEM TETOS no device: zoom 0.01→100 000, far dinâmico por
// frame (contém a cena) e grelha adaptativa (o passo vai ao shader).
// ---------------------------------------------------------------------------
TEST(wiring089_device_zoom_far_dinamico_e_grelha_adaptativa) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();

    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    EXPECT(g_ready);

    // um TIC lá longe (py=10 000 via CAMPO — o propósito 6 é o mesmo do CI)
    const Handle h = addMeshTic("Longe");
    ASSERT(h.valid());
    Transform3D* tr = g_scene.get(h)->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    tr->pos.y = 10000.0f;

    // o AABB da cena contém o TIC; o far derivado CONTÉM o AABB (pela
    // DISTÂNCIA ao ponto mais longe — a cena está longe, não só "grande")
    Vec3 mn, mx;
    f32 radius = 0.0f;
    camerautil::sceneAABB(g_scene, mn, mx, radius);
    EXPECT(mx.y >= 10000.0f);
    const f32 farthest = camerautil::sceneFarthest(mn, mx);
    EXPECT(farthest >= 10000.0f);
    f32 clipNear = 0.0f, clipFar = 0.0f;
    camerautil::editorClips(g_camera.dist, farthest, clipNear, clipFar);
    EXPECT(clipFar > 10000.0f);            // a cena cabe no frustum

    // ZOOM nos extremos + frames: SEM crash (o clamp novo deixa chegar lá)
    const f32 dists[4] = {0.01f, 6.0f, 5000.0f, 100000.0f};
    for (const f32 d : dists) {
        g_camera.setDistance(d);
        EXPECT(g_camera.dist == d);        // dentro do range novo
        const auto t0 = std::chrono::steady_clock::now();
        frame();
        EXPECT(msSince(t0) < 500.0);      // hang/crash = falha
    }
    // o far do frame nunca ficou atrás da cena (membro dinâmico setado)
    EXPECT(g_camera.farZ > farthest);

    // GRELHA adaptativa: o passo por zoom vai AO SHADER (uniform1f do step)
    const f32 steps[3] = {0.05f, 60.0f, 5000.0f};
    for (const f32 d : steps) {
        glstub::stats.lastUniform1f = -1.0f;
        g_grid.draw(Mat4::identity(), g_camera.eye(), d);
        EXPECT(::test::nearEqF(glstub::stats.lastUniform1f,
                               Grid::gridStepForDist(d), 1e-3f));
    }

    onAppCmd(&app, APP_CMD_TERM_WINDOW);
}


// ===========================================================================
// 0.8.10 — CASOS DO DEVICE (este TU inclui o main.cpp): migração e2e +
// setting "largar a fonte" + reconverter (os casos PUROS dos formatos/
// 500 MB/archives vivem no test_wiring010.cpp)
// ===========================================================================

TEST(wiring010_migracao_projeto_antigo_e2e) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    // 0.8.12: cache dir da app via JNI fake (o staging ja NAO e /tmp -
    // a migracao tem de sobreviver num ambiente SEM /tmp escrevivel)
    enableDeviceCacheDir();
    // 0.8.12: cache dir da app via JNI fake (o staging já NÃO é /tmp —
    // a migração tem de sobreviver num ambiente SEM /tmp escrevível)
    enableDeviceCacheDir();

    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    // projeto ANTIGO: meshes/casa.obj no storage + TIC com a ref LEGADA
    rawSt->makeDirs("meshes");
    rawSt->writeText("meshes/casa.obj", "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n");
    g_storage = std::move(st);
    g_projectReady = true;
    g_resources.setStorage(rawSt);
    g_gpu.init(&g_resources);

    const Handle h = g_scene.create("Casa");
    Tic* t = g_scene.get(h);
    ASSERT(t != nullptr);
    t->addComponent<Transform3D>();
    MeshRenderer* mr = t->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->meshPath = "meshes/casa.obj";

    // o PÓS-LOAD do device: migra (silenciosa) + fixup da ref
    postLoadMigrateAndFixup();

    EXPECT(rawSt->exists("assets/casa.gmesh"));
    EXPECT(mr->meshPath == "assets/casa.gmesh");
    EXPECT(logHas("asset: migracao 'meshes/casa.obj'"));
    EXPECT(logHas("asset: ref migrada 'meshes/casa.obj' -> 'assets/casa.gmesh'"));
    refreshCatalog();
    EXPECT(!g_catalog.meshes.empty());
    EXPECT(g_catalog.meshes[0] == "assets/casa.gmesh");
    Mesh* m = g_gpu.mesh("assets/casa.gmesh");
    ASSERT(m != nullptr);
    EXPECT(m->indexCount() == 3);
    std::printf("  [migracao] meshes/casa.obj → assets/casa.gmesh (%u verts / "
                "%u idx) — ref reescrita em silencio\n", m->vertexCount(),
                m->indexCount());
    EXPECT(logHas("asset: staging em '"));   // 0.8.12: staging cache/projeto
    rmrfCacheDir();
}

TEST(wiring010_setting_fonte_e_reconverter) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    g_projectReady = true;
    // 0.8.12: cache dir da app via JNI fake (staging sem /tmp)
    enableDeviceCacheDir();

    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    g_storage = std::move(st);
    g_resources.setStorage(rawSt);
    g_gpu.init(&g_resources);

    const std::string objPath = "goni_w010_fonte.obj";
    FILE* f = std::fopen(objPath.c_str(), "wb");
    ASSERT(f != nullptr);
    const char* obj = "o fonte\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::fwrite(obj, 1, std::strlen(obj), f);
    std::fclose(f);

    // IMPORT com keepSource=TRUE (default): fonte fica em source/
    fileapi::DirEntry e;
    e.name = "goni_w010_fonte.obj";
    e.path = objPath;
    e.isDir = false;
    e.kind = 'm';
    browserImportFile(e);
    pumpImportJob();
    EXPECT(rawSt->exists("source/goni_w010_fonte.obj"));
    EXPECT(rawSt->exists("assets/goni_w010_fonte.gmesh"));

    // RECONVERTER (o botão do Settings): o convertido sai e volta da fonte
    EXPECT(rawSt->remove("assets/goni_w010_fonte.gmesh"));
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::reconvertFile("source/goni_w010_fonte.obj", *rawSt,
                                  nullptr, out, stats, err));
    EXPECT(rawSt->exists("assets/goni_w010_fonte.gmesh"));
    std::printf("  [reconverter] source/ → assets/ de volta (%u mesh(es), %llu "
                "B)\n", out.meshes.size(),
                static_cast<unsigned long long>(stats.outputBytes));

    // SETTING "largar a fonte": o import REMOVE a fonte pós-conversão
    g_keepSource = false;
    browserImportFile(e);
    pumpImportJob();
    EXPECT(rawSt->exists("assets/goni_w010_fonte.gmesh"));   // convertido vivo
    EXPECT(!rawSt->exists("source/goni_w010_fonte.obj"));    // fonte LARGADA
    EXPECT(logHas("fonte 'source/goni_w010_fonte.obj' largada"));
    std::printf("  [setting] fonte largada pós-import; assets/ fica\n");
    g_keepSource = true;
    std::remove(objPath.c_str());
    rmrfCacheDir();
}

// ===========================================================================
// 0.8.11 — ÁUDIO: casos do CAMINHO DO DEVICE (este TU inclui o
// platform/main.cpp — browserImportAudio/audioRecordToggle/audioBackendBoot/
// applyImportedAssetToSelectedTic/globais). A parte PURA (contentor .gi,
// misturador, probe com fakes, workspace com fake host, serializer) vive
// em test_wiring011.cpp.
// ===========================================================================

// helper local: um WAV PCM16 REAL no disco (o browserImportAudio lê por
// File API) — mono 22050, 0.3 s de senoide
std::string writeTempWav(const char* path) {
    const u32 rate = 22050;
    std::vector<i16> pcm;
    for (u32 i = 0; i < rate * 3 / 10; ++i) {
        pcm.push_back(static_cast<i16>(std::sin(
            static_cast<f64>(i) / rate * 6.283185307179586 * 440.0) * 12000));
    }
    std::vector<u8> wav;
    const u32 dataBytes = static_cast<u32>(pcm.size()) * 2;
    auto push = [&wav](const void* p, size_t n) {
        const u8* b = static_cast<const u8*>(p);
        wav.insert(wav.end(), b, b + n);
    };
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    const u32 riff = 36 + dataBytes;
    push(&riff, 4);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E'});
    wav.insert(wav.end(), {'f', 'm', 't', ' '});
    const u32 fmtSz = 16;
    const u16 ch = 1, bits = 16, fmt = 1;
    const u32 byteRate = rate * ch * bits / 8;
    const u16 blockAlign = static_cast<u16>(ch * bits / 8);
    push(&fmtSz, 4);
    push(&fmt, 2);
    push(&ch, 2);
    push(&rate, 4);
    push(&byteRate, 4);
    push(&blockAlign, 2);
    push(&bits, 2);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    push(&dataBytes, 4);
    push(pcm.data(), dataBytes);
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        return "";
    }
    std::fwrite(wav.data(), 1, wav.size(), f);
    std::fclose(f);
    return path;
}

// ---------------------------------------------------------------------------
// 0.8.11-1. IMPORT de áudio e2e pelo caminho do device: WAV real no disco →
// browserImportFile(kind 's') → browserImportAudio → audio/<nome>.gi +
// catálogo (workspace E Inspector) + rácio no engine.log + diálogo
// "aplicar ao TIC?" quando há AudioPlayer selecionado; o "Sim" atribui.
// ---------------------------------------------------------------------------
TEST(wiring011_device_import_wav_e2e_aplica_ao_tic) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();

    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    g_storage = std::move(st);
    g_projectReady = true;

    // TIC de ÁUDIO selecionado (o alvo do diálogo pós-import)
    const Handle h = createTicFromPreset(g_scene, PresetKind::Audio, nullptr,
                                         nullptr);
    ASSERT(h.valid());
    g_editor.selected = h;
    ASSERT(g_scene.get(h)->getComponent<AudioPlayer>() != nullptr);

    // WAV real no disco (mono 22050, 0.3 s) + entrada do navegador
    const std::string tmp = writeTempWav("goni_w011_salto.wav");
    ASSERT(!tmp.empty());
    fileapi::DirEntry e;
    e.name = "goni_w011_salto.wav";
    e.path = tmp;
    e.isDir = false;
    e.kind = 's';   // 0.8.11: o FileApi classifica wav/ogg/mp3

    browserImportFile(e);   // o dispatcher real do toque no navegador

    // o clip vive no projeto (audio/goni_w011_salto.gi) e LÊ-SE de volta
    EXPECT(rawSt->exists("audio/goni_w011_salto.gi"));
    EXPECT(!g_audioCatalog.empty());
    EXPECT(g_audioCatalog[0] == "audio/goni_w011_salto.gi");
    EXPECT(g_catalog.audio == g_audioCatalog);   // o Inspector vê o MESMO
    const GiClip* clip = audioClipFor("audio/goni_w011_salto.gi");
    ASSERT(clip != nullptr);
    EXPECT(clip->sampleRate == 22050);
    EXPECT(clip->channels == 1);
    EXPECT(clip->frames > 6000);
    EXPECT(clip->codec == GiCodec::Adpcm);
    std::printf("  [import] wav %.1fs → %s (%.2fs, codec=%s)\n",
                clip->duration() + 0.0f, "audio/goni_w011_salto.gi",
                clip->duration(), giCodecName(clip->codec));

    // a linha do RÁCIO no engine.log (a exigência do prompt)
    EXPECT(logHas("audio: import"));
    EXPECT(logHas("ratio="));

    // o diálogo "aplicar ao TIC?" abriu (kind 'a' — áudio)
    EXPECT(g_applyAsk.open);
    EXPECT(g_editor.applyAsk);
    EXPECT(g_applyAsk.kind == 'a');
    EXPECT(g_applyAsk.rel == "audio/goni_w011_salto.gi");

    // o "Sim": atribui o clip ao AudioPlayer do TIC selecionado
    applyImportedAssetToSelectedTic();
    AudioPlayer* au = g_scene.get(h)->getComponent<AudioPlayer>();
    EXPECT(au->clipPath == "audio/goni_w011_salto.gi");
    EXPECT(logHas("clip 'audio/goni_w011_salto.gi' atribuido"));
    EXPECT(!g_applyAsk.open);
    std::remove(tmp.c_str());
}

// ---------------------------------------------------------------------------
// 0.8.11-2. IMPORT sem alvo: TIC de Mesh selecionado → SEM diálogo (o
// honesto é entrar no catálogo; atribui-se pelo seletor de clips depois).
// ---------------------------------------------------------------------------
TEST(wiring011_device_import_sem_audioplayer_sem_dialogo) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    ensureEngineReady();
    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    g_storage = std::move(st);
    g_projectReady = true;
    addMeshTic("Mesh");   // TIC SEM AudioPlayer

    const std::string tmp = writeTempWav("goni_w011_sem.wav");
    ASSERT(!tmp.empty());
    fileapi::DirEntry e;
    e.name = "goni_w011_sem.wav";
    e.path = tmp;
    e.isDir = false;
    e.kind = 's';
    browserImportFile(e);
    EXPECT(rawSt->exists("audio/goni_w011_sem.gi"));
    EXPECT(!g_applyAsk.open);   // sem AudioPlayer → SEM pergunta
    std::remove(tmp.c_str());
}

// ---------------------------------------------------------------------------
// 0.8.11-3. GRAVAÇÃO e2e no caminho do host (o mic SINTÉTICO — a mesma
// máquina de estados da thread): toggle ON → worker corre → toggle OFF →
// o PCM vira audio/rec-<unix>.gi + catálogo + rácio no log.
// ---------------------------------------------------------------------------
TEST(wiring011_device_gravacao_sintetica_e2e) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();

    // SEM projeto: honesto, não grava (nada crasha)
    audioRecordToggle();
    EXPECT(!g_audioRec.on.load());

    // A PONTE DO MIC (o fake JNI faz o papel da VvActivity): concedida →
    // true; negada → false (o toggle mostra o toast e NÃO grava)
    javaRegistersGranted();
    g_jni.mic_granted = true;
    EXPECT(storage::jniEnsureMicPermission());
    g_jni.mic_granted = false;
    EXPECT(!storage::jniEnsureMicPermission());
    EXPECT(logHas("ensureMicPermission"));   // a ponte loga o estado
    g_jni.mic_granted = true;

    auto st = std::make_unique<FakeStorage>();
    FakeStorage* raw = st.get();
    g_storage = std::move(st);
    g_projectReady = true;

    audioRecordToggle();   // ON: o worker arranca
    EXPECT(g_audioRec.on.load());
    EXPECT(g_audioRec.th.joinable());
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    audioRecTick();   // o frame alimenta o temporizador
    {
        const std::lock_guard<std::mutex> lk(g_audioRec.mx);
        EXPECT(g_audioRec.pcm.size() > 2000);   // a senoide acumulou
    }
    EXPECT(g_audioRec.level.load() > 0.2f);     // o medidor vê sinal

    audioRecordToggle();   // OFF: o PCM vira .gi ADPCM
    EXPECT(!g_audioRec.on.load());
    bool hasRec = false;
    for (const std::string& c : g_audioCatalog) {
        if (c.find("audio/rec-") == 0) {
            hasRec = true;
        }
    }
    EXPECT(hasRec);
    EXPECT(logHas("audio: gravado"));
    EXPECT(logHas("ratio="));
    // o clip gravado LÊ-SE de volta (mono 44100)
    for (const std::string& c : g_audioCatalog) {
        if (c.find("audio/rec-") == 0) {
            const GiClip* clip = audioClipFor(c);
            ASSERT(clip != nullptr);
            EXPECT(clip->sampleRate == 44100);
            EXPECT(clip->channels == 1);
            std::printf("  [gravar] %s: %.2fs %u frames\n", c.c_str(),
                        clip->duration(), static_cast<unsigned>(clip->frames));
        }
    }
}

// ---------------------------------------------------------------------------
// 0.8.11-4. BOOT do backend (caminho do device, stub no host): o INIT
// arranca AAudio→stub ATIVO; o PROBE (Settings → diagnóstico) escreve a
// tabela no engine.log; a TROCA de backend mantém o misturador vivo.
// ---------------------------------------------------------------------------
TEST(wiring011_device_boot_probe_e_troca_de_backend) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();

    audioBackendBoot();
    EXPECT(g_audioBackendReady);
    EXPECT(g_audioOut != nullptr);
    EXPECT(logHas("audio: backend"));
    // o callback do backend puxa o misturador: um mix direto funciona
    {
        std::vector<f32> buf(2048);
        audioout::setMixFn([](f32* out, u32 frames, u32 ch, u32 rate) {
            for (u32 i = 0; i < frames * ch; ++i) {
                out[i] = static_cast<f32>(i % (rate / 100)) / 1000.0f;
            }
        });
        // (o stub host não puxa o callback por si; o contrato setMixFn é
        // aferido: o AudioOutDevice REAL o lê no dataCb)
        audioout::MixFn fn = audioout::currentMixFn();
        EXPECT(static_cast<bool>(fn));
        fn(buf.data(), 1024, 2, 44100);
        EXPECT(buf[0] == 0.0f);
        EXPECT(buf[1] == 0.001f);
    }

    // o PROBE (Settings → "diagnostico audio"): tabela no log, decisão ok
    audioProbeRun();
    EXPECT(logHas("audio: probe"));
    EXPECT(logHas("DECISAO"));

    // a TROCA de backend: o misturador segue (a interface é a mesma)
    EXPECT(audioBackendSwitch(true));
    EXPECT(g_audioOut != nullptr);
    EXPECT(g_audioBackendReady);
    EXPECT(logHas("audio: TROCA de backend"));
    audioBackendSwitch(false);   // volta (idempotente no stub)
    EXPECT(g_audioBackendReady);

    // lifecycle: pause/resume não crasham (APP_CMD_PAUSE/RESUME do main)
    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_PAUSE);
    onAppCmd(&app, APP_CMD_RESUME);
    EXPECT(logHas("audio: PAUSE"));
    EXPECT(logHas("audio: RESUME"));

    if (g_audioOut) {
        g_audioOut->stop();
    }
    g_audioOut.reset();
    g_audioBackendReady = false;
}

// ---------------------------------------------------------------------------
// 0.8.11-5. FRAME no modo ÁUDIO: o workspace substitui o viewport (rect do
// editor de UI), o frame COMPLETA (sem early-return escondido), os glifos
// de altifalante NÃO desenham no modo áudio (só no 3D) e o Inspector com
// AudioPlayer selecionado desenha a secção Audio com o clip atribuído.
// ---------------------------------------------------------------------------
TEST(wiring011_device_frame_audio_mode_e_glyphs) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();
    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    EXPECT(g_ready);
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        EXPECT(g_font.loadFromPaths(paths, 1, 28.0f));
    }
    g_ui.setFont(&g_font);
    g_projectReady = true;

    // projeto com um clip + TIC de áudio com o clip atribuído
    auto st = std::make_unique<FakeStorage>();
    FakeStorage* rawSt = st.get();
    g_storage = std::move(st);
    const Handle h = createTicFromPreset(g_scene, PresetKind::Audio, nullptr,
                                         nullptr);
    g_editor.selected = h;
    AudioPlayer* au = g_scene.get(h)->getComponent<AudioPlayer>();
    ASSERT(au != nullptr);
    au->clipPath = "audio/x.gi";
    au->posicional = true;
    au->raioExterno = 4.0f;
    // um .gi mínimo no storage (o catálogo + o Inspector "clip: x")
    {
        const std::vector<i16> pcm(4410, 8000);
        GiWriteIn wi;
        wi.codec = GiCodec::Adpcm;
        wi.sampleRate = 44100;
        wi.channels = 1;
        wi.frames = pcm.size();
        wi.pcm16 = pcm.data();
        wi.name = "x";
        std::vector<u8> gi;
        std::string err;
        ASSERT(writeGi(wi, gi, err));
        ASSERT(rawSt->writeBytes("audio/x.gi", gi.data(), gi.size()));
    }
    refreshAudioCatalog();

    // (a) frame LIMPO em 3D: os GLIFOS do altifalante desenham (pass UI)
    glstub::reset();
    const auto t0 = std::chrono::steady_clock::now();
    frame();
    EXPECT(msSince(t0) < 500.0);
    EXPECT(glstub::stats.drawArraysCalls > 0);

    // (b) frame no modo ÁUDIO: o workspace É o viewport; o frame COMPLETA
    // (sem crash, sem hang) — o toolbar continua a desenhar (o G3 com o
    // ÁUDIO ativo é o CAMINHO DE VOLTA ao 3D)
    g_editor.audioMode = true;
    const auto t1 = std::chrono::steady_clock::now();
    frame();
    EXPECT(msSince(t1) < 500.0);

    // (c) o PREVIEW do Inspector: o flag liga a voz no misturador (o frame
    // chama audioPreviewTick(*selTic)); um clip de 0.1 s acaba sozinho
    au->previewing = true;
    {
        const auto t2 = std::chrono::steady_clock::now();
        frame();
        EXPECT(msSince(t2) < 500.0);
        EXPECT(au->voiceId >= 0);        // a voz NASCEU pelo caminho real
        EXPECT(g_audioEngine.activeVoices() == 1);
        EXPECT(logHas("audio: preview no TIC"));
        // esgota o clip (0.1 s = 4410 frames @ 44100): o fim natural
        // desliga o flag (o mix corre AQUI como o callback faria)
        std::vector<f32> buf(8192 * 2);
        g_audioEngine.mix(buf.data(), 8192, 2, 44100);
        audioPreviewTick(*g_scene.get(h));
        EXPECT(au->voiceId == -1);
        EXPECT(!au->previewing);
    }
    g_editor.audioMode = false;

    onAppCmd(&app, APP_CMD_TERM_WINDOW);
}

// ===========================================================================
// 0.9.1 — ORIENTAÇÃO + IME DO SISTEMA (campanha 0.9): casos do CAMINHO DO
// DEVICE (o TU inclui platform/main.cpp — openTextWindow/closeTextWindow,
// o bridge JNI com o fake activity e o INIT_WINDOW real). A parte PURA
// (fila ime::, política de orientação, textwin draw) vive em
// test_wiring091.cpp.
// ===========================================================================

// ---------------------------------------------------------------------------
// 0.9.1-1. ABRIR → portrait + imeShow (o par INSEPARÁVEL); o IME escreve
// no buffer (nativeOnImeText/Key → fila → frame); DEL apaga 1 code point.
// ---------------------------------------------------------------------------
TEST(wiring091_device_textwin_abre_portrait_ime_escreve) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();

    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    EXPECT(g_ready);
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        EXPECT(g_font.loadFromPaths(paths, 1, 28.0f));
    }
    g_ui.setFont(&g_font);

    // ABRIR: o par portrait+IME (o estado vive no ime::; o Java executa)
    ime::clearForTest();
    g_jni.void_calls.clear();
    openTextWindow();
    EXPECT(g_editor.textWin.open);
    EXPECT(ime::orientation() == ime::Orientation::Portrait);
    bool sawPortrait = false, sawImeShow = false;
    for (const auto& c : g_jni.void_calls) {
        if (c.first == "setOrientation" && c.second == 1) sawPortrait = true;
        if (c.first == "imeShow") sawImeShow = true;
    }
    EXPECT(sawPortrait);   // setRequestedOrientation(PORTRAIT) pedido
    EXPECT(sawImeShow);    // InputMethodManager.showSoftInput pedido
    EXPECT(logHas("orientacao: portrait pedida (janela de texto aberta)"));

    // IME escreve: commit "Ola" → ENTER (66) → "mundo"; o frame consome a
    // fila (o MESMO código que corre no device — os natives reais do bridge)
    jclass cls = g_jni.env ? nullptr : nullptr;   // o fake aceita nullptr
    auto push = [&](const char* s) {
        const jstring js = g_jni.newString(s);
        Java_vv_goni_VvActivity_nativeOnImeText(g_jni.env, cls, js);
    };
    auto key = [&](int kc) {
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, cls,
                                               static_cast<jint>(kc), 0);
    };
    push("Ola");
    key(66);       // KEYCODE_ENTER
    push("mundo");
    key(67);       // KEYCODE_DEL (apaga o 'o' — 1 code point)
    push("!");
    frame();
    EXPECT(g_editor.textWin.buf == "Ola\nmund!");

    // frame com a janela aberta: desenha de verdade (modal) e completa
    const auto t0 = std::chrono::steady_clock::now();
    frame();
    EXPECT(msSince(t0) < 500.0);
    EXPECT(glstub::stats.drawArraysCalls > 0);

    // FECHAR: o par landscape+imeHide (o espelho EXATO do abrir)
    g_jni.void_calls.clear();
    closeTextWindow();
    EXPECT(!g_editor.textWin.open);
    EXPECT(ime::orientation() == ime::Orientation::Landscape);
    bool sawLandscape = false, sawImeHide = false;
    for (const auto& c : g_jni.void_calls) {
        if (c.first == "setOrientation" && c.second == 0) sawLandscape = true;
        if (c.first == "imeHide") sawImeHide = true;
    }
    EXPECT(sawLandscape);
    EXPECT(sawImeHide);
    EXPECT(logHas("orientacao: landscape pedida (janela de texto fechada)"));

    // IME com a janela fechada: não vinga (não contamina o teclado in-app)
    push("invasao");
    frame();
    EXPECT(g_editor.textWin.buf.empty());

    onAppCmd(&app, APP_CMD_TERM_WINDOW);
}

// ---------------------------------------------------------------------------
// 0.9.1-2. ROTAÇÃO com a janela aberta: TERM_WINDOW + INIT_WINDOW com a
// superfície em PORTRAIT (720×1536) — o lifecycle re-upa TUDO (a regressão
// "glifos brancos" — o atlas re-bake é o caminho 0.6.7) e a JANELA SOBREVIVE
// com o buffer INTACTO (estado da engine, não da GPU).
// ---------------------------------------------------------------------------
TEST(wiring091_device_rotacao_nao_corrompe_render_nem_perde_buffer) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    resetEngineForTest();
    javaRegistersGranted();

    android_app app;
    std::memset(&app, 0, sizeof(app));
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        EXPECT(g_font.loadFromPaths(paths, 1, 28.0f));
    }
    g_ui.setFont(&g_font);

    ime::clearForTest();
    openTextWindow();
    // escreve pelo IME (o caminho real do bridge)
    {
        const jstring js = g_jni.newString("texto antes de rodar");
        Java_vv_goni_VvActivity_nativeOnImeText(g_jni.env, nullptr, js);
        frame();
    }
    EXPECT(g_editor.textWin.buf == "texto antes de rodar");
    const std::string bufAntes = g_editor.textWin.buf;

    // ---- a ROTAÇÃO: o surface morre e renasce em PORTRAIT (720×1536) ----
    eglstub::g_surfaceW = 720;
    eglstub::g_surfaceH = 1536;
    onAppCmd(&app, APP_CMD_TERM_WINDOW);
    EXPECT(!g_ready);
    onAppCmd(&app, APP_CMD_INIT_WINDOW);
    EXPECT(g_ready);
    EXPECT(g_egl.width() == 720);     // a superfície rodou
    EXPECT(g_egl.height() == 1536);
    // o lifecycle fix re-upa TUDO no contexto NOVO (sem glifos brancos — a
    // regressão 0.6.7; no device a linha "RE-UPLOAD no contexto novo" aparece
    // nas fontes do sistema, no hospedeiro a fixture recarrega no mesmo
    // caminho — o que se aferva é o CONTEXTO NOVO + a fonte OK)
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        EXPECT(g_font.loadFromPaths(paths, 1, 28.0f));
    }
    g_ui.setFont(&g_font);
    EXPECT(g_font.ok());
    EXPECT(logHas("lifecycle: INIT_WINDOW #"));

    // a janela SOBREVIVE (estado da engine) e o frame corre em portrait
    EXPECT(g_editor.textWin.open);
    EXPECT(g_editor.textWin.buf == bufAntes);
    const auto t0 = std::chrono::steady_clock::now();
    frame();
    EXPECT(msSince(t0) < 500.0);
    EXPECT(glstub::stats.drawArraysCalls > 0);

    // e o IME continua a escrever após a rotação
    {
        const jstring js = g_jni.newString(" e depois");
        Java_vv_goni_VvActivity_nativeOnImeText(g_jni.env, nullptr, js);
        frame();
    }
    EXPECT(g_editor.textWin.buf == "texto antes de rodar e depois");

    // fechar em portrait: landscape reposto (o par vale SEMPRE)
    closeTextWindow();
    EXPECT(ime::orientation() == ime::Orientation::Landscape);

    // o DEVICE VOLTA a landscape (o próximo INIT é a app inteira)
    eglstub::g_surfaceW = 1280;
    eglstub::g_surfaceH = 720;
    onAppCmd(&app, APP_CMD_TERM_WINDOW);
}
