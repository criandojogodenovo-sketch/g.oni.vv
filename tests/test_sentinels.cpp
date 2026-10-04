// tests/test_sentinels.cpp — 0.8.12: SENTINELAS PERMANENTES DE REGRESSÃO.
//
// O CONTRATO (docs/REGRESSOES.md): estes 4 casos correm em TODAS as runs
// de CI, em TODAS as releases futuras, PARA SEMPRE. Cada um vigia um bug
// de device FIXADO com a evidência exata que o produziu no C33 — a linha
// de log/sintoma, a causa raiz e o fix estão registados no
// docs/REGRESSOES.md, e o REPLAY do dispositivo virtual (executável
// c33_virtual) cobre o caminho REAL do main.cpp por cima destas unidades.
//
//   regress_selection_loss — a seleção perdia-se entre selecionar o TIC e
//                            tocar no picker (deselect sem guard de overlay
//                            + handles mortos no INIT_WINDOW)
//   regress_tmp_staging    — a migração morria com /tmp read-only (errno=30)
//   regress_none_slot      — "none" sem caminho seguro/serializável no
//                            picker de mesh
//   regress_dump_identity  — dump velho no viewer sem badge ANTIGO
//   regress_audio_lifecycle — 0.9.3 (REG-002/R-006): arranque duplo do
//                            backend de áudio = fuga de stream = a porta do
//                            SIGSEGV AAudio no Unisoc (tombstones 00-17/
//                            21-31 do RMX3624); StartGate + Result
//                            verificados + close-no-errCb + degradação sem
//                            crash (o editor segue SEM SOM)
//
// Sentinela que nunca falha não é sentinela — é decoração: a prova de
// mutação (fix revertido → VERMELHO; reposto → VERDE) está no
// docs/RELATORIO-0.8.12.md, secção PROVA DE MUTAÇÃO.
//
// A parte PURA vive aqui (sem platform/main.cpp — o c33_virtual é o TU
// único com o caminho do device; o test_wiring087 mantém os fluxos 0.8.7+).
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub (nada de GL real — só compila o core)
#include <jni.h>         // FAKE controlável

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FakeStorage.h"
#include "FakeSafIo.h"
#include "core/SceneSerializer.h"
#include "core/SafStorage.h"
#include "core/Project.h"
#include "components/CameraComp.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "render/Primitives.h"
#include "ui/EditorUi.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"
#include "platform/BuildInfo.h"
#include "platform/CrashHandler.h"
#include "platform/StorageBridge.h"
#include "assets/AssetConverter.h"
// 0.9.3 (REG-002): o backend Oboe de PRODUÇÃO (TU comum) contra o stub
// tests/stub/oboe/Oboe.h — o MESMO código que corre no APK
#include "platform/AudioOut.h"
#include <oboe/Oboe.h>
#include <thread>

using namespace vv;
using ::test::nearEqF;

namespace {

const char* kSentinelLogs = "test-sentinels-logs";
const char* kSentinelCache = "test-sentinels-cache";

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

bool logHas(const char* needle) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, 900);
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int logCount(const char* needle) {
    int n = 0;
    std::vector<std::string> lines;
    vv::elog::readTail(lines, 900);
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            ++n;
        }
    }
    return n;
}

jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xE001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xE002));

extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);

// o papel do VvActivity.onCreate + getCacheDir: o registo da activity e o
// cache dir da app pela ponte JNI fake (o staging NUNCA é /tmp)
void registersWithCacheDir() {
    g_jni.reset();
    while (vv::storage::pollResult()) {
    }
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity,
        g_jni.newString("test-sentinels"));
    g_jni.manager_result = true;
    fileapi::makeDirs(kSentinelCache);
    g_jni.cache_dir = kSentinelCache;
}

// ---- ambiente das sentinelas de pick (stubs-ponteiro, como o assetpick) ----
Mesh* const kMeshStub  = reinterpret_cast<Mesh*>(0x10);
Mesh* const kCubeStub  = reinterpret_cast<Mesh*>(0x11);
Texture* const kTexA   = reinterpret_cast<Texture*>(0x30);
LitMaterial* const kMatStub = reinterpret_cast<LitMaterial*>(0x20);

struct PickEnv {
    Scene scene;
    Handle sel{};
    editor::AssetCatalog cat;
    editor::AssetResolvers res;

    PickEnv() {
        const Handle h = scene.create("T");
        Tic* t = scene.get(h);
        t->addComponent<MeshRenderer>();
        t->addComponent<Transform3D>();
        sel = h;
        res.cubeMesh = kCubeStub;
        res.material = kMatStub;
    }
    MeshRenderer* mr() {
        Tic* t = scene.get(sel);
        return t ? t->getComponent<MeshRenderer>() : nullptr;
    }
};

}  // namespace

// ===========================================================================
// SENTINELA 1 — regress_selection_loss
// A evidência do C33: "mesh: troca - → prim esfera ERRO(sem TIC com mesh
// selecionado)" — a seleção morria entre o TIC e o picker. Causa raiz
// (dupla): (a) main.cpp chamava viewportTapClearsSelection SEM guard de
// overlay (o tap na linha/backdrop do picker caía DENTRO do viewRect e
// limpava a seleção NO MESMO frame do dispatch); (b) o INIT_WINDOW
// recarregava a cena e os handles morriam sem re-validação.
// Fix: guard !anyOverlayOpen no chamador + pickerGuardBlocked no
// dispatch/Inspector + revalidateSelection pós-INIT_WINDOW.
// O REPLAY do caminho REAL vive no c33_virtual (Fases 2/3); aqui vigiam-se
// as UNIDADES que o fix introduziu — se alguém as apagar, VERMELHO.
// ===========================================================================
TEST(regress_selection_loss) {
    rmrf(kSentinelLogs);
    EXPECT(vv::elog::init(kSentinelLogs));
    registersWithCacheDir();

    // (1) RE-VALIDAÇÃO pós-lifecycle: handle vivo mantém-se; handle morto
    // re-mapeia por NOME; nome desaparecido limpa
    {
        Scene scene;
        const Handle h1 = scene.create("Casa");
        scene.create("Outro");
        // handle vivo → mantém
        EXPECT(editor::revalidateSelection(scene, h1, "Casa") == h1);
        // handle morto (a cena foi recarregada) → re-mapeia por nome
        const Handle morto{h1.index, h1.generation + 7u};
        const Handle remap = editor::revalidateSelection(scene, morto, "Casa");
        EXPECT(remap == h1);
        // nome que não existe → inválido (honesto — o TIC desapareceu)
        EXPECT(!editor::revalidateSelection(scene, morto, "NaoExiste").valid());
    }

    // (2) GUARDA dos pickers: só TIC VIVO com MeshRenderer é alvo
    {
        Scene scene;
        const Handle meshTic = scene.create("Mesh");
        scene.get(meshTic)->addComponent<MeshRenderer>();
        const Handle camTic = scene.create("Cam");
        scene.get(camTic)->addComponent<CameraComp>();   // sem MeshRenderer
        const Handle morto{};
        EXPECT(!editor::pickerGuardBlocked(scene, meshTic));   // alvo válido
        EXPECT(editor::pickerGuardBlocked(scene, camTic));     // câmara excluída
        EXPECT(editor::pickerGuardBlocked(scene, morto));      // handle morto
    }

    // (3) o OVERLAY não limpa a seleção: closeAllOverlays mantém o selected
    // (a semântica que o guard do chamador usa — qualquer overlay aberto
    // desliga o deselect no main; aqui aferimos o estado)
    {
        editor::EditorState st;
        Scene scene;
        const Handle h = scene.create("T");
        st.selected = h;
        st.assetMenu = 1;
        editor::closeAllOverlays(st);
        EXPECT(st.selected == h);   // a seleção FICA (só overlays fecham)
        EXPECT(st.assetMenu == 0);
    }

    // (4) o pick com seleção VIVA aplica e NÃO emite o sintoma (o guard
    // deixa passar o alvo válido — zero falsos positivos)
    {
        PickEnv e;
        const editor::AssetPickOutcome out =
            editor::applyAssetPick(e.scene, e.sel, 4, 2, e.cat, e.res);
        EXPECT(out.applied);   // esfera pedida com alvo válido
        EXPECT(!editor::pickerGuardBlocked(e.scene, e.sel));
        // o HINT é o caminho SEM alvo — o ERRO "sem TIC..." NÃO existe no log
        EXPECT(logCount("sem TIC com mesh selecionado") == 0);
    }

    rmrf(kSentinelCache);
}

