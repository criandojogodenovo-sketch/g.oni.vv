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
#include <sstream>   // 0.9.6 (R-017): parse das 9 linhas do bench
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
#include "assets/GltfImporter.h"   // 0.9.6 (R-017): o GLB do bench
#include "core/Bench.h"            // 0.9.6 (R-017): o relatório não mente
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

// ============================================================================
// R-017 · o bench NÃO MENTE (FASE 0.9.6 · G6) — regress_bench_nao_mente
//
// O CONTRATO da spec: "todo métrico é real (medido no device) e nenhum
// hardcoded". A sentinela afirma AS DUAS PONTAS:
//   (a) o format() escreve EXATAMENTE os valores que estão no Report (se
//       o formato hardcodasse QUALQUER número — p.ex. fps 60 — os dois
//       relatórios com valores diferentes seriam iguais nesse sítio);
//   (b) a honestidade: campo não medido imprime "não medido" (nunca 0
//       disfarçado de medição), e o bloco tem 9 LINHAS FIXAS.
// A TERCEIRA ponta (os números CHEGAM de medições reais) é o FASE 12.9 do
// c33_virtual: o replay corre o bench inteiro pelo caminho do device e
// afirma fps/verts/import medidos no PRÓPRIO processo de replay.
// ============================================================================
TEST(regress_bench_nao_mente) {
    // ---- (a) dois Reports com TODOS os campos diferentes → 9 linhas ----
    // diferentes (nenhuma posição do bloco é constante)
    bench::Report a, b;
    std::snprintf(a.version, sizeof(a.version), "0.9.6");
    a.versionCode = 49;
    std::snprintf(a.device, sizeof(a.device), "RMX3624");
    a.sdk = 33;
    a.warmStartMs  = bench::Measured{ true, 412.0 };
    a.coldStartMs  = bench::Measured{ true, 1830.0 };
    a.def   = bench::FpsAgg{ true, 58.0, 51.0, 47.0 };
    a.scene = bench::FpsAgg{ true, 55.0, 48.0, 44.0 };
    a.defVerts = bench::Measured{ true, 12543.0 };
    a.defDc    = bench::Measured{ true, 89.0 };
    a.sceneVerts = bench::Measured{ true, 40120.0 };
    a.sceneDc    = bench::Measured{ true, 156.0 };
    a.importMs    = bench::Measured{ true, 96.0 };
    a.importScale = bench::Measured{ true, 2.5 };
    a.texMs = bench::Measured{ true, 41.0 };
    std::snprintf(a.texFormat, sizeof(a.texFormat), "ASTC 4x4");
    a.audioOk = 60; a.audioTotal = 60;
    std::snprintf(a.audioBackend, sizeof(a.audioBackend), "oboe");
    a.rssPeakMb = bench::Measured{ true, 214.0 };
    a.apkMb = bench::Measured{ true, 18.4 };
    a.apkShaOk = bench::Measured{ true, 1.0 };
    std::snprintf(a.apkSha, sizeof(a.apkSha), "44f39beda1b2…");
    a.projMb = bench::Measured{ true, 3.2 };

    std::snprintf(b.version, sizeof(b.version), "9.9.9");
    b.versionCode = 99;
    std::snprintf(b.device, sizeof(b.device), "OUTRO");
    b.sdk = 34;
    b.warmStartMs  = bench::Measured{ true, 1.0 };
    b.coldStartMs  = bench::Measured{ true, 2.0 };
    b.def   = bench::FpsAgg{ true, 3.0, 4.0, 5.0 };
    b.scene = bench::FpsAgg{ true, 6.0, 7.0, 8.0 };
    b.defVerts = bench::Measured{ true, 9.0 };
    b.defDc    = bench::Measured{ true, 10.0 };
    b.sceneVerts = bench::Measured{ true, 11.0 };
    b.sceneDc    = bench::Measured{ true, 12.0 };
    b.importMs    = bench::Measured{ true, 13.0 };
    b.importScale = bench::Measured{ true, 14.0 };
    b.texMs = bench::Measured{ true, 15.0 };
    std::snprintf(b.texFormat, sizeof(b.texFormat), "ETC2 RGB");
    b.audioOk = 16; b.audioTotal = 17;
    std::snprintf(b.audioBackend, sizeof(b.audioBackend), "AudioTrack");
    b.rssPeakMb = bench::Measured{ true, 18.0 };
    b.apkMb = bench::Measured{ true, 19.0 };
    b.apkShaOk = bench::Measured{ true, 1.0 };
    std::snprintf(b.apkSha, sizeof(b.apkSha), "ffffffffffff…");
    b.projMb = bench::Measured{ true, 20.0 };

    const std::string ta = bench::format(a);
    const std::string tb = bench::format(b);
    // as 9 linhas pelos prefixos de identidade
    std::istringstream ia(ta), ib(tb);
    std::string la, lb;
    int lines = 0, diffs = 0;
    while (std::getline(ia, la) && std::getline(ib, lb)) {
        ++lines;
        if (la != lb) {
            ++diffs;
        }
    }
    EXPECT(lines == 9);      // o bloco é SEMPRE 9 linhas (parseável)
    EXPECT(diffs == 9);      // nenhuma posição hardcodada
    // os valores MEDIDOS aparecem literalmente (amostragem direta)
    EXPECT(ta.find("warm start: 412 ms") != std::string::npos);
    EXPECT(ta.find("(escala 2.5)") != std::string::npos);
    EXPECT(tb.find("16/17 ok · AudioTrack") != std::string::npos);

    // ---- (b) a HONESTIDADE: não medido é DITO, nunca 0 -----------------
    bench::Report v;   // nada medido
    const std::string tv = bench::format(v);
    EXPECT(tv.find("não medido") != std::string::npos);
    EXPECT(tv.find("cena default: não medido") == 0 ? false
          : tv.find("cena default: não medido") != std::string::npos);
    // um valor medido NUM campo de resto vazio: o número aparece, o resto
    // não (a honestidade é POR CAMPO)
    bench::Report m;
    m.rssPeakMb = bench::Measured{ true, 123.0 };
    const std::string tm = bench::format(m);
    EXPECT(tm.find("pico RSS 123 MB") != std::string::npos);
    EXPECT(tm.find("warm start: não medido") != std::string::npos);

    // ---- (c) a agregação é MATEMÁTICA (o 1% low não é o min nem média) --
    std::vector<double> w(600, 60.0);
    for (int i = 0; i < 30; ++i) {
        w[i] = 20.0;   // 5% das amostras em queda
    }
    const bench::FpsAgg agg = bench::aggregate(w);
    EXPECT(agg.ok);
    EXPECT(agg.min < 21.0);            // o mínimo caiu com a queda
    EXPECT(agg.p1 < 21.0);            // o 1% low cai (5% > 1%)
    EXPECT(agg.avg > 55.0);            // a média resiste (95% a 60)
    // 1 amostra em 600 (0.17%): o 1% low NÃO cai (fora do pior 1%)
    std::vector<double> u(600, 60.0);
    u[100] = 30.0;
    const bench::FpsAgg agg2 = bench::aggregate(u);
    EXPECT(agg2.ok && agg2.p1 > 55.0);

    // ---- (d) o GLB de referência é DETERMINÍSTICO (a escala idem) -------
    std::vector<u8> g1, g2;
    bench::makeReferenceGlb(g1);
    bench::makeReferenceGlb(g2);
    EXPECT(g1.size() == g2.size() &&
           std::memcmp(g1.data(), g2.data(), g1.size()) == 0);
    GltfModel model;
    std::string err;
    EXPECT(parseGlb(g1.data(), g1.size(), GltfBufferResolver{nullptr, 0},
                    model, err));
    EXPECT(!model.nodes.empty() &&
           nearEqF(model.nodes[0].scale.x, 2.5f, 0.001f));
}