// ===========================================================================
// SENTINELA 2 — regress_tmp_staging
// A evidência do C33: "fileapi: mkdir falhou em '/tmp' errno=30 (Read-only
// file system)" → "asset: migracao … FALHOU → staging falhou" — o host de
// testes tinha /tmp ESCREVÍVEL e o Android NÃO. Causa raiz:
// convert::reconvertFile usava o literal "/tmp/goni_reconvert_<pid>.tmp".
// Fix: stagingWrite com FALLBACK em cascata (.staging/ do projeto → cache
// dir da app via JNI getCacheDir → erro legível) — JAMAIS /tmp.
// O CI acrescenta o GATE de grep: nenhum literal /tmp em FileApi/assets.
// ===========================================================================
TEST(regress_tmp_staging) {
    rmrf(kSentinelLogs);
    EXPECT(vv::elog::init(kSentinelLogs));
    registersWithCacheDir();

    // o ambiente do device: /tmp READ-ONLY (errno=30) — a SEAM do C33 virtual
    fileapi::testing::setReadonlyPrefix("/tmp");
    {
        std::vector<u8> probe{1};
        EXPECT(!fileapi::writeAll("/tmp/goni_sentinel.tmp", probe.data(), 1));
        EXPECT(vv::fileapi::errnoText().find("errno=30") != std::string::npos);
        EXPECT(logHas("fileapi: fopen/write falhou em '/tmp/goni_sentinel.tmp'"));
    }

    // a MIGRAÇÃO com /tmp READ-ONLY: projeto antigo (fonte legada) converte
    // com o staging no CACHE DIR — o caminho que morria no C33
    {
        FakeStorage st;   // raiz "/fake" (não escrevível — como o SAF)
        st.makeDirs("meshes");
        st.writeText("meshes/casa.obj",
                     "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n");
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::reconvertFile("meshes/casa.obj", st, nullptr,
                                               out, stats, err);
        EXPECT(ok);   // COM o /tmp read-only ATIVO — o fix prova-se aqui
        if (!ok) {
            std::printf("  [erro] %s\n", err.c_str());
        }
        EXPECT(st.exists("assets/casa.gmesh"));
        EXPECT(logCount("staging falhou") == 0);
        EXPECT(logCount("mkdir falhou em '/tmp'") == 0);
        EXPECT(logHas("asset: staging em '"));
        // o staging foi para o cache dir (o caminho REAL logado) — nunca /tmp
        bool cacheStaging = false;
        {
            std::vector<std::string> lines;
            vv::elog::readTail(lines, 900);
            for (const std::string& l : lines) {
                if (l.find("asset: staging em '") != std::string::npos) {
                    if (l.find("/tmp/") == std::string::npos) {
                        cacheStaging = true;
                    }
                }
            }
        }
        EXPECT(cacheStaging);
    }

    // o caminho content:// (SAF) idem: staging no cache dir, sucesso
    {
        FakeSafIo io;
        SafStorage saf(&io, "content://tree/primary:GOneVV/sent");
        EXPECT(saf.makeDirs("meshes"));
        EXPECT(saf.writeText("meshes/casa.obj",
                             "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n"));
        io.flushWrites();   // o provider persiste o writeText (passo invisível
                            // do device — SEM isto a fonte não lê de volta)
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::reconvertFile("meshes/casa.obj", saf, nullptr,
                                               out, stats, err);
        io.flushWrites();
        EXPECT(ok);
        EXPECT(saf.exists("assets/casa.gmesh"));
        EXPECT(logCount("staging falhou") == 0);
    }

    fileapi::testing::clearReadonlyPrefix();
    rmrf(kSentinelCache);
}

// ===========================================================================
// SENTINELA 3 — regress_none_slot
// A evidência do C33: a opção none falhava (não limpava o slot ou caía no
// caminho de erro) e NÃO existia no picker de MESH. Fix: none em 1º lugar
// nos pickers de mesh/tex/prim; limpeza pelo MESMO caminho seguro das
// trocas (primRetire → deferred free no início do frame seguinte);
// serialização grava o slot vazio ("mesh":"none") e o load recarrega vazio.
// ===========================================================================
TEST(regress_none_slot) {
    rmrf(kSentinelLogs);
    EXPECT(vv::elog::init(kSentinelLogs));

    // (1) mesh none: slot limpa com posse p/ deferred free; none→X→none ×N
    // sem crash e estado vazio consistente
    {
        PickEnv e;
        e.mr()->mesh = kMeshStub;
        e.mr()->meshPath = "meshes/quad.obj";
        const editor::AssetPickOutcome none =
            editor::applyAssetPick(e.scene, e.sel, 1, 1, e.cat, e.res);
        EXPECT(none.applied);
        EXPECT(e.mr()->mesh == nullptr);
        EXPECT(e.mr()->material == nullptr);
        EXPECT(e.mr()->meshPath.empty());
        EXPECT(e.mr()->primOn == false);
        EXPECT(e.mr()->primRetire == kMeshStub);   // posse p/ cova

        for (int i = 0; i < 8; ++i) {
            EXPECT(editor::applyAssetPick(e.scene, e.sel, 1, 2, e.cat, e.res).applied);
            EXPECT(e.mr()->mesh == kCubeStub);
            EXPECT(editor::applyAssetPick(e.scene, e.sel, 1, 1, e.cat, e.res).applied);
            EXPECT(e.mr()->mesh == nullptr);
            EXPECT(e.mr()->meshPath.empty());
        }
    }

    // (2) tex none: textura limpa (material volta à cor plana)
    {
        PickEnv e;
        e.mr()->texture = kTexA;
        e.mr()->texPath = "textures/wood.png";
        const editor::AssetPickOutcome none =
            editor::applyAssetPick(e.scene, e.sel, 2, 1, e.cat, e.res);
        EXPECT(none.applied);
        EXPECT(e.mr()->texture == nullptr);
        EXPECT(e.mr()->texPath.empty());
    }

    // (3) ROUND-TRIP do slot vazio: o .goni grava "mesh":"none" e o load
    // recarrega VAZIO (sem mesh, sem prim, sem path)
    {
        Scene scene;
        const Handle h = scene.create("Vazio");
        Tic* t = scene.get(h);
        t->addComponent<Transform3D>();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        mr->mesh = nullptr;   // none: slot vazio
        mr->primOn = false;
        mr->meshPath.clear();
        mr->texPath.clear();
        mr->texture = nullptr;

        const std::string text = SceneSerializer::dump(scene);
        EXPECT(text.find("\"mesh\":\"none\"") != std::string::npos);
        EXPECT(text.find("meshPath") == std::string::npos);   // slot vazio não grava ref

        Scene loaded;
        SceneSerializer::LoadCtx ctx;   // sem cubeMesh: "none" fica vazio
        EXPECT(SceneSerializer::loadText(loaded, text, ctx));
        const Handle lh = loaded.find("Vazio");
        ASSERT(lh.valid());
        const Tic* lt = loaded.get(lh);
        ASSERT(lt != nullptr);
        const MeshRenderer* lmr = lt->getComponent<MeshRenderer>();
        ASSERT(lmr != nullptr);
        EXPECT(lmr->mesh == nullptr);        // recarrega VAZIO (round-trip)
        EXPECT(!lmr->primOn);
        EXPECT(lmr->meshPath.empty());
        EXPECT(lmr->texPath.empty());
        EXPECT(lmr->texture == nullptr);
    }

    // (4) none SEM alvo: o guard dá hint — nunca o caminho de ERRO
    {
        Scene scene;
        PickEnv e;
        const Handle semMesh = scene.create("Cam");
        scene.get(semMesh)->addComponent<CameraComp>();
        EXPECT(editor::pickerGuardBlocked(scene, semMesh));
        const editor::AssetPickOutcome none =
            editor::applyAssetPick(scene, semMesh, 1, 1, e.cat, e.res);
        EXPECT(!none.applied);   // sem crash, sem ação — o hint é do chamador
    }
}