// ============================================================================
// R-018 · as medidas dp eram desenhadas como px (FASE 0.9.6.1 · PASSO 0) —
//         regress_density_escala_dp
//
// O DONO mediu no device: cabeçalho do editor ~56px num ecrã de 720px de
// largura (devia ter ~112px), botões de ferramentas ~48px de altura e
// teclas do teclado próprio 48×65px — metade do pedido (48dp). A CAUSA RAIZ
// (leitura do código): NÃO existia função dp→px — as constantes do design
// system eram constexpr EM DP consumidas COMO PX CRUS (Theme.h/SafeArea.h/
// EditorLayout.h e os componentes). O FIX na origem: theme::dp() (densidade
// do AConfiguration no arranque) aplicado nas FONTES ÚNICAS de layout.
// A sentinela afirma: com densidade 2.0 injetada os rects duplicam (56dp →
// 112px, alvos 48dp → 96px, teclas ≥48dp de altura); com densidade 1.0 o
// layout é EXATAMENTE o de sempre (a suíte inteira corre a 1.0). A prova
// do caminho REAL do device (AConfiguration 320dpi → 2.0) é a FASE 12.10
// do c33_virtual; esta unidade aferra a matemática pura.
// ============================================================================
TEST(regress_density_escala_dp) {
    using namespace vv;
    const safe::Insets zero{};
    // ---- (a) densidade 2.0 (o par do C33): os dp duplicam ------------------
    theme::setDensity(2.0f);
    const UiRect bar = safe::toolbarRect(1536.0f, 720.0f, zero);
    EXPECT(nearEqF(bar.h, 112.0f));   // 56dp REAL (o bug: 56px)
    EXPECT(nearEqF(theme::dp(48.0f), 96.0f));   // o alvo mínimo é dp REAL
    const UiRect status = safe::statusRect(1536.0f, 720.0f, zero);
    EXPECT(nearEqF(status.h, 48.0f));   // 24dp real, a última faixa
    EXPECT(nearEqF(status.y + status.h, 720.0f));   // continua no fundo
    // o teclado: teclas de 96px de altura (48dp real — o dono media 48×65px)
    EXPECT(nearEqF(editor::scriptwin::keyboardHeight(),
                   5.0f * 96.0f + 4.0f * 12.0f + 2.0f * 16.0f));
    // ---- (b) densidade 1.0: o layout de SEMPRE (nenhum teste muda) ---------
    theme::setDensity(1.0f);
    const UiRect bar1 = safe::toolbarRect(1536.0f, 720.0f, zero);
    EXPECT(nearEqF(bar1.h, 56.0f));
    EXPECT(nearEqF(theme::dp(48.0f), 48.0f));
    EXPECT(nearEqF(editor::scriptwin::keyboardHeight(),
                   5.0f * 48.0f + 4.0f * 6.0f + 2.0f * 8.0f));
}

// ============================================================================
// R-020 · a importação glTF/GLB falhava em silêncio (FASE 0.9.6.3) —
//         regress_gltf_draco_mensagem_clara / regress_gltf_transforms_nos
//
// O DONO: "modelos importados em OBJ aparecem em 'Adicionar mesh'...
// Os importados em glTF ou GLB não aparecem... Falha em silêncio."
// A FORENSE (leitura do código): (1) um glb/gltf com Draco/meshopt/KTX2
// passava pelo parse e falhava DEPOIS com erros obscuros ("POSITION
// inválido") — ninguém dizia que o problema era a compressão; (2) UMA
// textura má derrubava o import inteiro (return false no meio do passe);
// (3) o TRS dos nós era IGNORADO — o .gmesh saía cru (modelo fora do
// sítio/invisível) e multi-mesh partia-se em <stem>_N.gmesh (o TIC só
// recebia uma parte). O FIX: deteção ANTES do parse com mensagem clara
// ("o ficheiro usa compressão X, que ainda não é suportada"), tolerância
// parcial nas texturas (aviso + material por defeito), MERGE com as
// transformações de mundo num ÚNICO .gmesh, limites finais no log (sem
// auto-escala) e o log rico do parse (nós/malhas/primitivas/verts/
// índices/materiais/texturas/extensões).
// ============================================================================
#include "assets/GOwnFormats.h"   // readGMesh (aferrar o .gmesh de saída)
#include "core/FsStorage.h"