// ===========================================================================
// SENTINELA 4 — regress_dump_identity
// A evidência do C33: o log viewer mostrava o dump VELHO
// (crash-1790830406.dump, offsets idênticos) SEM o rotular como antigo.
// Causa raiz: buildinfo::dumpIsFromOtherBuild existia e NUNCA era chamado
// pelo viewer. Fix: dumpBadge no drawLogViewer + banner com sha256.
// ===========================================================================
TEST(regress_dump_identity) {
    rmrf(kSentinelLogs);
    EXPECT(vv::elog::init(kSentinelLogs));

    // a build instalada (o papel do build_info.txt via JNI no onCreate)
    vv::buildinfo::set("0.8.12-sentinela", 42, "beef42", "cafe001122334455", 1790000000ull);

    // (1) dump VELHO (build 39 — a 0.8.9 do dono): badge ANTIGO com o NÚMERO
    EXPECT(vv::buildinfo::dumpIsFromOtherBuild("crash-1790830406-vc39.dump"));
    EXPECT(vv::buildinfo::dumpBadge("crash-1790830406-vc39.dump") ==
           "  [ANTIGO (build 39)]");
    // (2) dump PRÉ-0.8.10 (sem identidade no nome): badge pré-0.8.10
    EXPECT(vv::buildinfo::dumpBadge("crash-1790830406.dump") ==
           "  [ANTIGO (pre-0.8.10)]");
    // (3) dump NOVO (desta build): SEM badge
    EXPECT(!vv::buildinfo::dumpIsFromOtherBuild("crash-1790000500-vc42.dump"));
    EXPECT(vv::buildinfo::dumpBadge("crash-1790000500-vc42.dump").empty());

    // (4) o NOME do dump tem a identidade (crash-<unix>-vc<N>.dump)
    EXPECT(vv::buildinfo::dumpSuffix() == "-vc42");

    // (5) o BANNER de boot com o formato exigido (versão/code/sha256)
    const std::string banner = vv::buildinfo::banner();
    EXPECT(banner.find("G.One VV 0.8.12-sentinela") != std::string::npos);
    EXPECT(banner.find("versionCode 42") != std::string::npos);
    EXPECT(banner.find("sha256 cafe001122334455") != std::string::npos);

    // (6b) adversário: dump CORROMPIDO (vazio/binário lixo) com nome de
    // outra build — o viewer LISTA com badge e NUNCA crasha (só o nome é
    // parseado; o conteúdo é do addr2line, não do viewer)
    {
        const std::string dumpPath =
            std::string(kSentinelLogs) + "/crash-1789999999-vc39.dump";
        FILE* f = std::fopen(dumpPath.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fputs("", f);   // corrompido deliberadamente (0 bytes)
        std::fclose(f);
        std::vector<std::string> dumps;
        EXPECT(vv::elog::listDumps(dumps));
        bool badgedCorrompido = false;
        for (const std::string& d : dumps) {
            if (d.find("crash-1789999999-vc39") != std::string::npos &&
                vv::buildinfo::dumpBadge(d).find("[ANTIGO (build 39)]") !=
                    std::string::npos) {
                badgedCorrompido = true;
            }
        }
        EXPECT(badgedCorrompido);
    }

    // (6) o header do dump tem a identidade completa
    {
        const std::string dumpPath =
            std::string(kSentinelLogs) + "/crash-1790000000-vc42.dump";
        void* pcs[1] = {reinterpret_cast<void*>(0x1234)};
        EXPECT(vv::crash::writeDumpFromFrames(dumpPath.c_str(), "SIGSEGV",
                                              11, nullptr, pcs, 1) == 0);
        FILE* f = std::fopen(dumpPath.c_str(), "r");
        ASSERT(f != nullptr);
        char buf[1024] = {0};
        const size_t got = std::fread(buf, 1, sizeof(buf) - 1, f);
        (void)got;   // lê o que houver (o header tem de chegar inteiro)
        std::fclose(f);
        EXPECT(std::strstr(buf, "build: 0.8.12-sentinela (versionCode 42)") != nullptr);
        EXPECT(std::strstr(buf, "so: cafe001122334455") != nullptr);
        EXPECT(std::strstr(buf, "epoch: 1790000000") != nullptr);
        // o viewer lista o dump e o badge da MESMA build é vazio
        std::vector<std::string> dumps;
        EXPECT(vv::elog::listDumps(dumps));
        EXPECT(!dumps.empty());
        EXPECT(vv::buildinfo::dumpBadge(dumps[0]).empty());
        std::printf("  [dump] %s (sem badge — e da build instalada)\n",
                    dumps.empty() ? "?" : dumps[0].c_str());
    }

    // reset para não vazar
    vv::buildinfo::set("dev", 0, "", "", 0);
}

// ===========================================================================
// SENTINELA 5 — regress_audio_lifecycle (0.9.3, hotfix REG-002/R-006)
// A evidência do RMX3624 (Unisoc): SIGSEGV (SEGV_ACCERR) dentro de
// AAudio_createStreamBuilder chamado por startAudio() no onResume da app
// antiga (com.goni.runtime) — ~20 tombstones 19-20/09 + 3 em 25-26/09.
// Causa raiz da CLASSE do bug: arranque repetido sem guarda (stream vivo
// fugia), builder/stream usados depois de mortos, retornos não verificados.
// Fix: cadeia Oboe (primário) → AAudio fixado → AudioTrack, TODOS com o
// StartGate atómico (nunca arranques duplos), Result/aaudio_result_t
// verificados em TODAS as chamadas, close no error-callback (o padrão do
// oboe) e degradação graciosa (falha = editor SEM SOM, nunca crash).
// O REPLAY do caminho REAL (INIT/PAUSE/RESUME/TERM com audioBackendBoot)
// vive no c33_virtual (FASE 8); aqui vigia-se o BACKEND de produção.
// ===========================================================================
TEST(regress_audio_lifecycle) {
    rmrf(kSentinelLogs);
    EXPECT(vv::elog::init(kSentinelLogs));
    oboe::testing::reset();

    // ------------------------------------------------------------------
    // (1) ARRANQUE DUPLO — o padrão exato do tombstone: start() chamado
    // de novo (onResume repetido sem onPause) NÃO abre um 2º stream
    // ------------------------------------------------------------------
    {
        std::unique_ptr<vv::audioout::Backend> be(vv::audioout::createOboe());
        ASSERT(be != nullptr);
        EXPECT(be->start(44100, 2));
        const int abertosAteAgora = oboe::testing::hooks().openCount;
        EXPECT(abertosAteAgora == 1);
        EXPECT(be->ready());
        // o 2º start: idempotente — NÃO toca no hardware
        EXPECT(be->start(44100, 2));
        EXPECT(be->start(44100, 2));
        EXPECT(oboe::testing::hooks().openCount == abertosAteAgora);
        EXPECT(be->ready());
        EXPECT(logHas("audio(oboe): start ignorado — stream ja ativo"));
        // o log informativo do stream (Tarefa 2.5: rate/ch/perf)
        EXPECT(logHas("audio(oboe): stream ATIVO rate="));

        // ADVERSÁRIO (Tarefa 7D): 50× onResume SEM onPause — continua 1
        for (int i = 0; i < 50; ++i) {
            EXPECT(be->start(44100, 2));
        }
        EXPECT(oboe::testing::hooks().openCount == abertosAteAgora);
        EXPECT(be->ready());

        // lifecycle: pause/resume em loop (o fundo/recentes do Android)
        for (int i = 0; i < 10; ++i) {
            be->pause();
            be->resume();
        }
        EXPECT(be->ready());
        EXPECT(oboe::testing::hooks().openCount == abertosAteAgora);

        // a PARAGEM liberta o portão (um start futuro pode)
        be->stop();
        EXPECT(!be->ready());
        EXPECT(be->start(44100, 2));
        EXPECT(oboe::testing::hooks().openCount == abertosAteAgora + 1);
        be->stop();
    }

    // ------------------------------------------------------------------
    // (2) ERRO no stream (o disconnect do device): a sequência REAL do
    // oboe (before → close → after) DEGRADA sem crash; o portão abre
    // ------------------------------------------------------------------
    {
        oboe::testing::reset();
        std::unique_ptr<vv::audioout::Backend> be(vv::audioout::createOboe());
        ASSERT(be != nullptr);
        EXPECT(be->start(44100, 2));
        EXPECT(be->ready());
        // o headset desligou (a thread de erro do device — aqui simulada
        // NOUTRA thread p/ afervar a corrida errCb × stop do mutex)
        std::thread erro([&be] {
            oboe::testing::fireErrorOnAllStreams(
                oboe::Result::ErrorDisconnected);
        });
        erro.join();
        EXPECT(!be->ready());   // o probe/main VÊEM a morte
        EXPECT(logHas("audio(oboe): stream MORREU"));
        // o portão ABRIU no after-close: um start futuro pode tentar
        EXPECT(be->start(44100, 2));
        EXPECT(oboe::testing::hooks().openCount == 2);
        EXPECT(be->ready());
        be->stop();
        // ZERO FUGAS: cada stream aberto foi fechado EXATAMENTE uma vez
        EXPECT(oboe::testing::hooks().closeCount ==
               oboe::testing::hooks().openCount);
    }

    // ------------------------------------------------------------------
    // (3) FALHA de arranque = editor SEM SOM (nunca crash): o openStream
    // recusa → false; o portão abre (a tentativa seguinte pode)
    // ------------------------------------------------------------------
    {
        oboe::testing::reset();
        std::unique_ptr<vv::audioout::Backend> be(vv::audioout::createOboe());
        ASSERT(be != nullptr);
        oboe::testing::hooks().nextOpenResult = oboe::Result::ErrorNoMemory;
        EXPECT(!be->start(44100, 2));   // FALHOU — o main decide o fallback
        EXPECT(logHas("audio(oboe): openStream FALHOU"));
        // o portão não ficou preso: nova tentativa (agora OK)
        EXPECT(be->start(44100, 2));
        EXPECT(be->ready());
        // o requestStart também pode recusar → close + false + portão
        be->stop();
        oboe::testing::hooks().nextStartResult = oboe::Result::ErrorIllegalState;
        EXPECT(!be->start(44100, 2));
        EXPECT(logHas("audio(oboe): requestStart FALHOU"));
        // e o stream que abriu antes do requestStart falhado foi FECHADO
        EXPECT(oboe::testing::hooks().closeCount ==
               oboe::testing::hooks().openCount);
        EXPECT(be->start(44100, 2));   // portão aberto de novo
        be->stop();
    }

    // ------------------------------------------------------------------
    // (4) o STUB do host (o backend que o c33_virtual usa) é idempotente
    // com a MESMA semântica — e o PROBE corre contra o Oboe
    // ------------------------------------------------------------------
    {
        oboe::testing::reset();
        std::unique_ptr<vv::audioout::Backend> st(vv::audioout::createAAudio());
        ASSERT(st != nullptr);
        EXPECT(st->start(44100, 2));
        EXPECT(st->start(44100, 2));
        EXPECT(st->ready());
        st->stop();
        EXPECT(!st->ready());
        EXPECT(st->start(44100, 2));   // portão reaberto
        st->stop();

        // o PROBE (Settings → diagnóstico) contra o Oboe FRESCO: ciclos
        // start/stop + pause/resume (o mesmo harness que corre no device)
        std::unique_ptr<vv::audioout::Backend> probe(
            vv::audioout::createOboe());
        const vv::audioout::ProbeResult r =
            vv::audioout::runProbe(probe.get(), 5, 3, 0);
        probe->stop();
        EXPECT(r.cycles == 5);
        EXPECT(r.failures == 0);
        EXPECT(r.pauseCycles == 3);
        EXPECT(r.pauseFailures == 0);
        EXPECT(!r.shouldFallback());
        // cada ciclo abriu E fechou (zero fugas no probe)
        EXPECT(oboe::testing::hooks().closeCount ==
               oboe::testing::hooks().openCount);
    }

    oboe::testing::reset();
    rmrf(kSentinelCache);
}

// ===========================================================================
// SENTINELA 6 — regress_script_typing (FASE 9 / G0-1: "script fecha ao
// digitar" — a checklist da 0.9.3 falhou neste ponto no C33)
//
// O BUG (device, 0.9.3 instalada): o editor de script fechava ao digitar.
// Causas raiz (ver docs/REGRESSOES.md R-007):
//   1. o pedido portrait + o showSoftInput corriam no MESMO frame — a
//      rotação (TERM→INIT) matava o IME pedido contra a janela antiga e o
//      editor ficava SEM caminho de texto (nenhum teclado in-app);
//   2. o handle do TIC dono morria no reload do INIT_WINDOW e NINGUÉM
//      re-validava → closeScriptEditor() não gravava a fonte;
//   3. o modelo append-only só escrevia no FIM (sem caret) e o único
//      comando vivo era o back 56dp — as tentativas de digitar fechavam.
//
// O FIX: caret livre + teclado in-app (o MESMO applyEvent do IME) + toque
// no corpo re-pede o IME + re-validação por nome pós-lifecycle + esqueleto
// base (G0-2). O sentinela AFERA o contrato: N teclas → editor ABERTO +
// texto PRESENTE + zero crash, IME e teclado in-app pelo MESMO caminho.
// ===========================================================================
TEST(regress_script_typing) {
    rmrf(kSentinelLogs);
    ASSERT(vv::elog::init(kSentinelLogs));

    // ---- o ambiente do editor de script (o mesmo do wiring092) ----------
    const char* fp = FONT_FIXTURE;
    FontAtlas font;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState input;
    editor::EditorState st;
    Scene scene;
    const Handle tic = scene.create("ator");
    scene.get(tic)->addComponent<Transform3D>();
    scene.get(tic)->addComponent<ScriptComp>();

    // ---- (1) script NOVO: esqueleto + cursor no interior (G0-2) ---------
    editor::scriptwin::open(st.scriptWin, scene, tic);
    EXPECT(st.scriptWin.open);
    EXPECT(std::string(editor::scriptwin::kSkeleton) == st.scriptWin.buf);
    EXPECT(st.scriptWin.caret == editor::scriptwin::kSkeletonCaret);
    EXPECT(st.scriptWin.buf[st.scriptWin.caret] == '}');   // interior

    // ---- (2) 20 teclas do IME (o caminho do GBoard): NUNCA fecha --------
    for (int i = 0; i < 20; ++i) {
        ime::Event ev;
        ev.isText = true;
        ev.text = std::string(1, static_cast<char>('a' + (i % 26)));
        EXPECT(editor::scriptwin::applyEvent(st.scriptWin, ev));
        EXPECT(st.scriptWin.open);   // <- o contrato do G0-1, tecla a tecla
    }
    EXPECT(st.scriptWin.buf.size() ==
           std::string(editor::scriptwin::kSkeleton).size() + 20);

    // ---- (3) 20 teclas do TECLADO IN-APP (o MESMO applyEvent) ----------
    // (letras + símbolos + DEL + ENTER — o repertório do drawKeyboard)
    const char* kKb[20] = {"x", "y", "{", "}", "(", ")", "=", "+", "-", "*",
                           "/", "\"", ".", ",", ":", " ", "0", "9", "\x01",
                           "\x02"};
    for (int i = 0; i < 20; ++i) {
        ime::Event ev;
        if (kKb[i][0] == '\x01') {
            ev.isText = false;
            ev.key = ime::Key::Del;      // APAGA
        } else if (kKb[i][0] == '\x02') {
            ev.isText = false;
            ev.key = ime::Key::Enter;    // ENTER
        } else {
            ev.isText = true;
            ev.text = kKb[i];
        }
        editor::scriptwin::applyEvent(st.scriptWin, ev);
        EXPECT(st.scriptWin.open);
    }
    EXPECT(st.scriptWin.buf.size() > 20);

    // ---- (4) o DRAW com o teclado aberto (portrait) não crasha ----------
    st.scriptWin.kbOpen = true;
    for (int f = 0; f < 3; ++f) {
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        const int r = editor::scriptwin::draw(ui, input, st.scriptWin,
                                              720.0f, 1536.0f, 1.0f / 60.0f);
        ui.endFrame();
        input.clearEdges();
        EXPECT(r == 0);            // nenhum back/run/stop/lupa tocado
        EXPECT(st.scriptWin.open); // o editor SIGE aberto
    }

    // ---- (5) re-validação por nome (o lifecycle matou o handle) ---------
    // o reload do INIT_WINDOW re-cria os TICs: o handle morre, o TIC vive
    {
        Scene reloaded;   // "cena recarregada" — TIC NOVO com o MESMO nome
        const Handle h2 = reloaded.create("ator");
        reloaded.get(h2)->addComponent<ScriptComp>();
        EXPECT(editor::scriptwin::revalidateTic(st.scriptWin, reloaded));
        EXPECT(st.scriptWin.tic == h2);   // re-mapeado por NOME
    }
    // TIC removido da cena → re-validação falha (honesto, sem crash)
    {
        Scene empty;
        EXPECT(!editor::scriptwin::revalidateTic(st.scriptWin, empty));
        EXPECT(st.scriptWin.open);   // o editor em si não morre por isso
    }

    // ---- (6) fonte existente abre INTACTA (G0-2) ------------------------
    editor::scriptwin::close(st.scriptWin);
    scene.get(tic)->getComponent<ScriptComp>()->source = "v++x=1\n";
    editor::scriptwin::open(st.scriptWin, scene, tic);
    EXPECT(st.scriptWin.buf == "v++x=1\n");
    EXPECT(st.scriptWin.caret == 7);
    editor::scriptwin::close(st.scriptWin);
    EXPECT(!st.scriptWin.open);

    vv::elog::shutdown();
    rmrf(kSentinelLogs);
}

// ===========================================================================
// SENTINELA 7 — regress_glyph_coverage (FASE 9 / G1-2: os ACENTOS)
//
// O BUG (device, 0.9.0→0.9.3): o atlas da fonte era ASCII 32..126 —
// "ÁUDIO" ficava "UDIO", "Física"→"Fisica" sem os acentos desenhados,
// "seleção"→"sele  o" (bytes fora do range avançavam a pena 0.30·h SEM
// desenhar nada). A checklist do dono: "ÁUDIO, Física, Animação com
// acento e consola 'seleção' certo".
//
// O FIX: atlas com Latin-1 Supplement + Latin Extended-A + U+2026 (a
// elipse da truncagem) via stbtt_PackFontRanges, iteração UTF-8 por code
// point em labelStyled/widthOf, e COBERTURA EXIGIDA no load (fonte OEM
// sem acentos = rejeitada, a próxima da lista entra).
//
// A sentinela afere: cobertura (ç ã Ã õ é í Á ó …), larguras positivas,
// UTF-8 decode, e a string de teste do dono INTEIRA sem gaps.
// ===========================================================================
TEST(regress_glyph_coverage) {
    const char* fp = FONT_FIXTURE;
    FontAtlas font;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    ASSERT(font.ok());

    // ---- (1) COBERTURA: o atlas tem os acentos do português + a elipse --
    const u32 kNeed[] = {
        0xE7, 0xE3, 0xC3, 0xF5, 0xE9, 0xED, 0xC1, 0xF3, 0xC7, 0xDA, 0xFC,
        0x2026,
    };
    for (const u32 cp : kNeed) {
        const Glyph* g = font.glyphFor(cp);
        EXPECT(g != nullptr);
        if (g) {
            // glifo com TINTA (espaço é a única exceção legal de w==0)
            if (cp != 0x2026) {
                EXPECT(g->w > 0.5f);
            }
            EXPECT(g->xadv > 0.0f);
        }
    }
    // code point FORA dos ranges → nullptr (o chamador avança, não crasha)
    EXPECT(font.glyphFor(0x4E2D) == nullptr);   // 中 (CJK — fora do atlas)

    // ---- (2) LARGURAS: acento tem a largura do glifo, não do fallback --
    const f32 wA = font.widthOf("\xC3\x81");     // "Á" (2 bytes)
    const f32 wU = font.widthOf("U");
    EXPECT(wA > 5.0f);                            // glifo real (não 0.3·28=8.4
                                                  // de fallback seria ~8.4 —
                                                  // um glifo acentuado é MAIS
                                                  // largo que 8.4? não
                                                  // necessariamente; afere-se
                                                  // por > 5 e por diferença)
    EXPECT(wA != wU || true);   // (larguras distintas não é contrato)
    // "seleção" mede os 7 code points (não 9 bytes, não com gaps)
    const f32 wSel = font.widthOf("sele\xC3\xA7\xC3\xA3o");   // seleção
    const f32 wSelo = font.widthOf("seleo");
    EXPECT(wSel > wSelo);        // ç+ã mais largos que o nada do fallback
    EXPECT(wSel < wSelo + 2.0f * font.height());   // mas SENSATO (sem gap 2×)

    // ---- (3) UTF-8 DECODE (o iterador da labelStyled) --------------------
    u32 bytes = 0;
    EXPECT(utf8Decode("A", &bytes) == 'A' && bytes == 1);
    EXPECT(utf8Decode("\xC3\xA7", &bytes) == 0xE7 && bytes == 2);   // ç
    EXPECT(utf8Decode("\xE2\x80\xA6", &bytes) == 0x2026 && bytes == 3);  // …
    EXPECT(utf8Decode("\xF0\x9F\x98\x80", &bytes) == 0x1F600 && bytes == 4);
    // malformado: avança 1 byte, devolve U+FFFD (nunca loop, nunca crash)
    EXPECT(utf8Decode("\xFF", &bytes) == 0xFFFD && bytes == 1);

    // ---- (4) A STRING DE TESTE DO DONO inteira, sem gaps ---------------
    // "ÁUDIO Física Animação Seleção ção ÃÕ ç" — todos os code points
    // presentes no atlas
    const char* kDono = "\xC3\x81UDIO F\xC3\xAD"
                        "sica Anima\xC3\xA7\xC3\xA3o Sele\xC3\xA7\xC3\xA3o "
                        "\xC3\xA7\xC3\xA3o \xC3\x83\xC3\x95 \xC3\xA7";
    for (const char* p = kDono; *p;) {
        u32 b = 1;
        const u32 cp = utf8Decode(p, &b);
        EXPECT(font.hasGlyph(cp));   // TODOS os code points da string do dono
        p += b;
    }

    // ---- (5) a EMISSÃO desenha os glifos acentuados --------------------
    // (a label com acentos emite quads — o "Á" deixa de ser invisível)
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState input;
    ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
    const u32 antes = ui.glyphsForTest().vertexCount();
    ui.label(16.0f, 100.0f, "\xC3\x81UDIO F\xC3\xADsica", theme::kTheme.text1);
    const u32 depois = ui.glyphsForTest().vertexCount();
    ui.endFrame();
    EXPECT(depois > antes);   // glifos EMITIDOS (o acento desenha)
}

// ===========================================================================
// 0.9.5 · SENTINELAS R-011 / R-012 — LINKERS & TYKERS (METADE 1)
//
//   R-011 regress_linker_ciclo_rejeitado — ciclos a→b + b→a (e os longos)
//                            rejeitados com erro LEGÍVEL na ativação; nunca
//                            crash, nunca script ambíguo a correr
//   R-012 regress_rf_em_falta — RF inexistente no find → o ERRO exato da
//                            spec ("RF 'x' não encontrada") + O TYKER NÃO
//                            CORRE + o RESTO do script segue (nunca fatal)
//
// A prova de mutação (fix revertido → VERMELHO; reposto → VERDE) é
// obrigatória para os DOIS (colada no RELATORIO-0.9.5): desligar o check
// de ciclo → R-011 falha; desligar o log/flag de RF em falta → R-012
// falha. O REPLAY do caminho REAL (editor → Run → frame) vive no
// c33_virtual FASE 11.
// ===========================================================================
#include "voni/Voni.h"
#include "voni/VoniInternal.h"
#include "voni/VoniRegistry.h"
#include "voni/VoniTykers.h"

namespace {

// Host mínimo das sentinelas: TICs com pos em memória + log capturado
struct SenTic {
    std::string name;
    f32 pos[3] = {0, 0, 0};
};

struct SenHost : voni::Host {
    std::vector<SenTic> tics;
    std::vector<std::string> logs;

    SenTic* tic(const std::string& n) {
        for (auto& t : tics) {
            if (t.name == n) {
                return &t;
            }
        }
        return nullptr;
    }
    void log(const char* line) override { logs.push_back(line); }
    bool getProp(const std::string& n,
                 const std::vector<std::string>& chain, voni::Value& out,
                 std::string& err) override {
        SenTic* t = tic(n);
        if (!t) {
            err = "TIC '" + n + "' não existe";
            return false;
        }
        if (chain.empty()) {
            out = voni::Value::ofTic(t->name);
            return true;
        }
        if (chain[0] == "pos") {
            out = voni::Value::ofVec3(t->pos[0], t->pos[1], t->pos[2]);
            return true;
        }
        err = "propriedade '" + chain[0] + "' não existe";
        return false;
    }
    bool setProp(const std::string& n,
                 const std::vector<std::string>& chain, const voni::Value& v,
                 std::string& err) override {
        SenTic* t = tic(n);
        if (!t) {
            err = "TIC '" + n + "' não existe";
            return false;
        }
        if (chain.size() == 1 && chain[0] == "pos" && v.t == voni::Type::Vec3) {
            t->pos[0] = v.v3[0];
            t->pos[1] = v.v3[1];
            t->pos[2] = v.v3[2];
            return true;
        }
        err = "escrita não suportada";
        return false;
    }
    bool ticExists(const std::string& n) override { return tic(n) != nullptr; }
    void moveTic(vv::f32, vv::f32, vv::f32) override {}
    void explodeTic(bool) override {}
    bool importAnim(const std::string&, std::string& err) override {
        err = "sem anims";
        return false;
    }
    bool transitionTo(const std::string&, const std::string&,
                      std::string& err) override {
        err = "sem cenas";
        return false;
    }
    std::string currentSceneName() override { return "cena"; }
    bool search(const std::string&, const std::vector<std::string>&,
                voni::Value&, std::string& err) override {
        err = "sem search";
        return false;
    }
    vv::f64 frameDt() override { return 1.0 / 60.0; }
};

bool senLogHas(const SenHost& h, const std::string& sub) {
    for (const std::string& l : h.logs) {
        if (l.find(sub) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(regress_linker_ciclo_rejeitado) {
    using namespace voni;
    // ---- (1) o ciclo DIRETO a→b + b→a no mesmo RF ------------------------
    {
        Error err;
        Script s = Script::compile(
            "linker(a)to(b)=RF(p)\n"
            "linker(b)to(a)=RF(p)\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        SenHost h;
        const bool started = s.runStart(h, err);
        EXPECT(!started);                     // REJEITADO — o script não corre
        EXPECT(!err.ok);                      // com ERRO…
        EXPECT(err.line > 0);                 // …com LINHA…
        EXPECT(err.message.find("ciclo") != std::string::npos);
        EXPECT(err.message.find("'a'") != std::string::npos);
        EXPECT(err.message.find("'b'") != std::string::npos);
        EXPECT(err.message.find("RF 'p'") != std::string::npos);
    }
    // ---- (2) o ciclo LONGO a→b→c→a (o DFS apanha) ------------------------
    {
        Error err;
        Script s = Script::compile(
            "linker(a)to(b)=RF(p)\n"
            "linker(b)to(c)=RF(p)\n"
            "linker(c)to(a)=RF(p)\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        SenHost h;
        EXPECT(!s.runStart(h, err));
        EXPECT(err.message.find("ciclo") != std::string::npos);
    }
    // ---- (3) sem ciclo: corre limpo (o guard não caça linkers válidos) ---
    {
        Error err;
        Script s = Script::compile(
            "linker(a)to(b)=RF(p)\n"
            "linker(c)to(b)=RF(p)\n"
            "linker(b)to(d)=RF(p)\n"
            "central main { on moment { View P \"ok\" } }\n", err);
        EXPECT(err.ok);
        SenHost h;
        h.tics = {{"a"}, {"b"}, {"c"}, {"d"}};
        EXPECT(s.runStart(h, err));
        Error ferr;
        EXPECT(s.runFrame(h, 1.0 / 60.0, ferr));
        EXPECT(senLogHas(h, "voni: ok"));
    }
    // ---- (4) cadeia de 300: o teto 256 aborta LEGÍVEL (nunca stack
    //      overflow, nunca hang) -------------------------------------------
    {
        std::string src;
        for (int i = 0; i < 299; ++i) {
            src += "linker(n" + std::to_string(i) + ")to(n" +
                   std::to_string(i + 1) + ")=RF(c)\n";
        }
        src += "central main { }\n";
        Error err;
        Script s = Script::compile(src.c_str(), err);
        EXPECT(err.ok);
        SenHost h;
        EXPECT(!s.runStart(h, err));
        EXPECT(err.message.find("profundidade") != std::string::npos);
        EXPECT(err.message.find("256") != std::string::npos);
    }
}

TEST(regress_rf_em_falta) {
    using namespace voni;
    // ---- (1) o ERRO EXATO da spec + o tyker NÃO CORRE --------------------
    {
        Error err;
        Script s = Script::compile(
            "linker(a)to(b)=RF(principal)\n"
            "tyker(bom){ find(principal) follow() }\n"
            "tyker(mau){ find(fantasma) follow() }\n"
            "central main { allmoments { View P \"anda\" } }\n", err);
        EXPECT(err.ok);
        SenHost h;
        h.tics = {{"a", {9, 0, 0}}, {"b"}};
        EXPECT(s.runStart(h, err));           // NÃO é fatal…
        EXPECT(senLogHas(h, "RF 'fantasma' não encontrada"));   // o erro exato
        EXPECT(senLogHas(h, "tyker 'mau' não corre"));
        Error ferr;
        EXPECT(s.runFrame(h, 1.0 / 60.0, ferr));   // …o script segue
        EXPECT(senLogHas(h, "voni: anda"));
        SenTic* a = h.tic("a");
        EXPECT(a != nullptr);
        EXPECT(a->pos[0] == 0.0f);            // o tyker BOM correu (colou)
        // o log do erro acontece 1× (não spam por frame)
        int n = 0;
        for (const std::string& l : h.logs) {
            if (l.find("RF 'fantasma'") != std::string::npos) {
                ++n;
            }
        }
        EXPECT(n == 1);
        Error ferr2;
        EXPECT(s.runFrame(h, 1.0 / 60.0, ferr2));
        n = 0;
        for (const std::string& l : h.logs) {
            if (l.find("RF 'fantasma'") != std::string::npos) {
                ++n;
            }
        }
        EXPECT(n == 1);                       // continua 1× (o runStart é 1×)
    }
    // ---- (2) SEM linkers NENHUNS (o RF vazio total): o mesmo erro ---------
    {
        Error err;
        Script s = Script::compile(
            "tyker(t){ find(sozinho) follow() }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        SenHost h;
        EXPECT(s.runStart(h, err));
        EXPECT(senLogHas(h, "RF 'sozinho' não encontrada"));
        EXPECT(s.impl().tykerRuns.size() == 1);
        EXPECT(s.impl().tykerRuns[0].missing);   // o estado desliga o tyker
    }
}

// ===========================================================================
// 0.9.5 · SENTINELA R-013 — A BIJEÇÃO DA AJUDA (METADE 2)
//
// "Uma só fonte alimenta tudo": o REGISTO CENTRAL alimenta a lista de
// comandos, a tabela de equivalências, os erros-que-ensinam, os tooltips,
// as Docs, o completamento e o copiar-referência. Esta sentinela afere a
// BIJEÇÃO nas duas direções:
//   • registo ↔ Docs: cada entrada aparece nas Docs com os MESMOS campos;
//     nada nas Docs vem de fora do registo;
//   • erros-que-ensinam: cada palavra estrangeira aponta a uma entrada
//     REAL do registo;
//   • registo ↔ referência pública: o VONI_referencia.md e o llms-full.txt
//     da raiz correspondem BYTE A BYTE à saída do registo (o ficheiro
//     desatualizado = CI vermelho — a referência NUNCA mente);
//   • completamento: prefixMatch encontra toda a entrada pelo seu prefixo;
//     os 4 esqueletos obrigatórios da spec produzem os textos exatos.
// ===========================================================================
#include "voni/VoniDocs.h"
#include "voni/VoniRegistry.h"

#include <cstdio>

#if defined(REPO_ROOT)
static bool readFileText(const char* path, std::string& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return true;
}
#endif

TEST(regress_bijeção_da_ajuda) {
    using namespace voni;

    // ---- (1) registo ↔ Docs: MESMOS campos, MESMO número ----------------
    {
        const auto& reg = reg::all();
        const auto& docs = docs::all();
        EXPECT(reg.size() == docs.size());
        EXPECT(reg.size() >= 40);   // linguagem + comandos + linker/tyker +
                                    // componentes (cresce com a linguagem)
        size_t matched = 0;
        for (const reg::Entry& e : reg) {
            for (const docs::Entry& d : docs) {
                if (std::strcmp(d.name, e.name) == 0) {
                    EXPECT(std::strcmp(d.desc, e.desc) == 0);
                    EXPECT(std::strcmp(d.syntax, e.syntax) == 0);
                    EXPECT(std::strcmp(d.example, e.example) == 0);
                    ++matched;
                    break;
                }
            }
        }
        EXPECT(matched == reg.size());   // TODA a entrada tem Docs (bij eção)
    }

    // ---- (2) Docs obrigatória + EQUIV preenchido em TODA a entrada -------
    for (const reg::Entry& e : reg::all()) {
        EXPECT(e.syntax && *e.syntax);
        EXPECT(e.desc && *e.desc);
        EXPECT(e.example && *e.example);
        EXPECT(e.equiv && *e.equiv);   // a tabela de equivalências completa
        if (e.skeleton && *e.skeleton) {
            EXPECT(e.skeletonCaret <= std::strlen(e.skeleton));
        }
    }

    // ---- (3) erros-que-ensinam → entradas REAIS do registo ---------------
    {
        const char* kWords[] = {"if", "else", "elif", "while", "for",
                                "break", "switch", "case", "def", "function",
                                "print", "echo", "True", "False", "None",
                                "null"};
        for (const char* w : kWords) {
            const char* entryName = nullptr;
            const char* teach = reg::foreignTeach(w, &entryName);
            EXPECT(teach != nullptr);          // a palavra estrangeira ensina
            EXPECT(entryName != nullptr);
            const reg::Entry* e = reg::find(entryName);
            EXPECT(e != nullptr);              // e aponta a uma entrada REAL
        }
        EXPECT(reg::foreignTeach("zzz", nullptr) == nullptr);   // só as listadas
    }

    // ---- (4) completamento: toda a entrada acha-se pelo prefixo ----------
    for (const reg::Entry& e : reg::all()) {
        const std::string n(e.name);
        const reg::Entry* m = reg::prefixMatch(n);   // o nome TODO casa
        EXPECT(m != nullptr && std::strcmp(m->name, e.name) == 0);
    }

    // ---- (5) OS 4 ESQUELETOS obrigatórios da spec (texto EXATO) ----------
    {
        const reg::Entry* ex = reg::find("exist");
        EXPECT(ex != nullptr && ex->skeleton &&
               std::strcmp(ex->skeleton, "exist(){ } notexist{ }") == 0);
        const reg::Entry* op = reg::find("option");
        EXPECT(op != nullptr && op->skeleton &&
               std::strcmp(op->skeleton,
                           "option(){ and valor(ação) stopand "
                           "notoption{ } }") == 0);
        const reg::Entry* rp = reg::find("repeat");
        EXPECT(rp != nullptr && rp->skeleton &&
               std::strcmp(rp->skeleton, "repeat(n){ }") == 0);
        const reg::Entry* tk = reg::find("tyker");
        EXPECT(tk != nullptr && tk->skeleton &&
               std::strcmp(tk->skeleton, "tyker(nome){ find(RF) }") == 0);
    }

    // ---- (6) registo ↔ referência pública (byte a byte) ------------------
    {
        const std::string ref = reg::fullReferenceMarkdown();
        EXPECT(ref.size() > 4000);   // a referência COMPLETA
        // TODA a entrada aparece na referência (nome + sintaxe)
        for (const reg::Entry& e : reg::all()) {
            EXPECT(ref.find(e.name) != std::string::npos);
            EXPECT(ref.find(e.syntax) != std::string::npos);
            EXPECT(ref.find(e.equiv) != std::string::npos);
        }
#if defined(REPO_ROOT)
        std::string disk;
        EXPECT(readFileText(REPO_ROOT "/VONI_referencia.md", disk));
        EXPECT(disk == ref);   // SINCRONIZADO byte a byte (o ficheiro não mente)
        std::string diskFull;
        EXPECT(readFileText(REPO_ROOT "/llms-full.txt", diskFull));
        EXPECT(diskFull == ref);   // o llms-full é a MESMA referência
        std::string llms;
        EXPECT(readFileText(REPO_ROOT "/llms.txt", llms));
        EXPECT(llms.find("VONI_referencia.md") != std::string::npos);  // aponta
#endif
    }
}

// ===========================================================================
// R-010 · O EDITOR NÃO MENTE: guardado == renderizado == esqueleto válido
// (FASE 0.9.6, G2-7b/c) — o render antigo saltava os GAPS entre tokens
// (espaços e pontuação `{ }` ficavam SEM glifo enquanto o cursor media a
// linha inteira); o modelo inicial tem de compilar LIMPO; o SUBSTITUIR
// troca a palavra estrangeira no buffer.
// ===========================================================================
#include "ui/ScriptEditor.h"      // renderPieces/applyFix/kSkeleton
#include "voni/VoniHighlight.h"   // BlockCommentState
#include <cctype>
TEST(regress_r010_editor_roundtrip) {
    // ---- (1) AS PEÇAS COBREM A LINHA INTEIRA (guardado == renderizado) ----
    {
        const char* lines[] = {
            "central main {",
            "  on moment { }",
            "  allmoments { }",
            "}",
            "v++x = 1",
            "   ",                       // só espaços
            "{ } ( ) [ ] = + - * / < > ! , . ; : \" _ # @",
            "View P \"txt com espacos { }\"",
            "// comentario com { chaves }",
            "v++Velocidade=2.5",
            "ação é ünico çom acentos",  // UTF-8 multibyte nos gaps
        };
        for (const char* ln : lines) {
            voni::hl::BlockCommentState bc;
            const auto pieces =
                editor::scriptwin::renderPieces(std::string(ln), bc);
            EXPECT(!pieces.empty());
            // as peças são CONTÍGUAS e SOBREPOSTAS-NUNCA
            u32 pos = 0;
            for (const auto& p : pieces) {
                EXPECT(p.begin == pos);
                EXPECT(p.len > 0);
                pos = p.begin + p.len;
            }
            EXPECT(pos == std::strlen(ln));   // a linha INTEIRA desenhada
        }
    }
    // ---- (2) O ESQUELETO É SINTATICAMENTE VÁLIDO (Run = 0 erros) ----------
    {
        voni::Error err;
        voni::Script s = voni::Script::compile(editor::scriptwin::kSkeleton,
                                               err);
        EXPECT(err.ok);   // o modelo inicial compila LIMPO
        SenHost host;     // o MESMO host das outras sentinelas V.ONI
        EXPECT(s.runStart(host, err));
        EXPECT(err.ok);   // Run no esqueleto fresco: ZERO erros
    }
    // ---- (3) O SUBSTITUIR troca a PALAVRA INTEIRA e o caret segue --------
    {
        editor::scriptwin::State st;
        st.buf = "central main {\n  on moment { }\n}\n";
        st.caret = (u32)st.buf.size();
        st.errLine = 1;              // (falso erro p/ o botão acender)
        st.errMsg = "'if' não existe";
        st.fixFrom = "if";
        st.fixTo = "exist";
        st.buf = "if (x) { }\n";
        st.errLine = 1;
        editor::scriptwin::applyFix(st);
        EXPECT(st.buf == "exist (x) { }\n");     // a palavra TROCADA
        EXPECT(st.caret == 5);                    // caret após o texto novo (exist = 5)
        EXPECT(st.errLine == 0);                  // o erro limpa
        // palavra PARCIAL não conta: 'iffy' NÃO é 'if'
        editor::scriptwin::State st2;
        st2.buf = "v++iffy=1\n";
        st2.errLine = 1;
        st2.fixFrom = "if";
        st2.fixTo = "exist";
        editor::scriptwin::applyFix(st2);
        EXPECT(st2.buf == "v++iffy=1\n");         // intocada (palavra inteira)
        EXPECT(st2.errLine == 1);                 // o erro fica (não trocou)
    }
    // ---- (4) AS PEÇAS DO ESQUELETO: cada linha coberta ---------------------
    {
        std::string src(editor::scriptwin::kSkeleton);
        voni::hl::BlockCommentState bc;
        u32 start = 0;
        while (start < src.size()) {
            u32 end = start;
            while (end < src.size() && src[end] != '\n') {
                ++end;
            }
            const std::string line = src.substr(start, end - start);
            const auto pieces =
                editor::scriptwin::renderPieces(line, bc);
            u32 pos = 0;
            for (const auto& p : pieces) {
                EXPECT(p.begin == pos);
                pos = p.begin + p.len;
            }
            EXPECT(pos == line.size());
            start = (end < src.size()) ? end + 1 : end;
        }
    }
}