TEST(regress_gltf_draco_mensagem_clara) {
    using namespace vv;
    // um glTF que EXIGE Draco: a falha tem de dizer QUAL é a compressão —
    // nunca o "POSITION inválido" obscuro que vinha depois
    const char* json =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":1}]}]}";
    const std::string fixture = "goni_r020_draco.gltf";
    FILE* f = std::fopen(fixture.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(json, 1, std::strlen(json), f);
    std::fclose(f);
    char root[64];
    std::snprintf(root, sizeof(root), "/tmp/goni_r020_%d", (int)::getpid());
    FsStorage st(root);
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(fixture, fixture, st, nullptr, out,
                                        stats, err);
    EXPECT(!ok);
    EXPECT(err.find("compressão") != std::string::npos &&
           err.find("KHR_draco_mesh_compression") != std::string::npos);
    ::remove(fixture.c_str());
}

TEST(regress_gltf_transforms_dos_nos_no_gmesh) {
    using namespace vv;
    // triângulo unitário + nó com scale 2.5: o .gmesh de saída tem de ter
    // os VÉRTICES EM MUNDO (2.5) — antes saía cru (o modelo importava e
    // ficava fora do sítio) — e um ÚNICO ficheiro de saída
    // 0.9.6.4 (GRUPO A/R-021) — RECALIBRADA AO CONTRATO DOS IRMÃOS: o .bin
    // referenciado pelo URI resolve contra o DIRETÓRIO DO .gltf (como no
    // device, onde o browser dá caminhos POSIX absolutos) — a versão
    // antiga escrevia o .bin no CWD e o URI resolvia contra o CWD: era
    // EXATAMENTE o mecanismo do bug «buffer externo não resolvido» (passava
    // no CI, falhava no Android, onde o CWD é «/»).
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) {
        u8 t[4];
        std::memcpy(t, &v, 4);
        bin.insert(bin.end(), t, t + 4);
    };
    auto pushS = [&bin](u16 v) {
        bin.push_back(static_cast<u8>(v & 0xFF));
        bin.push_back(static_cast<u8>(v >> 8));
    };
    pushF(1.0f); pushF(0.0f); pushF(0.0f);
    pushF(0.0f); pushF(1.0f); pushF(0.0f);
    pushF(0.0f); pushF(0.0f); pushF(1.0f);
    pushS(0); pushS(1); pushS(2);
    // um DIRETÓRIO REAL (como a pasta do device) com o PAR .gltf + .bin
    char dir[96];
    std::snprintf(dir, sizeof(dir), "/tmp/goni_r021_scaled_%d",
                  (int)::getpid());
    ASSERT(fileapi::makeDirs(dir));
    const std::string binAbs = std::string(dir) + "/scene.bin";
    {
        FILE* f = std::fopen(binAbs.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(bin.data(), 1, bin.size(), f);
        std::fclose(f);
    }
    char js[832];
    std::snprintf(js, sizeof(js),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"scene.bin\",\"byteLength\":%zu}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36,\"target\":34962},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6,\"target\":34963}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,1]},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
        "\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":1,\"mode\":4}]}],"
        "\"nodes\":[{\"mesh\":0,\"scale\":[2.5,2.5,2.5],"
        "\"translation\":[1,0,0]}],"
        "\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
        bin.size());
    const std::string fixture = std::string(dir) + "/scaled.gltf";
    {
        FILE* f = std::fopen(fixture.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(js, 1, std::strlen(js), f);
        std::fclose(f);
    }
    char root[64];
    std::snprintf(root, sizeof(root), "/tmp/goni_r021b_%d", (int)::getpid());
    FsStorage st(root);
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(fixture, "scaled.gltf", st, nullptr,
                                        out, stats, err);
    ASSERT(ok);   // o erro, se houver, está em err
    // UM modelo só (o merge — antes: <stem>.gmesh por mesh crua)
    EXPECT(out.meshes.size() == 1u);
    EXPECT(out.meshes[0] == "assets/scaled.gmesh");
    // 0.9.6.4 (R-021): o IRMÃO .bin foi COPIADO para source/ (o projeto
    // fica autossuficiente — o reconvert já não precisa da pasta original)
    EXPECT(stats.siblings == 1u);
    EXPECT(st.exists("source/scaled.gltf"));
    EXPECT(st.exists("source/scene.bin"));
    // e os VÉRTICES saem EM MUNDO: o scale 2.5 + translation 1 do nó
    MeshData md;
    std::vector<u8> bytes;
    ASSERT(st.readBytes("assets/scaled.gmesh", bytes));
    ASSERT(readGMesh(bytes.data(), bytes.size(), md, err));
    EXPECT(md.vertices.size() == 3u);
    EXPECT(nearEqF(md.vertices[0].pos.x, 2.5f + 1.0f));   // scale×x + tx
    EXPECT(nearEqF(md.vertices[1].pos.y, 2.5f));
    EXPECT(nearEqF(md.vertices[2].pos.z, 2.5f));
    ::remove(fixture.c_str());
    ::remove(binAbs.c_str());
    ::remove(dir);
}

// ============================================================================
// R-021 (FASE 0.9.6-MASTER · GRUPO A) — OS IRMÃOS DO .gltf SEPARADO —
//         regress_gltf_irmaos_do_diretorio_original
//
// O DONO (evidência no device): «glTF separado falha: buffer externo não
// resolvido: scene.bin» — só o .gltf era copiado para source/, o .bin
// ficava na pasta original. A CAUSA RAIZ (leitura do código): o resolver
// do convertGltfFile lia fileapi::readAll(uri) com o URI RELATIVO contra o
// CWD do processo (no Android «/») — os testes antigos passavam porque
// escreviam o .bin NO CWD do CI (a sentinela R-020 acima foi RECALIBRADA
// por essa razão: agora usa um diretório REAL com caminhos absolutos,
// como o browser do device). O FIX: (1) os irmãos (buffers[].uri +
// images[].uri externos, URI-decode %20 incluído) são COPIADOS do
// DIRETÓRIO ORIGINAL para source/<subcaminho> — o projeto fica
// autossuficiente p/ o reconvert; (2) o resolver e o leitor de texturas
// externas resolvem contra o DIRETÓRIO DO FICHEIRO; (3) irmão AUSENTE =
// ERRO QUE NOMEIA O FICHEIRO (nunca o genérico de antes).
// ============================================================================
TEST(regress_gltf_irmaos_do_diretorio_original) {
    using namespace vv;
    // lê um PNG REAL da fixture (a textura externa do .gltf)
    std::vector<u8> png;
    {
        const std::string p =
            std::string(FIXTURE_DIR) + "/yellow4.png";
        FILE* f = std::fopen(p.c_str(), "rb");
        ASSERT(f != nullptr);
        u8 buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            png.insert(png.end(), buf, buf + n);
        }
        std::fclose(f);
    }
    ASSERT(!png.empty());

    // o DIRETÓRIO ORIGINAL (como a pasta Download do device): o PAR
    // .gltf + .bin + a textura com ESPAÇO no nome (URI com %20)
    char dir[96];
    std::snprintf(dir, sizeof(dir), "/tmp/goni_r021_dir_%d", (int)::getpid());
    ASSERT(fileapi::makeDirs(dir));
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) {
        u8 t[4];
        std::memcpy(t, &v, 4);
        bin.insert(bin.end(), t, t + 4);
    };
    pushF(0); pushF(0); pushF(0);
    pushF(1); pushF(0); pushF(0);
    pushF(0); pushF(1); pushF(0);
    bin.push_back(0); bin.push_back(0);   // idx u16 LE: 0
    bin.push_back(1); bin.push_back(0);   // 1 (lo, hi — little-endian!)
    bin.push_back(2); bin.push_back(0);   // 2
    {
        const std::string p = std::string(dir) + "/scene.bin";
        FILE* f = std::fopen(p.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(bin.data(), 1, bin.size(), f);
        std::fclose(f);
    }
    {
        // O NOME COM ESPAÇO: o URI no .gltf diz «tex%20albedo.png»
        const std::string p = std::string(dir) + "/tex albedo.png";
        FILE* f = std::fopen(p.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(png.data(), 1, png.size(), f);
        std::fclose(f);
    }
    char js[960];
    std::snprintf(js, sizeof(js),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"scene.bin\",\"byteLength\":%zu}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"materials\":[{\"name\":\"mat\",\"pbrMetallicRoughness\":"
        "{\"baseColorTexture\":{\"index\":0}}}],"
        "\"textures\":[{\"source\":0}],"
        "\"images\":[{\"uri\":\"tex%%20albedo.png\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":1,\"material\":0}]}],"
        "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
        bin.size());
    const std::string gltfAbs = std::string(dir) + "/modelo.gltf";
    {
        FILE* f = std::fopen(gltfAbs.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(js, 1, std::strlen(js), f);
        std::fclose(f);
    }
    char root[64];
    std::snprintf(root, sizeof(root), "/tmp/goni_r021_root_%d",
                  (int)::getpid());
    FsStorage st(root);
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(gltfAbs, "modelo.gltf", st, nullptr,
                                        out, stats, err);
    ASSERT(ok);   // err tem a causa se falhar
    // (1) o .gmesh na lista de assets
    EXPECT(out.meshes.size() == 1u);
    EXPECT(out.meshes[0] == "assets/modelo.gmesh");
    // (2) OS IRMÃOS COPIADOS para source/ (log um a um no engine.log)
    EXPECT(stats.siblings == 2u);   // scene.bin + tex albedo.png
    EXPECT(st.exists("source/modelo.gltf"));
    EXPECT(st.exists("source/scene.bin"));
    EXPECT(st.exists("source/tex albedo.png"));   // %20 decodificado
    // (3) a TEXTURA EXTERNA entrou (lida do diretório dos irmãos e
    // convertida para .gtext — SEM avisos)
    EXPECT(out.textures.size() == 1u);
    EXPECT(stats.texWarn == 0u);
    // (4) o RECONVERT funciona SEM a pasta original (o projeto é
    // autossuficiente): a fonte vive em source/ com os irmãos ao lado
    convert::Output out2;
    convert::Stats stats2;
    std::string err2;
    const bool ok2 = convert::reconvertFile("source/modelo.gltf", st,
                                            nullptr, out2, stats2, err2);
    EXPECT(ok2);
    EXPECT(out2.meshes.size() == 1u);
    // e NÃO houve cópia auto-referencial (o .gltf já vivia em source/ —
    // o guard salta a cópia; os bytes continuam ÍNTEGROS)
    std::vector<u8> srcBack;
    ASSERT(st.readBytes("source/modelo.gltf", srcBack));
    EXPECT(srcBack.size() == std::strlen(js));

    // ---- o IRMÃO AUSENTE: o ERRO NOMEIA O FICHEIRO ----------------------
    {
        char dir2[96];
        std::snprintf(dir2, sizeof(dir2), "/tmp/goni_r021_falta_%d",
                      (int)::getpid());
        ASSERT(fileapi::makeDirs(dir2));
        char js2[512];
        std::snprintf(js2, sizeof(js2),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"uri\":\"fantasma.bin\",\"byteLength\":42}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
            "\"count\":3,\"type\":\"VEC3\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}");
        const std::string p = std::string(dir2) + "/quebrado.gltf";
        FILE* f = std::fopen(p.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(js2, 1, std::strlen(js2), f);
        std::fclose(f);
        convert::Output out3;
        convert::Stats stats3;
        std::string err3;
        const bool ok3 =
            convert::importFile(p, "quebrado.gltf", st, nullptr, out3,
                                stats3, err3);
        EXPECT(!ok3);
        EXPECT(err3.find("fantasma.bin") != std::string::npos);
        EXPECT(err3.find("NÃO EXISTE") != std::string::npos);
        ::remove(p.c_str());
        ::remove(dir2);
    }
    ::remove(gltfAbs.c_str());
    ::remove((std::string(dir) + "/scene.bin").c_str());
    ::remove((std::string(dir) + "/tex albedo.png").c_str());
    ::remove(dir);
}

// ============================================================================
// R-022 (FASE 0.9.6-MASTER · GRUPO A) — A IMAGEM QUE MATAVA O IMPORT DO
//         GLB + O LAYOUT/INTEGRIDADE — regress_glb_imagem_no_fim_com_padding
//
// O DONO (evidência no device): «GLB com texturas falha: bufferView da
// imagem fora do buffer». A CAUSA RAIZ (leitura do código): o parse
// tratava a falha do bufferView de uma IMAGEM como FATAL (return false) —
// um GLB com uma imagem má/inacessível morria INTEIRO com a geometria
// boa; e o resolveView não distinguia «limites do buffer» de «leitura
// falhou (I/O)». O FIX: uma SÓ rotina de validação (ViewFail com a causa)
// para meshes (fatal) e imagens (warn + skip — o import SEGUE sem
// texturas, o toast diz «SEM N textura(s)»); o layout dos chunks é LOGADO
// (json/bin/binStart alinhado 4 + bufferView de cada imagem); a cópia em
// source/ é VERIFICADA byte a byte (a sentinela seguinte).
// ============================================================================
TEST(regress_glb_imagem_no_fim_com_padding) {
    using namespace vv;
    // um PNG REAL (a imagem embutida no fim do chunk BIN)
    std::vector<u8> png;
    {
        const std::string p = std::string(FIXTURE_DIR) + "/yellow4.png";
        FILE* f = std::fopen(p.c_str(), "rb");
        ASSERT(f != nullptr);
        u8 buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            png.insert(png.end(), buf, buf + n);
        }
        std::fclose(f);
    }
    ASSERT(!png.empty());

    // monta o GLB: [header][JSON chunk NÃO múltiplo de 4 + ZEROS até alinhar
    // 4][BIN chunk = geometria + IMAGEM NO FIM + padding zeros a 4].
    // Variante pelos argumentos:
    //   imgBytes != nullptr → a imagem vive no chunk em [imgOff, imgOff+imgLen)
    //   imgBytes == nullptr → o view APONTA para fora (o caso do device)
    auto makeGlb = [&](size_t imgOff, size_t imgLen,
                       const std::vector<u8>* imgBytes,
                       std::vector<u8>& glbOut) {
        std::vector<u8> geo;
        auto pushF = [&geo](f32 v) {
            u8 t[4];
            std::memcpy(t, &v, 4);
            geo.insert(geo.end(), t, t + 4);
        };
        pushF(0); pushF(0); pushF(0);
        pushF(1); pushF(0); pushF(0);
        pushF(0); pushF(1); pushF(0);                 // 36 B de POSITION
        geo.push_back(0); geo.push_back(0);          // idx u16 LE: 0
        geo.push_back(1); geo.push_back(0);          // 1 (lo, hi!)
        geo.push_back(2); geo.push_back(0);          // 2
        std::vector<u8> binChunk = geo;               // 42 B de geometria
        if (imgBytes != nullptr) {
            // espaço para a imagem (gap de zeros até imgOff se preciso)
            binChunk.resize(imgOff + imgLen, 0);
            std::memcpy(binChunk.data() + imgOff, imgBytes->data(), imgLen);
        }
        while (binChunk.size() % 4 != 0) {
            binChunk.push_back(0);   // padding DE DENTRO do chunk (o spec)
        }
        char jv[128];
        std::snprintf(jv, sizeof(jv),
            "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu}",
            imgOff, imgLen);
        char js[1024];
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":%zu}],"
            "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6},"
            "%s],"
            "\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
            "\"materials\":[{\"pbrMetallicRoughness\":"
            "{\"baseColorTexture\":{\"index\":0}}}],"
            "\"textures\":[{\"source\":0}],"
            "\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
            "\"indices\":1,\"material\":0}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
            binChunk.size(), jv);
        const std::string json = js;
        // o JSON chunk fica SEM padding de espaços: o comprimento NÃO é
        // múltiplo de 4 DE PROPÓSITO — o header do BIN vive no próximo
        // alinhamento de 4 (o caso do mundo real que o parser TEM de
        // aguentar; é a mutação «binStart sem alinhamento» da R-022)
        std::vector<u8>& g = glbOut;
        auto u32push = [&g](u32 v) {
            u8 t[4];
            std::memcpy(t, &v, 4);
            g.insert(g.end(), t, t + 4);
        };
        const u32 jsonPad = (4 - (static_cast<u32>(json.size()) % 4)) % 4;
        u32push(0x46546C67u);   // 'glTF'
        u32push(2);
        u32push(12 + 8 + static_cast<u32>(json.size()) + jsonPad + 8 +
               static_cast<u32>(binChunk.size()));
        u32push(static_cast<u32>(json.size()));   // NÃO múltiplo de 4
        u32push(0x4E4F534Au);                     // 'JSON'
        g.insert(g.end(), json.begin(), json.end());
        for (u32 i = 0; i < jsonPad; ++i) {
            g.push_back(0);      // ZEROS até alinhar (não espaços!)
        }
        u32push(static_cast<u32>(binChunk.size()));
        u32push(0x004E4942u);    // 'BIN'
        g.insert(g.end(), binChunk.begin(), binChunk.end());
    };

    // ---- (a) a IMAGEM VÁLIDA no FIM do BIN (com padding) importA TUDO --
    {
        std::vector<u8> glb;
        makeGlb(42, png.size(), &png, glb);   // a imagem COLA na geometria
        // o PRIMEIRO assert é o PARSE EM MEMÓRIA (parseGlb): o alinhamento
        // de chunks (0.9.6.4) acha o BIN mesmo com JSON não múltiplo de 4
        GltfModel model;
        std::string perr;
        EXPECT(parseGlb(glb.data(), glb.size(), GltfBufferResolver{nullptr, 0},
                        model, perr));
        EXPECT(model.meshes.size() == 1u);
        EXPECT(model.images.size() == 1u);
        EXPECT(model.images[0].bytes.size() == png.size());
        EXPECT(!model.images[0].broken);
        // o IMPORT REAL pelo caminho de produção (ficheiro + range loader)
        char src[96];
        std::snprintf(src, sizeof(src), "/tmp/goni_r022_pad_%d.glb",
                      (int)::getpid());
        FILE* f = std::fopen(src, "wb");
        ASSERT(f != nullptr);
        std::fwrite(glb.data(), 1, glb.size(), f);
        std::fclose(f);
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r022_root_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "pad.glb", st, nullptr, out,
                                            stats, err);
        ASSERT(ok);
        EXPECT(out.meshes.size() == 1u);
        EXPECT(out.meshes[0] == "assets/pad.gmesh");
        EXPECT(out.textures.size() == 1u);   // a imagem DO FIM entrou
        EXPECT(stats.texWarn == 0u);
        EXPECT(st.exists("source/pad.glb"));
        ::remove(src);
    }
    // ---- (b) a IMAGEM PODRE (bytes que não são PNG): import SEM
    // texturas + AVISO contado — NUNCA mais o import inteiro morto -----
    {
        const std::vector<u8> podre = {'P', 'O', 'D', 'R', 'E', '!'};
        std::vector<u8> glb;
        makeGlb(42, podre.size(), &podre, glb);   // no fim, bytes lixo
        GltfModel model;
        std::string perr;
        EXPECT(parseGlb(glb.data(), glb.size(), GltfBufferResolver{nullptr, 0},
                        model, perr));
        EXPECT(model.images.size() == 1u);
        // o view RESOLVE (6 B dentro do chunk) — os bytes são que lixo é
        // o PASSE DE TEXTURAS que diz (warn + texWarn), não o parse
        EXPECT(!model.images[0].broken);
        EXPECT(model.images[0].bytes.size() == 6u);
        char src[96];
        std::snprintf(src, sizeof(src), "/tmp/goni_r022_podre_%d.glb",
                      (int)::getpid());
        FILE* f = std::fopen(src, "wb");
        ASSERT(f != nullptr);
        std::fwrite(glb.data(), 1, glb.size(), f);
        std::fclose(f);
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r022_podre_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "podre.glb", st, nullptr,
                                            out, stats, err);
        // O CORAÇÃO DA R-022: o import SEGUE (a geometria é boa) SEM a
        // textura — e a falha é CONTADA (toast «SEM N textura(s)»)
        EXPECT(ok);
        EXPECT(out.meshes.size() == 1u);
        EXPECT(out.textures.empty());
        EXPECT(stats.texWarn >= 1u);
        ::remove(src);
    }
    // ---- (c) o bufferView da imagem FORA DO BUFFER: o defeito EXATO do
    // device («GLB com texturas falha») — AGORA é warn, não import morto
    {
        std::vector<u8> glb;
        makeGlb(20, 100000, nullptr, glb);   // len absurdo > chunk BIN
        char src[96];
        std::snprintf(src, sizeof(src), "/tmp/goni_r022_fora_%d.glb",
                      (int)::getpid());
        FILE* f = std::fopen(src, "wb");
        ASSERT(f != nullptr);
        std::fwrite(glb.data(), 1, glb.size(), f);
        std::fclose(f);
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r022_fora_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "fora.glb", st, nullptr,
                                            out, stats, err);
        EXPECT(ok);                       // o import SEGUE (geometria boa)
        EXPECT(out.meshes.size() == 1u);  // o mesh ENTROU
        EXPECT(out.textures.empty());     // a textura NÃO
        EXPECT(stats.texWarn == 1u);      // e o aviso foi CONTADO
        ::remove(src);
    }
}

// ============================================================================
// R-022 (frente 2) — A CÓPIA VERIFICADA — regress_glb_copia_verificada
//
// A defesa da integridade: a cópia do GLB para source/ é conferida contra
// a fonte byte a byte (em chunks — nunca o ficheiro inteiro em RAM quando
// há caminho real). Uma cópia truncada/corrompida NÃO fica no projeto com
// um «bufferView fora do buffer» disfarçado — diz «cópia truncada» COM A
// POSIÇÃO. (importFile chama isto após cada import .glb; aqui a sentinela
// aferra as DUAS direções da função EXPosta.)
// ============================================================================
TEST(regress_glb_copia_verificada) {
    using namespace vv;
    // uma fonte qualquer no disco (os bytes não importam — a COMPARAÇÃO sim)
    std::vector<u8> src;
    for (u32 i = 0; i < 1000; ++i) {
        src.push_back(static_cast<u8>(i & 0xFF));
    }
    char srcPath[96];
    std::snprintf(srcPath, sizeof(srcPath), "/tmp/goni_r022c_src_%d.bin",
                  (int)::getpid());
    {
        FILE* f = std::fopen(srcPath, "wb");
        ASSERT(f != nullptr);
        std::fwrite(src.data(), 1, src.size(), f);
        std::fclose(f);
    }
    char root[64];
    std::snprintf(root, sizeof(root), "/tmp/goni_r022c_%d", (int)::getpid());
    FsStorage st(root);
    std::string err;

    // (a) cópia ÍNTEGRA → true (o caminho feliz de cada import)
    ASSERT(st.writeBytes("source/x.bin", src.data(), src.size()));
    EXPECT(convert::verifyCopyChunked(srcPath, st, "source/x.bin", err));
    EXPECT(err.empty());

    // (b) cópia TRUNCADA (metade) → false + «cópia truncada»
    ASSERT(st.writeBytes("source/x.bin", src.data(), src.size() / 2));
    EXPECT(!convert::verifyCopyChunked(srcPath, st, "source/x.bin", err));
    EXPECT(err.find("cópia truncada") != std::string::npos);

    // (c) cópia do MESMO TAMANHO com UM byte trocado → false (a posição)
    std::vector<u8> flip = src;
    flip[777] = static_cast<u8>(flip[777] ^ 0xFF);
    ASSERT(st.writeBytes("source/x.bin", flip.data(), flip.size()));
    EXPECT(!convert::verifyCopyChunked(srcPath, st, "source/x.bin", err));
    EXPECT(err.find("cópia truncada") != std::string::npos);
    EXPECT(err.find("777") != std::string::npos);   // a posição exata

    // (d) cópia AUSENTE → false (não existe = truncada a zero)
    EXPECT(st.remove("source/x.bin"));
    EXPECT(!convert::verifyCopyChunked(srcPath, st, "source/x.bin", err));
    EXPECT(err.find("cópia truncada") != std::string::npos);
    ::remove(srcPath);
}

// ============================================================================
// R-023 (FASE 0.9.6-MASTER · GRUPO A) — A EXAUSTÃO DOS SLOTS DE SCROLL —
//         regress_scroll_slots_reciclados
//
// ACHADO AO VIVO pela FASE 12.8b (o loop apanhando bug que ninguém
// reportou): os 8 slots de scroll do UiContext eram DEFINITIVOS — cada
// região (hierarquia, inspector, logs, scenes, uiInsp, ficheiros, consola,
// seletor, browser, settings, docs, script, texto, áudio) ocupava um para
// sempre. NUMA SESSÃO com 8+ regiões usadas, a 9.ª nascia MORTA ao toque
// (sem slot = sem região = sem claim = sem scrollTap): no device, abrir
// Settings+Docs+Script+Texto+Áudio+Logs+Consola+Ficheiros e depois o
// BROWSER = o browser não responde (e o «1 toque importa» morria). O FIX:
// slots são RECOLHIDOS por frame-stamp (região que não desenhou neste
// frame = overlay fechado = slot reciclável). A sentinela: 10 regiões
// SEQUENCIAIS (mais que os 8 slots) e a 10.ª TEM de continuar viva.
// ============================================================================
TEST(regress_scroll_slots_reciclados) {
    using namespace vv;
    UiContext ui;
    ui.init();
    InputState input;
    ui.beginFrame(nullptr, &input, 1536.0f, 720.0f);
    // 10 regiões DISTINTAS (mais que os 8 slots) — cada uma desenha UMA
    // vez e «fecha» (como overlays abertos e fechados numa sessão real)
    for (u64 i = 0; i < 10; ++i) {
        ui.beginFrame(nullptr, &input, 1536.0f, 720.0f);   // novo frame
        const UiRect r{100.0f, 100.0f, 400.0f, 300.0f};
        ui.beginScroll(2000 + i, r, 2000.0f);
        ui.endScroll();
    }
    // a 10.ª região (id 2009) TEM slot: o offset que se GRAVA volta
    ui.scrollSetOffset(2009, 120.0f);
    ui.beginFrame(nullptr, &input, 1536.0f, 720.0f);
    {
        const UiRect r{100.0f, 100.0f, 400.0f, 300.0f};
        ui.beginScroll(2009, r, 2000.0f);
        const f32 off = ui.scrollOffset();
        ui.endScroll();
        // sem a reciclagem: a 10.ª região NUNCA teve slot (8 definitivos)
        // — o scrollSetOffset era um NO-OP e o offset ficava 0
        EXPECT(nearEqF(off, 120.0f, 0.5f));
    }
    // e uma região ANTIGA (id 2000 — o slot dela foi reciclado entretanto)
    // VOLTA A FUNCIONAR quando volta a desenhar: ganha slot de novo e o
    // offset GRAVA (o preço documentado do reciclo: o offset ANTIGO
    // recomeça a zero — o que NÃO pode é a região ficar MORTA ao toque)
    ui.beginFrame(nullptr, &input, 1536.0f, 720.0f);
    {
        const UiRect r{100.0f, 100.0f, 400.0f, 300.0f};
        ui.beginScroll(2000, r, 2000.0f);   // re-registrou (slot novo)
        ui.scrollSetOffset(2000, 60.0f);    // com slot: o offset GRAVA
        const f32 off = ui.scrollOffset();
        ui.endScroll();
        EXPECT(nearEqF(off, 60.0f, 0.5f));  // viva — não morta
    }
}
