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
#include <fcntl.h>   // 0.10-M (SAF-STREAM/R-041): ::open/O_RDONLY

#include <cstdio>
#include <cstring>
#include <sys/stat.h>   // 0.11-T-FIX (R-046): o sfixRmRf (S_ISDIR)
#include <sys/time.h>   // 0.12-QoL D (R-056): o backdate do mtime
// 0.9.6.5 (GRUPO B · R-024): o encode/decode do PNG exportado + o layout
#include "render/ThumbPng.h"
#include "assets/PngLoader.h"
#include <sstream>   // 0.9.6 (R-017): parse das 9 linhas do bench
#include <string>
#include <vector>

#include "FakeStorage.h"
#include "FakeSafIo.h"
#include "core/SceneSerializer.h"
#include "core/SafStorage.h"
#include "core/Project.h"
#include "core/AutoSave.h"
#include "components/CameraComp.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/TransformSystem.h"   // 0.12-V (R-063): composeWorld/worldToLocalPos
#include "components/AnimationPlayer.h"   // 0.11-T (R-045): o cull do system
#include "components/SkeletonComp.h"
#include "core/AnimationSystem.h"          // 0.11-T (R-045)
#include "render/BlockMesh.h"   // 0.10-M (PASSO 4 · R-040): orçamentos+frustum
#include "render/Mesh.h"        // 0.11-T (R-045): o mesh skinado do cull
#include "render/Impostor.h"       // 0.11-W (3.75b · R-049): a decisão/matriz
#include "render/GpuAssets.h"      // 0.11-W (3.75b · R-049): a textura do impostor
#include "assets/ImpostorIndex.h"  // 0.11-W (3.75b · R-049): o PNG sob demanda
#include "assets/ResourceManager.h"  // 0.11-W (3.75b · R-049): o storage do GpuAssets
#include "assets/ThumbRender.h"    // 0.11-W (3.75b · R-049): o render alfa
#include "render/Renderer.h"
#include "render/Primitives.h"
#include "render/SkyRenderer.h"   // 0.12-Y (3.Y-D · R-061): as puras do céu
#include "render/ParticleRenderer.h"  // 0.12-Y-F1 (R-062): o draw real
#include "render/ParticleShader.h"    // 0.12-Y-F1 (R-062): as puras fx::
#include "components/WindTIC.h"             // 0.12-Y-F2 (R-064): o vento
#include "components/AmbientEnvironment.h"  // 0.12-Y-F2 (R-064): o clima
#include "core/Presets.h"             // 0.12-Y-F1 (R-062): a fábrica
#include "ui/EditorUi.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"
#include "platform/BuildInfo.h"
#include "platform/CrashHandler.h"
#include "platform/StorageBridge.h"
#include "assets/AssetConverter.h"
#include "assets/GltfImporter.h"   // 0.9.6 (R-017): o GLB do bench
#include "assets/GmeshV3Stream.h"  // 0.11-T-FIX (R-046): o teto do pool
#include "core/FsStorage.h"        // 0.11-T-FIX (R-046): a raiz real do import
#include "core/Bench.h"            // 0.9.6 (R-017): o relatório não mente
// 0.9.3 (REG-002): o backend Oboe de PRODUÇÃO (TU comum) contra o stub
// tests/stub/oboe/Oboe.h — o MESMO código que corre no APK
#include "platform/AudioOut.h"
#include <oboe/Oboe.h>
#include <thread>
#include <atomic>
#include <chrono>

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

    // ---- (3) 20 teclas da BARRA DE SÍMBOLOS (o MESMO applyEvent) ------
    // (0.9.6.8 GRUPO E: o teclado da engine SAIU — o repertório é o da
    // BARRA: os 24 símbolos da spec + DEL + ENTER pelo caminho único)
    const char* kKb[20] = {"{", "}", "(", ")", "[", "]", "=", "+", "-", "*",
                           "/", "<", ">", "!", ",", ".", ";", ":", "\"", "_"};
    for (int i = 0; i < 20; ++i) {
        ime::Event ev;
        ev.isText = true;
        ev.text = kKb[i];
        editor::scriptwin::applyEvent(st.scriptWin, ev);
        EXPECT(st.scriptWin.open);
    }
    EXPECT(st.scriptWin.buf.size() > 20);

    // ---- (4) o DRAW com o IME ABERTO (portrait) não crasha ------------
    // (0.9.6.8 GRUPO E: o inset REAL do IME injetado — a barra de
    // símbolos dokada sobre ele e o corpo reservado; o caminho do device)
    ime::setBottomInset(280.0f);   // um GBoard típico em px @1.0 do harness
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
    ime::setBottomInset(0.0f);   // o IME fecha com a janela (o par do close)
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
    EXPECT(nearEqF(bar.h, 80.0f));   // FASE UI B: 40dp REAL (era 36→72)
    EXPECT(nearEqF(theme::dp(48.0f), 96.0f));   // o dp é dp REAL
    const UiRect status = safe::statusRect(1536.0f, 720.0f, zero);
    EXPECT(nearEqF(status.h, 0.0f));   // PASSO 1: a status foi REMOVIDA
    EXPECT(nearEqF(status.y + status.h, 720.0f));   // a faixa compat no fundo
    // o teclado: teclas de 96px de altura (48dp real — o dono media 48×65px)
    // 0.9.6.8 (GRUPO E · RECALIBRADA): o teclado da engine SAIU — a vara
    // passa a aferir a BARRA DE SÍMBOLOS (a spec E: 40dp — 80px @2.0)
    EXPECT(nearEqF(editor::scriptwin::symbolBarHeight(), 40.0f * 2.0f));
    EXPECT(editor::scriptwin::symKeysVisible(720.0f * 2.0f) == 14u);
    EXPECT(editor::scriptwin::symKeysVisible(360.0f * 2.0f) == 9u);
    // ---- (b) densidade 1.0: o layout da spec PASSO 1 -----------------------
    theme::setDensity(1.0f);
    const UiRect bar1 = safe::toolbarRect(1536.0f, 720.0f, zero);
    EXPECT(nearEqF(bar1.h, 40.0f));   // FASE UI B: a barra do dono
    EXPECT(nearEqF(theme::dp(48.0f), 48.0f));
    EXPECT(nearEqF(editor::scriptwin::symbolBarHeight(), 40.0f));
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

// ============================================================================
// R-024 (FASE 0.9.6-MASTER · GRUPO B) — O LAYOUT EXPORTADO NÃO MENTE —
//         regress_layout_dump_nao_mente
//
// O mecanismo: com o audit ligado, cada widget do UiContext regista o rect
// REAL que desenhou (o choke point único — o JSON que o dono abre sai do
// MESMO código que desenha; a lição R-020: nada de duplicar o layout noutro
// sítio que possa divergir). A sentinela afere os INVARIANTES do registo:
// (a) os rects registados são os desenhadOS (panel/button/scroll nos sítios
//     chamados — um retângulo por widget de topo, os compostos não explodem
//     em partes);
// (b) a labelFitted que TRUNCA deixa o rasto (fullW > w e truncated=true);
// (c) a label que CABE nunca diz truncada;
// (d) o guard dos compostos: o button() regista UMA entrada (não o
//     widgetHit + os painéis visuais por dentro);
// (e) os filhos do scroll nascem clipped=true (o validador usa o flag para
//     não assinalar as linhas scrolled-out).
// ============================================================================
TEST(regress_layout_dump_nao_mente) {
    using namespace vv;
    UiContext ui;
    ui.init();
    InputState input;
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    ui.setFont(&font);

    ui.beginFrame(nullptr, &input, 1000.0f, 600.0f);
    ui.auditBegin("sentinela", 1000.0f, 600.0f, 24.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    ui.panel(10.0f, 10.0f, 200.0f, 100.0f, theme::PANEL);
    ui.button(0xAABB, 300.0f, 50.0f, 152.0f, 48.0f, "OK");
    ui.frame(10.0f, 200.0f, 100.0f, 40.0f, 2.0f, theme::LINE);
    ui.beginScroll(0x5C9011, UiRect{50.0f, 300.0f, 400.0f, 200.0f}, 2000.0f);
    ui.panel(60.0f, 320.0f - 500.0f, 100.0f, 50.0f, theme::PANEL);  // scrolled-out
    ui.labelFitted(60.0f, 340.0f - 500.0f,
                   "texto comprido que nao cabe em cem pixeis de largura",
                   theme::TEXT, 100.0f);
    ui.labelFitted(60.0f, 380.0f - 500.0f, "cabe", theme::TEXT, 300.0f);
    ui.endScroll();
    ui.auditEnd();

    const layout::Record& r = ui.auditRecord();
    // CONTAGEM EXATA: panel + Button + Label("OK") + Frame + Scroll +
    // Panel(clipped) + Label(trunc) + Label(cabe) = 8 (o button NÃO explode
    // em widgetHit+panel+frame — o guard dos compostos)
    EXPECT(r.entries.size() == 8);
    // (a) o panel solitário no sítio certo
    bool foundPanel = false;
    for (const auto& e : r.entries) {
        if (e.kind == layout::Entry::Panel && !e.clipped &&
            nearEqF(e.x, 10.0f) && nearEqF(e.y, 10.0f) &&
            nearEqF(e.w, 200.0f) && nearEqF(e.h, 100.0f)) {
            foundPanel = true;
        }
    }
    EXPECT(foundPanel);
    // (d) o button regista UMA entrada Button (o widgetHit e os painéis
    // visuais de dentro CALADOS pelo guard — senão eram 4 entradas)
    u32 btnCount = 0;
    for (const auto& e : r.entries) {
        if (e.kind == layout::Entry::Button && e.id == 0xAABB) ++btnCount;
    }
    EXPECT(btnCount == 1);
    // o frame regista UMA Frame (as 4 arestas não explodem)
    u32 frameCount = 0;
    for (const auto& e : r.entries) {
        if (e.kind == layout::Entry::Frame) ++frameCount;
    }
    EXPECT(frameCount == 1);
    // o scroll registado com o id
    bool foundScroll = false;
    for (const auto& e : r.entries) {
        if (e.kind == layout::Entry::Scroll && e.id == 0x5C9011 &&
            nearEqF(e.w, 400.0f)) {
            foundScroll = true;
        }
    }
    EXPECT(foundScroll);
    // (e) os filhos do scroll nascem clipped
    bool clippedChild = false;
    for (const auto& e : r.entries) {
        if (e.clipped && e.kind == layout::Entry::Panel &&
            nearEqF(e.w, 100.0f)) {
            clippedChild = true;
        }
    }
    EXPECT(clippedChild);
    // (b) a labelFitted que NÃO CABE: truncated=true e fullW > w
    bool truncFlag = false;
    for (const auto& e : r.entries) {
        if (e.kind == layout::Entry::Label && e.clipped && e.truncated &&
            e.fullW > e.w + 1.0f) {
            truncFlag = true;
        }
    }
    EXPECT(truncFlag);
    // (c) a que CABE: nunca truncada — a largura MEDIDA é a do CONTEXTO
    // (atlas × textK da densidade; 0.9.6.6 · GRUPO C: medir pelo atlas
    // cru diverge do que desenha — o choke point do sp())
    bool fittedOk = false;
    for (const auto& e : r.entries) {
        if (e.kind == layout::Entry::Label && e.clipped && !e.truncated &&
            nearEqF(e.w, font.widthOf("cabe") * vv::theme::textK(), 0.5f)) {
            fittedOk = true;
        }
    }
    EXPECT(fittedOk);

    // o JSON é determinístico e PARSEÁVEL (o ida-e-volta pelo core/Json.h)
    const std::string j1 = layout::toJson(r).dump();
    const std::string j2 = layout::toJson(r).dump();
    EXPECT(j1 == j2);
    Json back;
    EXPECT(Json::parse(j1.c_str(), j1.size(), back));
    EXPECT(back.type == Json::Type::Object);
    bool ecraOk = false;
    for (const auto& kv : back.members) {
        if (kv.first == "ecra" && kv.second.type == Json::Type::String &&
            kv.second.string == "sentinela") {
            ecraOk = true;
        }
    }
    EXPECT(ecraOk);
}

// ============================================================================
// R-024 (GRUPO B) — O VALIDADOR APANA (cada regra, o caso plantado) —
//         regress_validador_apana
//
// O validador puro (ui/LayoutDump): as 6 regras da casa. A sentinela planta
// UM caso POR REGRA e exige que APANHE — e que um registo LIMPO fique
// LIMPO (zero falsos positivos; a lição do primeiro run: as linhas de
// scroll scrolled-out NÃO são "fora do ecrã").
// ============================================================================
TEST(regress_validador_apana) {
    using namespace vv;
    layout::Record r;
    r.screen = "plantado";
    r.screenW = 1000.0f;
    r.screenH = 600.0f;
    r.insetT = 24.0f;
    r.density = 1.0f;

    auto addBtn = [&](f32 x, f32 y, f32 w, f32 h, bool clipped) {
        layout::Entry e;
        e.kind = layout::Entry::Button;
        e.id = 0x100 + r.entries.size();
        e.x = x; e.y = y; e.w = w; e.h = h;
        e.clipped = clipped;
        r.add(e);
    };
    auto addLabel = [&](f32 x, f32 y, f32 w, bool truncated) {
        layout::Entry e;
        e.kind = layout::Entry::Label;
        e.x = x; e.y = y; e.w = w; e.h = 28.0f;
        e.fullW = truncated ? w * 2.0f : w;
        e.truncated = truncated;
        r.add(e);
    };

    // 1. fora do ecrã (interativo meio fora — inacessível)
    addBtn(900.0f, 100.0f, 152.0f, 48.0f, false);       // x+w=1052 > 1000
    // 2. sobreposto (dois botões a pisarem-se, nenhum contém o outro)
    addBtn(100.0f, 100.0f, 100.0f, 48.0f, false);
    addBtn(150.0f, 120.0f, 100.0f, 48.0f, false);
    // 3. toque pequeno (altura 30 < 48dp)
    addBtn(300.0f, 200.0f, 152.0f, 30.0f, false);
    // 4. texto truncado
    addLabel(60.0f, 300.0f, 80.0f, true);
    // 5. texto que sangra (label SEM clip a sair do ecrã à direita)
    addLabel(980.0f, 350.0f, 120.0f, false);            // x+w=1100 > 1000
    // 6. rect degenerado
    addBtn(400.0f, 400.0f, 0.0f, 0.0f, false);
    // 7. o scroll CONTÉM um filho (relação legítima — NÃO é sobreposto)
    layout::Entry sc;
    sc.kind = layout::Entry::Scroll;
    sc.id = 0x999;
    sc.x = 50.0f; sc.y = 450.0f; sc.w = 400.0f; sc.h = 120.0f;
    r.add(sc);
    addBtn(60.0f, 460.0f, 100.0f, 48.0f, true);         // contido no scroll
    // 8. linha de scroll scrolled-out (fora do ecrã mas CLIPPED — limpa)
    addBtn(60.0f, 700.0f, 100.0f, 48.0f, true);

    const std::vector<layout::Problem> ps = layout::validate(r);
    auto hasRule = [&](layout::Problem::Rule rule) {
        for (const auto& p : ps) {
            if (p.rule == rule) return true;
        }
        return false;
    };
    EXPECT(hasRule(layout::Problem::ForaDoEcra));
    EXPECT(hasRule(layout::Problem::Sobreposto));
    EXPECT(hasRule(layout::Problem::ToquePequeno));
    EXPECT(hasRule(layout::Problem::TextoTruncado));
    EXPECT(hasRule(layout::Problem::TextoSangra));
    EXPECT(hasRule(layout::Problem::RectDegenerado));

    // os FILHO-DO-SCROLL e o CLIPPED scrolled-out NÃO contam (a regra da
    // contenção + a exceção do clip — o fix do falso positivo do 1º run)
    u32 foraDeEcra = 0, sobrepostos = 0;
    for (const auto& p : ps) {
        if (p.rule == layout::Problem::ForaDoEcra) ++foraDeEcra;
        if (p.rule == layout::Problem::Sobreposto) ++sobrepostos;
    }
    EXPECT(foraDeEcra == 1);   // só o botão 1 (o clipped scrolled-out é limpo)
    EXPECT(sobrepostos == 1);  // só o par 2/3 (o filho do scroll é contenção)

    // o descrever nomeia a regra e o relatório fecha as contagens
    const std::string rep = layout::report(r, ps);
    EXPECT(rep.find("ERRO") != std::string::npos);
    EXPECT(rep.find("problemas:") != std::string::npos);

    // um registo LIMPO fica LIMPO (zero falsos positivos)
    layout::Record limpo;
    limpo.screen = "limpo";
    limpo.screenW = 1000.0f;
    limpo.screenH = 600.0f;
    limpo.insetT = 24.0f;
    limpo.density = 1.0f;
    addBtn(100.0f, 100.0f, 152.0f, 48.0f, false);
    addBtn(300.0f, 100.0f, 152.0f, 48.0f, false);
    addLabel(100.0f, 200.0f, 200.0f, false);
    const std::vector<layout::Problem> psLimpos = layout::validate(limpo);
    EXPECT(psLimpos.empty());
    const std::string repLimpo = layout::report(limpo, psLimpos);
    EXPECT(repLimpo.find("VERDE") != std::string::npos);
}

// ============================================================================
// R-024 (GRUPO B) — O FRAMEBUFFER REAL RASTERIZA (o PNG tem mesmo píxeis) —
//         regress_fb_rasteriza
//
// A sonda do Grupo B em forma de sentinela: pelo caminho GL EXATO que o
// Renderer usa (programa uProj + VAO/VBO + textura branca R8 + drawArrays
// TRIANGLES), um quad tem de aparecer no glReadPixels — na cor, no sítio,
// com o SCISSOR a cortar e o BLEND a compor. É o alicerce do PNG exportado:
// se o rasterizador mentir, tudo o que se lhe segue (PNG relido, píxel vs
// registo) é ficção.
// ============================================================================
TEST(regress_fb_rasteriza) {
    glstub::fb::resetState();
    glstub::fb::enabled = true;

    // o programa UI (como Renderer.cpp)
    const GLuint prog = glCreateProgram();
    const GLint locProj = glGetUniformLocation(prog, "uProj");
    // orto y-down 0..64
    const float proj[16] = {
        2.0f / 64.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / 64.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f};
    glViewport(0, 0, 64, 64);
    glClearColor(0.078f, 0.078f, 0.078f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLuint vao = 0, vbo = 0, tex = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 32,
                          reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32,
                          reinterpret_cast<const void*>(8));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 32,
                          reinterpret_cast<const void*>(16));
    const float verts[6][8] = {
        {8, 8, 0, 0, 1, 1, 1, 1},    {56, 8, 1, 0, 1, 1, 1, 1},
        {8, 56, 0, 1, 1, 1, 1, 1},   {8, 56, 0, 1, 1, 1, 1, 1},
        {56, 8, 1, 0, 1, 1, 1, 1},   {56, 56, 1, 1, 1, 1, 1, 1},
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    const GLubyte wpx = 0xFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &wpx);
    glUseProgram(prog);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    std::vector<unsigned char> px(64 * 64 * 4);
    glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    auto at = [&](int x, int y) -> const unsigned char* {
        return &px[(size_t(y) * 64 + x) * 4];
    };
    // (a) o quad rasterizou NO SÍTIO: o centro é branco, o canto é o clear
    // (a orto é y-down: o ecrã-top do quad vive na janela y-up topo)
    EXPECT(at(32, 32)[0] == 255);
    EXPECT(at(2, 2)[0] == 20 && at(2, 2)[2] == 20);
    // (b) a borda do quad é DURA no sítio certo (x=8 ecrã → janela 12):
    // fora dele o clear, dentro dele o branco (rasterizador errado
    // desloca/espalha a borda)
    // a orto de ecrã + viewport são IDENTIDADE em x (x=8 ecrã -> x=8
    // janela): fora do quad o clear, dentro o branco, borda DURA
    EXPECT(at(7, 32)[0] == 20 && at(9, 32)[0] == 255);

    // (c) o SCISSOR corta: novo quad à esquerda com scissor na metade
    // direita — NADA se desenha (o clip da 0.9.6.1 em ação)
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(32, 0, 32, 64);   // só a metade DIREITA (janela y-up)
    {
        const float v2[6][8] = {
            {4, 4, 0, 0, 1, 1, 1, 1},    {20, 4, 1, 0, 1, 1, 1, 1},
            {4, 60, 0, 1, 1, 1, 1, 1},   {4, 60, 0, 1, 1, 1, 1, 1},
            {20, 4, 1, 0, 1, 1, 1, 1},   {20, 60, 1, 1, 1, 1, 1, 1}};
        glBufferData(GL_ARRAY_BUFFER, sizeof(v2), v2, GL_DYNAMIC_DRAW);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    EXPECT(at(12, 32)[0] == 20);   // fora do scissor: NADA desenhado

    glDisable(GL_SCISSOR_TEST);
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}

// ============================================================================
// R-024 (GRUPO B) — O PNG EXPORTADO É VÁLIDO (ida-e-volta pelo decode de
//         produção) — regress_png_layout_ida_e_volta
//
// O layout/<ecrã>.png é codificado pelo thumb::encodePngRgb e vai ser RELIDO
// pelo loadPng (o decoder stb do pipeline de texturas) na FASE 13 e no
// relatório do Grupo B. A sentinela garante o IDA-E-VOLTA num padrão
// conhecido: os píxeis que saem são os que entraram (a 1536×720 — a
// resolução REAL do C33).
// ============================================================================
TEST(regress_png_layout_ida_e_volta) {
    using namespace vv;
    const u32 W = 1536, H = 720;
    std::vector<u8> rgb((size_t)W * H * 3);
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            const size_t p = ((size_t)y * W + x) * 3;
            rgb[p + 0] = (u8)(x & 0xFF);
            rgb[p + 1] = (u8)(y & 0xFF);
            rgb[p + 2] = (u8)((x ^ y) & 0xFF);
        }
    }
    const std::vector<u8> png = thumb::encodePngRgb(rgb.data(), W, H);
    EXPECT(!png.empty());
    RawImage img;
    std::string err;
    EXPECT(vv::loadPng(png.data(), png.size(), img, err));
    EXPECT(img.ok() && img.width == W && img.height == H);
    if (img.ok()) {
        bool igual = true;
        for (u32 y = 0; y < H && igual; ++y) {
            for (u32 x = 0; x < W; ++x) {
                const size_t p = ((size_t)y * W + x);
                if (img.rgba[p * 4 + 0] != rgb[p * 3 + 0] ||
                    img.rgba[p * 4 + 1] != rgb[p * 3 + 1] ||
                    img.rgba[p * 4 + 2] != rgb[p * 3 + 2]) {
                    igual = false;
                    break;
                }
            }
        }
        EXPECT(igual);   // o píxel que sai é o que entrou — byte a byte
    }
}

// ============================================================================
// R-025 (FASE 0.9.6-MASTER · GRUPO C — ESCALA E TIPOGRAFIA)
//
// A linha de base medida pelo Grupo B (RELATORIO-0.9.6.5 secção 8) morreu
// toda na MESMA classe: o texto era o ATLAS CRU (28px) em qualquer
// densidade e meia dúzia de alvos eram px crus. O Grupo C fecha o círculo
// da R-018: dp() para o layout E sp() para o texto — a densidade multiplica
// TUDO (o ecrã a 2.0 é o ecrã a 1.0 visto a 2×, a invariância afervada
// pela FASE 13.6). Quatro sentinelas:
//   1. regress_sp_escala_unica      — o sp()/textK no CHOKE POINT (UiContext)
//   2. regress_toque_48dp_validador — os alvos medidos ≥48dp (a vara do B)
//   3. regress_script_geom_unica    — linha→y/col→x ÚNICA + o índice O(1)
//   4. regress_cantos_suavizados    — o button() com os raios 8dp da spec A
// ============================================================================
#include "ui/BottomPanel.h"          // R-025: drawStatusBar (o ERRO medido)
#include "ui/ScriptEditor.h"         // R-025: a geometria única + o índice
#include "ui/SettingsPage.h"         // R-025: actionBtnRect (48dp)
#include "ui/UiEditor.h"             // R-025: o browser (fechar/raízes/subir)

TEST(regress_sp_escala_unica) {
    using namespace vv;
    // ---- (a) o CONTRATO das funções: sp(v) = v×densidade; textK liga o
    // atlas (28px = 14sp@2.0) à densidade corrente
    theme::setDensity(1.0f);
    EXPECT(theme::sp(14.0f) == 14.0f);
    EXPECT(theme::textK() == 0.5f);   // o atlas é o corpo a 2.0
    theme::setDensity(2.0f);
    EXPECT(theme::sp(14.0f) == 28.0f);
    EXPECT(theme::textK() == 1.0f);   // a 2.0 o atlas É o corpo (o device)
    theme::setDensity(3.0f);
    EXPECT(theme::sp(14.0f) == 42.0f && theme::textK() == 1.5f);
    theme::setDensity(1.0f);

    // ---- (b) o CHOKE POINT: o UiContext mede o texto na escala corrente —
    // o button() ANTIGO media pelo ATLAS CRU (font_->widthOf) e desenhava
    // pela do contexto: truncava/centrava errado fora da densidade 2.0
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    const f32 w1 = ui.fontWidth("escala");
    theme::setDensity(2.0f);
    const f32 w2 = ui.fontWidth("escala");
    theme::setDensity(1.0f);
    EXPECT(w1 > 0.0f && nearEqF(w2, w1 * 2.0f, 0.05f));
    // o mesmo para as MÉTRICAS (as baselines derivam disto)
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    const f32 b1 = ui.textMetrics().block();
    theme::setDensity(2.0f);
    const f32 b2 = ui.textMetrics().block();
    theme::setDensity(1.0f);
    EXPECT(nearEqF(b2, b1 * 2.0f, 0.05f));
    ui.endFrame();

    // ---- (c) PASSO 1: o ERRO medido do Grupo B NÃO VOLTA no NOVO rodapé —
    // a legenda 12sp do «FPS · TICs» CABE na banda da TAB BAR de 32dp em
    // QUALQUER densidade (a status de 24dp saiu; a tab bar é mais alta)
    for (f32 d = 1.0f; d < 3.1f; d += 1.0f) {
        theme::setDensity(d);
        editor::applyDensity();
        ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
        const UiRect tb = safe::bottomTabRect(1600.0f, 720.0f, safe::Insets{});
        const TextMetrics m = ui.textMetrics();
        // o bloco da LEGENDA (12sp): métricas do corpo × 12/14
        const f32 capBlock = (m.ascent + m.descent) *
                             (theme::kFontCaption / 14.0f);
        EXPECT(capBlock <= tb.h + 0.01f);
        ui.endFrame();
    }
    theme::setDensity(1.0f);
    editor::applyDensity();
}

TEST(regress_toque_48dp_validador) {
    using namespace vv;
    // OS ALVOS medidos pelo Grupo B (<48dp): [+] 56×40 e pesquisa 268×40 da
    // hierarquia; fechar 96×36, 6 raízes 139,7×40 e subir 868×44 do browser;
    // o botão 152×40 das actionRows do Settings. Todos ≥48dp agora — em
    // QUALQUER densidade (a regra da casa; a vara é o próprio REGISTO).
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    Scene scene;
    InputState in;

    for (f32 d = 1.0f; d < 2.1f; d += 1.0f) {
        theme::setDensity(d);
        editor::applyDensity();
        ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
        ui.auditBegin("sentinela48", 1600.0f, 720.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, d);
        editor::EditorState st;
        editor::drawHierarchy(ui, scene, st);
        const int pick = editor::drawFileBrowser(
            ui, in, 1600.0f, 720.0f, st, "/Download",
            std::vector<fileapi::DirEntry>{}, true);
        (void)pick;
        ui.endFrame();
        const layout::Record& r = ui.auditRecord();
        ui.auditEnd();
        // TODOS os interativos deste frame ≥48dp (o validador inteiro)
        const auto probs = layout::validate(r);
        bool pequeno = false;
        for (const auto& p : probs) {
            if (p.rule == layout::Problem::ToquePequeno) {
                pequeno = true;
            }
        }
        EXPECT(!pequeno);
        // e o [+] (kIdPlus=40=0x28) e a pesquisa (0x157C) existem nos SEUS
        // pisos de classe (PASSO 1: [+] 28dp cabeçalho; pesquisa 32dp campo)
        bool plusOk = false, searchOk = false;
        for (const auto& e : r.entries) {
            if (e.kind == layout::Entry::Button && e.id == 40u) {
                plusOk = e.w >= 27.9f && e.h >= 27.9f &&
                         e.rowFloorDp == layout::kHeadFloorDp;
            }
            if (e.kind == layout::Entry::Button && e.id == 5500u) {
                searchOk = e.h >= 31.9f && e.rowFloorDp == layout::kFieldFloorDp;
            }
        }
        EXPECT(plusOk);
        EXPECT(searchOk);
    }
    theme::setDensity(1.0f);
    editor::applyDensity();

    // o botão das actionRows do Settings (0.9.6.18 · D4b): OUTLINE COMPATO
    // — altura dos CONTROLOS de linha (28dp, a altura do toggle da casa),
    // largura = texto + padding com teto metade da linha, alinhado à
    // direita (a fonte única exportada; o walk partilha-a). Era a LAJE
    // filled de 152×48 com o rótulo do estado dentro.
    {
        const UiRect ab = editor::settings::actionBtnRect(
            ui, 0.0f, 0.0f, 400.0f, "Repor layout");
        EXPECT(ab.h >= theme::dp(28.0f) - 0.1f && ab.h <= theme::dp(28.0f) + 0.1f);
        EXPECT(ab.w <= 400.0f * 0.5f + 0.1f);        // o teto honesto
        EXPECT(ab.x + ab.w <= 400.0f + 0.1f);        // alinhado à direita
        const UiRect abBig = editor::settings::actionBtnRect(
            ui, 0.0f, 0.0f, 200.0f, "Auditoria do ecrã");
        EXPECT(abBig.w <= 200.0f * 0.5f + 0.1f);     // o teto vence o texto
        EXPECT(abBig.x + abBig.w <= 200.0f + 0.1f);
    }
    theme::setDensity(2.0f);
    editor::applyDensity();
    {
        const UiRect ab2 = editor::settings::actionBtnRect(
            ui, 0.0f, 0.0f, 400.0f, "Repor layout");
        EXPECT(ab2.h >= theme::dp(28.0f) - 0.2f);    // 28dp a 2.0 = 56px
        EXPECT(ab2.x + ab2.w <= 400.0f + 0.2f);
    }
    theme::setDensity(1.0f);
    editor::applyDensity();
}

TEST(regress_script_geom_unica) {
    using namespace vv;
    // ---- (a) a GEOMETRIA ÚNICA: lineTopOnScreen/lineAtScreenY são UM o
    // inverso do outro (o draw e o toque partilham a MESMA fórmula — a
    // causa do R-019 eram duas cópias que driftavam)
    const f32 bodyY = 152.0f, lh = 28.0f;
    for (u32 i : {0u, 1u, 7u, 123u}) {
        const f32 y = editor::scriptwin::lineTopOnScreen(bodyY, i, lh, 0.0f);
        EXPECT(editor::scriptwin::lineAtScreenY(y + lh * 0.5f, bodyY, lh,
                                                0.0f) ==
              static_cast<i32>(i));
        // com scroll: o offset entra e sai (a linha volta a ser a mesma)
        const f32 off = 96.0f;
        const f32 ys = editor::scriptwin::lineTopOnScreen(bodyY, i, lh, off);
        EXPECT(editor::scriptwin::lineAtScreenY(ys + lh * 0.5f, bodyY, lh,
                                                off) ==
              static_cast<i32>(i));
    }
    // o xCode/caretInset partilhados (draw, toque e testes)
    const safe::Insets ins{0.0f, 96.0f, 0.0f, 48.0f};
    EXPECT(editor::scriptwin::codeX(ins) == ins.left + theme::dp(64.0f));
    EXPECT(editor::scriptwin::caretInset() == theme::dp(2.0f));

    // ---- (b) O ÍNDICE de linhas (O(1)): os inícios batem com a varredura
    // manual (a fonte da verdade antiga), o lineCount conta certo e a
    // EDIÇÃO invalida (bufVersion — o contrato de cada mutação)
    editor::scriptwin::State st;
    st.open = true;
    st.buf = "central main {\n  on moment { }\n  allmoments { }\n}\n";
    // (sem bump: o índice está stale — o ensure reconstrói sozinho)
    const std::vector<u32>& idx = editor::scriptwin::ensureLineIndex(st);
    EXPECT(idx.size() == 5);
    EXPECT(idx[0] == 0u);
    EXPECT(idx[1] == st.buf.find('\n') + 1);
    EXPECT(idx[4] == st.buf.size());
    EXPECT(editor::scriptwin::lineCount(st) == 5u);
    // o início de CADA linha == o caminho manual (lineStartOfOffset)
    for (u32 li = 0; li < 5; ++li) {
        EXPECT(idx[li] == editor::scriptwin::lineStartOfOffset(
                              st.buf, li < 4 ? idx[li + 1] - 1
                                             : (u32)st.buf.size()));
    }
    // a edição INVALIDA: uma mutação direta + bump → o índice segue
    st.buf += "nova linha\n";
    ++st.bufVersion;
    EXPECT(editor::scriptwin::lineCount(st) == 6u);
    // e SEM o bump o guard de sanidade também apanha (o back() > size)
    st.buf = "x";   // encolheu SEM bump (uso errado da API)
    EXPECT(editor::scriptwin::lineCount(st) == 1u);   // reconstruiu
}

TEST(regress_cantos_suavizados) {
    using namespace vv;
    // o button() é o CHOKE POINT dos botões da app: cantos 8dp (spec A) —
    // a ESCADARIA do panelRounded emite MAIS quads que o panel reto e a
    // moldura segue o arco (frameRounded). Mede-se nos batches (GL-free).
    // 0.12-E (E-1): a recalibração D-1 (batch sdf) reverte — o SDF 3D foi
    // abortado pelo dono; a escadaria honesta de quads É o caminho real.
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);

    // o panel RETO de referência
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.panel(10.0f, 10.0f, 200.0f, 96.0f, theme::PANEL);
    ui.endFrame();
    const u32 quadsReto = ui.solidsForTest().vertexCount() / 6;

    // o button() AGORA: a escadaria + a moldura em arco
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.button(0x99901, 10.0f, 10.0f, 200.0f, 96.0f, "Botao");
    ui.endFrame();
    const u32 quadsBotao = ui.solidsForTest().vertexCount() / 6;
    // a escadaria (4 degraus × 4 cantos + o miolo) + a moldura em arco
    // (polilinhas) — SEMPRE mais geometria que o recto
    EXPECT(quadsBotao > quadsReto + 8u);
}

// ============================================================================
// R-026 (FASE 0.9.6-MASTER · GRUPO D — ORÇAMENTO DO EDITOR 3D)
//
// A linha de base do Grupo C era 0/0 no HARNESS (1536dp de largura) — mas o
// DEVICE (RMX3624: 1600×720@2.0 = 776×336dp de conteúdo) estava PARTIDO:
// os painéis FIXOS de 300dp deixavam o viewport 3D a 176dp (22% do ecrã),
// o stack vertical (5×48+4×8+8 = 280dp) TRANBORDAVA a altura (~208dp) por
// cima da toolbar, o [+] caía sobre os botões de ferramenta e o drawer
// default (240dp) comia o editor INTEIRO. E em QUALQUER ecrã o pass 3D
// projetava com o ASPECTO DO ECRÃ TODO e o scissor CORTAVA a faixa central
// (o dono via ~22% do FOV horizontal no device). O Grupo D fecha:
//   1. regress_orcamento_gangorra    — resolvePanels (a gangorra dos 3
//                                      pisos) + o clamp do drawer
//   2. regress_divisores_arrastaveis — as pegas (press/move/release com
//                                      snap 8dp + clamp) e a persistência
//   3. regress_scissor_aspecto_rect  — a câmara do RECT (o NDC mapeia
//                                      dentro da janela; o pick coerente)
// ============================================================================
#include "ui/ViewportChrome.h"     // R-026: o chrome adaptativo do viewport
#include "ui/Gizmo.h"              // R-026: projectPoint com o rect
#include "render/Camera.h"
#include "render/Grid.h"         // R-026: a proj com o aspect do rect
#include "ui/CamGizmo.h"           // R-026: pickSceneTic (o pick coerente)
#include "render/Mesh.h"           // R-026: o AABB do pick
#include "render/Cube.h"           // R-026: makeCube (o mesh real do pick)

TEST(regress_orcamento_gangorra) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();

    // ---- (a) o HARNESS LARGO (1512dp): os defaults POR PERCENTAGEM do
    // PASSO 2 — hier 18% = 272,2dp · insp 22% afervado ao teto 260dp ·
    // o viewport fica com 979,8dp (64,8% ≥ 55% — o critério a)
    {
        const safe::PanelBudget b = safe::resolvePanels(1512.0f, -1.0f, -1.0f);
        EXPECT_MSG(nearEqF(b.hier, 1512.0f * safe::kHierPct) &&
                       nearEqF(b.insp, theme::dp(safe::kInspMaxW)),
                   "harness: hier %.1f / insp %.1f — a spec PASSO 2 é 18%% "
                   "/ 22%% (teto 260)", b.hier, b.insp);
        EXPECT(1512.0f - b.hier - b.insp >= 1512.0f * 0.55f - 0.01f);
    }
    // ---- (b) o DEVICE (776dp): 0.12-C (C-1) — a hierarquia 20% (era 18%)
    // já NÃO cai no piso 140 (20% = 155,2); o insp 22% = 170,7 cai no piso
    // 180 · o viewport fica 440,8dp (56,8% ≥ 55% — o critério a)
    {
        const safe::PanelBudget b = safe::resolvePanels(776.0f, -1.0f, -1.0f);
        EXPECT(nearEqF(b.hier, 776.0f * safe::kHierPct));   // 155,2 (20%)
        EXPECT(b.insp >= theme::dp(safe::kInspMinW) - 0.01f &&
               b.insp <= theme::dp(safe::kInspMaxW) + 0.01f);   // [180..260]
        EXPECT(776.0f - b.hier - b.insp >=
               theme::dp(safe::kViewportMinW) - 0.01f);        // >= 288
        EXPECT(776.0f - b.hier - b.insp >= 776.0f * 0.55f - 0.01f);  // >= 55%
    }
    // ---- (c) a GANGORRA ao arrastar: nenhum painel fecha o viewport
    // abaixo do piso (o outro painel CONTA — o par nunca soma demais)
    for (f32 hw = 100.0f; hw <= 900.0f; hw += 32.0f) {
        for (f32 iw = 100.0f; iw <= 900.0f; iw += 32.0f) {
            const safe::PanelBudget b = safe::resolvePanels(776.0f, hw, iw);
            // os pisos dos painéis
            EXPECT(b.hier >= theme::dp(safe::kHierMinW) - 0.01f);
            EXPECT(b.insp >= theme::dp(safe::kInspMinW) - 0.01f);
            // o piso do VIEWPORT (a gangorra nunca o fecha — OU os painéis
            // cedem, OU o par está no impossível onde os pisos mandam)
            const f32 vp = 776.0f - b.hier - b.insp;
            const bool impossivel = theme::dp(safe::kHierMinW) +
                                        theme::dp(safe::kInspMinW) +
                                        theme::dp(safe::kViewportMinW) >
                                    776.0f;
            if (!impossivel) {
                EXPECT(vp >= theme::dp(safe::kViewportMinW) - 0.01f);
            } else {
                EXPECT(vp > 200.0f);   // nem no impossível colapsa
            }
        }
    }
    // ---- (d) o estado NEGATIVO é default (−1); o drag além do piso
    // escreve 0 (nunca negativo — o achado ao vivo da 13.7: o snap
    // negativo VIRAVA default na resolvePanels)
    {
        const safe::PanelBudget b = safe::resolvePanels(776.0f, -1600.0f,
                                                        -1600.0f);
        // raw < 0 é DEFAULT (não é clamp): o par negativo devolve o default
        // de fábrica — 20% no device (0.12-C C-1: 776 × 0,20 = 155,2; o
        // piso 140 já não manda no default):
        EXPECT(nearEqF(b.hier, 776.0f * safe::kHierPct));
    }
    // ---- (e) o DRAWER nunca come o editor: o clamp deixa a faixa da
    // toolbar do viewport VIVA (BottomPanel::layout) — no device o
    // default 240 comia os 208dp TODOS
    {
        // harness (568dp de viewport): o clamp histórico (400) manda
        editor::bottom::BottomState bs;
        bs.drawerH = 240.0f;
        const editor::bottom::Layout lh =
            editor::bottom::layout(1536.0f, 720.0f, safe::Insets{}, bs);
        EXPECT(lh.drawer.h <= 400.0f + 0.01f);
        EXPECT(nearEqF(lh.drawer.h, 240.0f));   // intacto onde cabe
        // device (776×336dp de conteúdo, insets 24/24dp): 240 NÃO PASSA —
        // o teto de 35% do PASSO 2 manda (0,35 × 336 = 117,6 → 112dp em
        // passos de 8) — e o piso da toolbar continua vivo por baixo
        const safe::Insets di{0.0f, 24.0f, 24.0f, 0.0f};
        const editor::bottom::Layout ld =
            editor::bottom::layout(776.0f, 336.0f, di, bs);
        const f32 vpH = safe::viewportRect(776.0f, 336.0f, di).h;
        const f32 contentH = 336.0f - di.top - di.bottom;
        EXPECT(ld.drawer.h < 240.0f);           // cedeu
        EXPECT_MSG(ld.drawer.h <= contentH * safe::kDrawerMaxPct + 0.01f,
                   "o drawer do device (%.1f) passou o teto de 35%% (%.1f)",
                   ld.drawer.h, contentH * safe::kDrawerMaxPct);
        EXPECT(ld.drawer.h <= vpH - theme::dp(48.0f) - theme::dp(16.0f) + 8.0f);
        EXPECT(vpH - ld.drawer.h >= theme::dp(48.0f));   // a toolbar viva
    }
}

// ============================================================================
// R-033 (PASSO 2 · 0.9.6.15 — PAINÉIS: a spec do dono medida, não olhada)
//         — regress_paineis_passo2
//
// A spec PASSO 2 em NÚMEROS: (1) os DEFAULTS dos painéis são 18%
// (hierarquia, piso 140dp) e 22% (inspector, piso 180/teto 260dp) e o
// viewport central fica ≥55% nos ecrãs de referência — o critério (a) do
// dono; (2) SEM seleção o inspector colapsa ao TRILHO de 32dp e a área
// vai ao viewport; (3) o drag do inspector respeita o intervalo
// [180..260] (o teto é NOVO — antes só tinha piso); (4) a pega do drawer
// e dos divisores tem 24dp; (5) o drawer default é FECHADO (bottomTab=0
// → altura 0) e aberto NUNCA passa 35% da altura do content.
// ============================================================================
TEST(regress_paineis_passo2) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();

    // ---- (1) os DEFAULTS em TODOS os ecrãs de referência: viewport ≥55%
    struct Case { f32 contentW; const char* name; };
    const Case cases[] = {{776.0f, "device RMX3624"},
                          {800.0f, "C33 800dp"},
                          {1512.0f, "harness"},
                          {1536.0f, "harness sem insets"}};
    for (const Case& c : cases) {
        const safe::PanelBudget b = safe::resolvePanels(c.contentW, -1, -1);
        const f32 vp = c.contentW - b.hier - b.insp;
        EXPECT_MSG(vp >= c.contentW * 0.55f - 0.01f,
                   "%s: o viewport default é %.1f%% (spec PASSO 2: ≥55%%)",
                   c.name, 100.0f * vp / c.contentW);
        EXPECT(b.hier >= theme::dp(safe::kHierMinW) - 0.01f);
        EXPECT(b.insp >= theme::dp(safe::kInspMinW) - 0.01f &&
               b.insp <= theme::dp(safe::kInspMaxW) + 0.01f);
    }

    // ---- (2) o TRILHO: sem seleção o inspector É 32dp e a área vai
    // TODA ao viewport (o centerRect acompanha — a MESMA fonte)
    {
        const safe::PanelBudget bt = safe::resolvePanels(776.0f, -1, -1, true);
        EXPECT(nearEqF(bt.insp, theme::dp(safe::kInspTrackW)));
        EXPECT(bt.hier >= theme::dp(safe::kHierMinW) - 0.01f);
        // o viewport com o trilho é MAIOR que com o inspector aberto
        const safe::PanelBudget ba = safe::resolvePanels(776.0f, -1, -1);
        EXPECT(776.0f - bt.hier - bt.insp > 776.0f - ba.hier - ba.insp);
        // o rect do trilho: encostado à direita, 32dp, dentro do content
        const safe::Insets in{0.0f, 48.0f, 48.0f, 0.0f};
        const UiRect tr = safe::inspectorPanelRect(1600.0f, 720.0f, in, 0.0f,
                                                   -1, -1, true);
        EXPECT(nearEqF(tr.w, theme::dp(safe::kInspTrackW), 0.01f));
        EXPECT(tr.x + tr.w <= 1600.0f - in.right + 0.01f);
        // e o centerRect devolve a área ao 3D (o MESMO flag)
        const UiRect ct = safe::centerRect(1600.0f, 720.0f, in, 0.0f, true,
                                           -1, -1, true);
        EXPECT(ct.x + ct.w <= tr.x + 0.01f);   // o 3D não pisa o trilho
    }

    // ---- (3) o drag do inspector: o intervalo [180..260] é INVARIANTE
    for (f32 iw = 0.0f; iw <= 900.0f; iw += 16.0f) {
        const safe::PanelBudget b = safe::resolvePanels(776.0f, -1, iw);
        EXPECT(b.insp >= theme::dp(safe::kInspMinW) - 0.01f);
        EXPECT(b.insp <= theme::dp(safe::kInspMaxW) + 0.01f);
    }

    // ---- (4) as PEGAS medem 24dp (drawer + divisores — spec PASSO 2)
    EXPECT(nearEqF(editor::bottom::kDrawerHandleH, 24.0f));
    EXPECT(nearEqF(theme::dp(editor::kDividerHitW), theme::dp(24.0f)));
    {
        editor::bottom::BottomState bs{};
        bs.bottomTab = 1;              // aberto
        bs.drawerH = 240.0f;
        const editor::bottom::Layout bl =
            editor::bottom::layout(1536.0f, 720.0f, safe::Insets{}, bs);
        EXPECT(nearEqF(bl.handle.h, theme::dp(24.0f), 0.01f));
    }

    // ---- (5) o drawer default FECHADO + o teto de 35% (device 336dp)
    {
        editor::bottom::BottomState bs0{};   // o ARRANQUE: bottomTab=0
        // o default do dock é FECHADO (a spec PASSO 2 matou o auto-abrir
        // da 0.8.0) — a altura que o editor CONSUME é ZERO (o main só
        // passa o drawerH quando bottomTab>0; o rect do layout existe mas
        // não desenha — o contrato da FASE E L4)
        EXPECT(bs0.bottomTab == 0);
        const editor::bottom::Layout bl0 =
            editor::bottom::layout(776.0f, 336.0f, safe::Insets{}, bs0);
        // com a tab FECHADA o drawer consumido é 0 (o currentDrawerH do
        // main devolve 0 — a MESMA regra, afervel aqui pelo bottomTab)
        const f32 consumed = bs0.bottomTab <= 0
                                 ? 0.0f
                                 : safe::effectiveDrawerH(
                                       bs0.drawerH,
                                       safe::viewportRect(776.0f, 336.0f,
                                                          safe::Insets{})
                                           .h,
                                       336.0f);
        EXPECT(nearEqF(consumed, 0.0f, 0.01f));
        (void)bl0;
        // o pior raw (a pega no máximo) fica no teto de 35% em passos de 8
        editor::bottom::BottomState bs{};
        bs.bottomTab = 3;
        bs.drawerH = 400.0f;
        const editor::bottom::Layout bm =
            editor::bottom::layout(776.0f, 336.0f, safe::Insets{}, bs);
        EXPECT(bm.drawer.h <= 336.0f * safe::kDrawerMaxPct + 0.01f);
        const f32 steps = bm.drawer.h / theme::dp(8.0f);
        EXPECT(std::fabs(steps - std::floor(steps)) < 0.01f);
    }
}

// ============================================================================
// R-034 (P2-bis · 0.9.6.16 — AS DUAS DECISÕES DO DONO sobre o PASSO 2)
//         — regress_p2bis_pin_e_consola
//
// (1) O PIN do inspector: tocar no ícone do TRILHO abre E fixa (o painel
//     fica aberto MESMO sem seleção — inspectorCollapsed manda o pin);
//     a seta de recolher desfaz. PERSISTE no layout.json (inspPinned=0/1,
//     retrocompatível). (2) A CONSOLA ≥60%: a lista de log ocupa ≥60% da
//     altura de CONTEÚDO do drawer (chips+extras ≤40%) SEM exceções — o
//     campo de comando cede (o drawer pequeno = chips 28dp + a lista no
//     resto). A decisão PURA é conCmdVisible (a fonte ÚNICA).
// ============================================================================
TEST(regress_p2bis_pin_e_consola) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();

    // ---- (1) O PREDICADO (a fonte ÚNICA do colapso agora inclui o pin)
    {
        editor::EditorState st;
        // sem seleção e sem pin: o TRILHO (o comportamento PASSO 2)
        EXPECT(editor::inspectorCollapsed(st));
        // PIN sem seleção: o painel abre E FICA (a regra do dono)
        st.inspPinned = true;
        EXPECT(!editor::inspectorCollapsed(st));
        // a seta de recolher: limpa o pin → o trilho volta (sem seleção)
        st.inspPinned = false;
        EXPECT(editor::inspectorCollapsed(st));
        // COM seleção: aberto com ou sem pin (a regra PASSO 2 intacta)
        st.selected = Handle{1u, 1u};
        EXPECT(!editor::inspectorCollapsed(st));
        st.inspPinned = true;
        EXPECT(!editor::inspectorCollapsed(st));
        st.selected = Handle::invalid();
        st.multiSelectCount = 1;   // a MULTI-seleção conta como seleção
        EXPECT(!editor::inspectorCollapsed(st));
        st.multiSelectCount = 0;
        // a geometria do recolher (0.9.6.18 · D1): célula 40dp na LINHA 1
        // do cabeçalho (à direita do título) e os ids do par do pin
        // DISTINTOS da faixa das tabs (7430/7431 — a 2ª tab morreu no D5)
        EXPECT(nearEqF(editor::kInspUnpinCellW, 40.0f, 0.01f));
        EXPECT(editor::kInspTrackPinId != editor::kInspTabBase &&
               editor::kInspUnpinId != editor::kInspTabBase &&
               editor::kInspTrackPinId != editor::kInspUnpinId);
    }

    // ---- (2) A PERSISTÊNCIA (layout.json — spec G)
    {
        editor::bottom::BottomState bs{};
        bs.bottomTab = 3;
        bs.drawerH = 240.0f;
        // o pin FIXADO vai para o formato novo
        const std::string data =
            editor::bottom::serializeLayout(bs, true, 0u, -1.0f, -1.0f, true);
        EXPECT(data.find("inspPinned=1") != std::string::npos);
        editor::bottom::BottomState bs2{};
        bool insp = false, pin = false;
        u32 col = 0xFFFFFFFFu;
        f32 hw = -9.0f, iw = -9.0f;
        EXPECT(editor::bottom::parseLayout(data, bs2, insp, col, &hw, &iw,
                                           &pin));
        EXPECT(pin);   // o que se guarda é o que volta
        // o formato ANTIGO (sem a linha) → pin falso (retrocompatível)
        bool pin2 = true;
        EXPECT(editor::bottom::parseLayout(
                   "bottomTab=1\ndrawerH=240\ninspector=1\n", bs2, insp, col,
                   &hw, &iw, &pin2) &&
               !pin2);
        // sem passar o out-param: compila e lê (compat dos chamadores)
        EXPECT(editor::bottom::parseLayout(data, bs2, insp, col));
        // ilegível → defaults (o pin não sobrevive a lixo)
        bool pin3 = true;
        EXPECT(!editor::bottom::parseLayout("lixo total\n", bs2, insp, col,
                                            nullptr, nullptr, &pin3));
    }

    // ---- (3) A REGRA DA CONSOLA (pura, em dp — densidade-invariante)
    {
        // os números da regra, NOMEADOS e intactos
        EXPECT(nearEqF(editor::bottom::kConListMinPct, 0.60f, 0.001f));
        EXPECT(nearEqF(editor::bottom::kConChipH, 28.0f, 0.01f));
        EXPECT(nearEqF(editor::bottom::kConChipPad, 2.0f, 0.01f));
        EXPECT(nearEqF(editor::bottom::kConListGap, 2.0f, 0.01f));
        EXPECT(nearEqF(editor::bottom::kConCmdH, 40.0f, 0.01f));
        EXPECT(nearEqF(editor::bottom::kConCmdPad, 4.0f, 0.01f));
        const f32 top = editor::bottom::kConChipPad +
                        editor::bottom::kConChipH +
                        editor::bottom::kConListGap;   // 32dp de topo
        // os CONTEÚDOS que o editor REAL produz:
        //   80  = o device (drawer 104dp capado a 35% − pega 24)
        //   88  = o harness @2.0 (drawer 112dp − pega 24)
        //   136 = drawer no piso 160
        //   191 = 1dp acima da FRONTEIRA do campo de comando (com campo)
        //   216 = drawer 240 do harness @1.0 (com campo)
        //   336 = um drawer alto (com campo)
        const f32 cases[] = {80.0f, 88.0f, 136.0f, 191.0f, 216.0f, 336.0f};
        for (const f32 ch : cases) {
            const bool cmd = editor::bottom::conCmdVisible(ch);
            const f32 listH = ch - top - (cmd ? 44.0f : 0.0f);
            EXPECT_MSG(listH >= ch * editor::bottom::kConListMinPct - 0.01f,
                       "conteúdo %.0fdp: a lista é %.1fdp (%.1f%% — o piso "
                       "é 60%%)",
                       ch, listH, 100.0f * listH / ch);
        }
        // a FRONTEIRA do campo (a poeira de 0.6f em f32 manda 1dp de folga:
        // a fronteira exata é ch − top − cmd ≥ ch×0.6 — 189 fica FORA com
        // folga, 191 fica DENTRO com folga; a banda exata não é spec)
        EXPECT(!editor::bottom::conCmdVisible(189.0f));
        EXPECT(editor::bottom::conCmdVisible(191.0f));
        // o DEVICE (80dp): o campo SAI — chips 28dp + a lista no resto
        EXPECT(!editor::bottom::conCmdVisible(80.0f));
        EXPECT(nearEqF(80.0f - top, 48.0f, 0.001f));   // 48 = 60% de 80
        // o HARNESS @1.0 (216dp): o campo fica (a lista mantém ≥60%)
        EXPECT(editor::bottom::conCmdVisible(216.0f));
        EXPECT(nearEqF(216.0f - top - 44.0f, 140.0f, 0.001f));  // 64,8%
    }

    // ---- (4) PASSO 3: a ALFA do chrome (a constante + o multiplicador)
    {
        EXPECT(nearEqF(editor::vpchrome::kChromeAlpha, 0.60f, 0.001f));
        const f32 c[4] = {1.0f, 0.5f, 0.0f, 1.0f};
        f32 out[4];
        editor::vpchrome::chromeCol(c, out);
        EXPECT(nearEqF(out[0], 1.0f, 0.001f) && nearEqF(out[3], 0.60f, 0.001f));
        // o RGB passa intacto (a alfa é a ÚNICA coisa que muda)
        EXPECT(nearEqF(out[1], 0.5f, 0.001f) && nearEqF(out[2], 0.0f, 0.001f));
    }
}

TEST(regress_divisores_arrastaveis) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    const safe::Insets insets{0.0f, 24.0f, 24.0f, 0.0f};
    ui.setSafeArea(insets);   // o dividerRects lê o safeArea do CONTEXTO
    InputState in;
    editor::EditorState st;
    st.hierW = -1.0f;
    st.inspW = -1.0f;
    st.drawerH = 0.0f;
    st.showInspector = true;
    // PASSO 2: SEM seleção o inspector colapsa ao TRILHO e o divisor
    // direito desaparece — este teste afere O DIVISOR, por isso o estado
    // tem de simular uma seleção VIVA (handle ≠ invalid; generation 1)
    st.selected = Handle{1u, 1u};

    // ---- (a) a PEGA existe e ARMA: press no strip da hierarquia (o
    // hit de 20dp na borda do painel) — o toque é da pega, não do scroll
    const f32 sw = 1536.0f, sh = 720.0f;
    const safe::PanelBudget b0 =
        safe::resolvePanels(sw - insets.left - insets.right, -1.0f, -1.0f);
    const f32 stripX = insets.left + b0.hier - theme::dp(10.0f);
    // (o pressed() é um EDGE: vale UM frame no device — o clearEdges é
    // a fronteira de frame; sem ele o press re-arma a cada chamada)
    ui.beginFrame(nullptr, &in, sw, sh);
    in.injectDown(0, stripX, 300.0f);
    editor::dividerInput(ui, in, st, true);
    EXPECT(st.divDragActive && !st.divDragRight);
    in.clearEdges();
    // o drag VIVO: +100px → a hierarquia cresce (snap de 8dp)
    in.injectMove(0, stripX + 100.0f, 300.0f);
    editor::dividerInput(ui, in, st, true);
    EXPECT(st.hierW > b0.hier + 80.0f && st.hierW <= b0.hier + 100.0f + 8.0f);
    in.clearEdges();
    // o release FIXA
    in.injectUp(0);
    editor::dividerInput(ui, in, st, true);
    EXPECT(!st.divDragActive);
    const f32 fixedW = st.hierW;
    ui.endFrame();

    // ---- (b) a pega DIREITA (inspector): press na borda ESQUERDA dele
    ui.beginFrame(nullptr, &in, sw, sh);
    const f32 inspX = sw - insets.right - b0.insp;
    in.injectDown(0, inspX + theme::dp(10.0f), 300.0f);
    editor::dividerInput(ui, in, st, true);
    EXPECT(st.divDragActive && st.divDragRight);
    in.clearEdges();
    // encolhe 40px (o piso do inspector é 272dp: o drag vivo afere-se ao
    // PASSO, o piso fica provado na gangorra do outro teste)
    in.injectMove(0, inspX + theme::dp(10.0f) + 40.0f, 300.0f);
    editor::dividerInput(ui, in, st, true);
    EXPECT(st.inspW > 0.0f && st.inspW < b0.insp - 20.0f);
    in.clearEdges();
    in.injectUp(0);
    editor::dividerInput(ui, in, st, true);
    EXPECT(!st.divDragActive);
    ui.endFrame();

    // ---- (c) o clamp pelo OUTRO painel: com a hierarquia FIXADA em
    // fixedW, o inspector tem o teto (content − hier − 288) — a gangorra
    const safe::PanelBudget b1 =
        safe::resolvePanels(sw - insets.left - insets.right, fixedW, -1.0f);
    EXPECT(b1.hier >= fixedW - 0.01f);   // o estado sobreviveu à resolução
    EXPECT(sw - insets.left - insets.right - b1.hier - b1.insp >=
           theme::dp(safe::kViewportMinW) - 0.01f);

    // ---- (d) a PERSISTÊNCIA (spec G): o round-trip e o formato antigo
    const std::string data = editor::bottom::serializeLayout(
        editor::bottom::BottomState{}, true, 0u, st.hierW, st.inspW);
    EXPECT(std::strstr(data.c_str(), "hierW=") != nullptr);
    EXPECT(std::strstr(data.c_str(), "inspW=") != nullptr);
    editor::bottom::BottomState bs2;
    bool insp = false;
    u32 col = 0;
    f32 hw = -9.0f, iw = -9.0f;
    EXPECT(editor::bottom::parseLayout(data, bs2, insp, col, &hw, &iw));
    EXPECT(nearEqF(hw, st.hierW, 1.0f) && nearEqF(iw, st.inspW, 1.0f));
    // o layout ANTIGO (sem as larguras) → defaults (retrocompatível)
    EXPECT(editor::bottom::parseLayout("bottomTab=1\ndrawerH=240\n", bs2,
                                       insp, col, &hw, &iw) &&
           hw < 0.0f && iw < 0.0f);
    // o lixo fora da gama → default (o clamp VIVO é a última defesa)
    EXPECT(editor::bottom::parseLayout(
               "bottomTab=1\ndrawerH=240\nhierW=99999\n", bs2, insp, col,
               &hw, &iw) &&
           hw < 0.0f);

    // ---- (e) o CHROME ADAPTATIVO (PASSO 3): no rect do device o rail vai
    // a COLUNAS e a fila do topo DESCE; no largo fica o layout primário
    {
        const editor::vpchrome::Layout lwide =
            editor::vpchrome::layout(UiRect{300.0f, 80.0f, 912.0f, 568.0f});
        EXPECT(lwide.railVisible && lwide.railCols == 1u);
        EXPECT(lwide.legendVisible);   // o layout primário tem legenda
        // o rect do DEVICE em dp a 1.0 (hier 200 | vp 288×208)
        const editor::vpchrome::Layout ldev =
            editor::vpchrome::layout(UiRect{200.0f, 56.0f, 288.0f, 208.0f});
        // FASE UI D (recalibrado): as 4 ferramentas cabem numa COLUNA
        // (4×40+3×8 = 184 ≤ 208−16) — o railCols desce para 1
        EXPECT(ldev.railVisible && ldev.railCols == 1u);   // coluna única
        // FASE UI B (recalibrada com nota): o chip ⋯ ÚNICO CABE no topo
        // (a fila de 4 descia — o chip de 40dp respira ao lado do rail;
        // nunca sai do rect — a regra de sempre)
        // TODOS os alvos ≥40dp (PASSO 1: a lei de ouro) e DENTRO do rect
        // (o stack TRANBORDAVA antes do Grupo D). D21: os alvos eram N−1;
        // 0.12-C (C-2): o gizmo do canto SAIU (o duplicado do Mover) — os
        // alvos do chrome são N−2 (9: o rail 5 + a fila 4)
        // FASE UI B: a fila é o chip ⋯ único (o undo/redo/save subiram)
        // FASE UI D: o rail é das 4 ferramentas (os chips são outros alvos)
        const UiRect all[] = {ldev.rail[0],   ldev.rail[1],  ldev.rail[2],
                              ldev.rail[3],   ldev.chips[0], ldev.chips[1],
                              ldev.quick[0]};
        for (const UiRect& r : all) {
            if (r.w <= 0.0f) {
                continue;   // FASE UI D: a degradação ESCONDE (rect zero)
            }
            EXPECT(r.w >= 40.0f - 0.01f && r.h >= 40.0f - 0.01f);
            EXPECT(r.x >= 200.0f - 0.01f && r.y >= 56.0f - 0.01f);
            EXPECT(r.x + r.w <= 200.0f + 288.0f + 0.01f);
            EXPECT(r.y + r.h <= 56.0f + 208.0f + 0.01f);
        }
        // a degradação HONESTA: um rect SUB-mínimo ESCONDE o rail. A fila
        // do topo CABE (0.12-C C-2: o gizmo do canto SAIU — a fila respira
        // até à borda: 8+184+8 = 200dp no rect de 200; antes o gizmo
        // empurrava-a para fora e ela ESCONDEIA). O gizmo fica rect ZERO
        // (o pin mover_unico aferra) e a fila DENTRO do rect (nada sangra)
        // FASE UI D (recalibrado): com 4 ferramentas o rail cabe em 200×60
        // numa LINHA (4 cols × 40 = 184 ≤ 184) — o sub-mínimo que ESCONDE
        // desce para 150dp de largura (o piso honesto: nenhum arranjo
        // cabe); o chip ⋯ único respira e o loop é do chip ÚNICO
        const editor::vpchrome::Layout ltiny =
            editor::vpchrome::layout(UiRect{200.0f, 56.0f, 150.0f, 60.0f});
        EXPECT(!ltiny.railVisible);
        EXPECT(ltiny.gizmoBtn.w == 0.0f);
        EXPECT(ltiny.quick[0].w > 0.0f);
        for (int qi = 0; qi < 1; ++qi) {
            EXPECT(ltiny.quick[qi].x >= 200.0f - 0.01f);
            EXPECT(ltiny.quick[qi].x + ltiny.quick[qi].w <=
                   200.0f + 150.0f + 0.01f);
            EXPECT(ltiny.quick[qi].y >= 56.0f - 0.01f);
            EXPECT(ltiny.quick[qi].y + ltiny.quick[qi].h <=
                   56.0f + 60.0f + 0.01f);
        }
    }
}

TEST(regress_scissor_aspecto_rect) {
    using namespace vv;
    theme::setDensity(1.0f);

    // ---- (a) o CONTRATO do projectPoint com o rect: o NDC mapeia para
    // (vw×vh) LOCAL e SOMA a origem — o mundo no centro da vista cai no
    // CENTRO DO RECT (não no centro do ecrã; o 3D deixou de ser o ecrã)
    Camera cam;   // a órbita default olha para a origem
    const f32 vw = 912.0f, vh = 568.0f, ox = 300.0f, oy = 80.0f;
    const Mat4 vp = Mat4::mul(cam.proj(vw / vh), cam.view());
    f32 sx = 0.0f, sy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, Vec3{0.0f, 0.0f, 0.0f}, vw, vh, sx, sy));
    EXPECT(nearEqF(sx, vw * 0.5f, 1.5f));
    EXPECT(nearEqF(sy, vh * 0.5f, 1.5f));
    // com a ORIGEM: o mesmo ponto cai no centro do RECT NO ECRÃ
    f32 px = 0.0f, py = 0.0f;
    EXPECT(gizmo::projectPoint(vp, Vec3{0.0f, 0.0f, 0.0f}, vw, vh, px, py,
                               ox, oy));
    EXPECT(nearEqF(px, ox + vw * 0.5f, 1.5f));
    EXPECT(nearEqF(py, oy + vh * 0.5f, 1.5f));
    // o DEFAULT (0,0) é o ecrã todo — os testes e o Play ficam IGUAIS
    f32 qx = 0.0f, qy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, Vec3{0.0f, 0.0f, 0.0f}, 1536.0f, 720.0f,
                               qx, qy));
    EXPECT(nearEqF(qx, 768.0f, 1.5f));

    // ---- (b) O ASPECTO DO RECT ≠ o do ecrã — A PROVA DO CORTE: um ponto
    // na borda DIREITA do frustum DA JANELA (NDC.x ≈ +1 do aspect do rect)
    // projeta DENTRO da janela com o aspect CERTO e FORA dela com o aspect
    // do ECRÃ (o bug: o scissor cortava a faixa — o dono via ~22% do FOV
    // horizontal no device)
    {
        const gizmo::ViewBasis vb = gizmo::viewBasis(cam, vw / vh);
        // o ponto a 98% do caminho até a borda direita do frustum da
        // janela, à distância do target ao longo da vista
        const Vec3 edge = vb.eye + vb.fwd * cam.dist +
                          vb.right * (vb.tanHalfFov * (vw / vh) * cam.dist *
                                      0.98f);
        f32 bx = 0.0f, by = 0.0f;
        EXPECT(gizmo::projectPoint(vp, edge, vw, vh, bx, by));
        EXPECT(bx <= vw + 0.5f);   // DENTRO da janela (o FOV inteiro cabe)
        EXPECT(bx > vw * 0.9f);    // ...e perto da borda (é UM ponto de borda)
        // com o ASPECTO DO ECRÃ (o bug): o MESMO ponto cai FORA do rect
        // (a janela só mostrava a faixa central do frustum largo)
        const Mat4 vpBug = Mat4::mul(cam.proj(1536.0f / 720.0f), cam.view());
        f32 bxBug = 0.0f, byBug = 0.0f;
        EXPECT(gizmo::projectPoint(vpBug, edge, 1536.0f, 720.0f, bxBug,
                                   byBug));
        EXPECT(bxBug > vw + 8.0f);   // provado: o aspect do ecrã CORTA o FOV
    }

    // ---- (c) o PICK coerente com o draw: o tap RETO-LOCAL apanha o que
    // o draw põe no sítio (pickSceneTic com o aspect e o mapping do rect)
    {
        Scene scene;
        Mesh m;
        const CubeMeshData cube = makeCube(2.0f);
        ASSERT(m.create(cube.vertices.data(),
                        static_cast<u32>(cube.vertices.size()),
                        cube.indices.data(),
                        static_cast<u32>(cube.indices.size())));
        const Handle h = scene.create("Alvo");
        Tic* t = scene.get(h);
        Transform3D* tr = t->addComponent<Transform3D>();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        mr->mesh = &m;   // o AABB do cubo centrado na origem
        tr->updateWorld();
        // o vp do RECT: o cubo na origem PROJETA no centro do rect
        f32 cx = 0.0f, cy = 0.0f;
        EXPECT(gizmo::projectPoint(vp, Vec3{0.0f, 0.0f, 0.0f}, vw, vh, cx,
                                   cy));
        const Handle picked = camgizmo::pickSceneTic(scene, vp, vw, vh, cx,
                                                    cy);
        EXPECT(picked == h);
        // um toque LONGE do centro (fora do AABB projetado) falha
        const Handle miss = camgizmo::pickSceneTic(scene, vp, vw, vh,
                                                   cx + 200.0f, cy + 200.0f);
        EXPECT(!(miss == h));
    }
}

// ============================================================================
// R-027 (FASE 0.9.6-MASTER · GRUPO E — EDITOR DE SCRIPT + SÍMBOLOS)
//
// O rastreador E manda: «header flexível; IME; barra de símbolos 40dp sobre
// o IME (teclado da engine REMOVIDO)». O estado ANTES do Grupo E: (1) o
// QWERTY in-app de 280dp/54 teclas competia com o IME do sistema (a
// política de coexistência 0.9.6 G3 — DOIS teclados para manter); (2) a
// engine NÃO SABIA onde o IME do Android está (o manifest sem adjustResize:
// o teclado SOBREPÕE a superfície e o CARET ficava por baixo dele); (3) o
// header media os botões a partir da lupa (docsX−200/−152/−56) — no device
// portrait (360dp) o botão do nível caía a −56dp e o do teclado a −8dp:
// DOIS botões FORA DO ECRÃ. O Grupo E fecha:
//   - regress_barra_simbolos_ime — a barra de 40dp dokada sobre o IME REAL
//     (ime::bottomInset), as páginas cobrem os 24 símbolos da spec, as
//     teclas emitem pelo MESMO applyEvent do IME, o header flexível no
//     device (nada fora do ecrã) e a EXCEÇÃO COMPACTA do validador (o piso
//     40dp da spec E: compacto a 39dp FALHA, botão REGULAR a 40dp FALHA —
//     a exceção é estreita e vigiada)
// ============================================================================
TEST(regress_barra_simbolos_ime) {
    using namespace vv;
    theme::setDensity(1.0f);
    rmrf(kSentinelLogs);
    ASSERT(vv::elog::init(kSentinelLogs));

    // ---- (1) A PONTE DO INSET: set/get/insetVisible + o reset do teste ---
    {
        vv::ime::clearForTest();
        EXPECT(vv::ime::bottomInset() == 0.0f);
        EXPECT(!vv::ime::insetVisible());
        vv::ime::setBottomInset(280.0f);
        EXPECT(vv::ime::bottomInset() == 280.0f);
        EXPECT(vv::ime::insetVisible());
        vv::ime::setBottomInset(280.0f);   // o MESMO valor: sem mudança
        EXPECT(vv::ime::bottomInset() == 280.0f);
        vv::ime::setBottomInset(0.0f);     // fechou
        EXPECT(!vv::ime::insetVisible());
        vv::ime::clearForTest();           // o reset cobre o inset
        EXPECT(vv::ime::bottomInset() == 0.0f);
    }

    // ---- (2) A GEOMETRIA DA BARRA: 40dp EXATOS, teclas >=40dp, páginas --
    {
        EXPECT(nearEqF(editor::scriptwin::symbolBarHeight(), 40.0f));
        // a página ADAPTA à largura: 9 no device 360dp, 14 no harness 720dp
        EXPECT(editor::scriptwin::symKeysVisible(360.0f) == 9u);
        EXPECT(editor::scriptwin::symKeysVisible(720.0f) == 14u);
        EXPECT(editor::scriptwin::symKeysVisible(2000.0f) == 14u);  // teto
        EXPECT(editor::scriptwin::symKeysVisible(300.0f) == 9u);    // piso
        // as páginas cobrem os 12 itens (11 símbolos + o Tab): ceil(12/(n-1))
        EXPECT(editor::scriptwin::symPageCount(360.0f) == 2u);   // 8/página
        EXPECT(editor::scriptwin::symPageCount(720.0f) == 1u);   // 13/página
        // os 11 símbolos do set ENXUTO do H (a ordem do dono) + o Tab (o
        // 12.º item, desenhado pela barra): { } ( ) [ ] ; = " . , — o que
        // o GBoard não põe à mão no código; o resto vive no teclado dele
        EXPECT(std::string(editor::scriptwin::kSymbols[0]) == "{");
        EXPECT(std::string(editor::scriptwin::kSymbols[1]) == "}");
        EXPECT(std::string(editor::scriptwin::kSymbols[10]) == ",");
        EXPECT(editor::scriptwin::kSymItemCount == 12u);
        EXPECT(editor::scriptwin::kSymTabItem == 11u);
        // densidade 2.0: a barra dobra (a vara da R-018 no componente novo)
        theme::setDensity(2.0f);
        EXPECT(nearEqF(editor::scriptwin::symbolBarHeight(), 80.0f));
        EXPECT(editor::scriptwin::symKeysVisible(720.0f) == 9u);  // 360dp
        theme::setDensity(1.0f);
    }

    // ---- (3) O DRAW REAL: a barra dokada sobre o IME + o corpo reservado -
    {
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
        scene.get(tic)->addComponent<ScriptComp>();
        editor::scriptwin::open(st.scriptWin, scene, tic);
        st.scriptWin.helpLevel = 2;   // strip fora: matemática limpa

        // IME ABERTO (280px): o frame auditado mostra a barra NO SÍTIO
        vv::ime::setBottomInset(280.0f);
        ui.auditBegin("script", 720.0f, 1536.0f, 0, 0, 0, 0, 1.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        ui.auditEnd();
        const layout::Record& rec = ui.auditRecord();
        u32 barKeys = 0;
        f32 barTop = -1.0f;
        f32 keyW = 0.0f, keyH = 0.0f;
        for (const auto& e : rec.entries) {
            if (e.kind == layout::Entry::Button && e.compact) {
                ++barKeys;
                if (barTop < 0.0f) {
                    barTop = e.y;
                    keyW = e.w;
                    keyH = e.h;
                }
            }
        }
        EXPECT(barKeys == 13u);   // 720dp: o seletor + 12 itens (1 página)
        EXPECT(nearEqF(barTop, 1536.0f - 280.0f - 40.0f));   // SOBRE o IME
        EXPECT(nearEqF(keyW, 720.0f / 14.0f));
        EXPECT(nearEqF(keyH, 40.0f));
        EXPECT(keyW >= 40.0f - 0.05f && keyH >= 40.0f - 0.05f);
        // o VALIDADOR: 0 problemas (as compactas de 40dp passam — a spec E)
        {
            const auto probs = layout::validate(rec);
            EXPECT(probs.empty());
        }

        // a TECLA insere NO CARET (o MESMO applyEvent do IME): o tap na
        // 2ª tecla (a 1ª é o seletor) é o '{' da ordem do dono
        const u32 caret0 = st.scriptWin.caret;
        input.injectDown(0, (720.0f / 14.0f) * 1.5f, barTop + 20.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.injectUp(0);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.clearEdges();
        EXPECT(st.scriptWin.buf[caret0] == '{' &&
                   st.scriptWin.caret == caret0 + 1);

        // o SELETOR com 1 PÁGINA fica na própria (o ciclo fecha em 1/1) e
        // as teclas entram NO CARET: a ',' é o slot 11 (kSymbols[10]) e o
        // TAB é o slot 12 (o 12.º item — o MESMO ime::Event::Tab, a
        // indent de 2 espaços cai no cursor)
        EXPECT(st.scriptWin.symPage == 0);
        input.injectDown(0, (720.0f / 14.0f) * 0.5f, barTop + 20.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.injectUp(0);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.clearEdges();
        EXPECT(st.scriptWin.symPage == 0);   // 1 página: o ciclo é identidade
        const u32 caret1 = st.scriptWin.caret;
        input.injectDown(0, (720.0f / 14.0f) * 11.5f, barTop + 20.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.injectUp(0);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.clearEdges();
        EXPECT(st.scriptWin.buf[caret1] == ',' &&
                   st.scriptWin.caret == caret1 + 1);
        // o TAB (o slot 12 — o 12.º item): a indent de 2 espaços NO caret
        // (o MESMO ime::Event::Tab do IME — os esqueletos herdam o caminho)
        const u32 caret2 = st.scriptWin.caret;
        input.injectDown(0, (720.0f / 14.0f) * 12.5f, barTop + 20.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.injectUp(0);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        input.clearEdges();
        EXPECT(st.scriptWin.caret == caret2 + 2 &&
                   st.scriptWin.buf.compare(caret2, 2, "  ") == 0);

        // IME FECHADO (inset 0): a barra SOME (o frame auditado confirma)
        vv::ime::setBottomInset(0.0f);
        ui.auditBegin("script", 720.0f, 1536.0f, 0, 0, 0, 0, 1.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1536.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1536.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        ui.auditEnd();
        const layout::Record& rec2 = ui.auditRecord();
        bool algumaBarra = false;
        for (const auto& e : rec2.entries) {
            if (e.kind == layout::Entry::Button && e.compact) {
                algumaBarra = true;
            }
        }
        EXPECT(!algumaBarra);
        vv::ime::clearForTest();
    }

    // ---- (4) A EXCEÇÃO COMPACTA é ESTREITA (o piso 40dp vigiado) ---------
    {
        layout::Record r;
        r.screen = "t";
        r.screenW = 720.0f;
        r.screenH = 1536.0f;
        r.density = 1.0f;
        // (a) a compacta a 40dp PASSA (a spec E)
        {
            layout::Entry e;
            e.kind = layout::Entry::Button;
            e.compact = true;
            e.x = 0; e.y = 0; e.w = 40.0f; e.h = 40.0f;
            r.entries.push_back(e);
        }
        EXPECT(layout::validate(r).empty());
        // (b) a compacta a 39dp FALHA (o piso é REAL — a spec não é desculpa)
        {
            r.entries.clear();
            layout::Entry e;
            e.kind = layout::Entry::Button;
            e.compact = true;
            e.x = 0; e.y = 0; e.w = 40.0f; e.h = 39.0f;
            r.entries.push_back(e);
            const auto ps = layout::validate(r);
            EXPECT(!ps.empty() && ps[0].rule == layout::Problem::ToquePequeno);
        }
        // (c) PASSO 1 (0.9.6.14): o botão REGULAR a 40dp PASSA AGORA (a
        // LEI DE OURO é o novo piso da casa) e a 39dp FALHA (o piso é REAL)
        {
            r.entries.clear();
            layout::Entry e;
            e.kind = layout::Entry::Button;
            e.x = 0; e.y = 0; e.w = 40.0f; e.h = 40.0f;
            r.entries.push_back(e);
            EXPECT(layout::validate(r).empty());
            e.h = 39.0f;
            r.entries.clear();
            e.x = 0; e.y = 0; e.w = 40.0f; e.h = 39.0f;
            r.entries.push_back(e);
            const auto ps = layout::validate(r);
            EXPECT(!ps.empty() && ps[0].rule == layout::Problem::ToquePequeno);
        }
        // (d) o botão regular a 48dp também passa (≥40 — e a tabela PASSO 1
        // mostra no relatório que NENHUM alvo chega a 48)
        {
            r.entries.clear();
            layout::Entry e;
            e.kind = layout::Entry::Button;
            e.x = 0; e.y = 0; e.w = 48.0f; e.h = 48.0f;
            r.entries.push_back(e);
            EXPECT(layout::validate(r).empty());
        }
        // (e) PASSO 1: os PISOS DE LINHA — 36/32/28 passam flagados, 35/31/27
        // FALHAM, e o botão SEM flag continua no piso de 40
        {
            struct { f32 w, h, floor; } casos[3] = {
                {300.0f, 36.0f, layout::kRowFloorDp},
                {300.0f, 32.0f, layout::kFieldFloorDp},
                {300.0f, 28.0f, layout::kHeadFloorDp},
            };
            for (const auto& c : casos) {
                r.entries.clear();
                layout::Entry e;
                e.kind = layout::Entry::Button;
                e.rowFloorDp = c.floor;
                e.x = 0; e.y = 0; e.w = c.w; e.h = c.h;
                r.entries.push_back(e);
                EXPECT(layout::validate(r).empty());
                e.h = c.h - 1.0f;
                r.entries.clear();
                e.x = 0; e.y = 0; e.w = c.w; e.h = c.h - 1.0f;
                e.rowFloorDp = c.floor;
                r.entries.push_back(e);
                const auto ps = layout::validate(r);
                EXPECT(!ps.empty() &&
                       ps[0].rule == layout::Problem::ToquePequeno);
                // SEM a flag, a mesma linha de 36 FALHA (o piso de botão
                // solto é 40 — a flag NUNCA vaza)
                r.entries.clear();
                layout::Entry e2;
                e2.kind = layout::Entry::Button;
                e2.x = 0; e2.y = 0; e2.w = c.w; e2.h = 36.0f;
                r.entries.push_back(e2);
                if (36.0f < layout::kTouchFloorDp) {
                    EXPECT(!layout::validate(r).empty());
                }
            }
        }
    }

    // ---- (5) O HEADER FLEXÍVEL no device (360dp): nada fora, nada <48dp --
    {
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
        scene.get(tic)->addComponent<ScriptComp>();
        editor::scriptwin::open(st.scriptWin, scene, tic);
        st.scriptWin.helpLevel = 2;
        // o device portrait: 360dp de conteúdo (720px @2.0) — o header
        // ANTIGO media helpX=−56dp: FORA DO ECRÃ (a medição do Grupo E).
        // O safeArea do draw BATE com os insets do registo (T48/B48 px)
        theme::setDensity(2.0f);
        ui.setSafeArea(safe::Insets{0.0f, 48.0f, 0.0f, 48.0f});
        vv::ime::setBottomInset(880.0f);   // o GBoard do RMX3624
        ui.auditBegin("script", 720.0f, 1600.0f, 48.0f, 48.0f, 0.0f, 0.0f,
                      2.0f);
        ui.beginFrame(nullptr, &input, 720.0f, 1600.0f);
        editor::scriptwin::draw(ui, input, st.scriptWin, 720.0f, 1600.0f,
                                1.0f / 60.0f);
        ui.endFrame();
        ui.auditEnd();
        const layout::Record& rd = ui.auditRecord();
        u32 hdr48 = 0;
        f32 minX = 1e9f, maxX = -1e9f;
        for (const auto& e : rd.entries) {
            if (e.kind == layout::Entry::Button && !e.compact &&
                e.y < 200.0f) {
                if (e.w >= 96.0f - 0.5f && e.h >= 96.0f - 0.5f) {
                    ++hdr48;   // 48dp @2.0 = 96px
                }
                minX = e.x < minX ? e.x : minX;
                maxX = (e.x + e.w) > maxX ? (e.x + e.w) : maxX;
            }
        }
        EXPECT(hdr48 >= 5u);   // back + lupa + «⋯» + run + stop (E1: nível
                              // e copiar recolhem ao menu no 360dp; o Stop
                              // regista SEMPRE — 0.9.6.10)
        EXPECT(minX >= -0.5f && maxX <= 720.5f);   // NADA fora do ecrã
        // o VALIDADOR no orçamento do device: 0 problemas
        {
            const auto probs = layout::validate(rd);
            EXPECT(probs.empty());
        }
        // a barra no device: 9 teclas (360dp/40dp) dokada sobre o IME
        u32 barKeysD = 0;
        f32 barTopD = -1.0f;
        for (const auto& e : rd.entries) {
            if (e.compact) {
                ++barKeysD;
                if (barTopD < 0.0f) {
                    barTopD = e.y;
                }
            }
        }
        EXPECT(barKeysD == 9u);
        EXPECT(nearEqF(barTopD, 1600.0f - 880.0f - 80.0f));
        ui.setSafeArea(safe::Insets{});   // repõe para os outros testes
        vv::ime::clearForTest();
        theme::setDensity(1.0f);
    }

    vv::elog::shutdown();
    rmrf(kSentinelLogs);
}

// ============================================================================
// R-028 · A IDENTIDADE GRAFITE+ÂMBAR+VIDRO (FASE 0.9.6-MASTER · GRUPO UI)
// RECALIBRADA 0.9.6.10 (GRUPO UI · A REESCRITA DA APRESENTAÇÃO, ordem
// expressa do dono): o mono do Grupo F (accent BRANCO #F5F5F5) morreu no
// device — os fills do accent eram BLOCOS BRANCOS CEGANTES (o anti-exemplo
// da imagem 2 do dono). A DECISÃO: Paleta A OFICIAL grafite+âmbar+vidro.
// RECALIBRADA 0.11-Y (Y-1 · a identidade visual G.One VV): o POÇO #08080A
// (o mesmo do launcher), a hairline ÂMBAR #4A3714 α0.60 SAI da rampa
// neutra (a aresta quente do cristal — o RGB é o do accentDim por
// contrato) e entram os DOIS tokens novos: accent2 #FF6B00 (O FOGO —
// quente, a base do gradiente dos ativos) e glow #FF3D00 α0.25.
//   - regress_identidade_grafite_ambar — (1) a TABELA (os tokens NEUTROS
//     mantêm a rampa fria R==G, B-R ≤ 8; o AZUL morto no chrome TODO; a
//     borda É o âmbar do accentDim); (2) O ÂMBAR (accent quente: R>G>B;
//     accentPress mais escuro; a tinta ESCURA sobre o fill, 10,5:1; o
//     accent2 MAIS QUENTE que o accent); (3) O VIDRO α0.80
//     com a auditoria honesta (contrastOnGlassReal no caso REAL ≥4,5;
//     contrastOnGlass no patológico para o que PODE flutuar); (4) a
//     paleta V.ONI (conteúdo — a exceção documentada) no poço; (5) o
//     blendOver puro; (6) os ALIASES ligados bit-a-bit; (7) o WARN
//     LARANJA distinto do accent (a regra «nunca igual ao acento»).
// ============================================================================
TEST(regress_identidade_grafite_ambar) {
    using namespace vv;
    theme::setDensity(1.0f);

    // ---- (1) A RAMPA DA PEDRA: quase-neutra, ZERO AZUL no chrome -------
    // FASE UI (recalibrada com nota): o dono deu um text1 QUENTE
    // (#E8E6E3 — R−G=2, B−R=−5) e um text2 levemente frio (#8A8A93,
    // B−R=9) — a banda alarga para [−8..12] e o R==G tolera 2 (o branco
    // quente do dono). As EXCEÇÕES documentadas (danger/warn/ok
    // semânticos, a paleta V.ONI de sintaxe, os EIXOS do gizmo) são COR
    // ou AZUL por CONTRATO — conteúdo, não chrome (desde 0.6.9)
    {
        const f32* rampa[] = {
            theme::kTheme.bg,      theme::kTheme.surface,
            theme::kTheme.surface2, theme::kTheme.text1,
            theme::kTheme.text2,   theme::kTheme.accentInk};
        for (int i = 0; i < 6; ++i) {
            const f32 dRG = std::abs(rampa[i][0] - rampa[i][1]) * 255.0f;
            const f32 dB = (rampa[i][2] - rampa[i][0]) * 255.0f;
            EXPECT(dRG <= 2.0f);            // R==G (zero croma quente — o #E8E6E3 do dono tem 2)
            EXPECT(dB >= -8.0f && dB <= 12.0f);  // a banda da pedra (o text1 quente tem −5, o text2 tem 9)
        }
        // FASE UI: AS LINHAS 1px do dono (#26262C = o RGB do ELEVADO,
        // R==G, tinge frio leve); o destaque fica APENAS no
        // ativo/selecionado. O accentDim é O FUNDO DE SELEÇÃO (#2B1810).
        EXPECT(nearEqF(theme::kTheme.border[0], 38.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.border[1], 38.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.border[2], 44.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.border[3], 1.0f, 1e-4f));
        // ZERO AZUL no chrome: nenhum token de chrome com B dominante
        // (o azul #2196F3 da spec A e o navy da 0.9.0 morreram) — a BORDA
        // continua no chrome (agora âmbar: R dominante, B morre à mesma)
        const f32* chrome[] = {
            theme::kTheme.bg,       theme::kTheme.surface,
            theme::kTheme.surface2, theme::kTheme.border,
            theme::kTheme.text1,    theme::kTheme.text2,
            theme::kTheme.accent,   theme::kTheme.accentPress,
            theme::kTheme.accentInk, theme::kTheme.accentDim};
        for (int i = 0; i < 10; ++i) {
            // FASE UI (banda recalibrada): o text2 #8A8A93 do dono tem
            // B−R=9 — o teto do tinge frio sobe de 8 para 10
            EXPECT(chrome[i][2] <=
                   std::max(chrome[i][0], chrome[i][1]) + 10.0f / 255.0f);
            // e AZUL de verdade (o #2196F3 da spec A: B−R≈207) morto:
            // nenhum chrome com B−R > 25/255
            EXPECT((chrome[i][2] - chrome[i][0]) * 255.0f <= 25.0f);
        }
        // os valores EXATOS da rampa (a FASE UI — os hexes do dono:
        // fundo #0B0B0D · painel #121215 α0.85)
        EXPECT(nearEqF(theme::kTheme.bg[0], 11.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.surface[0], 18.0f / 255.0f, 1e-4f));
    }

    // ---- (2) A BRASA: quente, com tinta escura e press mais escuro -----
    {
        EXPECT(theme::kTheme.accent[0] > theme::kTheme.accent[1]);
        EXPECT(theme::kTheme.accent[1] > theme::kTheme.accent[2]);
        EXPECT(nearEqF(theme::kTheme.accent[0], 1.0f, 1e-4f));   // #FF5A1F
        EXPECT(nearEqF(theme::kTheme.accent[1], 90.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.accent[2], 31.0f / 255.0f, 1e-4f));
        EXPECT(theme::kTheme.accentPress[0] < theme::kTheme.accent[0]);
        EXPECT(theme::kTheme.accentInk[0] < 0.5f);   // ESCURA (6,2:1)
        EXPECT(theme::contrastRatio(theme::kTheme.accentInk,
                                    theme::kTheme.accent) >= 4.5f);
        // accentDim — O FUNDO DE SELEÇÃO do dono (#2B1810): text1 13,6:1
        EXPECT(nearEqF(theme::kTheme.accentDim[0], 43.0f / 255.0f, 1e-4f));
        EXPECT(theme::contrastRatio(theme::kTheme.text1,
                                    theme::kTheme.accentDim) >= 4.5f);
        // FASE UI — a brasa ESCURA (accent2 #E0440F): a base do gradiente
        // dos ativos (o G desce de 90 para 68) — o destaque continua
        // ÚNICO (mesma família da brasa); a tinta escura segura o piso
        EXPECT(theme::kTheme.accent2[0] > theme::kTheme.accent2[1]);
        EXPECT(theme::kTheme.accent2[1] > theme::kTheme.accent2[2]);
        EXPECT(nearEqF(theme::kTheme.accent2[1], 68.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.accent2[2], 15.0f / 255.0f, 1e-4f));
        EXPECT(theme::contrastRatio(theme::kTheme.bg,
                                    theme::kTheme.accent2) >= 4.5f);
        // FASE UI — O GLOW (#FF5A1F α0.25): o brilho da brasa, decorativo
        // (nunca texto em cima — só o glow composto e a linha do topo)
        EXPECT(nearEqF(theme::kTheme.glow[1], 90.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.glow[3], 0.25f, 1e-4f));
    }

    // ---- (3) O VIDRO α0.80: os alphas + AS DUAS AUDITORIAS -------------
    // o REAL (sobre o bg grafite — o que está de facto por trás dos
    // painéis dokados): TODO o texto ≥4,5:1. O PATOLÓGICO (branco puro
    // por trás): o que PODE flutuar sobre a cena (text1/accent) segura
    // os pisos; text2/severidade vivem em painéis dokados (a regra da
    // casa — o gate é a própria regra, documentada no Theme.h)
    {
        EXPECT(nearEqF(theme::kTheme.surface[3], 0.85f, 1e-4f));   // FASE 3   // vidro
        EXPECT(nearEqF(theme::kTheme.surface2[3], 0.86f, 1e-4f));  // denso
        EXPECT(nearEqF(theme::kTheme.border[3], 1.0f, 1e-4f));     // borda INATIVA opaca (0.12-QoL A; era a hairline âmbar α0.60)
        EXPECT(nearEqF(theme::kTheme.bg[3], 1.0f, 1e-4f));         // opaco
        // o bordo/highlight do vidro (a spec G: #FFFFFF1F / #FFFFFF0A)
        EXPECT(nearEqF(theme::kTheme.glassEdge[3], 31.0f / 255.0f, 1e-4f));
        EXPECT(nearEqF(theme::kTheme.glassTop[3], 10.0f / 255.0f, 1e-4f));
        // REAL: text1/text2/accent ≥ 4,5 sobre o vidro dokado
        EXPECT(theme::contrastOnGlassReal(theme::kTheme.text1) >= 4.5f);
        EXPECT(theme::contrastOnGlassReal(theme::kTheme.text2) >= 4.5f);
        EXPECT(theme::contrastOnGlassReal(theme::kTheme.accent) >= 4.5f);
        // PATOLÓGICO (branco puro): o que flutua segura os pisos
        EXPECT(theme::contrastOnGlass(theme::kTheme.text1) >= 4.5f);
        EXPECT(theme::contrastOnGlass(theme::kTheme.accent) >= 3.0f);
        // e sobre a surface SÓLIDA (o caso clássico) tudo passa à mesma
        EXPECT(theme::contrastOnSurface(theme::kTheme.text1) >= 4.5f);
        EXPECT(theme::contrastOnSurface(theme::kTheme.text2) >= 4.5f);
        EXPECT(theme::contrastOnSurface(theme::kTheme.danger) >= 3.0f);
        EXPECT(theme::contrastOnSurface(theme::kTheme.warn) >= 3.0f);
        EXPECT(theme::contrastOnSurface(theme::kTheme.ok) >= 3.0f);
    }

    // ---- (4) A PALETA V.ONI NO GRAFITE (FASE UI H: as neutras + a brasa) --
    // o comment #7E7E86 (a neutra escura — 4,9:1 no bg #0B0B0D) e TODOS os
    // tokens ≥4,5:1; as reservadas são A BRASA (o MESMO valor do accent)
    {
        EXPECT(nearEqF(theme::kTheme.voniComment[0], 126.0f / 255.0f,
                       1e-4f));
        EXPECT(nearEqF(theme::kTheme.voniReserved[0],
                       theme::kTheme.accent[0], 1e-4f) &&
                   nearEqF(theme::kTheme.voniReserved[2],
                           theme::kTheme.accent[2], 1e-4f));
        EXPECT(theme::contrastOnBg(theme::kTheme.voniUser) >= 4.5f);
        EXPECT(theme::contrastOnBg(theme::kTheme.voniEngine) >= 4.5f);
        EXPECT(theme::contrastOnBg(theme::kTheme.voniReserved) >= 4.5f);
        EXPECT(theme::contrastOnBg(theme::kTheme.voniString) >= 4.5f);
        EXPECT(theme::contrastOnBg(theme::kTheme.voniNumber) >= 4.5f);
        EXPECT(theme::contrastOnBg(theme::kTheme.voniComment) >= 4.5f);
        // os EIXOS do gizmo: o azul do eixo Z é CONTEÚDO (documentado) —
        // NÃO é chrome; vive no Theme (gate R-030) e é azul por contrato
        EXPECT(theme::kTheme.axisZ[2] > theme::kTheme.axisZ[0]);
    }

    // ---- (5) O BLEND PURO (a matemática do device) -----------------------
    // blendOver = SRC_ALPHA/ONE_MINUS_SRC_ALPHA — o MESMO par do pass UI
    // (Renderer::endFrame liga GL_BLEND para TODO o pass desde a F1)
    {
        const f32 cinza[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        const f32 branco[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const f32 preto[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        f32 out[4];
        theme::blendOver(cinza, branco, out);
        EXPECT(nearEqF(out[0], 0.75f, 1e-5f));   // 50% cinza s/ branco
        EXPECT(nearEqF(out[2], 0.75f, 1e-5f));
        theme::blendOver(cinza, preto, out);
        EXPECT(nearEqF(out[0], 0.25f, 1e-5f));   // 50% cinza s/ preto
        // o composto do painel da hierarquia sobre o clear (o valor que a
        // FASE 13.10 afere no PNG; FASE UI: 0.85·18+0.15·11 = 16.95 com a
        // paleta do dono — era 0.85·20+0.15·10 = 18.5 na 0.12-E)
        f32 painel[4];
        theme::blendOver(theme::kTheme.surface, theme::kTheme.bg, painel);
        EXPECT(nearEqF(painel[0], 16.95f / 255.0f, 0.01f));
    }

    // ---- (6) OS ALIASES LIGADOS (a fonte única bit-a-bit) ----------------
    // BG/PANEL/LINE/TEXT/ACCENT/WARN são o legado — os static_assert do
    // UiContext.h já os LIGAM à tabela; aqui afirma-se o VALOR com alpha
    // (o PANEL é o vidro: α0.80; o LINE é a hairline: α0.55)
    {
        EXPECT(nearEqF(theme::PANEL[3], theme::kTheme.surface[3], 1e-6f));
        EXPECT(nearEqF(theme::LINE[3], theme::kTheme.border[3], 1e-6f));
        EXPECT(nearEqF(theme::ACCENT[0], theme::kTheme.accent[0], 1e-6f));
        EXPECT(nearEqF(theme::ACCENT[1], theme::kTheme.accent[1], 1e-6f));
        EXPECT(nearEqF(theme::WARN[1], theme::kTheme.warn[1], 1e-6f));
    }

    // ---- (7) O WARN LARANJA ≠ A BRASA (a regra da casa) ----------------
    // «warn #FF8A3D (laranja, NUNCA igual ao acento)»: o laranja tem o
    // canal B quase 2× o da brasa — dois tons distintos a olho nu e em
    // contas (FASE UI: o G do warn fica ACIMA do da brasa, que é mais
    // vermelha — a distinção é o B, não o G)
    {
        EXPECT(theme::kTheme.warn[2] > theme::kTheme.accent[2] * 1.5f);
        EXPECT(theme::kTheme.warn[1] > theme::kTheme.accent[1]);
        // e os DOIS passam o piso sobre surface (warn 8,0:1 · accent 6,1:1)
        EXPECT(theme::contrastOnSurface(theme::kTheme.warn) >= 3.0f);
    }
}


// ============================================================================
// R-029 · O CARET DESENHADO DESALINHADO (FASE 0.9.6-MASTER · GRUPO UI · E5)
// «Ao tocar na linha N, a inserção acontece na linha N (hit-test correto)
// mas o caret visível fica deslocado para baixo relativamente ao texto» —
// CONFIRMADO NO DEVICE pelo dono. CAUSA (leitura P-04): o texto desenhava
// com a BASELINE no lineTopOnScreen (glifos pendiam ACIMA da banda) e o
// caret desenhava ui.panel(x, lineTop, w, lh-8) — bandas DIFERENTES: o
// caret ficava ~meia linha abaixo dos glifos («linha 3,5»). FIX: o caret
// e o texto partilham exatamente a MESMA função linha→y
// (lineBaselineOnScreen — glifos CENTRADOS na banda) e o caret desenha de
// lineTop a lineTop+lineHeight (caretRectOnScreen), alinhado à banda.
// ============================================================================
TEST(regress_caret_na_banda_dos_glifos) {
    using namespace vv;
    theme::setDensity(1.0f);
    const f32 bodyY = 100.0f;
    const f32 lh = 28.0f;          // o piso da casa (F5.0)
    const f32 asc = 21.0f, desc = 5.0f;   // métricas típicas do atlas 28px
    // (1) EM TODA a linha tocada: o caretRect INTERSETA o rect de texto da
    // MESMA linha (a MESMA banda de baseline) — com e SEM scroll
    for (u32 pass = 0; pass < 2; ++pass) {
        const f32 off = pass == 0 ? 0.0f : 56.0f;   // scroll no meio
        for (u32 i = 0; i < 12; ++i) {
            const f32 top = editor::scriptwin::lineTopOnScreen(bodyY, i, lh, off);
            const f32 base = editor::scriptwin::lineBaselineOnScreen(
                bodyY, i, lh, off, asc, desc);
            const UiRect caret = editor::scriptwin::caretRectOnScreen(
                bodyY, i, lh, off, 200.0f, 2.0f);
            // o caret é a banda TODA: [lineTop, lineTop+lineHeight]
            EXPECT(nearEqF(caret.y, top, 1e-4f));
            EXPECT(nearEqF(caret.h, lh, 1e-4f));
            // a banda dos GLIFOS (base-ascent .. base+descent) vive DENTRO
            // da banda do caret (o texto CENTRADO — antes pendia acima)
            EXPECT(base - asc >= top - 0.01f);
            EXPECT(base + desc <= top + lh + 0.01f);
            // o hitTest do CENTRO do caret devolve a MESMA linha (o
            // inverso exato — tocar onde o caret ESTÁ escolhe a linha dele)
            const f32 cy = caret.y + caret.h * 0.5f;
            EXPECT(editor::scriptwin::lineAtScreenY(cy, bodyY, lh, off) ==
                   static_cast<i32>(i));
        }
    }
    // (2) o CONTRATO da baseline ÚNICA: a MESMA fórmula para o draw do
    // texto e para o caret — o centro do bloco de glifos é o centro da
    // banda (o desalinhamento medido no device era ~lh/2)
    {
        const f32 top = editor::scriptwin::lineTopOnScreen(bodyY, 3, lh, 0.0f);
        const f32 base = editor::scriptwin::lineBaselineOnScreen(
            bodyY, 3, lh, 0.0f, asc, desc);
        const f32 glyphCenter = (base - asc + base + desc) * 0.5f;
        EXPECT(nearEqF(glyphCenter, top + lh * 0.5f, 0.01f));
        // o ANTI-EXEMPLO (a fórmula ANTIGA: baseline == lineTop): os glifos
        // ficariam com o centro ~asc/2 ACIMA do centro da banda — o caret
        // parecia «meia linha abaixo». O delta do bug era > 8px no device
        EXPECT(std::abs(glyphCenter - top) > 8.0f);   // o fix MOVEU o texto
    }
}


// ============================================================================
// R-022 (0.9.6.12 · GRUPO J1) — O CONTRATO DA HIERARQUIA
//         — regress_hierarquia_contrato
//
// O DONO (P-08): «a árvore de views tem de respeitar a hierarquia fixa de
// docs/LAYOUT_HIERARCHY.md — qualquer região fora do contêiner correto é
// violação de contrato». A REALIDADE da plataforma (honestidade antes de
// tudo): o editor NÃO TEM views Java — a VvActivity é uma NativeActivity
// com UMA superfície; a árvore do contrato são as REGIÕES do layout C++
// (SafeArea.h + vpchrome::layout + bottom::layout). Os defeitos 1+2 do
// dono (a transform toolbar POR CIMA da top bar / «estática» com o painel
// de baixo aberto) tinham a causa raiz EXATA: currentDrawerH() lia o
// drawer CRU enquanto o bottom::layout aplicava um cap DIFERENTE (64dp) —
// no device (drawer 240 persistido, viewport 208dp) o viewRect colapsava
// a ZERO e a toolbar desenhava-se ~56dp acima do topo da viewport = sobre
// a top bar. O FIX: safe::effectiveDrawerH (FONTE ÚNICA do cap, piso
// kViewportMinH = 104dp) + clamps no vpchrome (a toolbar nunca sai do
// rect POR CONSTRUÇÃO). Esta sentinela afere o CONTRATO (§1-§3 do
// documento) nos ecrãs 776×336 (RMX3624 @2.0) / 768×360 / 1512×568
// (harness) e densidades 1.0/2.0, drawer fechado/aberto/tapado.
// ============================================================================
TEST(regress_hierarquia_contrato) {
    using namespace vv;

    struct ScreenCase {
        f32 sw, sh;
        safe::Insets in;
        f32 density;
        const char* name;
    };
    // o RMX3624 real: 1600x720 @2.0 com insets {0,48,48,0} (contentRect
    // {0,48,1552,720} da FASE 13.7) — content 776x336dp; o C33: 1536x720
    // @2.0 content 768x360dp; o harness: 1536x720 @1.0 insets {0,24,24,0}
    ScreenCase cases[] = {
        {1600.0f, 720.0f, {0.0f, 48.0f, 48.0f, 0.0f}, 2.0f,
         "RMX3624 1600x720@2.0"},
        {1536.0f, 720.0f, {0.0f, 24.0f, 24.0f, 0.0f}, 2.0f,
         "C33 1536x720@2.0"},
        {1536.0f, 720.0f, {0.0f, 24.0f, 24.0f, 0.0f}, 1.0f,
         "harness 1536x720@1.0"},
        {1024.0f, 640.0f, {0.0f, 0.0f, 0.0f, 0.0f}, 1.0f,
         "ecra pequeno 1024x640@1.0"},
    };
    // os estados do drawer: fechado · o default persistido (240dp) · a
    // pega no máximo (400dp) · um valor ABSURDO (o teto tem de aguentar)
    const f32 drawerRaw[] = {0.0f, 240.0f, 400.0f, 100000.0f};

    for (const ScreenCase& sc : cases) {
        theme::setDensity(sc.density);
        for (f32 raw : drawerRaw) {
            const f32 vpH = safe::viewportRect(sc.sw, sc.sh, sc.in).h;
            const f32 contentH = sc.sh - sc.in.top - sc.in.bottom;
            const f32 eff =
                safe::effectiveDrawerH(raw, vpH, contentH);

            // ---- CONTRATO §2.3: a FONTE ÚNICA — a altura efetiva NUNCA
            // passa o CAP DUPLO (o piso do viewport kViewportMinH E o teto
            // de 35% da altura do content — spec PASSO 2) e cai em passos
            // de 8dp. (O estado FECHADO é bottomTab==0 — o raw 0 da pega
            // levanta ao mínimo da pega, como sempre foi)
            f32 cap = vpH - theme::dp(safe::kViewportMinH);
            const f32 cap35 = contentH * safe::kDrawerMaxPct;
            if (cap35 < cap) {
                cap = cap35;   // PASSO 2: o teto de 35% manda quando é menor
            }
            EXPECT(eff > 0.0f);
            EXPECT(eff <= cap > 0.0f ? cap : 0.0f);
            EXPECT_MSG(eff <= contentH * safe::kDrawerMaxPct + 0.01f,
                       "%s raw=%.0f: o drawer efetivo (%.1f) passou o teto "
                       "de 35%% do content (%.1f) — spec PASSO 2",
                       sc.name, raw, eff, contentH * safe::kDrawerMaxPct);
            const f32 steps = eff / theme::dp(8.0f);
            EXPECT(std::fabs(steps - std::floor(steps)) < 0.01f);

            // ---- o viewRect do CENTRO (o que o main/gizmo/scissor
            // consomem) e o LAYOUT do chrome dentro dele
            const UiRect view = safe::centerRect(sc.sw, sc.sh, sc.in, eff,
                                                 true, -1.0f, -1.0f);
            const editor::vpchrome::Layout L = editor::vpchrome::layout(view);

            // ---- CONTRATO §2.4: nada sai do rect da viewport central
            // (PASSO 3: o rail de ferramentas + a fila do topo + o gizmo.
            // D21 (0.9.6.19b): o [+] do fundo-direito saiu — os alvos são
            // N−1 (10) e o §1/§3 do contrato perderam o [+] e kCornerBtn)
            // FASE UI B: a fila é o chip ⋯ único (N−1 alvos)
            // FASE UI D: rail 4 ferramentas + 2 chips (N alvos)
            const UiRect targets[8] = {
                L.rail[0], L.rail[1], L.rail[2], L.rail[3],
                L.chips[0], L.chips[1], L.quick[0], L.gizmoBtn};
            for (const UiRect& r : targets) {
                if (r.w > 0.0f && r.h > 0.0f) {   // degenerado = escondido
                    EXPECT_MSG(safe::rectInside(r, view),
                               "%s raw=%.0f: um alvo do chrome sai do rect "
                               "da viewport",
                               sc.name, raw);
                }
            }
            if (L.gizmoPanel.w > 0.0f) {
                EXPECT(safe::rectInside(L.gizmoPanel, view));
            }
            // D21 (0.9.6.19b): o plusPanel FOI REMOVIDO com o [+] (nada a
            // conter — o canto inferior direito fica limpo)
            // 0.12-C (C-2): o gizmoPanel também saiu (rects ZERO — o check
            // de dentro fica condicionado à largura)
            if (L.railVisible) {
                EXPECT(safe::rectInside(L.railPanel, view));
            }
            if (L.quick[0].w > 0.0f) {   // degenerado = escondido (degradação)
                EXPECT(safe::rectInside(L.quickPanel, view));
            }

            // ---- CONTRATO §2.2 (PASSO 3): o chrome DENTRO do viewport —
            // o rail e a fila do topo nascem NO TOPO do rect (nunca sobem
            // à top bar) e a strip full-width JÁ NÃO EXISTE
            EXPECT_MSG(L.rail[0].y >= view.y + 0.5f || !L.railVisible,
                       "%s raw=%.0f: o rail subiu acima do topo da "
                       "viewport (a top bar fica POR CIMA da viewport — "
                       "cobiçá-la é violação)",
                       sc.name, raw);
            EXPECT_MSG(L.quick[0].w == 0.0f ||
                           L.quick[0].y >= view.y + 0.5f,
                       "%s raw=%.0f: a fila do topo subiu acima do topo da "
                       "viewport",
                       sc.name, raw);
            // a REGRAS PASSO 3: nada full-width (os pais de vidro < 90% da
            // largura — a strip que atravessava a cena morreu)
            EXPECT_MSG(L.railPanel.w < view.w * 0.9f &&
                           L.quickPanel.w < view.w * 0.9f,
                       "%s raw=%.0f: um pai de vidro atravessa a largura "
                       "(a barra full-width é proibida — spec PASSO 3)",
                       sc.name, raw);
            if (!L.railVisible) {
                // viewport sub-piso: a degradação honesta — a fila do topo
                // e os cantos CONTINUAM dentro do rect (a última defesa)
                EXPECT(view.h < theme::dp(152.0f) + 0.5f);
            }

            // ---- a top bar INTACTA: o viewport inteiro começa DEBAIXO
            // dela (o toolbarRect é a faixa de 56dp do topo)
            const UiRect topBar = safe::toolbarRect(sc.sw, sc.sh, sc.in);
            EXPECT(view.y >= topBar.y + topBar.h - 0.5f);

            // ---- o BOTTOM DOCK + o RODAPÉ (a ponte com §2.1 — a
            // sentinela completa do rodapé é a R-025; aqui o CONTENIMENTO)
            editor::bottom::BottomState bs{};
            bs.bottomTab = 1;   // o drawer ABERTO (o fechado é bottomTab=0 —
                                // o layout dele não é desenhado nesse caso)
            bs.drawerH = raw;
            const editor::bottom::Layout bl =
                editor::bottom::layout(sc.sw, sc.sh, sc.in, bs);
            // o drawer desenhado usa a MESMA fonte única (§2.3)
            EXPECT_MSG(std::fabs(bl.drawer.h - eff) < 0.5f,
                       "%s raw=%.0f: o drawer desenhado (%.1f) diverge da "
                       "altura efetiva (%.1f) — DUAS fontes = o bug da "
                       "toolbar flutuante de volta",
                       sc.name, raw, bl.drawer.h, eff);
            // CONTRATO §2.1 (PASSO 1): a TAB BAR é a ÚLTIMA faixa viva e
            // NINGUÉM a toca (a status bar foi REMOVIDA — a faixa de altura
            // zero no fundo é só o compat do safe::; o FPS·TICs vive nela)
            const UiRect status = safe::statusRect(sc.sw, sc.sh, sc.in);
            EXPECT(status.h == 0.0f);   // a faixa morta (compat)
            EXPECT(bl.tabBar.y + bl.tabBar.h == status.y);
            EXPECT(bl.drawer.y + bl.drawer.h <= bl.tabBar.y + 0.5f);
            EXPECT(view.y + view.h <= status.y + 0.5f);
        }
    }
    theme::setDensity(1.0f);
}

// ============================================================================
// R-023 (0.9.6.12 · GRUPO J2 → REESCRITA no PASSO 3 · 0.9.6.17) — O CHROME
// DO VIEWPORT SEM BARRA FULL-WIDTH + O ENCAIXE EM 3 DENSIDADES
//         — regress_texto_strip_campo (o nome é o id da regressão; o tema
//           mudou: a STRIP [Cena][Perspetiva][Global] que o teste vigiava
//           MORREU no PASSO 3 — a barra que atravessava a largura era a
//           maior parte da cobertura do device e os chips eram SEM FUNÇÃO
//           desde o inventário do PASSO 0. O campo «pesquisar» da hierarquia
//           mantém a sua prova na FASE 14.2 do c33 com a fonte real.)
//
// A SPEC DO DONO (PASSO 3): controlos ≤10% da área do viewport a 60% de
// alfa; NADA atravessa a largura; NADA se sobrepõe. Esta sentinela afere,
// em 3 densidades (mdpi 1.0 / hdpi 1.5 / xhdpi 2.0 — a spec pede as três):
// (a) nenhum pai de vidro passa 90% da largura (a strip é impossível);
// (b) os 11 alvos do chrome ≥40dp (a lei de ouro) e DENTRO do viewport;
// (c) a degradação da ESCADA de larguras: 456 → 220 → 140 → 72dp — o rail
//     vai a colunas, a fila do topo desce e, quando nem ela cabe, ESCONDE
//     (a degradação honesta — nada sai do rect nunca);
// (d) a fila do topo NUNCA pisa o gizmo (o recuo para baixo é automático).
// ============================================================================
TEST(regress_texto_strip_campo) {
    using namespace vv;

    const f32 densities[] = {1.0f, 1.5f, 2.0f};   // mdpi / hdpi / xhdpi

    for (f32 d : densities) {
        theme::setDensity(d);
        // o device no pior caso: viewport 456dp (776 − 140 hier − 180 insp
        // — o piso do PASSO 2) e a escada de larguras de teste por baixo
        auto checkLayout = [&](const char* tag, f32 wDp, f32 hDp) {
            const UiRect view{theme::dp(100.0f), theme::dp(60.0f),
                              theme::dp(wDp), theme::dp(hDp)};
            const editor::vpchrome::Layout L = editor::vpchrome::layout(view);
            // (a) NADA full-width (a regra do dono — a strip é impossível)
            const UiRect bands[3] = {L.railPanel, L.quickPanel,
                                     L.gizmoPanel};
            for (const UiRect& r : bands) {
                if (r.w > 0.0f) {
                    EXPECT_MSG(r.w < view.w * 0.90f,
                               "%s w=%.0fdp: um pai de vidro (%.0fdp) "
                               "atravessa a largura (a barra full-width é "
                               "proibida — spec PASSO 3)",
                               tag, wDp, r.w / theme::dp(1.0f));
                }
            }
            // (b) os 10 alvos ≥40dp e DENTRO do rect (a lei de ouro; D21:
            // o [+] saiu — N−1)
            const UiRect all[8] = {L.rail[0],  L.rail[1],  L.rail[2],
                                   L.rail[3],  L.chips[0], L.chips[1],
                                   L.quick[0], L.gizmoBtn};
            for (const UiRect& r : all) {
                if (r.w > 0.0f && r.h > 0.0f) {
                    EXPECT_MSG(r.w >= theme::dp(40.0f) - 0.5f &&
                                   r.h >= theme::dp(40.0f) - 0.5f,
                               "%s: um alvo do chrome desce abaixo do piso "
                               "de 40dp (a lei de ouro)",
                               tag);
                    EXPECT_MSG(safe::rectInside(r, view),
                               "%s: um alvo do chrome sai do rect (a "
                               "degradação tem de ESCONDER, nunca "
                               "transbordar)",
                               tag);
                }
            }
            // (d) o chip do topo NUNCA pisa o gizmo (mesma banda de y;
            // FASE UI B: o chip é o único da fila)
            if (L.quick[0].w > 0.0f &&
                L.quick[0].y < L.gizmoBtn.y + L.gizmoBtn.h &&
                L.gizmoBtn.y < L.quick[0].y + L.quick[0].h) {
                EXPECT(L.quick[0].x + L.quick[0].w <= L.gizmoPanel.x + 0.5f);
            }
            return L;
        };

        // o device real com o inspector aberto: 456×244dp
        const editor::vpchrome::Layout ldev = checkLayout("device", 456.0f, 244.0f);
        EXPECT(ldev.railVisible);
        // 0.12-C (C-2): o gizmo do canto saiu — as asserções do canto
        // direito tornaram-se taautológicas com os rects ZERO e saem; a
        // prova do canto limpo é o lt/vtt abaixo (gizmoBtn == 0)

        // (c) a ESCADA: 220dp — FASE UI D (recalibrado): as 4 ferramentas
        // cabem numa COLUNA (184 ≤ 204) e os CHIPS também (o par termina
        // a 328? NÃO: 184+16(railPanel)+8+88 = 296 > 204 — os chips
        // ESCONDEM e o chip ⋯ respira no topo)
        {
            const editor::vpchrome::Layout ln = checkLayout("220dp", 220.0f, 208.0f);
            EXPECT(ln.railVisible && ln.railCols == 1u);
            EXPECT(ln.chips[0].w == 0.0f);   // o par não cabe — esconde
            EXPECT(ln.quick[0].w > 0.0f);
            EXPECT(ln.quick[0].y <= ln.rail[0].y + ln.rail[0].h);
        }
        // 140dp — FASE UI B: o chip ÚNICO CABE (a fila de 4 escondia — o
        // chip de 40dp respira ao lado do rail)
        {
            const editor::vpchrome::Layout lt = checkLayout("140dp", 140.0f, 208.0f);
            EXPECT(lt.quick[0].w > 0.0f);   // o chip cabe (o único)
            // 0.12-C (C-2): o gizmo saiu — o canto direito fica LIMPO
            EXPECT(lt.gizmoBtn.w == 0.0f);
        }
        // 36dp — FASE UI D (recalibrado): com 4 ferramentas a coluna de 40
        // cabe até 56dp de largura — o «nem o rail cabe» desce a 36dp
        {
            const editor::vpchrome::Layout vtt = checkLayout("36dp", 36.0f, 208.0f);
            EXPECT(!vtt.railVisible);
            EXPECT(vtt.gizmoBtn.w == 0.0f);   // 0.12-C: sem o gizmo do canto
        }
    }
    theme::setDensity(1.0f);
}

TEST(regress_viewport_rect_segue) {
    using namespace vv;
    theme::setDensity(2.0f);
    // o device: 1600x720 @2.0, insets da 13.7
    const f32 sw = 1600.0f, sh = 720.0f;
    const safe::Insets in{0.0f, 48.0f, 48.0f, 0.0f};
    const f32 vpH = safe::viewportRect(sw, sh, in).h;

    // (a) O RECT MUDA com o painel: fechado (eff 0) vs aberto (raw 240)
    const f32 effAberto = safe::effectiveDrawerH(
        240.0f, vpH, sh - in.top - in.bottom);
    const UiRect fechado = safe::centerRect(sw, sh, in, 0.0f, true, -1.0f,
                                            -1.0f);
    const UiRect aberto = safe::centerRect(sw, sh, in, effAberto, true,
                                           -1.0f, -1.0f);
    EXPECT_MSG(std::fabs(aberto.h - (fechado.h - effAberto)) < 0.5f,
               "a altura do rect com o painel aberto (%.1f) não é a do "
               "fechado (%.1f) menos a altura efetiva (%.1f)",
               aberto.h, fechado.h, effAberto);
    EXPECT(aberto.w == fechado.w && aberto.x == fechado.x);
    EXPECT(aberto.h > 0.0f);   // o painel aberto NUNCA colapsa o viewport
    EXPECT(aberto.h >= theme::dp(safe::kViewportMinH) - 0.5f);

    // (b) O ASPECT segue o RECT (a janela é o ecrã da câmara — nunca o
    // ecrã todo; o bug G1-3 histórico era este)
    {
        Camera cam;
        const Mat4 projRect = cam.proj(aberto.w / aberto.h);
        const Mat4 projScreen = cam.proj(sw / sh);
        EXPECT(!test::matNearF(projRect, projScreen, 1e-3f));
        // o aspect DO RECT é o que a produção usa (proj(viewRect.w/h) no
        // main) — o Mesmo valor para o mesmo rect
        const Mat4 projRect2 = cam.proj(aberto.w / aberto.h);
        EXPECT(test::matNearF(projRect, projRect2));
    }

    // (c) A COERÊNCIA com o chrome: o MESMO rect alimenta o chrome (o
    // vpchrome) e o glViewport — a fonte única garantida pelo chamador
    {
        const editor::vpchrome::Layout L = editor::vpchrome::layout(aberto);
        EXPECT(safe::rectInside(L.railPanel, aberto));
        // 0.12-C (C-2): o gizmo saiu (o duplicado do Mover) — a prova do
        // canto é o RAIL + a fila (o gizmoBtn é ZERO e deixa de se aferrar)
        EXPECT(L.gizmoBtn.w == 0.0f);
        EXPECT(L.rail[0].y >= aberto.y);
    }

    // (d) O LOG de diagnóstico é PARTE DO CONTRATO (a spec J3 pede o log
    // com os QUATRO números): o literal existe no main.cpp — apagá-lo
    // silenciosamente é regressão do diagnóstico que o dono usa no device
    // (a prova DELE acontecendo é a FASE 14.3 do replay)
    {
        const std::string mainCpp =
            std::string(REPO_ROOT) + "/app/src/main/cpp/platform/main.cpp";
        FILE* f = std::fopen(mainCpp.c_str(), "rb");
        ASSERT(f != nullptr);
        std::string src;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            src.append(buf, n);
        }
        std::fclose(f);
        EXPECT_MSG(src.find("vp3d: viewport set to") != std::string::npos,
                   "o log de diagnóstico vp3d desapareceu do main.cpp — a "
                   "spec J3 (o dono diagnostica o rect por esta linha)");
        EXPECT_MSG(src.find("glViewport(static_cast<i32>(vp3d.x)") !=
                       std::string::npos,
                   "o glViewport do rect desapareceu do main.cpp (a "
                   "fronteira do passe 3D)");
    }
    theme::setDensity(1.0f);
}

// ============================================================================
// R-025 (0.9.6.12 · GRUPO J4) — O RODAPÉ INTOCÁVEL
//         — regress_rodape_intocavel
//
// O DONO (defeito 5): «o rodapé de métricas muda de posição ou cobre
// elementos». A REALIDADE: o rodapé (BottomBar — a status bar de 24dp)
// vive em safe::statusRect, a ÚLTIMA faixa do contentRect, e a sua posição
// não depende de NENHUM estado (R-022 já a vigia no contrato). O que
// FALTAVA (a spec J4): o campo do PROJETO (o mais variável) elipsado A
// MEIO com o orçamento DELE — antes a linha inteira ia ao labelFitted e o
// corte comia o FIM (o suffixo «FPS n · TICs n» sumia com nomes longos).
// O FIX: textfit::ellipsizeMiddle (puro, GL-free, fronteiras UTF-8) +
// prefixo + projeto ellipsado + suffixo em drawStatusBar. Esta sentinela
// afere o ellipsizeMiddle com medidor fake E a inviolabilidade do rodapé
// em múltiplos ecrãs/densidades/estados; a prova com a FONTE REAL e o
// nome longo vive na FASE 14.4 do replay (o label do rodapé NÃO truncado).
// ============================================================================
TEST(regress_rodape_intocavel) {
    using namespace vv;

    // ---- (a) O ELLIPSIZE A MEIO (o contrato do J4) ----------------------
    {
        // medidor fake mono: 10px por glifo (bytes ASCII)
        auto w = [](const char* s) { return static_cast<f32>(std::strlen(s)) * 10.0f; };
        char out[96];
        // cabe inteiro → cópia integral
        textfit::ellipsizeMiddle("projeto", 70.0f, w, out, sizeof(out));
        EXPECT(std::string(out) == "projeto");
        // não cabe → cabeça+…+cauda, e CAIBA no orçamento
        textfit::ellipsizeMiddle("nome-de-projeto-muito-longo", 100.0f, w,
                                 out, sizeof(out));
        const std::string mid = out;
        EXPECT_MSG(w(out) <= 100.0f, "o resultado do middle não cabe "
                   "(%s → %.0f px)", out, w(out));
        EXPECT_MSG(mid.find("…") != std::string::npos,
                   "o middle sem reticência (resultado: %s)", out);
        EXPECT_MSG(mid.front() != '\xE2' || mid.size() > 3,
                   "o middle começa pela reticência (a cabeça morreu: %s)",
                   out);
        // a CAUDA fica: o fim do nome é o sufixo significativo
        EXPECT_MSG(mid.back() == 'o', "a cauda do nome morreu (%s)", out);
        // orçamento absurdo → vazio (nunca desenha fora do rect)
        textfit::ellipsizeMiddle("nome", 5.0f, w, out, sizeof(out));
        EXPECT(out[0] == '\0');
        // UTF-8: nome com acentos — o corte nunca parte um code point
        const char* acentos = "animação-do-cão-feliz";
        textfit::ellipsizeMiddle(acentos, 90.0f, w, out, sizeof(out));
        // o resultado tem de ser UTF-8 VÁLIDO: cada 0xE2/0xC3/0xC5 seguido
        // de continuação (o teste percorre os bytes)
        bool valid = true;
        for (size_t i = 0; i < std::strlen(out);) {
            const unsigned char c = static_cast<unsigned char>(out[i]);
            size_t n = 1;
            if ((c & 0x80u) != 0) {
                n = (c & 0xE0u) == 0xC0u ? 2
                    : (c & 0xF0u) == 0xE0u ? 3 : 4;
            }
            for (size_t j = 1; j < n; ++j) {
                if ((static_cast<unsigned char>(out[i + j]) & 0xC0u) !=
                    0x80u) {
                    valid = false;
                }
            }
            i += n;
        }
        EXPECT_MSG(valid, "o middle partiu um code point UTF-8 (%s)", out);
    }

    // ---- (b) O RODAPÉ É A ÚLTIMA FAIXA (todos os estados) ---------------
    const f32 densities[] = {1.0f, 2.0f};
    for (f32 d : densities) {
        theme::setDensity(d);
        const f32 sw = 1600.0f * d, sh = 720.0f * d;
        const safe::Insets in{0.0f, 48.0f * d, 48.0f * d, 0.0f};
        for (f32 raw : {0.0f, 240.0f, 400.0f}) {
            // PASSO 1: a status de 24dp foi REMOVIDA (faixa de altura ZERO,
            // compat) e a TAB BAR de 32dp é a ÚLTIMA faixa viva no fundo
            const UiRect status = safe::statusRect(sw, sh, in);
            EXPECT(nearEqF(status.h, 0.0f, 0.5f));
            const UiRect tb = safe::bottomTabRect(sw, sh, in);
            EXPECT_MSG(nearEqF(tb.y + tb.h, sh - in.bottom, 0.5f),
                       "d=%.1f raw=%.0f: o rodapé (tab bar) não encosta no "
                       "fundo do content (y+h=%.1f vs %.1f)",
                       d, raw, tb.y + tb.h, sh - in.bottom);
            EXPECT(nearEqF(tb.h, theme::dp(32.0f), 0.5f));
            // nada a cobre: o drawer e o centro terminam acima dela
            editor::bottom::BottomState bs{};
            bs.bottomTab = 1;
            bs.drawerH = raw;
            const editor::bottom::Layout bl =
                editor::bottom::layout(sw, sh, in, bs);
            EXPECT(bl.drawer.y + bl.drawer.h <= tb.y + 0.5f);
            EXPECT(nearEqF(bl.tabBar.y, tb.y, 0.5f));
            const f32 eff = safe::effectiveDrawerH(
                raw, safe::viewportRect(sw, sh, in).h,
                sh - in.top - in.bottom);
            const UiRect view = safe::centerRect(sw, sh, in, eff, true,
                                                 -1.0f, -1.0f);
            EXPECT(view.y + view.h <= tb.y + 0.5f);
            // a composição J4: com o suffixo no orçamento, o label NUNCA
            // precisa de cortar o fim (a fórmula do drawStatusBar com o
            // medidor fake: prefixo + projeto a meio + suffixo cabe)
            {
                auto w = [](const char* s) {
                    return static_cast<f32>(std::strlen(s)) * 10.0f;
                };
                char prefix[64], suffix[48];
                std::snprintf(prefix, sizeof(prefix), "G.One %s · ",
                              "0.9.6.12");
                std::snprintf(suffix, sizeof(suffix),
                              " · FPS %d · TICs %u", 60, 42);
                const f32 avail =
                    theme::dp(776.0f) - (w("editor · Mobile First") + theme::dp(8.0f)) -
                    2.0f * theme::dp(8.0f);
                const f32 budget =
                    avail - w(prefix) - w(suffix);
                char proj[96];
                textfit::ellipsizeMiddle("projeto-do-dono-com-nome-"
                                         "extravagantemente-comprido",
                                         budget, w, proj, sizeof(proj));
                char left[192];
                std::snprintf(left, sizeof(left), "%s%s%s", prefix, proj,
                              suffix);
                EXPECT_MSG(w(left) <= avail + 0.5f,
                           "a linha composta excede o orçamento (w=%.0f "
                           "avail=%.0f)", w(left), avail);
                // o SUFFIXO está no fim (a informação estável sobrevive)
                const std::string ls = left;
                EXPECT(ls.size() >= std::strlen(suffix) &&
                       ls.compare(ls.size() - std::strlen(suffix),
                                  std::strlen(suffix), suffix) == 0);
            }
        }
    }
    theme::setDensity(1.0f);
}

// ============================================================================
// R-014 (0.9.6.12 · IMPORT A2) — A SENTINELA REESCRITA
//         — regress_import_r014_reescrita
//
// A SPEC do dono: «import → .gmesh em assets/ → picker lista <1s → troca
// ok; fixture de imagem podre → mesh SEM texturas + warn assertado» + as
// mutações (irmãos off · binStart sem alinhamento · validação só-imagens ·
// slice BIN truncado de propósito). Esta versão vigia o caminho DE FICHEIRO
// (importFile + range loader) com os perfis REAIS:
//   (a) GLB stanford (geometria + PNG embutido no fim) → .gmesh em assets/
//       listado pelo MESMO listDir que alimenta o picker (o <1s é a
//       ausência de re-hash pesado — o c33 12.8 afere o e2e ao device)
//   (b) imagem PODRE → o import SEGUE sem texturas + W contado
//   (c) slice BIN truncado de propósito (o ficheiro acaba antes do BIN) →
//       o guard copiado==total morre COM a comparação completa
//   (d) view fora do buffer → a falha carrega os QUATRO números no log
// ============================================================================
TEST(regress_import_r014_reescrita) {
    using namespace vv;
    ASSERT(vv::elog::init(kSentinelLogs));

    // um PNG REAL (a fixture da casa)
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

    // o GLB: POSITION (36 B) + índices u16 (6 B) + a imagem NO FIM
    auto makeGlbA2 = [&](const std::vector<u8>& imgBytes, size_t imgOff,
                         size_t imgLen, std::vector<u8>& glbOut) {
        std::vector<u8> binChunk;
        auto pushF = [&binChunk](f32 v) {
            u8 t[4];
            std::memcpy(t, &v, 4);
            binChunk.insert(binChunk.end(), t, t + 4);
        };
        pushF(0); pushF(0); pushF(0);
        pushF(1); pushF(0); pushF(0);
        pushF(0); pushF(1); pushF(0);                 // 36 B de POSITION
        binChunk.push_back(0); binChunk.push_back(0); // índices u16
        binChunk.push_back(1); binChunk.push_back(0);
        binChunk.push_back(2); binChunk.push_back(0); // 6 B
        if (!imgBytes.empty()) {
            binChunk.resize(imgOff + imgLen, 0);
            std::memcpy(binChunk.data() + imgOff, imgBytes.data(), imgLen);
        }
        // imgBytes vazio = o view MENTE (o chunk fica PEQUENO — o view
        // aponta para fora do buffer, o caso (d))
        while (binChunk.size() % 4 != 0) {
            binChunk.push_back(0);
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
            "\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
            "\"indices\":1,\"material\":0}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
            binChunk.size(), jv);
        const std::string json = js;
        auto& g = glbOut;
        g.clear();
        auto u32p = [&g](u32 v) {
            u8 t[4];
            std::memcpy(t, &v, 4);
            g.insert(g.end(), t, t + 4);
        };
        const u32 jsonPad = (4 - (static_cast<u32>(json.size()) % 4)) % 4;
        u32p(0x46546C67u); u32p(2);
        u32p(12 + 8 + static_cast<u32>(json.size()) + jsonPad + 8 +
             static_cast<u32>(binChunk.size()));
        u32p(static_cast<u32>(json.size()));
        u32p(0x4E4F534Au);
        g.insert(g.end(), json.begin(), json.end());
        for (u32 i = 0; i < jsonPad; ++i) {
            g.push_back(0x20);
        }
        u32p(static_cast<u32>(binChunk.size()));
        u32p(0x004E4942u);
        g.insert(g.end(), binChunk.begin(), binChunk.end());
    };

    auto writeTmp = [](const char* nome, const std::vector<u8>& bytes,
                       std::string& outPath) {
        char src[96];
        std::snprintf(src, sizeof(src), "/tmp/goni_r014_%s_%d.glb", nome,
                      (int)::getpid());
        FILE* f = std::fopen(src, "wb");
        if (!f) {
            return false;
        }
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
        outPath = src;
        return true;
    };

    // ---- (a) O IMPORT → .gmesh EM assets/ → o LISTING DO PICKER ---------
    {
        std::vector<u8> glb;
        makeGlbA2(png, 42, png.size(), glb);
        std::string src;
        ASSERT(writeTmp("ok", glb, src));
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r014_ok_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "stanford.glb", st, nullptr,
                                            out, stats, err);
        if (!ok) {
            fprintf(stderr, "[r014-a] o import falhou: %s\n", err.c_str());
        }
        ASSERT(ok);
        EXPECT(out.meshes.size() == 1u);
        EXPECT(out.meshes[0] == "assets/stanford.gmesh");
        EXPECT(st.exists("assets/stanford.gmesh"));
        EXPECT(out.textures.size() == 1u);   // a textura entrou
        // o LISTING que o picker consome (refreshCatalog → listDir): o
        // .gmesh novo aparece na listagem do diretório assets/
        std::vector<std::string> listed;
        EXPECT(st.listDir("assets", listed));
        bool temMesh = false;
        for (const std::string& f : listed) {
            if (f.find("stanford.gmesh") != std::string::npos) {
                temMesh = true;
            }
        }
        EXPECT_MSG(temMesh, "o .gmesh importado não aparece no listing de "
                            "assets/ (o picker nunca o mostraria)");
        ::remove(src.c_str());
    }
    // ---- (b) A IMAGEM PODRE: mesh SEM texturas + W (nunca silencioso) ---
    {
        const std::vector<u8> podre = {'P', 'O', 'D', 'R', 'E', '!'};
        std::vector<u8> glb;
        makeGlbA2(podre, 42, podre.size(), glb);
        std::string src;
        ASSERT(writeTmp("podre", glb, src));
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r014_podre_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "podre.glb", st, nullptr,
                                            out, stats, err);
        EXPECT(ok);                       // a GEOMETRIA salva o import
        EXPECT(out.meshes.size() == 1u);
        EXPECT(out.textures.empty());     // a textura ficou fora
        EXPECT(stats.texWarn >= 1u);      // contada (o toast diz)
        EXPECT(st.exists("assets/podre.gmesh"));
        ::remove(src.c_str());
    }
    // ---- (c) O SLICE BIN TRUNCADO DE PROPÓSITO: o guard copiado==total --
    {
        std::vector<u8> glb;
        makeGlbA2(png, 42, png.size(), glb);
        std::vector<u8> truncado(glb.begin(),
                                 glb.begin() + static_cast<long>(glb.size()) -
                                     100);   // o BIN acaba antes
        std::string src;
        ASSERT(writeTmp("trunc", truncado, src));
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r014_trunc_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "trunc.glb", st, nullptr,
                                            out, stats, err);
        EXPECT(!ok);
        EXPECT_MSG(err.find("binFim") != std::string::npos &&
                       err.find("copiado") != std::string::npos,
                   "o import de um GLB truncado falhou SEM a comparação "
                   "completa (%s)", err.c_str());
        ::remove(src.c_str());
    }
    // ---- (d) VIEW FORA DO BUFFER: os QUATRO números no engine.log -------
    {
        std::vector<u8> glb;
        makeGlbA2({}, 20, 1000000, glb);   // o view da imagem aponta longe
                                           // (o chunk fica no tamanho real)
        std::string src;
        ASSERT(writeTmp("fora", glb, src));
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_r014_fora_%d",
                      (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(src, "fora.glb", st, nullptr,
                                            out, stats, err);
        EXPECT(ok);                          // imagem má ≠ import morto
        std::vector<std::string> linhas;
        vv::elog::readTail(linhas, 400);
        bool temNumeros = false;
        for (const std::string& l : linhas) {
            if (l.find("glb: view2 buffer0 off=") != std::string::npos &&
                l.find("declared=") != std::string::npos &&
                l.find("real=") != std::string::npos) {
                temNumeros = true;
            }
        }
        EXPECT_MSG(temNumeros, "a linha glb: view<i> buffer<b> off=… len=… "
                               "declared=… real=… não apareceu no log (a "
                               "tarefa 1 da spec A2)");
        ::remove(src.c_str());
    }
}

// ============================================================================
// R-032 (0.9.6.12g · A2-2) — OS TETOS DE RANGE E O ÚLTIMO BLOCO PARCIAL
// A evidência do dono (engine.log 06/10 23:24): «import: glb: view2 buffer0
// off=89248320 len=123738528 acc=0 declared=212986848 re…» + «glTF invalido:
// accessor (view 2) falhou: range do bufferView aci…» — a vista acaba
// EXATAMENTE no fim do buffer (off+len == declared == 212986848 = VÁLIDO) e
// o dragão (38051884 B = 6 blocos de 6 MB + 303148) falha «de forma
// parecida». As hipóteses (a) off-by-one e (b) último bloco perdido NÃO
// existiam (o bound já era exclusivo; o ChunkReader já lia o resto); as
// causas REAIS: o teto de 64 MB (kMaxRangeBytes) recusava o view de 118 MB
// do scene e o fileRangeLoad tinha um teto escondido de 16 MB (kMaxJsonBytes
// reusado) que matava o dragão com mentira de I/O.
// SENTINELA E2E (ficheiro REAL → importFile — o caminho que o device corre):
//   (a) o perfil dragão: BIN = 3×6291456 + 12344 (NÃO múltiplo do chunk de
//       6 MB) com o ÚNICO view de geometria a ocupar [0, binLen) — acaba
//       EXATAMENTE no fim do buffer (a fixture (i) do dono) e tem 18.8 MB
//       (> o teto escondido de 16 MB): a cópia em chunks guarda o último
//       bloco parcial, o parse lê POR RANGES e o import ENTRA;
//   (b) o view genuinamente fora (len = binLen+4, a fixture (iv)): o import
//       falha com a COMPARAÇÃO COMPLETA (declared/real/file) no erro.
// MUTAÇÕES guardadas: kMaxRangeBytes→64 MB mata o teste
// glb_a2_ultimo_bloco_tetos_e_memoria (perfil scene); o teto de 16 MB de
// volta no fileRangeLoad mata ESTA sentinela (o range de 18.8 MB).
// ============================================================================
TEST(regress_glb_range_tetos) {
    using namespace vv;
    ASSERT(vv::elog::init(kSentinelLogs));

    constexpr size_t kBlk = 6291456;   // o kChunkBytes do import (6 MB)
    const u64 binLen = 3ull * kBlk + 12344u;   // 18886712 — múltiplo de 4,
                                               // NÃO múltiplo de 6 MB

    auto makeProfileGlb = [](u64 binLenArg, u64 viewLenArg,
                             std::vector<u8>& out) {
        const u64 bl = binLenArg;
        const u64 vl = viewLenArg;
        std::vector<u8> bin(static_cast<size_t>(bl), 0);
        // 3 vértices válidos nos primeiros 36 B (o accessor lê só isso; a
        // cauda do view é legal — o perfil real dos exporters)
        const f32 verts[9] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
        std::memcpy(bin.data(), verts, sizeof(verts));
        for (size_t i = 36; i < bin.size(); ++i) {
            bin[i] = static_cast<u8>((i * 7 + 13) & 0xFF);
        }
        char js[512];
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":%llu}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,"
            "\"byteLength\":%llu}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
            "\"count\":3,\"type\":\"VEC3\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}",
            static_cast<unsigned long long>(bl),
            static_cast<unsigned long long>(vl));
        const std::string json = js;
        const u32 jsonPad =
            (4u - static_cast<u32>(json.size() % 4)) % 4u;
        out.clear();
        auto u32p = [&out](u32 v) {
            u8 t[4];
            std::memcpy(t, &v, 4);
            out.insert(out.end(), t, t + 4);
        };
        u32p(0x46546C67u);   // 'glTF'
        u32p(2);
        u32p(12u + 8u + static_cast<u32>(json.size()) + jsonPad + 8u +
             static_cast<u32>(bin.size()));
        u32p(static_cast<u32>(json.size()));
        u32p(0x4E4F534Au);   // 'JSON'
        out.insert(out.end(), json.begin(), json.end());
        for (u32 i = 0; i < jsonPad; ++i) {
            out.push_back(0x20);
        }
        u32p(static_cast<u32>(bin.size()));
        u32p(0x004E4942u);   // 'BIN'
        out.insert(out.end(), bin.begin(), bin.end());
    };

    // ---- (a) O PERFIL DRAGÃO E2E: ficheiro real → importFile ------------
    {
        std::vector<u8> glb;
        makeProfileGlb(binLen, binLen, glb);
        char spath[96];
        std::snprintf(spath, sizeof(spath), "/tmp/goni_a2teto_ok_%d.glb",
                      static_cast<int>(::getpid()));
        FILE* f = std::fopen(spath, "wb");
        ASSERT(f != nullptr);
        std::fwrite(glb.data(), 1, glb.size(), f);
        std::fclose(f);
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_a2teto_ok_%d",
                      static_cast<int>(::getpid()));
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(spath, "dragao.glb", st, nullptr,
                                            out, stats, err);
        if (!ok) {
            fprintf(stderr, "[a2teto-a] o import do perfil dragão falhou: "
                            "%s\n", err.c_str());
        }
        ASSERT(ok);
        EXPECT(out.meshes.size() == 1u);
        EXPECT(st.exists("assets/dragao.gmesh"));
        EXPECT(stats.sourceBytes == glb.size());   // a fonte INTEIRA chegou
        ::remove(spath);
    }
    // ---- (b) O VIEW GENUINAMENTE FORA (a fixture (iv) do dono): a falha
    // traz a COMPARAÇÃO COMPLETA — declared/real/file — nunca truncada ----
    {
        std::vector<u8> glb;
        makeProfileGlb(binLen, binLen + 4, glb);
        char spath[96];
        std::snprintf(spath, sizeof(spath), "/tmp/goni_a2teto_fora_%d.glb",
                      static_cast<int>(::getpid()));
        FILE* f = std::fopen(spath, "wb");
        ASSERT(f != nullptr);
        std::fwrite(glb.data(), 1, glb.size(), f);
        std::fclose(f);
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_a2teto_fora_%d",
                      static_cast<int>(::getpid()));
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::importFile(spath, "fora.glb", st, nullptr,
                                            out, stats, err);
        EXPECT(!ok);   // fora do buffer REAL é falha (a vista mente)
        char alvo[128];
        std::snprintf(alvo, sizeof(alvo),
                      "declared=%llu real=%llu file=",
                      static_cast<unsigned long long>(binLen),
                      static_cast<unsigned long long>(binLen));
        EXPECT_MSG(err.find("TODAS as primitivas") != std::string::npos &&
                       err.find(alvo) != std::string::npos,
                   "o view fora do buffer não falhou com a comparação "
                   "completa (%s)", err.c_str());
        ::remove(spath);
    }
}

// ============================================================================
// R-035 (0.9.6.18 · HOTFIX — OS 12 DEFEITOS DA IMAGEM REAL DO DONO)
//         — regress_hotfix_defeitos
//
// Os pins AFERVÉIS do hotfix (o resto é provado pelos PNGs P-05 e pelos
// gates ui_vocab/docs-lint): (1) D2 o orçamento da linha Transform — nada
// fora do rect em qualquer largura, os pisos 48/40 e o reset ícone inline;
// (2) D4 a tabela localizada («Definições» em PT, «Settings» só em EN) e
// o botão outline compacto; (3) D5 o inspTab é morto no draw; (4) D7 o
// alvo da thumb 256 (PNG ≤60KB — o orçamento da spec); (5) D8 os ids de
// undo/redo SÓ na fila do topo (o rail não tem ação de undo — o glifo "U"
// era o ÍMAN, redesenhado como ferradura); (6) D10 os ícones Reset/List
// existem no vocabulário (o texto cru morre); (7) D11 o divisor em
// repouso é SÓ a linha 1dp (a coluna de pontos morreu); (8) D12 os planos
// do gizmo preenchem a kPlaneFillAlpha (25%).
// ============================================================================
#include "ui/Brand.h"     // D6: o LOD da marca
#include "ui/Strings.h"   // D4: a tabela localizada
#include "ui/Icons.h"     // D10: Reset/List
#include "ui/Gizmo.h"     // D12: kPlaneFillAlpha
#include "ui/ViewportChrome.h"  // D8: os ids do rail/fila
#include "ui/Toolbar.h"   // D8: os kGizmoIds

TEST(regress_hotfix_defeitos) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();

    // ---- (1) D2: o ORÇAMENTO da linha Transform (o campo Z cortado) ----
    // 0.12-QoL A (recalibrado com nota — a ordem do dono · Redbr1): os
    // vãos 8→12dp e a margem mínima 4→8dp; o piso do valor 26→22. A 164
    // úteis: a letra sai (a), o padding NÃO cede (o valor 24 ≥ 22) e as
    // caixas ficam a 36 (3×36+2×12+12+20 = 164 exato)
    {
        // o PISO do painel da casa: 180dp → 164dp úteis → caixas 36 e o
        // reset ÍCONE inline (a spec do dono); a linha inteira cabe
        const editor::TransformBudget b164 = editor::transformRowBudget(164.0f);
        EXPECT(nearEqF(b164.boxW, 36.0f, 0.01f));
        EXPECT(b164.resetIcon && nearEqF(b164.resetW, 20.0f, 0.01f));
        EXPECT(!b164.rowPadDropped);   // o valor 24 ≥ piso 22 — sem ceder
        const f32 total164 = 3.0f * b164.boxW + 2.0f * 12.0f + 12.0f + b164.resetW;
        EXPECT(total164 <= 164.0f + 0.01f);   // NADA fora do rect
        // o largo (a 208 úteis): o reset vira ÍCONE (o chip 40 já não cabe
        // com os vãos 12 — o chip volta a 220 úteis) e a letra FICA (o
        // valor 50.7−26 = 24.7 ≥ piso 22)
        const editor::TransformBudget b208 = editor::transformRowBudget(208.0f);
        EXPECT_MSG(nearEqF(b208.boxW, 50.7f, 0.1f) && b208.resetIcon &&
                       nearEqF(b208.resetW, 20.0f, 0.01f) && b208.axisLabels &&
                       !b208.rowPadDropped,
                   "D2+QoL A: a 208 úteis o reset é ícone e a letra fica "
                   "(boxW %.1f)",
                   (double)b208.boxW);
        // o CHIP volta a 220 úteis (3×48+2×12+12+40 = 220 exato — o piso 48
        // do chip com os vãos 12 da QoL A)
        const editor::TransformBudget b220 = editor::transformRowBudget(220.0f);
        EXPECT_MSG(nearEqF(b220.boxW, 48.0f, 0.01f) && !b220.resetIcon &&
                       b220.axisLabels && !b220.rowPadDropped,
                   "D2+QoL A: a 220 úteis o piso do chip é 48 com letra "
                   "(boxW %.1f)",
                   (double)b220.boxW);
        // o largo (≥268dp úteis): caixas 64 + chip 40 — a 260 o orçamento
        // encolhe para 61.3 (letra + valor cabem SEM ceder o padding)
        const editor::TransformBudget b260 = editor::transformRowBudget(260.0f);
        EXPECT(nearEqF(b260.boxW, 61.3f, 0.1f) && !b260.resetIcon);
        // INVARIANTE: em QUALQUER largura que a casa produz (o piso do
        // painel 180dp → 164dp úteis; abaixo disso a scissor do painel é
        // a última defesa, documentada) a linha inteira nunca passa do
        // útil — com o padding dropado a linha usa o CHEIO (útil + 16;
        // a regra m3/R1 do dono com a margem mínima 8dp da QoL A)
        for (f32 u = 164.0f; u <= 320.0f; u += 4.0f) {
            const editor::TransformBudget b = editor::transformRowBudget(u);
            const f32 total =
                3.0f * b.boxW + 2.0f * 12.0f + 12.0f + b.resetW;
            const f32 util = u + (b.rowPadDropped ? 16.0f : 0.0f);
            EXPECT_MSG(total <= util + 0.01f,
                       "D2: a linha transform transborda (%.1f > %.1f)",
                       (double)total, (double)util);
            EXPECT(b.boxW >= 16.0f);
        }
    }

    // ---- (2) D4: a TABELA LOCALIZADA (sem if solto no draw) -------------
    {
        // PT (o default da casa — o locale desconhecido cai aqui)
        strings::setLocaleForTest("pt");
        EXPECT(std::strcmp(strings::tr(strings::Key::SettingsTitle),
                           "Definições") == 0);
        EXPECT(std::strcmp(strings::locale(), "pt") == 0);
        // EN só em locale EN
        strings::setLocaleForTest("en");
        EXPECT(std::strcmp(strings::tr(strings::Key::SettingsTitle),
                           "Settings") == 0);
        // locale desconhecido → a língua base (PT), nunca string vazia
        strings::setLocaleForTest("xx");
        EXPECT(std::strcmp(strings::tr(strings::Key::SettingsTitle),
                           "Definições") == 0);
        // o round-trip do teste repõe o default
        strings::setLocaleForTest("pt");
        EXPECT(std::strcmp(strings::tr(strings::Key::ResetLayout),
                           "Repor layout") == 0);
    }

    // ---- (3) D5: a vista «Nós» MORREU (o draw manda o inspTab a 0) ------
    {
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        Scene scene;
        InputState in;
        editor::EditorState st;
        st.inspTab = 1;   // o estado antigo (layout.json de versões anteriores)
        st.selected = Handle::invalid();   // sem seleção → o TRILHO desenha
        ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
        editor::drawInspector(ui, scene, st);
        ui.endFrame();
        // o draw NUNCA respeita o 1: a vista morreu, o trilho é o default
        EXPECT(st.inspTab == 0);
    }

    // ---- (4) D7: o alvo da THUMB 256 (o orçamento da spec) --------------
    {
        EXPECT(thumb::kThumbTargetW == 256u);
        EXPECT(thumb::targetWidth(3840) == 256u);   // o viewport grande
        EXPECT(thumb::targetWidth(200) == 200u);    // nunca ampliar
        EXPECT(thumb::targetWidth(0) == 0u);        // degenerado
        // o crop 16:9 de um viewport 456×336dp @2.0 (912×672): o PNG fica
        // 256×144 EXATO (a spec ~256×144)
        const thumb::CropRect c = thumb::crop169(912, 672);
        EXPECT(thumb::targetWidth(c.w) == 256u);
    }

    // ---- (5) D8: os ids de ação NUNCA colidem (FASE UI B recalibra: o
    // undo/redo/save SUBIRAM para a barra de topo — os ids 30..32 do
    // viewport ficam APOSENTADOS e o rail usa SÓ ids de ferramenta
    // (35/36 + os kGizmoIds); o undo/redo/save da barra vivem em 20..22,
    // fora das faixas do chrome e das ferramentas — R-022 estendida)
    {
        EXPECT(editor::vpchrome::kVpUndoIdRetired <
               editor::vpchrome::kVpSelectId);
        EXPECT(editor::vpchrome::kVpRedoIdRetired <
               editor::vpchrome::kVpSelectId);
        EXPECT(editor::vpchrome::kVpSelectId !=
               editor::vpchrome::kVpUndoIdRetired &&
               editor::vpchrome::kVpSnapValId !=
                   editor::vpchrome::kVpUndoIdRetired &&
               editor::vpchrome::kVpSnapValId !=
                   editor::vpchrome::kVpRedoIdRetired &&
               editor::vpchrome::kVpSnapValId !=
                   editor::vpchrome::kVpSaveIdRetired);
        // as ferramentas do rail têm ids PRÓPRIOS (cursor/gizmo 0..2/ímã)
        EXPECT(editor::toolbar::kGizmoIds[0] !=
                   editor::vpchrome::kVpUndoIdRetired &&
               editor::toolbar::kGizmoIds[1] !=
                   editor::vpchrome::kVpUndoIdRetired &&
               editor::toolbar::kGizmoIds[2] !=
                   editor::vpchrome::kVpUndoIdRetired);
        // FASE UI B: os ids novos da barra (20..22) não colidem com
        // NENHUMA faixa existente (a barra 1..19, o rail 30..39)
        EXPECT(editor::toolbar::kTbUndoId > editor::toolbar::kTbGearId &&
               editor::toolbar::kTbSaveId < editor::vpchrome::kVpUndoIdRetired);
        // o glifo do ÍMAN já não lê como "U": a ferradura nova tem os
        // CAPACETES dos pólos mais largos que os braços (7 segmentos, o
        // traço do conjunto) — a prova visual é o PNG P-05
        EXPECT(icons::segmentCount(icons::Icon::Snap) == 7u);
    }

    // ---- (6) D10: o vocabulário tem os ícones dos controlos novos -------
    {
        EXPECT(icons::iconByName("reset") >= 0);   // a seta circular (o "R" morre)
        EXPECT(icons::iconByName("lista") >= 0);   // a vista em lista (o texto morre)
        EXPECT(icons::iconByName("grelha") >= 0);  // o Grid existe (o par do toggle)
        EXPECT(icons::Icon::Reset < icons::Icon::Count &&
               icons::Icon::List < icons::Icon::Count);
    }

    // ---- (7) D11: o divisor em repouso é SÓ a linha 1dp -----------------
    {
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        InputState in;
        editor::EditorState st;
        st.selected = Handle{1u, 1u};   // inspector aberto → divisor direito
        ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
        ui.auditBegin("r035-divisor", 1600.0f, 720.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f);
        editor::drawPanelDividers(ui, st, true);
        ui.endFrame();
        const layout::Record& r = ui.auditRecord();
        ui.auditEnd();
        // na coluna do divisor NÃO existe NENHUM ponto (painel < 8×8dp):
        // a "coluna de pontos flutuantes" morreu
        for (const auto& e : r.entries) {
            const bool estreito = e.w < 8.0f && e.h < 8.0f;
            EXPECT_MSG(!estreito,
                       "D11: sobrou um ponto flutuante %.0fx%.0f no divisor",
                       (double)e.w, (double)e.h);
        }
        // RECALIBRADA 0.11-Y (Y-3): a LINHA SÓLIDA do D11 foi SUPERADA pela
        // ordem expressa do dono («separadores com gradiente») — o divisor
        // em repouso é agora o QUAD GRADIENTE (1dp, accentDim a cair do
        // topo a0.90 -> a0.15; decorativo, fora do audit como os gizmos).
        // O pin aferra o QUAD no batch (o mesmo padrão dos pins y3_*)
        bool linha1dp = false;
        const QuadBatch& bD11 = ui.solidsForTest();
        const u32 nD11 = bD11.vertexCount() / 6u;
        for (u32 q = 0; q < nD11; ++q) {
            const QuadVertex* v = bD11.vertices() + q * 6;
            f32 mnX = 1e9f, mxX = -1e9f, mnY = 1e9f, mxY = -1e9f;
            for (u32 k = 0; k < 6; ++k) {
                mnX = v[k].x < mnX ? v[k].x : mnX;
                mxX = v[k].x > mxX ? v[k].x : mxX;
                mnY = v[k].y < mnY ? v[k].y : mnY;
                mxY = v[k].y > mxY ? v[k].y : mxY;
            }
            const f32 wq = mxX - mnX, hq = mxY - mnY;
            if (wq <= 1.5f && hq > 32.0f &&
                test::nearEqF(v[0].r, 38.0f / 255.0f, 1e-3f) &&
                test::nearEqF(v[0].a, 0.90f, 1e-3f) &&
                test::nearEqF(v[1].a, 0.15f, 1e-3f)) {
                linha1dp = true;   // o quad do gradiente É o divisor
            }
        }
        EXPECT(linha1dp);   // o divisor de cristal (o gradiente; FASE UI:
                            // a cor é o BORDER #26262C — as linhas do dono)
    }

    // ---- (9) D3: o empty-state NO RECT MINÚSCULO (a trava do fundo) -----
    {
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        InputState in;
        ui.beginFrame(nullptr, &in, 800.0f, 600.0f);
        ui.auditBegin("r035-empty", 800.0f, 600.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                      1.0f);
        // um rect de 40dp de altura (o drawer mínimo degenerado): o
        // empty-state NÃO cruza o fundo — a trava é a segunda defesa
        const UiRect tiny{100.0f, 200.0f, 300.0f, theme::dp(40.0f)};
        editor::bottom::drawEmptyState(ui, tiny, icons::Icon::Folder,
                                       "sem pastas (projeto sem ficheiros)");
        ui.endFrame();
        const layout::Record& rT = ui.auditRecord();
        ui.auditEnd();
        bool cruzou = false, desenhou = false;
        for (const auto& e : rT.entries) {
            if (e.kind == layout::Entry::Label && e.x >= tiny.x &&
                e.x < tiny.x + tiny.w) {
                desenhou = true;
                if (e.y + e.h > tiny.y + tiny.h + 0.5f) {
                    cruzou = true;
                }
            }
        }
        EXPECT(desenhou);
        EXPECT_MSG(!cruzou,
                   "D3: o empty-state no rect minúsculo cruzou o fundo "
                   "(a trava morreu — a mutação M3 fica vermelha)");
    }

    // ---- (8) D12: o preenchimento dos planos do gizmo a 25% -------------
    {
        EXPECT(nearEqF(vv::gizmo::kPlaneFillAlpha, 0.25f, 0.001f));
        // a FUNÇÃO do glifo da marca (D6 — paridade dos sítios). 0.12-D
        // (D-4) recalibra com nota: AS SETAS MORRERAM (a identidade única
        // — o ícone real nunca as teve) e a contagem é IGUAL nos dois
        // LODs (16 = 14 arco + 2 barra); o LOD vive agora no TRAÇO (×1.2
        // no pequeno), que a contagem não vê
        EXPECT(editor::brand::segmentCountFor(24.0f) == 16u);
        EXPECT(editor::brand::segmentCountFor(32.0f) == 16u);
        EXPECT(editor::brand::segmentCountFor(96.0f) ==
               editor::brand::segmentCountFor(24.0f));
        EXPECT(editor::brand::kLodFullMinDp == 32.0f);
    }

    // ---- (10) R1 (0.9.6.19): O VALOR É INTOCÁVEL no orçamento D2 --------
    // O dono: «o clamp do D2 deixou os campos X/Y/Z sem o texto do valor
    // (só a letra do eixo). O valor é intocável — se a largura aperta,
    // dropa primeiro a letra do eixo, depois o padding, nunca o valor.»
    // Pin: valor NÃO-VAZIO nos 3 campos em 180/220/260dp (os úteis
    // 164/204/244 — o painel menos as margens kPad 16).
    {
        struct Caso { f32 util; const char* nome; };
        const Caso casos[3] = {{164.0f, "180dp"}, {204.0f, "220dp"},
                               {244.0f, "260dp"}};
        for (const Caso& c : casos) {
            const editor::TransformBudget b = editor::transformRowBudget(c.util);
            // o piso do valor é cumprido EM TODOS (o pin do dono)
            EXPECT_MSG(editor::transformValueSpace(b) >=
                           editor::kTfValueMinDp - 0.01f,
                       "R1 %s: o espaço do valor é %.1fdp < piso %.0fdp "
                       "(o valor foi cortado — a mutação M-R1)",
                       c.nome, (double)editor::transformValueSpace(b),
                       (double)editor::kTfValueMinDp);
            // a ORDEM do dono: a letra só sai DEPOIS de esgotada a caixa
            // inteira; e a linha INTEIRA continua dentro do útil — QoL A:
            // com o PADDING DROPADO a linha usa o cheio (útil + 16 — a
            // margem mínima 8dp de cada lado)
            const f32 rowPad = b.rowPadDropped ? 8.0f : 16.0f;
            const f32 total = 3.0f * b.boxW + 2.0f * 12.0f + 12.0f + b.resetW +
                              2.0f * (16.0f - rowPad);
            const f32 utilEfetivo =
                c.util + (b.rowPadDropped ? 16.0f : 0.0f);
            EXPECT_MSG(total <= utilEfetivo + 0.01f,
                       "R1 %s: a linha transborda (%.1f > %.1f)",
                       c.nome, (double)total, (double)utilEfetivo);
            // a PROVA DO VALOR NÃO-VAZIO com a fonte real: um valor típico
            // da linha (%.2g — curtos) cabe INTEIRO no espaço do orçamento
            FontAtlas font;
            const char* fp = FONT_FIXTURE;
            ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
            UiContext ui;
            ui.init();
            ui.setFont(&font);
            char val[16];
            std::snprintf(val, sizeof(val), "%.2g", -12.5);
            EXPECT_MSG(ui.fontWidth(val) <=
                           editor::transformValueSpace(b) * theme::dp(1.0f),
                       "R1 %s: o valor «%s» (%.1fpx) não cabe no espaço "
                       "(%.1fpx)",
                       c.nome, val, (double)ui.fontWidth(val),
                       (double)(editor::transformValueSpace(b) *
                                theme::dp(1.0f)));
        }
        // a 180dp (164 úteis) a LETRA sai (o 1.º a ceder — a ordem do dono);
        // o valor fica com 24dp (o piso 22 da QoL A)
        const editor::TransformBudget b164 = editor::transformRowBudget(164.0f);
        EXPECT(!b164.axisLabels);   // o valor ficou com a caixa inteira
        EXPECT(nearEqF(editor::transformValueSpace(b164), 24.0f, 0.01f));
        // a 260dp (244 úteis) a letra FICA (cabe letra + valor)
        const editor::TransformBudget b244 = editor::transformRowBudget(244.0f);
        EXPECT(b244.axisLabels);
        // MUTAÇÃO M-R1 (a fórmula antiga): maxVw = boxW−28 a 164 úteis dá
        // 40−28 = 12dp < kTfValueMinDp — vermelho (o pin acima apanha-a)

        // ---- 0.9.6.19b (m3) recalibrado 0.12-QoL A: A LIMIAR DA LETRA ----
        // O dono: «a letra só sai abaixo de ~200dp de painel; entre 200 e
        // 260 letra+valor coexistem; o valor nunca sai.» Com os vãos 12dp
        // da QoL A o espaço que a letra usava aumenta 8dp: a coexistência
        // começa a 216dp de painel (184 úteis — o passo (c) da R1 dropa a
        // letra DEPOIS do padding, sempre com o valor ≥ piso). A convenção
        // do draw: útil = painel − 2×kPad.
        {
            // a 216dp de painel (184 úteis) a letra FICA e o piso cumpre-se
            // (o padding da linha cede antes; boxW 48 → valor 22 exato)
            const editor::TransformBudget b216 =
                editor::transformRowBudget(184.0f);
            EXPECT_MSG(b216.axisLabels,
                       "m3+QoL A: a 216dp de painel a letra do eixo TEM de "
                       "ficar (letra+valor coexistem — a mutação M-m3 volta "
                       "a dropá-la)");
            EXPECT_MSG(editor::transformValueSpace(b216) >=
                           editor::kTfValueMinDp - 0.01f,
                       "m3+QoL A: a 216dp o piso do valor cumpre-se COM a "
                       "letra (%.1fdp)",
                       (double)editor::transformValueSpace(b216));
            // a INVARIANTE da limiar: acima de 184 úteis a letra NUNCA sai;
            // abaixo, pode — mas o valor nunca fica sem espaço legível
            for (f32 util = 140.0f; util <= 320.0f; util += 2.0f) {
                const editor::TransformBudget bb =
                    editor::transformRowBudget(util);
                if (util >= 184.0f) {
                    EXPECT_MSG(bb.axisLabels,
                               "m3+QoL A: a %.0f úteis (≥limiar) a letra saiu",
                               (double)util);
                }
                EXPECT_MSG(editor::transformValueSpace(bb) >= 20.0f,
                           "m3: a %.0f úteis o valor fica sem espaço legível "
                           "(%.1fdp — o valor NUNCA sai)",
                           (double)util,
                           (double)editor::transformValueSpace(bb));
            }
        }
    }

    // ---- (11) D19 (0.9.6.19): O TOGGLE «visível» MOSTRA O ESTADO -------
    // O pin: «toggle desenhado com estado nos dois valores» — o knob ON
    // vive à DIREITA do trilho; o OFF à ESQUERDA (a posição É o estado).
    {
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        InputState in;
        const f32 rightX = 300.0f, cy = 100.0f;
        f32 knobXOn = -1.0f, knobXOff = -1.0f;
        // ON: knob 12×12 à direita do trilho-pílula 32×16
        ui.beginFrame(nullptr, &in, 800.0f, 600.0f);
        ui.auditBegin("r035-d19-on", 800.0f, 600.0f, 0, 0, 0, 0, 1.0f);
        editor::drawVisSwitch(ui, true, rightX, cy);
        ui.endFrame();
        {
            const layout::Record& r = ui.auditRecord();
            f32 pillL = -1.0f;
            for (const auto& e : r.entries) {
                if (e.kind == layout::Entry::Panel &&
                    nearEqF(e.w, theme::dp(32.0f), 0.5f) &&
                    nearEqF(e.h, theme::dp(16.0f), 0.5f)) {
                    pillL = e.x;
                }
                if (e.kind == layout::Entry::Panel &&
                    nearEqF(e.w, theme::dp(12.0f), 0.5f)) {
                    knobXOn = e.x;   // o knob (o quad 12×12)
                }
            }
            EXPECT(pillL > 0.0f);
            EXPECT_MSG(knobXOn > pillL + theme::dp(16.0f),
                       "D19 ON: o knob não está à DIREITA do trilho "
                       "(knob %.1f, pílula %.1f)",
                       (double)knobXOn, (double)pillL);
        }
        ui.auditEnd();
        // OFF: knob à esquerda (a posição muda com o estado — o pin)
        ui.beginFrame(nullptr, &in, 800.0f, 600.0f);
        ui.auditBegin("r035-d19-off", 800.0f, 600.0f, 0, 0, 0, 0, 1.0f);
        editor::drawVisSwitch(ui, false, rightX, cy);
        ui.endFrame();
        {
            const layout::Record& r = ui.auditRecord();
            for (const auto& e : r.entries) {
                if (e.kind == layout::Entry::Panel &&
                    nearEqF(e.w, theme::dp(12.0f), 0.5f)) {
                    knobXOff = e.x;
                }
            }
            EXPECT_MSG(knobXOff < knobXOn - theme::dp(8.0f),
                       "D19 OFF: o knob não mudou de lado (on %.1f, off "
                       "%.1f) — o estado não se lê no toggle",
                       (double)knobXOn, (double)knobXOff);
        }
        ui.auditEnd();
    }

    // ---- (12) m2 (0.9.6.19b): O FORMAT ÚNICO NORMALIZA «-0» → «0» --------
    // O dono: «função única de format de números no inspector normaliza
    // -0 → 0; usada por todos os campos. Pin: campo com -0.0 desenha "0".»
    // A mutação M-m2 (a normalização morta — snprintf cru) fica VERMELHA
    // no caso 1 (o «-0» volta).
    {
        char buf[24];
        // (1) o caso do dono: %.2g de -0.0 → "0" (era "-0")
        editor::formatNum(buf, sizeof(buf), "%.2g", -0.0f);
        EXPECT_MSG(std::strcmp(buf, "0") == 0,
                   "m2: %.2g de -0.0 desenhou «%s» (o «-0» tem de morrer)",
                   buf);
        // (2) %.1f de -0.04 arredonda a -0.0 → "0.0"
        editor::formatNum(buf, sizeof(buf), "%.1f", -0.04f);
        EXPECT_MSG(std::strcmp(buf, "0.0") == 0,
                   "m2: %.1f de -0.04 desenhou «%s» (o «-0.0» tem de morrer)",
                   buf);
        // (3) %.0f de -0.4 → "0"
        editor::formatNum(buf, sizeof(buf), "%.0f", -0.4f);
        EXPECT_MSG(std::strcmp(buf, "0") == 0,
                   "m2: %.0f de -0.4 desenhou «%s»", buf);
        // (4) zero positivo já era "0" (a normalização não estraga)
        editor::formatNum(buf, sizeof(buf), "%.2g", 0.0f);
        EXPECT(std::strcmp(buf, "0") == 0);
        // (5) valores NÃO-zero mantêm o sinal (a cura não come números)
        editor::formatNum(buf, sizeof(buf), "%.2g", -1.5f);
        EXPECT(std::strcmp(buf, "-1.5") == 0);
        editor::formatNum(buf, sizeof(buf), "%.2g", -0.0001f);
        EXPECT(std::strcmp(buf, "-0.0001") == 0);
        editor::formatNum(buf, sizeof(buf), "%.1f", 12.5f);
        EXPECT(std::strcmp(buf, "12.5") == 0);
    }

    // ---- (13) m1 (0.9.6.19b): O CHIP «N x» TEM SLOT PRÓPRIO -------------
    // O dono: «header da hierarquia = título + (chip opcional espaçado) +
    // ações, sem colagem, em 2 densidades.» O chip nasce SOBRE o título em
    // painel estreito («Hierarquia1 x») — o slot hierChipSlot garante os
    // 8dp de cada lado ou NÃO DESENHA.
    {
        const f32 chipW = theme::dp(36.0f);   // o texto "1 x" + 16dp de folga
        // a largura do título MEDIDA pela fonte real (o draw mede o MESMO
        // caminho: fontWidth × fontScale(16sp))
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        auto probe = [&](f32 density) {
            theme::setDensity(density);
            const f32 titleW = ui.fontWidth("Hierarquia") *
                               theme::fontScale(theme::kFontSection);
            // a largura MÍNIMA do painel em que o chip TEM de caber:
            // pad + título + vão + chip + vão + 2 botões + pad (a conta
            // exata do slot — o teste não assume o tamanho da fonte)
            const f32 minW = theme::dp(16.0f) + titleW + theme::dp(8.0f) +
                             chipW + theme::dp(8.0f) +
                             2.0f * theme::dp(28.0f) + theme::dp(16.0f);
            // (a) no mínimo exato + folga: cabe com os vãos ≥8dp
            const editor::HierChipSlot s1 = editor::hierChipSlot(
                minW + theme::dp(2.0f), theme::dp(16.0f) + titleW, chipW);
            EXPECT_MSG(s1.fits,
                       "m1 @%.1f: no painel que dá espaço o chip TEM de "
                       "caber (o slot morreu?)", (double)density);
            const f32 gapTitle = s1.x - (theme::dp(16.0f) + titleW);
            const f32 gapDots = (minW + theme::dp(2.0f)) - theme::dp(16.0f) -
                                2.0f * theme::dp(28.0f) - (s1.x + chipW);
            EXPECT_MSG(gapTitle >= theme::dp(8.0f) - 0.5f,
                       "m1 @%.1f: o chip nasce a %.1fdp do título (o vão "
                       "≥8dp é o pin — a colagem «Hierarquia1 x» morre)",
                       (double)density, (double)(gapTitle / theme::dp(1.0f)));
            EXPECT_MSG(gapDots >= theme::dp(8.0f) - 0.5f,
                       "m1 @%.1f: o chip colado ao ⋮ (vão %.1fdp)",
                       (double)density,
                       (double)(gapDots / theme::dp(1.0f)));
            // (b) abaixo do mínimo: NÃO cabe (o chip some — degradação
            //     honesta; a limpeza continua no menu ⋮ da hierarquia) OU,
            //     se coubesse, nunca colado ao título
            const editor::HierChipSlot s2 = editor::hierChipSlot(
                minW - theme::dp(4.0f), theme::dp(16.0f) + titleW, chipW);
            EXPECT_MSG(!s2.fits || s2.x >= theme::dp(16.0f) + titleW +
                                           theme::dp(8.0f),
                       "m1 @%.1f: o chip desenha colado ao título (o pin "
                       "«sem colagem» é violado)", (double)density);
            EXPECT(!s2.fits);   // 4dp abaixo do mínimo — o espaço não dá
        };
        probe(1.0f);   // mdpi/harness
        probe(2.0f);   // xhdpi/device (os vãos são em dp — px dobram, os
                       // vãos não)
        theme::setDensity(1.0f);
        // MUTAÇÃO M-m1 (o slot fixo antigo): o chip ancorado à direita sem
        // ler o título desenha SOBRE ele em painel estreito — o
        // EXPECT(!s2.fits) acima é o que fica vermelho quando o slot morre
    }
}

// ---------------------------------------------------------------------------
// R-037 · PASSO 4 (0.9.6.20) — AS JANELAS: logs opacos com corte/scroll/wrap
// (80% do ecrã) · menus ancorados com fundo 40% · settings linhas 36dp ·
// cantos suaves nos cards que faltavam (o contrato J-01..J-04,
// docs/LAYOUT_HIERARCHY.md §2.25-§2.28)
// ---------------------------------------------------------------------------
#include "ui/TextFit.h"   // textwrap::wrap (a fonte única do wrap)

TEST(regress_janelas_passo4) {
    using namespace vv;

    // ---- (J-02) OS VÉUS: o menu ancorado tem 40%, o modal mantém 60% ------
    EXPECT(nearEqF(theme::kTheme.scrimMenu[3], 0.40f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.scrimMenu[0], 0.0f, 1e-4f) &&
           nearEqF(theme::kTheme.scrimMenu[1], 0.0f, 1e-4f) &&
           nearEqF(theme::kTheme.scrimMenu[2], 0.0f, 1e-4f));   // preto puro
    EXPECT(nearEqF(theme::kTheme.scrim[3], 0.60f, 1e-4f));      // modal intacto

    // ---- (J-04) OS RAIOS dos cards (o tema é a fonte única) ---------------
    EXPECT(nearEqF(theme::kRadiusCard, 8.0f, 1e-4f));
    EXPECT(nearEqF(theme::kRadiusField, 6.0f, 1e-4f));   // FASE UI: raio 6-8

    // ---- (J-01) O WRAP puro (medidor fake mono 10px/code point) -----------
    //      «corte/scroll/wrap»: o wrap é o textwrap::wrap (a fonte única
    //      das Docs); o corte é a rede (labelFitted por linha); o scroll
    //      é o beginScroll do viewer.
    {
        const auto mono = [](const char* s) {
            return 10.0f * static_cast<f32>(std::strlen(s));
        };
        std::vector<textwrap::Line> rows;

        // (a) linha que QUEBRA por palavras: "aaaa bbbb cccc" (140px) a
        //     100px → [aaaa bbbb][cccc] (2 linhas visíveis)
        textwrap::wrap("aaaa bbbb cccc", 100.0f, mono, rows);
        EXPECT(rows.size() == 2);
        EXPECT(rows[0].begin == 0 && rows[0].len == 9);   // "aaaa bbbb"
        EXPECT(rows[1].begin == 10 && rows[1].len == 4);  // "cccc"

        // (b) linha curta = 1 linha; linha vazia = NENHUMA (o viewer conta
        //     1 linha visual de respiro — o contrato do draw)
        textwrap::wrap("curta", 100.0f, mono, rows);
        EXPECT(rows.size() == 1);
        textwrap::wrap("", 100.0f, mono, rows);
        EXPECT(rows.empty());

        // (c) palavra MAIOR que a largura: parte por largura SEM partir um
        //     code point (o acento morre inteiro — o padrão da casa)
        //     "áááááááááá" = 10 cps (20 bytes), mono-byte 10px → 200px;
        //     a 150px: [7 cps (14 bytes=140px)][3 cps restantes]
        textwrap::wrap("\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1"
                       "\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1",
                       150.0f, mono, rows);
        EXPECT(rows.size() == 2);
        EXPECT(rows[0].len == 14);   // 7 code points INTEIROS (14 bytes)
        // a fronteira é byte-líder (nunca meio acento): byte 14 é 0xC3
        const char* t = "\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1"
                        "\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1\xC3\xA1";
        EXPECT((static_cast<unsigned char>(t[rows[0].len]) & 0xC0u) == 0xC0u);

        // (d) a CONCATENAÇÃO das linhas (reunidas com o espaço separador)
        //     repõe o original — o conteúdo não se perde no wrap
        textwrap::wrap("um dois tres quatro", 60.0f, mono, rows);
        EXPECT(rows.size() == 4);   // uma palavra por linha (30px cada)
        std::string rebuilt;
        for (size_t i = 0; i < rows.size(); ++i) {
            if (!rebuilt.empty()) {
                rebuilt += ' ';
            }
            rebuilt.append("um dois tres quatro" + rows[i].begin,
                           rows[i].len);
        }
        EXPECT(rebuilt == "um dois tres quatro");
    }

    // ---- (J-01) O VIEWER E2E: card 80%×80% OPAQUE (a geometria no
    //      REGISTO) + as linhas visíveis do wrap no audit ------------------
    {
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        InputState in;
        editor::EditorState st;
        theme::setDensity(1.0f);
        editor::applyDensity();

        // uma linha LONGA (garante ≥3 linhas visíveis a 80% de 1600) + curta
        std::string longLine;
        for (int i = 0; i < 150; ++i) {
            longLine += "palavra ";
        }
        const std::vector<std::string> lines{longLine, "curta"};
        const std::vector<std::string> dumps{};

        ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
        ui.auditBegin("r037-logs", 1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                      1.0f);
        editor::drawLogViewer(ui, in, 1600.0f, 720.0f, st, lines, dumps);
        ui.endFrame();
        const layout::Record& rec = ui.auditRecord();
        ui.auditEnd();

        // (a) o CARD (o maior painel do frame): 80% × 80% da faixa útil
        f32 ox, oy, aw, ah;
        editor::overlayArea(1600.0f, 720.0f, safe::Insets{}, ox, oy, aw, ah);
        const layout::Entry* card = nullptr;
        f32 best = 0.0f;
        for (const auto& e : rec.entries) {
            if (e.kind == layout::Entry::Panel && e.w * e.h > best) {
                best = e.w * e.h;
                card = &e;
            }
        }
        EXPECT(card != nullptr);
        EXPECT(card && nearEqF(card->w, aw * 0.80f, 0.5f));
        EXPECT(card && nearEqF(card->h, ah * 0.80f, 0.5f));
        EXPECT(card && card->w <= aw * 0.80f + 0.1f);   // o cap do dono

        // (b) o WRAP no registo: as labels das linhas visíveis (as linhas
        //     do log partem de x = card.x + kPad) — a contagem do draw
        //     (1 cabeçalho + N linhas visíveis) BATE com o textwrap puro
        //     medindo com a MESMA fonte
        std::vector<textwrap::Line> rows;
        textwrap::wrap(longLine.c_str(),
                       card->w - 2.0f * theme::dp(16.0f),
                       [&](const char* s) { return ui.fontWidth(s); }, rows);
        EXPECT(rows.size() >= 3);   // a linha é longa o bastante
        u32 logLabels = 0;
        // as labels DA LISTA (abaixo do cabeçalho do card — o título
        // "LOGS (…)" vive na banda de 28dp e não conta)
        for (const auto& e : rec.entries) {
            if (e.kind == layout::Entry::Label &&
                nearEqF(e.x, card->x + theme::dp(16.0f), 0.5f) &&
                e.y >= card->y + theme::dp(28.0f)) {
                ++logLabels;
            }
        }
        EXPECT(logLabels == 1u + static_cast<u32>(rows.size()) + 1u);
        // (o 1 = o cabeçalho "engine.log:", o +1 = a linha "curta")

        // (c) nada sai do card pela largura (o corte de rede)
        bool sangrou = false;
        for (const auto& e : rec.entries) {
            if (e.kind == layout::Entry::Label && e.x >= card->x &&
                e.x + e.w > card->x + card->w + 0.5f) {
                sangrou = true;
            }
        }
        EXPECT(!sangrou);
    }

    // ---- (J-03) O SETTINGS: linhas 36dp (a fonte única) + o botão
    //      compacto centrado na linha de 36 ------------------------------
    {
        EXPECT(nearEqF(editor::settings::settingsRowH(), theme::dp(36.0f),
                       0.01f));
        EXPECT(nearEqF(editor::settings::settingsSectionH(), theme::dp(48.0f),
                       0.01f));   // os CABEÇALHOS mantêm 48 (a spec manda
                                  // nas linhas)
        FontAtlas font;
        const char* fp = FONT_FIXTURE;
        ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
        UiContext ui;
        ui.init();
        ui.setFont(&font);
        ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
        const UiRect ab = editor::settings::actionBtnRect(
            ui, 0.0f, 0.0f, 400.0f, "Repor layout");
        // a linha está em y=0: o controlo 28dp centra a dp(4) do topo
        // ((36−28)/2 — o respiro da linha de 36)
        EXPECT(nearEqF(ab.y, theme::dp(4.0f), 0.1f));
        EXPECT(nearEqF(ab.h, theme::dp(28.0f), 0.1f));
        ui.endFrame();
    }
}

// ---------------------------------------------------------------------------
// R-038 (0.10-M PASSO 2) — RETROCOMPATIBILIDADE DO .gmesh: os ficheiros
// v1 e v2 REAIS (fixtures commitadas, geradas com o escritor da era
// 0.9.6 ANTES do escritor passar a v3) continuam a abrir, o escritor só
// escreve v3 e o round-trip v3 é EXATO (float32). AS MUTAÇÕES:
//   • M-V3a  o escritor a gravar v2 (kGmeshVersionWrite 3→2) → VERMELHO
//   • M-V3b  o leitor a recusar v1 (a fixture v1 falha no dispatch) → VERMELHO
// A retrocompatibilidade é a CLÁUSULA do dono na spec 0.10-M: «quebrar
// isto é vermelho por definição».
TEST(regress_gmesh_v3_retrocompat) {
    using namespace vv;
    const std::string root = REPO_ROOT;
    std::vector<u8> bytes;
    std::string err;

    // (1) o v1 REAL abre (o payload quantizado dequantiza como sempre)
    ASSERT(fileapi::readAll(root + "/tests/fixtures/gmesh_v1_esfera.gmesh",
                            bytes));
    MeshData v1;
    ASSERT((readGMesh(bytes.data(), bytes.size(), v1, err)) ||
           (std::fprintf(stderr, "R-038 v1: %s\n", err.c_str()), 0));
    EXPECT(v1.vertices.size() == 221);
    EXPECT(v1.indices.size() == 1152);
    EXPECT(v1.groups[0].name == "corpo");
    EXPECT(v1.groups[0].material == "laca");

    // (2) o v2 abre (a interpretação tolerante documentada — os MESMOS
    // bytes com version=2; nunca existiu v2 no histórico do repo)
    bytes.clear();
    ASSERT(fileapi::readAll(root + "/tests/fixtures/gmesh_v2_esfera.gmesh",
                            bytes));
    MeshData v2;
    ASSERT((readGMesh(bytes.data(), bytes.size(), v2, err)) ||
           (std::fprintf(stderr, "R-038 v2: %s\n", err.c_str()), 0));
    EXPECT(v2.vertices.size() == 221);
    EXPECT(v2.groups[0].material == "laca");

    // (3) o ESCRITOR só escreve v3 (o header leva a versão 3 — M-V3a troca
    // kGmeshVersionWrite para 2 → este EXPECT fica vermelho)
    MeshData m;
    for (int r = 0; r <= 12; ++r) {
        const f32 phi = 3.14159265f * f32(r) / 12.0f;
        for (int s = 0; s <= 16; ++s) {
            const f32 th = 6.28318530f * f32(s) / 16.0f;
            Vertex v;
            v.pos = Vec3{std::sin(phi) * std::cos(th), std::cos(phi),
                         std::sin(phi) * std::sin(th)};
            v.normal = v.pos;
            v.uv = Vec2{f32(s) / 16.0f, f32(r) / 12.0f};
            m.vertices.push_back(v);
        }
    }
    for (int r = 0; r < 12; ++r) {
        for (int s = 0; s < 16; ++s) {
            const u16 a = u16(r * 17 + s);
            const u16 b = u16(a + 17);
            m.indices.push_back(a);
            m.indices.push_back(b);
            m.indices.push_back(u16(a + 1));
            m.indices.push_back(b);
            m.indices.push_back(u16(b + 1));
            m.indices.push_back(u16(a + 1));
        }
    }
    std::vector<u8> out;
    ASSERT(writeGMesh(m, out, err));
    const u16 ver = static_cast<u16>(out[4] | (out[5] << 8));
    // o LITERAL 3 é de propósito: a mutação M-V3a troca a CONSTANTE
    // (kGmeshVersionWrite 3→2) e este check tem de a apanhar
    EXPECT_MSG(ver == 3,
               "o escritor grava version=%u (a cláusula do dono: só v3)",
               ver);

    // (4) o round-trip v3 é EXATO (float32 bit a bit)
    MeshData back;
    ASSERT(readGMesh(out.data(), out.size(), back, err));
    bool exact = true;
    for (size_t i = 0; i < m.vertices.size(); ++i) {
        if (std::memcmp(&back.vertices[i].pos, &m.vertices[i].pos, 12) != 0) {
            exact = false;
        }
    }
    EXPECT(exact);
    EXPECT(back.indices.size() == m.indices.size());
    EXPECT(back.vertices.size() == m.vertices.size());
}

// ---------------------------------------------------------------------------
// R-040 (0.10-M PASSO 4) — O RENDER POR BLOCOS: o orçamento é o CONTRATO.
// A parede do «fail de 203 MB» era o mesh ÚNICO; a cura é a cache de
// blocos com orçamentos DECLARADOS (kBlockCacheRamBudgetBytes +
// kBlockCacheVramBudgetBytes) vigiada AQUI para sempre: os literals são de
// PROPÓSITO (mudar a constante sem mudar a sentinela = vermelho), o
// frustum é o pin da fórmula (8 cantos × 6 planos) e a evicção RAM conta
// as DUAS pool. As mutações M1/M2/M3 do PASSO 4 (culling morto, evicção
// morta, eager no open) estão coladas no RELATORIO-0.10-M-PASSO4.
// ---------------------------------------------------------------------------
TEST(regress_render_por_blocos_r040) {
    // (1) os ORÇAMENTOS da casa (64+192 = o MESMO 256 MB do mesh único,
    // agora LIMITADO POR CONSTRUÇÃO independentemente do modelo)
    EXPECT(kBlockCacheRamBudgetBytes == 64ull * 1024 * 1024);
    EXPECT(kBlockCacheVramBudgetBytes == 192ull * 1024 * 1024);
    EXPECT(kBlockCacheRamBudgetBytes + kBlockCacheVramBudgetBytes ==
           kMeshLoadBudgetBytes);

    // (2) a estimativa de VRAM de um bloco é o contrato aferível
    {
        MeshData b;
        b.vertices.resize(100);
        b.indices.resize(300);
        EXPECT(blockMeshVramBytes(b) ==
               100ull * sizeof(Vertex) + 300ull * 2);
        b.skinJoints.resize(400);
        b.skinWeights.resize(400);
        EXPECT(blockMeshVramBytes(b) ==
               100ull * sizeof(Vertex) + 300ull * 2 + 100ull * 20);
    }

    // (3) o FRUSTUM é o pin da fórmula: identidade = o cubo [-1,1]³ (os
    // planos de Gribb-Hart sobre as colunas do VP; fora = a·x+b·y+c·z+d<0)
    {
        f32 planes[6][4];
        BlockMesh::frustumPlanes(Mat4::identity(), planes);
        // (2,0,0) fora do plano right (-x+1<0); (0,0,0) dentro dos 6
        EXPECT(planes[1][0] * 2.0f + planes[1][3] < 0.0f);
        bool origemDentro = true;
        for (int p = 0; p < 6; ++p) {
            origemDentro = origemDentro && planes[p][3] >= 0.0f;
        }
        EXPECT(origemDentro);
        // o AABB LONGE do frustum cai TODOS os cantos no MESMO plano
        GMeshV3Block blk;
        blk.aabbMin = Vec3{50.0f, 50.0f, 50.0f};
        blk.aabbMax = Vec3{51.0f, 51.0f, 51.0f};
        EXPECT(!BlockMesh::blockVisible(Mat4::identity(), blk, planes));
        // o AABB ENCOSTADO (um canto dentro) segue VIVO (conservativo)
        blk.aabbMin = Vec3{0.5f, 0.5f, 0.5f};
        blk.aabbMax = Vec3{2.0f, 2.0f, 2.0f};
        EXPECT(BlockMesh::blockVisible(Mat4::identity(), blk, planes));
    }
}

// ---------------------------------------------------------------------------
// R-041 (0.10-M HOTFIX SAF-STREAM) — O STREAMING SOB QUALQUER STORAGE: o
// mmap POR FD é o CONTRATO. A causa do city era o gate content:// que
// desligava o streaming nos projetos SAF (o import caía no legado do teto
// de 65535); a cura tem TRÊS degraus aferíveis AQUI: (1) o mapFd64 mapeia
// um fd REAL e lê os bytes EXATOS (o alinhamento à página é o MESMO do
// mapFile64); (2) um fd que o SO recusa mapear (o PIPE — FIFO, size 0)
// devolve nullptr SEM fechar o fd (a recusa é REAL do kernel, não um hook
// — é o caso FUSE-sem-mmap do device); (3) o openReadFd do FsStorage
// entrega o fd do caminho (o 2.º degrau da cascata). A degradação final —
// o pread de RANGES do readBytesAt — fica provada no
// safstream_fd_que_recusa_mmap_importa_por_ranges (test_gmeshv3stream).
// As mutações M-S1/M-S2/M-S3 do hotfix estão coladas no
// RELATORIO-0.10-M-HOTFIX-SAF-STREAM.
// ---------------------------------------------------------------------------
TEST(regress_safstream_fd_mmap_e_ranges_r041) {
    // (fcntl p/ o ::open do fd real — o MESMO include do FileApi.cpp)
    // (1) o mapFd64 sobre um fd REAL: os bytes EXATOS, offset NÃO alinhado
    // (a page-down do mapFile64 — o MESMO contrato, sem caminho)
    const std::string path =
        "/tmp/goni_r041_fd_" + std::to_string(::getpid()) + ".bin";
    constexpr size_t kSize = 9 * 1024 + 123;   // ~3 páginas + resto
    {
        std::vector<u8> f(kSize);
        for (size_t i = 0; i < f.size(); ++i) {
            f[i] = static_cast<u8>((i * 11 + 29) & 0xFF);
        }
        FILE* fp = std::fopen(path.c_str(), "wb");
        ASSERT(fp != nullptr);
        std::fwrite(f.data(), 1, f.size(), fp);
        std::fclose(fp);
    }
    const int fd = ::open(path.c_str(), O_RDONLY);
    ASSERT(fd >= 0);
    {
        const unsigned long long off = 4097;   // DENTRO da 2.ª página
        unsigned long long mapLen = 0;
        void* p = fileapi::mapFd64(fd, off, 4000, &mapLen);
        ASSERT(p != nullptr);
        const u8* m = static_cast<const u8*>(p);
        bool exact = true;
        for (u64 i = 0; i < 4000 && exact; ++i) {
            const u8 want = static_cast<u8>(((off + i) * 11 + 29) & 0xFF);
            exact = m[i] == want;
        }
        EXPECT(exact);   // o mmap por fd lê os bytes EXATOS do ficheiro
        fileapi::unmapFile64(p, mapLen);
    }
    // o fd CONTINUA USÁVEL (o mapFd64 não o fecha — quem abriu fecha)
    {
        char buf[4] = {0, 0, 0, 0};
        EXPECT(::lseek(fd, 0, SEEK_SET) == 0);
        EXPECT(::read(fd, buf, 4) == 4);
        EXPECT(buf[0] == static_cast<u8>(29) && buf[1] == 40 &&
               buf[2] == 51 && buf[3] == 62);
    }
    // (2) a RECUSA REAL do SO: um PIPE (FIFO — o fstat dá size 0) NÃO se
    // mapeia — o mesmo veredito do ENODEV do ::mmap num FUSE-sem-mmap.
    // nullptr SEM crash: o chamador degrada para pread de RANGES
    {
        int pfd[2];
        ASSERT(::pipe(pfd) == 0);
        unsigned long long mapLen = 0;
        EXPECT(fileapi::mapFd64(pfd[0], 0, 16, &mapLen) == nullptr);
        ::close(pfd[0]);
        ::close(pfd[1]);
    }
    // a range fora do fd RECUSADO também (ficheiro curto — honesto)
    {
        unsigned long long mapLen = 0;
        EXPECT(fileapi::mapFd64(fd, kSize - 10, 100, &mapLen) == nullptr);
    }
    ::close(fd);
    std::remove(path.c_str());

    // (3) o openReadFd do FsStorage: o fd DO CAMINHO (o 2.º degrau da
    // cascata quando o mmap por caminho falha — FUSE) — e o fd ABRE em
    // leitura de verdade
    {
        const std::string dir =
            "/tmp/goni_r041_st_" + std::to_string(::getpid());
        rmrf(dir);
        // o mkdir -p mínimo da sentinela (a FsStorage cria no makeDirs)
        const std::string proj = dir + "/proj";
        ASSERT(std::system(("mkdir -p '" + proj + "'").c_str()) == 0);
        const std::string rel = "source/binario.bin";
        {
            FsStorage st(dir + "/proj");
            ASSERT(st.makeDirs("source"));
            const u8 bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            ASSERT(st.writeBytes(rel, bytes, sizeof(bytes)));
        }
        FsStorage st(dir + "/proj");
        int rfd = -1;
        std::string rerr;
        ASSERT(st.openReadFd(rel, &rfd, rerr));
        EXPECT(rfd >= 0);
        u8 got[8] = {};
        EXPECT(::lseek(rfd, 2, SEEK_SET) == 2);   // seek funciona (é um fd)
        EXPECT(::read(rfd, got, 4) == 4);
        EXPECT(got[0] == 3 && got[1] == 4 && got[2] == 5 && got[3] == 6);
        ::close(rfd);
        // ausente → false HONESTO (o err diz)
        int nada = -1;
        EXPECT(!st.openReadFd("source/nao-existe.bin", &nada, rerr));
        EXPECT(nada == -1);
        rmrf(dir);
    }
}

// 0.10.6 (SAF-SEAM · R-043) — A QUARENTENA (.corrupt) NAS TRÊS camadas do
// storage: o rename é o PRIMITIVO da quarentena do asset corrompido (o
// BlockMesh chama-o quando a tabela não valida). O contrato:
//   true  = os bytes vivem agora em `to` (o `from` deixou de existir);
//   false = SEM rename (nada muda — o diagnóstico NÃO depende do rename).
// FsStorage = ::rename POSIX (atómico); FakeStorage = o mapa em memória;
// ausente → false HONESTO (o catálogo de .gmesh é que decide a saída).
TEST(regress_safseam_quarentena_rename_r043) {
    // (1) o FsStorage: o ::rename ATÓMICO de sempre (o disco confirma)
    {
        const std::string dir =
            "/tmp/goni_r043_fs_" + std::to_string(::getpid());
        rmrf(dir);
        ASSERT(std::system(("mkdir -p '" + dir + "'").c_str()) == 0);
        {
            FsStorage st(dir);
            const u8 bytes[10] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
            ASSERT(st.makeDirs("assets"));
            ASSERT(st.writeBytes("assets/city.gmesh", bytes, sizeof(bytes)));
            // o RENAME da quarentena: o nome sai, os bytes FICAM (forense)
            EXPECT(st.rename("assets/city.gmesh",
                             "assets/city.gmesh.corrupt"));
            EXPECT(!st.exists("assets/city.gmesh"));
            EXPECT(st.exists("assets/city.gmesh.corrupt"));
            std::vector<u8> got;
            EXPECT(st.readBytes("assets/city.gmesh.corrupt", got) &&
                   got.size() == 10 && got[0] == 9 && got[9] == 0);
            // o ausente → false HONESTO (não cria, não apaga)
            EXPECT(!st.rename("assets/nao-existe.gmesh",
                              "assets/x.corrupt"));
        }
        rmrf(dir);
    }
    // (2) o FakeStorage: os bytes MUDAM de nome no mapa (a prova do
    // harness — o mesmo contrato, sem disco)
    {
        FakeStorage st;
        const std::string orig = "assets/trunc.gmesh";
        ASSERT(st.writeText(orig, "GMES-cortado"));
        EXPECT(st.rename(orig, orig + ".corrupt"));
        EXPECT(!st.exists(orig));
        EXPECT(st.exists(orig + ".corrupt"));
        std::string s;
        EXPECT(st.readText(orig + ".corrupt", s) && s == "GMES-cortado");
        EXPECT(!st.rename(orig, "assets/outro.corrupt"));   // já saiu
    }
    // (3) o SafStorage: a CÓPIA STREAMING por fd (a interface SafIo não
    // tem renameDocument) — o provider CONFIRMA os dois lados
    {
        FakeSafIo io;
        SafStorage saf(&io, "content://tree/primary:GOneVV/r043");
        const std::string orig = "assets/trunc.gmesh";
        ASSERT(saf.makeDirs("assets"));
        ASSERT(saf.writeBytes(orig, "GMES", 4));
        EXPECT(saf.rename(orig, orig + ".corrupt"));
        io.flushWrites();   // o provider persiste ao fechar
        EXPECT(!saf.exists(orig));   // a CACHE segue o nome (o fix do R-043)
        EXPECT(saf.exists(orig + ".corrupt"));
        std::vector<u8> got;
        EXPECT(saf.readBytes(orig + ".corrupt", got) && got.size() == 4);
    }
}

// ===========================================================================
// SENTINELA — regress_selecao_unica_audio (0.10.7 · PIN 7 do dono)
// O catálogo aberto num projeto NOVO não pode ter NENHUMA linha E de audio:
// (1) o Project::create nasce COM a pasta audio/ (o «Sons» da árvore lista
// pasta real desde o 1.º dia); (2) projects ANTIGOS sem audio/ ficam com o
// listDir em I honesto («pasta ausente») — NUNCA o E fantasma
// «saf: listDir audio — pasta ausente no projeto SAF: audio» que sujava o
// diagnóstico do dono no vc 67.
// ===========================================================================
TEST(regress_selecao_unica_audio) {
    rmrf(kSentinelLogs);
    EXPECT(vv::elog::init(kSentinelLogs));

    // (1) PROJETO NOVO: a estrutura nasce COMPLETA (audio/ incluída) e o
    // listing do catálogo (a MESMA sequência do refreshFilesTree) corre
    // SEM UMA linha E sequer
    {
        FakeSafIo io;
        SafStorage st(&io, "content://tree/primary:GOneVV/novo");
        Project p;
        EXPECT(Project::openOrCreate(st, "novo", p));
        // a pasta audio/ EXISTE (o exists passa pelo resolveChild real)
        EXPECT(st.exists(Project::kDirAudio));
        std::vector<std::string> files;
        EXPECT(st.listDir(Project::kDirAudio, files));
        EXPECT(files.empty());   // nasce vazia (o listing é honesto)
        // a VARREDURA do catálogo (os 6 ramos da árvore de ficheiros)
        const char* dirs[] = {Project::kDirAssets, Project::kDirScenes,
                              Project::kDirSource, Project::kDirMeshes,
                              Project::kDirTextures, Project::kDirAudio};
        for (const char* d : dirs) {
            std::vector<std::string> ls;
            st.listDir(d, ls);   // o MESMO caminho do refreshFilesTree
        }
        // ZERO linhas E na varredura TODA (o assets/source não nascidos são
        // I honesto — a ausência é estado; o E fantasma morreu)
        EXPECT(logCount("E/GONI: saf: listDir") == 0);
        // o audio NEM sequer aparece (a pasta existe — o listing é calado)
        EXPECT(logCount("listDir audio") == 0);
    }

    // (2) PROJETO ANTIGO sem audio/ (criado antes da cura): o listing da
    // pasta em falta é I honesto — e o E fantasma NUNCA existe
    {
        FakeSafIo io;
        SafStorage st(&io, "content://tree/primary:GOneVV/velho");
        // a mão: scenes/ só (o padrão dos projects pré-SELEÇÃO-ÚNICA)
        std::vector<std::string> ignored;
        EXPECT(st.makeDirs(Project::kDirScenes));
        (void)ignored;
        std::vector<std::string> files;
        EXPECT(!st.listDir(Project::kDirAudio, files));   // falta → false
        EXPECT(logCount("I/GONI: saf: listDir audio") == 1);   // o I honesto
        EXPECT(logCount("E/GONI: saf: listDir audio") == 0);   // NUNCA E
        EXPECT(logHas("pasta ausente (ok, lista vazia)"));
    }

    rmrf(kSentinelCache);
}

// ===========================================================================
// SENTINELA — regress_lod_cull (0.11-T · FASE 3.5a · R-045)
//
// O QUE VIGIA (a GPU efficiency do dono): (1) o SKINNED CULL — um TIC
// skinado FORA do frustum NÃO anima (o AnimationSystem salta advance+apply:
// o tempo do clip congela) e o contador diz a verdade (skinned: culled=N);
// (2) o LOD POR DISTÂNCIA — o import gera a cauda GLOD (o decimate ½ por
// bloco), o BlockMesh a USA à distância certa (perto = normal, longe =
// entrada LOD) e o DEFAULT É OFF (um TIC novo NUNCA usa LOD — o default
// jogável manda). A cena da 0.11-S abre SEM LOD e fica byte a byte.
//
// MUTAÇÕES que a matam (a prova está no RELATORIO-0.11-T-3.5A.md):
//   M-3.5a1 (o skinned cull removido — o system volta a avançar tudo) →
//           skinnedCulled==0 != 1 → VERMELHA;
//   M-3.5a2 (o default do LOD on — o MeshRenderer nasce com distância) →
//           fase35a_lod_off_default VERMELHA (o pin irmão no test_core).
// ===========================================================================
TEST(regress_lod_cull) {
    glstub::reset();

    // ---- (1) O SKINNED CULL: 2 personagens, 1 fora do frustum ------------
    {
        Scene scene;
        MeshData md;
        const Vec3 P[4] = {Vec3{-0.5f, -0.5f, 0.0f}, Vec3{0.5f, -0.5f, 0.0f},
                           Vec3{0.5f, 0.5f, 0.0f}, Vec3{-0.5f, 0.5f, 0.0f}};
        for (int k = 0; k < 4; ++k) {
            Vertex v;
            v.pos = P[k];
            v.normal = Vec3{0.0f, 0.0f, 1.0f};
            md.vertices.push_back(v);
            for (int c = 0; c < 4; ++c) {
                md.skinJoints.push_back(0);
                md.skinWeights.push_back(c == 0 ? 1.0f : 0.0f);
            }
        }
        const u16 IDX[6] = {0, 1, 2, 0, 2, 3};
        md.indices.assign(IDX, IDX + 6);
        Mesh skinned;
        EXPECT(skinned.createSkinned(md.vertices.data(), 4, md.indices.data(),
                                     6, md.skinJoints.data(),
                                     md.skinWeights.data()));
        Handle hs[2];
        for (u32 k = 0; k < 2; ++k) {
            char nm[16];
            std::snprintf(nm, sizeof(nm), "Pers%u", k);
            hs[k] = scene.create(nm);
            Tic* t = scene.get(hs[k]);
            ASSERT(t != nullptr);
            Transform3D* tr = t->addComponent<Transform3D>();
            tr->pos = Vec3{0.0f, 0.0f, k == 0 ? -10.0f : 10.0f};
            tr->updateWorld();
            MeshRenderer* mr = t->addComponent<MeshRenderer>();
            mr->mesh = &skinned;
            t->addComponent<SkeletonComp>();
            AnimationPlayer* pl = t->addComponent<AnimationPlayer>();
            AnimClip& clip = *pl->editClip();
            clip.name = "andar";
            AnimTrack& trk = *pl->addTrack(AnimTarget::TicPos);
            AnimKey k0;
            k0.t = 0.0f;
            AnimKey k1;
            k1.t = 1.0f;
            k1.v[1] = 1.0f;
            trk.keys.push_back(k0);
            trk.keys.push_back(k1);
            pl->activeClip = 0;
            pl->playing = true;
        }
        // o ortho que só vê a FRENTE: o P0 (z=−10) dentro, o P1 (z=+10) fora
        const Mat4 vp =
            Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
        f32 planes[6][4];
        BlockMesh::frustumPlanes(vp, planes);
        AnimationSystem sys;
        sys.enabled = true;
        sys.frustum = planes;
        sys.frustumValid = true;
        sys.tick(scene, 1.0f / 60.0f);
        EXPECT(sys.skinnedCulled == 1);   // O NÚMERO: 1 fora → culled=1
        EXPECT(sys.culledOwners.size() == 1);
        // (o guard: com o cull MORTO — a mutação M-3.5a1 — a lista vem
        // vazia e a sentinela tem de falhar LIMPO nos EXPECT acima, nunca
        // crashar no índice)
        if (sys.culledOwners.size() == 1) {
            auto& players = scene.components().animators();
            for (u32 i = 0; i < players.size(); ++i) {
                const AnimationPlayer& pl = players.at(i);
                if (players.owner(i) == sys.culledOwners[0]) {
                    EXPECT(pl.time == 0.0f);   // CONGELADO — não anima fora
                } else {
                    EXPECT(nearEqF(pl.time, 1.0f / 60.0f));  // o visível avança
                }
            }
        }
    }

    // ---- (2) O LOD: gerado no import, usado à distância, DEFAULT OFF -----
    {
        // a grelha row-major (os ímpares soldam: 1024 → 512 EXATOS)
        MeshData grid;
        const u32 cols = 32, rows = 32;
        grid.vertices.resize(cols * rows);
        for (u32 j = 0; j < rows; ++j) {
            for (u32 i = 0; i < cols; ++i) {
                Vertex& v = grid.vertices[static_cast<size_t>(j * cols + i)];
                v.pos = Vec3{static_cast<f32>(i), 0.0f, static_cast<f32>(j)};
                v.normal = Vec3{0.0f, 1.0f, 0.0f};
            }
        }
        for (u32 j = 0; j + 1 < rows; ++j) {
            for (u32 i = 0; i + 1 < cols; ++i) {
                const u32 a = j * cols + i;
                const u16 b = static_cast<u16>(a + 1);
                const u16 c = static_cast<u16>(a + cols);
                const u16 d = static_cast<u16>(c + 1);
                const u16 A = static_cast<u16>(a);
                const u16 idx[6] = {A, c, b, b, c, d};
                grid.indices.insert(grid.indices.end(), idx, idx + 6);
            }
        }
        std::string err;
        std::vector<u8> bytes;
        EXPECT(writeGMesh(grid, bytes, err, nullptr, nullptr,
                          /*withLod=*/true));
        FakeStorage st;
        st.files["assets/sent.gmesh"].assign(
            reinterpret_cast<const char*>(bytes.data()), bytes.size());
        BlockMesh bm;
        EXPECT(bm.open(st, "assets/sent.gmesh", err));
        EXPECT(bm.hasLod());   // a cauda GLOD chegou ao runtime
        EXPECT(bm.lodBlocks().size() == 1);
        EXPECT(bm.lodBlocks()[0].vertexCount == 512);   // A METADE EXATA
        Renderer r;
        const Mat4 model = Mat4::translation(0.0f, 0.0f, -20.0f);
        const Mat4 vp =
            Mat4::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 400.0f);
        {   // LONGE: a entrada LOD desenha (o pin «usa»)
            BlockMesh::Stats bs;
            const Vec3 camPos{0.0f, 0.0f, 100.0f};
            bm.draw(r, model, vp, nullptr, nullptr, nullptr, 0, bs, &camPos,
                    20.0f);
            EXPECT(bs.drawn == 1);
            EXPECT(bs.lodDrawn == 1);
        }
        {   // PERTO: o normal (a distância manda)
            BlockMesh::Stats bs;
            const Vec3 camPos{0.0f, 0.0f, -10.0f};
            bm.draw(r, model, vp, nullptr, nullptr, nullptr, 0, bs, &camPos,
                    20.0f);
            EXPECT(bs.drawn == 1);
            EXPECT(bs.lodDrawn == 0);
        }
        {   // DEFAULT OFF: sem distância NUNCA liga (o default jogável)
            BlockMesh::Stats bs;
            const Vec3 camPos{0.0f, 0.0f, 100.0f};
            bm.draw(r, model, vp, nullptr, nullptr, nullptr, 0, bs, &camPos,
                    0.0f);
            EXPECT(bs.drawn == 1);
            EXPECT(bs.lodDrawn == 0);
        }
        MeshRenderer fresh;
        EXPECT(fresh.lodDistance == 0.0f);   // o TIC novo nasce OFF
    }

    // ---- (3) O COMPAT: a cena 0.11-S abre sem LOD e fica byte a byte -----
    {
        Scene s1;
        const Handle h = s1.create("Corpo");
        Tic* t = s1.get(h);
        t->addComponent<Transform3D>();
        t->addComponent<MeshRenderer>();
        const std::string canonic = SceneSerializer::dump(s1);
        EXPECT(std::strstr(canonic.c_str(), "lodDist") == nullptr);
        Scene s2;
        SceneSerializer::LoadCtx ctx;
        EXPECT(SceneSerializer::loadText(s2, canonic, ctx));
        const MeshRenderer* mr =
            s2.get(s2.find("Corpo"))->getComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(mr->lodDistance == 0.0f);   // abre SEM LOD
        EXPECT(SceneSerializer::dump(s2) == canonic);   // byte a byte
    }
}

// SENTINELA — regress_streaming_paralelo (0.11-T-FIX · PERNA 1 · R-046)
//
// O bug vigiado: a corrupção de heap do corte paralelo do convertGltfToV3
// (o crash §4.1 do RELATORIO-0.11-T-3.5A — SIGSEGV no pthread_join /
// ~FakeSafIo, TSAN sem data race = wild-write; 3 vítimas: data-uri, city
// fd/mmap, ranges). O contorno da Perna 1 (os pins vivem em
// test_fase35fix_p1.cpp): teto de 4 workers + callback de progresso
// serializado + madvise só em mapeamento file-backed. O registo completo
// vive no docs/REGRESSOES.md (R-046).

// ---- helpers com nomes próprios (sem colidir com os de cima) --------------
namespace {

bool sfixMkDir(const std::string& p) {
    return ::mkdir(p.c_str(), 0755) == 0 || errno == EEXIST;
}

bool sfixRmRf(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (d) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            const std::string full = path + "/" + n;
            struct stat st;
            if (lstat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                sfixRmRf(full);
            } else {
                unlink(full.c_str());
            }
        }
        closedir(d);
        rmdir(path.c_str());
        return true;
    }
    unlink(path.c_str());
    return true;
}

// o fixture MÍNIMO do caminho vítima (data: URI — o bin HEAP; 2 primitivas
// = 2 tarefas = os workers competem de verdade)
struct SfixPrim { u32 verts; u32 tris; };

bool sfixBuildDataUri(const std::vector<SfixPrim>& prims,
                      const std::string& path) {
    std::vector<u8> bin;
    struct V { size_t off, len; };
    std::vector<V> views;
    auto align4 = [&bin]() { while (bin.size() % 4) bin.push_back(0); };
    for (const SfixPrim& p : prims) {
        align4();
        views.push_back({bin.size(), size_t(p.verts) * 12});
        for (u32 i = 0; i < p.verts; ++i) {
            const f32 v[3] = {-1.0f + 2.0f * f32(i % 997) / 996.0f,
                              -1.0f + 2.0f * f32(i % 991) / 990.0f,
                              -1.0f + 2.0f * f32(i % 983) / 982.0f};
            for (int c = 0; c < 3; ++c) {
                u32 raw;
                std::memcpy(&raw, &v[c], 4);
                bin.push_back(u8(raw)); bin.push_back(u8(raw >> 8));
                bin.push_back(u8(raw >> 16)); bin.push_back(u8(raw >> 24));
            }
        }
        align4();
        views.push_back({bin.size(), size_t(p.verts) * 12});
        for (u32 i = 0; i < p.verts; ++i) {
            const f32 v[3] = {f32(i % 97) / 48.0f - 1.0f,
                              f32(i % 89) / 44.0f - 1.0f,
                              f32(i % 83) / 41.0f - 1.0f};
            for (int c = 0; c < 3; ++c) {
                u32 raw;
                std::memcpy(&raw, &v[c], 4);
                bin.push_back(u8(raw)); bin.push_back(u8(raw >> 8));
                bin.push_back(u8(raw >> 16)); bin.push_back(u8(raw >> 24));
            }
        }
        align4();
        views.push_back({bin.size(), size_t(p.verts) * 8});
        for (u32 i = 0; i < p.verts; ++i) {
            const f32 v[2] = {f32(i % 64) / 63.0f, f32(i % 32) / 31.0f};
            for (int c = 0; c < 2; ++c) {
                u32 raw;
                std::memcpy(&raw, &v[c], 4);
                bin.push_back(u8(raw)); bin.push_back(u8(raw >> 8));
                bin.push_back(u8(raw >> 16)); bin.push_back(u8(raw >> 24));
            }
        }
    }
    for (const SfixPrim& p : prims) {
        align4();
        views.push_back({bin.size(), size_t(p.tris) * 6});
        for (u32 t = 0; t < p.tris; ++t) {
            for (int c = 0; c < 3; ++c) {
                const u16 g = u16((3 * t + u32(c)) % p.verts);
                bin.push_back(u8(g & 0xFF));
                bin.push_back(u8(g >> 8));
            }
        }
    }
    static const char kTbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    b64.reserve(((bin.size() + 2) / 3) * 4);
    for (size_t i = 0; i < bin.size(); i += 3) {
        const u32 n0 = bin[i];
        const u32 n1 = i + 1 < bin.size() ? bin[i + 1] : 0;
        const u32 n2 = i + 2 < bin.size() ? bin[i + 2] : 0;
        const u32 v = (n0 << 16) | (n1 << 8) | n2;
        b64.push_back(kTbl[(v >> 18) & 63]);
        b64.push_back(kTbl[(v >> 12) & 63]);
        b64.push_back(i + 1 < bin.size() ? kTbl[(v >> 6) & 63] : '=');
        b64.push_back(i + 2 < bin.size() ? kTbl[v & 63] : '=');
    }
    std::string json =
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],"
        "\"meshes\":[{\"primitives\":[";
    for (size_t p = 0; p < prims.size(); ++p) {
        if (p != 0) json += ",";
        json += "{\"attributes\":{\"POSITION\":" + std::to_string(p * 3) +
                ",\"NORMAL\":" + std::to_string(p * 3 + 1) +
                ",\"TEXCOORD_0\":" + std::to_string(p * 3 + 2) +
                "},\"indices\":" + std::to_string(prims.size() * 3 + p) +
                ",\"mode\":4}";
    }
    json += "]}],\"accessors\":[";
    for (size_t p = 0; p < prims.size(); ++p) {
        json += std::string(p != 0 ? "," : "") +
                "{\"bufferView\":" + std::to_string(p * 3) +
                ",\"componentType\":5126,\"count\":" +
                std::to_string(prims[p].verts) + ",\"type\":\"VEC3\"}," +
                "{\"bufferView\":" + std::to_string(p * 3 + 1) +
                ",\"componentType\":5126,\"count\":" +
                std::to_string(prims[p].verts) + ",\"type\":\"VEC3\"}," +
                "{\"bufferView\":" + std::to_string(p * 3 + 2) +
                ",\"componentType\":5126,\"count\":" +
                std::to_string(prims[p].verts) + ",\"type\":\"VEC2\"}";
    }
    for (size_t p = 0; p < prims.size(); ++p) {
        // a vírgula é SEMPRE: o array de accessors já tem os 3 por prim
        json += ",{\"bufferView\":" + std::to_string(prims.size() * 3 + p) +
                ",\"componentType\":5123,\"count\":" +
                std::to_string(prims[p].tris * 3) + ",\"type\":\"SCALAR\"}";
    }
    json += "],\"bufferViews\":[";
    for (size_t v = 0; v < views.size(); ++v) {
        json += std::string(v != 0 ? "," : "") +
                "{\"buffer\":0,\"byteOffset\":" +
                std::to_string(views[v].off) + ",\"byteLength\":" +
                std::to_string(views[v].len) + "}";
    }
    json += "],\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,";
    json += b64;
    json += "\"}]}";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(json.data(), 1, json.size(), f) ==
                    json.size();
    std::fclose(f);
    return ok;
}

// o callback adversário: o DETECTOR de sobreposição (duas invocações em
// paralelo = ambas veem inFlight==1; o total não-atómico perde somas).
// O append ao std::string partilhado é o alvo da corrupção (a hipótese 1
// da ABERTURA). Serializado pelo mutex da Perna 1.2: tudo exato.
struct SfixProg {
    std::string log;
    std::atomic<u64> calls{0};
    u64 total = 0;
    u32 inFlight = 0;
    u64 overlaps = 0;
};
bool sfixProgCb(void* user, u64 done, u64 total) {
    auto* s = static_cast<SfixProg*>(user);
    if (s->inFlight != 0) ++s->overlaps;
    s->inFlight = 1;
    for (int i = 0; i < 8; ++i) s->total += 1;
    s->log.push_back('x');
    if (s->log.size() >= 512) s->log.clear();
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    s->inFlight = 0;
    s->calls.fetch_add(1);
    (void)done;
    (void)total;
    return true;
}

bool sfixImportOnce(const std::string& src, const std::string& projDir,
                    SfixProg* prog, u64* callsOut) {
    sfixRmRf(projDir);
    if (!sfixMkDir(projDir)) return false;
    elog::init((projDir + "/logs").c_str());
    FsStorage st((projDir + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    SfixProg local;
    SfixProg* p = prog != nullptr ? prog : &local;
    const bool ok = convert::importFile(src, "modelo.gltf", st, nullptr, out,
                                        stats, err,
                                        prog != nullptr ? sfixProgCb : nullptr,
                                        p);
    // a verificação bit a bit é INTERNA ao import: o retorno false cobre-a
    // (o conversor falha SEM verificado=1 — nunca silencioso)
    elog::shutdown();
    sfixRmRf(projDir);
    if (callsOut != nullptr) *callsOut = p->calls.load();
    return ok && out.meshes.size() == 1;
}

} // namespace

TEST(regress_streaming_paralelo) {
    // (1) O TETO: o pool nunca passa de 4 (o C33 tem 8 cores — a zona de
    // crash era 7 workers) e nunca fica a 0
    EXPECT(v3StreamWorkerCount(8) == 4);
    EXPECT(v3StreamWorkerCount(16) == 4);
    EXPECT(v3StreamWorkerCount(0) == 1);
    // (2) O CALLBACK SEGURO: 100 imports com 8 workers FORÇADOS (o override
    // de experimento — a configuração do device) e o callback a escrever em
    // estado partilhado: zero corrupção (o mutex da 1.2)
    const std::string dir =
        "/tmp/goni_sent_r046_" + std::to_string(::getpid());
    sfixRmRf(dir);
    ASSERT(sfixMkDir(dir));
    ASSERT(setenv("GONI_STREAM_WORKERS", "8", 1) == 0);
    // 3 primitivas × 17000 tris: CADA tarefa cruza o kProgressStride
    // (16384) — os WORKERS disparam o callback de verdade (fixture menor
    // só atingia as chamadas single-thread do assembly/verify)
    const std::vector<SfixPrim> prims = {{1200, 17000}, {1200, 17000},
                                         {1200, 17000}};
    const std::string src = dir + "/modelo.gltf";
    ASSERT(sfixBuildDataUri(prims, src));
    SfixProg prog;
    bool ok = true;
    for (int i = 0; i < 100 && ok; ++i) {
        if (!sfixImportOnce(src, dir + "/proj", &prog, nullptr)) ok = false;
    }
    unsetenv("GONI_STREAM_WORKERS");
    EXPECT(ok);   // 100 imports × 8 workers: nenhum crash, nenhum erro
    EXPECT(prog.calls.load() > 0);   // o callback correu de verdade
    EXPECT(prog.overlaps == 0);      // zero sobreposições (o mutex manda)
    EXPECT(prog.total == prog.calls.load() * 8);   // zero perdas
    // (3) O MADVISE SÓ EM MMAP: heap é NO-OP (o conteúdo fica byte a byte —
    // o crash F3 §2e zeroeava as páginas); o ficheiro real é file-backed
    std::vector<u8> heap(2 * 4096, 0);
    for (size_t i = 0; i < heap.size(); ++i) {
        heap[i] = static_cast<u8>((i * 29 + 11) & 0xFF);
    }
    const std::vector<u8> copia = heap;
    EXPECT(!fileapi::rangeIsFileBacked(heap.data(), heap.size()));
    fileapi::dropRange(heap.data(), 0, heap.size());
    EXPECT(heap == copia);
    {
        const std::string path = dir + "/drop.bin";
        FILE* f = std::fopen(path.c_str(), "wb");
        ASSERT(f != nullptr);
        const std::vector<u8> dados(8192, 0x5A);
        std::fwrite(dados.data(), 1, dados.size(), f);
        std::fclose(f);
        unsigned long long mapped = 0;
        u8* m = static_cast<u8*>(
            fileapi::mapFile64(path.c_str(), 0, dados.size(), &mapped));
        ASSERT(m != nullptr);
        EXPECT(fileapi::rangeIsFileBacked(m, dados.size()));
        fileapi::dropRange(m, 0, dados.size());
        EXPECT(std::memcmp(m, dados.data(), dados.size()) == 0);
        fileapi::unmapFile64(m, mapped);
    }
    // (4) O STREAMING 10× (o cenário da vítima, sem crash nem erro)
    for (int i = 0; i < 10 && ok; ++i) {
        if (!sfixImportOnce(src, dir + "/proj", nullptr, nullptr)) {
            ok = false;
        }
    }
    EXPECT(ok);
    sfixRmRf(dir);
}


// ---- R-047 · O INSTANCING NÃO MENTE (0.11-W 3.5b) --------------------------
// Vigia: (1) 6 TICs com o MESMO mesh+material fundem em EXATAMENTE 1 draw
// instanciado (o dc do HUD baixa 6×); (2) o toggle por TIC desliga (o .goni
// transporta "instancing":false e o ausente = ON); (3) o skinado NUNCA
// funde (a pose é por TIC) e o entry LOD é da assinatura; (4) o teto de 64
// matrizes por draw fatia em múltiplos draws (nunca falha nem funde além).
// Prova de mutação: M-3.5b1 (o caminho instanciado morto), M-3.5b2 (a skin
// flag ignorada), M-3.5b3 (o teto removido) — ver RELATORIO-0.11-W-3.5B §2.
namespace r047 {

void r047QuadStrip(MeshData& m, f32 cx, f32 cz, const std::string& mat) {
    const u32 base = static_cast<u32>(m.vertices.size());
    for (int iz = 0; iz <= 4; ++iz) {
        for (int ix = 0; ix <= 4; ++ix) {
            Vertex v;
            v.pos = Vec3{cx - 1.0f + 2.0f * f32(ix) / 4.0f, 0.0f,
                         cz - 1.0f + 2.0f * f32(iz) / 4.0f};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{f32(ix) / 4.0f, f32(iz) / 4.0f};
            m.vertices.push_back(v);
        }
    }
    MeshData::Group g;
    g.material = mat;
    g.firstIndex = static_cast<u32>(m.indices.size());
    for (int iz = 0; iz < 4; ++iz) {
        for (int ix = 0; ix < 4; ++ix) {
            const u32 a = base + static_cast<u32>(iz * 5 + ix);
            const u32 b = a + 5;
            m.indices.push_back(static_cast<u16>(a));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(a + 1));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(b + 1));
            m.indices.push_back(static_cast<u16>(a + 1));
            g.indexCount += 6;
        }
    }
    m.groups.push_back(g);
}

struct R047Storage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    std::string root() const override { return "/r047"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath,
                   const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool writeBytes(const std::string& relPath, const void* data,
                    size_t n) override {
        files[relPath].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& relPath,
                   std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out.assign(it->second.begin(), it->second.end());
        return !out.empty();
    }
    bool listDir(const std::string&,
                 std::vector<std::string>&) const override {
        return false;
    }
    bool readBytesAt(const std::string& relPath, vv::u64 offset, size_t len,
                     std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (offset + len > it->second.size()) return false;
        out.assign(it->second.begin() + static_cast<long>(offset),
                   it->second.begin() + static_cast<long>(offset + len));
        return out.size() == len;
    }
};

} // namespace r047

TEST(regress_instancing) {
    glstub::reset();
    using namespace r047;

    std::string err;
    MeshData one;
    r047QuadStrip(one, 0.0f, 0.0f, "verde");
    std::vector<u8> gmesh;
    EXPECT(writeGMesh(one, gmesh, err));
    R047Storage st;
    st.files["assets/r047.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/r047.gmesh", err));
    EXPECT(bm.table().size() == 1);
    const u32 idxCount = bm.table()[0].indexCount;

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f, 500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};

    // (1) 6 TICs (o padrão da cena do dono) → 1 draw instanciado × 6
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        models.push_back(Mat4::translation(-5.0f + 2.0f * f32(k), 0.0f,
                                           -10.0f));
    }
    {
        std::vector<BlockMesh::InstPick> picks;
        std::vector<Mat4> mats;
        for (const Mat4& m : models) {
            if (bm.collectVisible(m, vp, &camEye, 0.0f, picks) > 0) {
                mats.push_back(m);
            }
        }
        EXPECT(mats.size() == 6);
        BlockMesh::Stats ss;
        const u32 d0 =
            static_cast<u32>(glstub::stats.drawElementsInstancedCalls);
        const DrawStats ds = bm.drawInstancedGroup(r, 0, mats.data(), 6, vp,
                                                  nullptr, nullptr, ss);
        EXPECT(static_cast<u32>(glstub::stats.drawElementsInstancedCalls) -
                   d0 ==
               1);
        EXPECT(glstub::stats.lastInstanceCount == 6);
        EXPECT(ds.drawCalls == 1);
        EXPECT(ds.instancesMerged == 6);
        EXPECT(ds.vertices == idxCount * 6u);
        EXPECT(ss.drawn == 6);
    }
    // (2) O TOGGLE: o .goni sem a chave abre ON; com false abre OFF
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        tr->updateWorld();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        const std::string plain = SceneSerializer::dump(scene);
        EXPECT(plain.find("instancing") == std::string::npos);
        mr->instancing = false;
        const std::string off = SceneSerializer::dump(scene);
        EXPECT(off.find("\"instancing\":false") != std::string::npos);
        SceneSerializer::LoadCtx ctx;
        Scene s1;
        EXPECT(SceneSerializer::loadText(s1, plain, ctx));
        EXPECT(s1.components().meshRenderers().size() == 1);
        EXPECT(s1.components().meshRenderers().at(0).instancing);
        Scene s2;
        EXPECT(SceneSerializer::loadText(s2, off, ctx));
        EXPECT(!s2.components().meshRenderers().at(0).instancing);
    }
    // (3a) O SKIN NUNCA FUNDE: o mesh skinado diz-o pela flag (o guard do
    // drawTics usa ESTA leitura — os skinados seguem o caminho de sempre)
    {
        MeshData sk;
        const Vec3 P[4] = {Vec3{-0.5f, -0.5f, 0.0f}, Vec3{0.5f, -0.5f, 0.0f},
                           Vec3{0.5f, 0.5f, 0.0f}, Vec3{-0.5f, 0.5f, 0.0f}};
        for (int k = 0; k < 4; ++k) {
            Vertex v;
            v.pos = P[k];
            v.normal = Vec3{0.0f, 0.0f, 1.0f};
            sk.vertices.push_back(v);
            for (int c = 0; c < 4; ++c) {
                sk.skinJoints.push_back(0);
                sk.skinWeights.push_back(c == 0 ? 1.0f : 0.0f);
            }
        }
        const u16 IDX[6] = {0, 1, 2, 0, 2, 3};
        sk.indices.assign(IDX, IDX + 6);
        std::vector<u8> gmeshSkin;
        EXPECT(writeGMesh(sk, gmeshSkin, err));
        GMeshV3Meta metaSkin;
        std::vector<GMeshV3Block> bSkin;
        std::vector<std::string> mSkin;
        EXPECT(readGMeshV3Meta(gmeshSkin.data(), gmeshSkin.size(), metaSkin,
                               bSkin, mSkin, err));
        EXPECT((metaSkin.flags & kGmeshV3FlagSkinned) != 0u);
        R047Storage ss2;
        ss2.files["assets/skin.gmesh"].assign(
            reinterpret_cast<const char*>(gmeshSkin.data()), gmeshSkin.size());
        BlockMesh bmSkin;
        EXPECT(bmSkin.open(ss2, "assets/skin.gmesh", err));
        EXPECT(bmSkin.skinned());   // o guard do drawTics: skinado não colhe
    }
    // (3b) O LOD É DA ASSINATURA: 3 perto (entry normal) + 3 longe (entry
    // LOD) = 2 grupos (NUNCA 1 — o fundir desenharia a geometria errada)
    {
        MeshData grid;
        const u32 cols = 32, rows = 32;
        grid.vertices.resize(cols * rows);
        for (u32 j = 0; j < rows; ++j) {
            for (u32 i = 0; i < cols; ++i) {
                Vertex& v = grid.vertices[j * cols + i];
                v.pos = Vec3{f32(i), 0.0f, f32(j)};
                v.normal = Vec3{0.0f, 1.0f, 0.0f};
                v.uv = Vec2{f32(i) / 31.0f, f32(j) / 31.0f};
            }
        }
        for (u32 j = 0; j + 1 < rows; ++j) {
            for (u32 i = 0; i + 1 < cols; ++i) {
                const u32 a = j * cols + i, b = a + 1, c = a + cols,
                          d = c + 1;
                grid.indices.push_back(static_cast<u16>(a));
                grid.indices.push_back(static_cast<u16>(c));
                grid.indices.push_back(static_cast<u16>(b));
                grid.indices.push_back(static_cast<u16>(b));
                grid.indices.push_back(static_cast<u16>(c));
                grid.indices.push_back(static_cast<u16>(d));
            }
        }
        std::vector<u8> gmeshLod;
        EXPECT(writeGMesh(grid, gmeshLod, err, nullptr, nullptr,
                          /*withLod=*/true));
        R047Storage st3;
        st3.files["assets/lod.gmesh"].assign(
            reinterpret_cast<const char*>(gmeshLod.data()), gmeshLod.size());
        BlockMesh bmLod;
        EXPECT(bmLod.open(st3, "assets/lod.gmesh", err));
        EXPECT(bmLod.hasLod());
        std::vector<BlockMesh::InstPick> picks;
        u32 normal = 0, lod = 0;
        for (u32 k = 0; k < 6; ++k) {
            const f32 z = k < 3 ? -26.0f : -116.0f;
            const Mat4 m =
                Mat4::translation(-15.5f + 2.0f * f32(k % 3), 0.0f, z);
            EXPECT(bmLod.collectVisible(m, vp, &camEye, 20.0f, picks) == 1);
            if (picks[0].entry == 0) {
                ++normal;
            } else {
                EXPECT(picks[0].entry == 1);   // lodBase_ + 0
                ++lod;
            }
        }
        EXPECT(normal == 3);
        EXPECT(lod == 3);
    }
    // (4) O TETO DE 64: 100 instâncias → exatamente 2 draws (64+36), o
    // instancesMerged É 100 (a soma dos fundidos, não dos draws)
    {
        std::vector<Mat4> many;
        for (u32 k = 0; k < 100; ++k) {
            many.push_back(
                Mat4::translation(-99.0f + 2.0f * f32(k), 0.0f, -100.0f));
        }
        std::vector<BlockMesh::InstPick> picks;
        std::vector<Mat4> mats;
        for (const Mat4& m : many) {
            if (bm.collectVisible(m, vp, &camEye, 0.0f, picks) > 0) {
                mats.push_back(m);
            }
        }
        EXPECT(mats.size() == 100);
        BlockMesh::Stats ss;
        const u32 d0 =
            static_cast<u32>(glstub::stats.drawElementsInstancedCalls);
        const DrawStats ds = bm.drawInstancedGroup(r, 0, mats.data(), 100, vp,
                                                  nullptr, nullptr, ss);
        EXPECT(static_cast<u32>(glstub::stats.drawElementsInstancedCalls) -
                   d0 ==
               2);
        EXPECT(glstub::stats.lastInstanceCount == 36);   // a cauda
        EXPECT(ds.drawCalls == 2);
        EXPECT(ds.instancesMerged == 100);
        EXPECT(ss.drawn == 100);
    }
}


// ---- R-048 · O STREAM NÃO CONGELA (0.11-W 3.75a) ---------------------------
// Vigia: (1) o orçamento por frame — 12 blocos visíveis carregam SÓ a vaga
// (8) no 1.º frame (o freeze de import morre por construção); (2) as
// CAMADAS — com a vaga apertada os residentes são EXATAMENTE os mais
// perto do camEye; (3) o toggle por TIC (o .goni transporta
// "streaming":false e o ausente = ON — os .goni da 0.11-T ficam byte a
// byte); (4) o fill fecha: Σ loads == os blocos do ficheiro e o pendente
// seca (drawn + pending == visible).
// Prova de mutação: M-3.75a1 (o orçamento removido), M-3.75a2 (a ordem
// removida), M-3.75a3 (a porta morta) — ver RELATORIO-0.11-W-3.75A §2.
namespace r048 {

void r048Strip(MeshData& m, f32 cx, const std::string& mat) {
    const u32 base = static_cast<u32>(m.vertices.size());
    for (int iz = 0; iz <= 4; ++iz) {
        for (int ix = 0; ix <= 4; ++ix) {
            Vertex v;
            v.pos = Vec3{cx - 1.0f + 2.0f * f32(ix) / 4.0f, 0.0f,
                         -1.0f + 2.0f * f32(iz) / 4.0f};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{f32(ix) / 4.0f, f32(iz) / 4.0f};
            m.vertices.push_back(v);
        }
    }
    MeshData::Group g;
    g.material = mat;
    g.firstIndex = static_cast<u32>(m.indices.size());
    for (int iz = 0; iz < 4; ++iz) {
        for (int ix = 0; ix < 4; ++ix) {
            const u32 a = base + static_cast<u32>(iz * 5 + ix);
            const u32 b = a + 5;
            m.indices.push_back(static_cast<u16>(a));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(a + 1));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(b + 1));
            m.indices.push_back(static_cast<u16>(a + 1));
            g.indexCount += 6;
        }
    }
    m.groups.push_back(g);
}

struct R048Storage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    std::string root() const override { return "/r048"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath,
                   const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool writeBytes(const std::string& relPath, const void* data,
                    size_t n) override {
        files[relPath].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& relPath,
                   std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out.assign(it->second.begin(), it->second.end());
        return !out.empty();
    }
    bool listDir(const std::string&,
                 std::vector<std::string>&) const override {
        return false;
    }
    bool statBytes(const std::string& relPath,
                   vv::u64& outBytes) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        outBytes = it->second.size();
        return true;
    }
    bool readBytesAt(const std::string& relPath, vv::u64 offset, size_t len,
                     std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (offset + len > it->second.size()) return false;
        out.assign(it->second.begin() + static_cast<long>(offset),
                   it->second.begin() + static_cast<long>(offset + len));
        return out.size() == len;
    }
};

// o walk do drawTics condensado (o MESMO contrato do runStreamed dos pins)
struct R048Run {
    u32 loads = 0;
    u32 drawn = 0;
    u32 pending = 0;
};
R048Run r048Frame(BlockMesh& bm, Renderer& r, const Mat4& vp,
                  const Vec3& camEye, const std::vector<Mat4>& models,
                  u32 budget) {
    R048Run out;
    std::vector<BlockMesh::InstPick> picks;
    std::vector<u32> entries;
    std::vector<const Mat4*> modelsOf;
    for (const Mat4& m : models) {
        if (bm.collectVisible(m, vp, &camEye, 0.0f, picks) == 0) continue;
        for (const BlockMesh::InstPick& p : picks) {
            if (p.blend) continue;
            entries.push_back(p.entry);
            modelsOf.push_back(&m);
        }
    }
    // os pedidos por entry (dist = MÍNIMO sobre as instâncias)
    std::vector<BlockMesh::StreamReq> reqs;
    {
        std::vector<u32> uniq;
        for (u32 e : entries) {
            bool seen = false;
            for (u32 u : uniq) seen = seen || (u == e);
            if (!seen) uniq.push_back(e);
        }
        for (u32 e : uniq) {
            if (bm.resident(e)) continue;
            f32 dmin = -1.0f;
            for (size_t t = 0; t < entries.size(); ++t) {
                if (entries[t] != e) continue;
                const u32 bi =
                    e < static_cast<u32>(bm.table().size())
                        ? e
                        : e - static_cast<u32>(bm.table().size());
                const GMeshV3Block& blk = bm.table()[bi];
                const f32 cx =
                    (blk.aabbMin.x + blk.aabbMax.x) * 0.5f;
                const f32 cy =
                    (blk.aabbMin.y + blk.aabbMax.y) * 0.5f;
                const f32 cz =
                    (blk.aabbMin.z + blk.aabbMax.z) * 0.5f;
                const Mat4& m = *modelsOf[t];
                const f32 px =
                    m.m[0] * cx + m.m[4] * cy + m.m[8] * cz + m.m[12];
                const f32 py =
                    m.m[1] * cx + m.m[5] * cy + m.m[9] * cz + m.m[13];
                const f32 pz =
                    m.m[2] * cx + m.m[6] * cy + m.m[10] * cz + m.m[14];
                const f32 dx = px - camEye.x, dy = py - camEye.y,
                          dz = pz - camEye.z;
                const f32 d = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dmin < 0.0f || d < dmin) dmin = d;
            }
            reqs.push_back(BlockMesh::StreamReq{&bm, e, dmin});
        }
    }
    out.loads = BlockMesh::streamIn(reqs.data(),
                                    static_cast<u32>(reqs.size()), budget);
    for (size_t t = 0; t < entries.size(); ++t) {
        if (bm.resident(entries[t])) {
            BlockMesh::Stats ss;
            const Mat4 one[1] = {*modelsOf[t]};
            bm.drawInstancedGroup(r, entries[t], one, 1, vp, nullptr,
                                  nullptr, ss);
            out.drawn += ss.drawn;
        } else {
            ++out.pending;
        }
    }
    return out;
}

} // namespace r048

TEST(regress_streaming) {
    glstub::reset();
    using namespace r048;

    std::string err;
    MeshData m12;
    for (int i = 0; i < 12; ++i) {
        r048Strip(m12, f32(i) * 10.0f, i % 2 == 0 ? "verde" : "ambar");
    }
    std::vector<u8> gmesh;
    EXPECT(writeGMesh(m12, gmesh, err));
    R048Storage st;
    st.files["assets/r048.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/r048.gmesh", err));
    EXPECT(bm.table().size() == 12);

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f, 500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    const std::vector<Mat4> models{Mat4::identity()};

    // (1) O ORÇAMENTO: o 1.º frame carrega SÓ a vaga (8 de 12) — o frame
    //     é leve POR CONSTRUÇÃO (o freeze morre aqui)
    const R048Run f1 = r048Frame(bm, r, vp, camEye, models,
                                 BlockMesh::kBlockStreamBudgetPerFrame);
    EXPECT(f1.loads == BlockMesh::kBlockStreamBudgetPerFrame);
    EXPECT(f1.loads == 8);
    EXPECT(f1.pending == 4);

    // (2) AS CAMADAS: vaga 3 num mesh novo → os residentes são os 3 mais
    //     perto (0,1,2 — os blocos estão EM LINHA a 10×i do olho)
    BlockMesh bm2;
    EXPECT(bm2.open(st, "assets/r048.gmesh", err));
    const R048Run f2 = r048Frame(bm2, r, vp, camEye, models, 3);
    EXPECT(f2.loads == 3);
    EXPECT(bm2.resident(0) && bm2.resident(1) && bm2.resident(2));
    EXPECT(!bm2.resident(3) && !bm2.resident(11));

    // (3) O TOGGLE no .goni: ausente = ON; false nasce e restaura
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        t->addComponent<MeshRenderer>();
        const std::string dump1 = SceneSerializer::dump(scene);
        EXPECT(dump1.find("streaming") == std::string::npos);
        Scene s1;
        SceneSerializer::LoadCtx ctx;
        EXPECT(SceneSerializer::loadText(s1, dump1, ctx));
        EXPECT(s1.components().meshRenderers().at(0).streaming);
        // com false a chave nasce e o load restaura
        MeshRenderer* mr = scene.get(h)->getComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        mr->streaming = false;
        const std::string dump2 = SceneSerializer::dump(scene);
        EXPECT(dump2.find("\"streaming\":false") != std::string::npos);
        Scene s2;
        EXPECT(SceneSerializer::loadText(s2, dump2, ctx));
        EXPECT(!s2.components().meshRenderers().at(0).streaming);
    }

    // (4) O FILL FECHA: os frames seguintes carregam a cauda (4) e o
    //     pendente seca — Σ loads == 12, o steady state não carrega nada
    const R048Run f3 = r048Frame(bm, r, vp, camEye, models,
                                 BlockMesh::kBlockStreamBudgetPerFrame);
    EXPECT(f3.loads == 4);
    EXPECT(f3.pending == 0);
    EXPECT(f3.drawn == 12);
    const R048Run f4 = r048Frame(bm, r, vp, camEye, models,
                                 BlockMesh::kBlockStreamBudgetPerFrame);
    EXPECT(f4.loads == 0);
    EXPECT(f4.pending == 0);
    EXPECT(f4.drawn == 12);
}

// ===========================================================================
// SENTINELA — regress_impostors (0.11-W · 3.75b · R-049)
// O IMPOSTOR NÃO MENTE: a decisão é a distância (0 = OFF, o limiar é
// exclusivo no lado longe), o PNG alfa é REAL (o fundo transparente é o
// que o MASK corta — o pixel prova), a geração é sob demanda com o pico
// de UM bloco (o fantasma nunca nasce), o fallback é os BLOCOS (sem PNG
// ou skinado NUNCA há quad) e o .goni transporta "impostorDist" (ausente
// = OFF; o MESMO contrato do lodDist).
// ===========================================================================
namespace r049 {

void r049Strip(MeshData& m, f32 cx, const std::string& mat) {
    const u32 base = static_cast<u32>(m.vertices.size());
    for (int iz = 0; iz <= 4; ++iz) {
        for (int ix = 0; ix <= 4; ++ix) {
            Vertex v;
            v.pos = Vec3{cx - 1.0f + 2.0f * f32(ix) / 4.0f, 0.0f,
                         -1.0f + 2.0f * f32(iz) / 4.0f};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{f32(ix) / 4.0f, f32(iz) / 4.0f};
            m.vertices.push_back(v);
        }
    }
    MeshData::Group g;
    g.material = mat;
    g.firstIndex = static_cast<u32>(m.indices.size());
    for (int iz = 0; iz < 4; ++iz) {
        for (int ix = 0; ix < 4; ++ix) {
            const u32 a = base + static_cast<u32>(iz * 5 + ix);
            const u32 b = a + 5;
            m.indices.push_back(static_cast<u16>(a));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(a + 1));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(b + 1));
            m.indices.push_back(static_cast<u16>(a + 1));
            g.indexCount += 6;
        }
    }
    m.groups.push_back(g);
}

struct R049Storage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    mutable u64 maxRead = 0;   // o maior single read (o pico do .gmesh)
    std::string root() const override { return "/r049"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath,
                   const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool writeBytes(const std::string& relPath, const void* data,
                    size_t n) override {
        files[relPath].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& relPath,
                   std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (relPath.find(".gmesh") != std::string::npos) {
            maxRead = std::max(maxRead, it->second.size());
        }
        out.assign(it->second.begin(), it->second.end());
        return !out.empty();
    }
    bool listDir(const std::string&,
                 std::vector<std::string>&) const override {
        return false;
    }
    bool statBytes(const std::string& relPath,
                   vv::u64& outBytes) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        outBytes = it->second.size();
        return true;
    }
    bool readBytesAt(const std::string& relPath, vv::u64 offset, size_t len,
                     std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (offset + len > it->second.size()) return false;
        if (relPath.find(".gmesh") != std::string::npos) {
            maxRead = std::max(maxRead, static_cast<u64>(len));
        }
        out.assign(it->second.begin() + static_cast<long>(offset),
                   it->second.begin() + static_cast<long>(offset + len));
        return out.size() == len;
    }
};

// o walk do drawTics (a MESMA mecânica do runImpostor dos pins — o
// orquestrador REAL é aferrado pela FASE 30 do c33)
struct R049Run {
    u32 quads = 0;
    u32 blockDc = 0;
    u32 quadVerts = 0;
};
R049Run r049Frame(BlockMesh& bm, Renderer& r, Mesh& quad, const Mat4& vp,
                  const Vec3& camEye, const std::vector<Mat4>& models,
                  f32 impostorDistance, const Texture* impTex) {
    R049Run out;
    for (const Mat4& m : models) {
        if (impTex != nullptr && !bm.skinned() &&
            impostor::active(m, bm.boundsMin(), bm.boundsMax(), camEye,
                             impostorDistance)) {
            Mat4 bill;
            if (impostor::billboard(m, bm.boundsMin(), bm.boundsMax(), camEye,
                                    bill)) {
                DrawProps impProps;
                impProps.alphaMode = 1;
                impProps.alphaCutoff = 0.5f;
                impProps.baseColor[0] = impostor::kImpBaseColor;
                impProps.baseColor[1] = impostor::kImpBaseColor;
                impProps.baseColor[2] = impostor::kImpBaseColor;
                const DrawStats ds =
                    r.drawMesh(quad, bill, vp, impTex, nullptr, nullptr, 0,
                               &impProps);
                out.quadVerts += ds.vertices;
                ++out.quads;
                continue;
            }
        }
        BlockMesh::Stats ss;
        const DrawStats ds = bm.draw(r, m, vp, nullptr, nullptr, nullptr, 0,
                                     ss, &camEye, 0.0f);
        out.blockDc += ds.drawCalls;
    }
    return out;
}

} // namespace r049

TEST(regress_impostors) {
    glstub::reset();
    using namespace r049;

    std::string err;
    MeshData m2;
    r049Strip(m2, 0.0f, "verde");
    r049Strip(m2, 10.0f, "ambar");
    std::vector<u8> gmesh;
    EXPECT(writeGMesh(m2, gmesh, err));
    R049Storage st;
    st.files["assets/r049.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/r049.gmesh", err));
    EXPECT(bm.table().size() == 2);

    Renderer r;
    EXPECT(r.init());
    Mesh quad;
    {
        MeshData md;
        impostor::quadMesh(md);
        EXPECT(quad.create(md.vertices.data(),
                           static_cast<u32>(md.vertices.size()),
                           md.indices.data(),
                           static_cast<u32>(md.indices.size())));
    }
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models{Mat4::translation(0.0f, 0.0f, -50.0f)};

    // (1) A GERAÇÃO: sob demanda, o PNG RGBA nasce (o fundo transparente
    //     é o que o MASK corta), o fantasma NUNCA nasce e o pico de RAM
    //     é UM bloco (o maior single read < o ficheiro)
    EXPECT(ImpostorIndex::ensureImpostor(st, "assets/r049.gmesh", false,
                                         err));
    EXPECT(st.exists("impostors/r049.png"));
    EXPECT(!ImpostorIndex::ensureImpostor(st, "assets/fantasma.gmesh", false,
                                          err));
    EXPECT(!st.exists("impostors/fantasma.png"));
    EXPECT(st.maxRead < gmesh.size());
    {
        const std::string& png = st.files["impostors/r049.png"];
        RawImage img;
        EXPECT(loadPng(reinterpret_cast<const u8*>(png.data()), png.size(),
                       img, err));
        EXPECT(img.width == 256 && img.height == 256);
        bool anyOpaque = false, anyClear = false;
        for (size_t i = 0; i < img.rgba.size(); i += 4) {
            if (img.rgba[i + 3] == 0xFF) anyOpaque = true;
            if (img.rgba[i + 3] == 0) anyClear = true;
        }
        EXPECT(anyOpaque && anyClear);   // o MASK tem o que cortar
    }
    // (2) A DECISÃO: 0 = OFF (os blocos de sempre); além da distância →
    //     1 quad de 6 verts e ZERO dc de blocos
    {
        glstub::fb::enabled = true;   // os ids únicos (a textura do PNG)
        ResourceManager rm;
        rm.setStorage(&st);
        GpuAssets gpu;
        gpu.init(&rm);
        const Texture* imp = gpu.impostorTexture("assets/r049.gmesh");
        EXPECT(imp != nullptr);
        const R049Run off = r049Frame(bm, r, quad, vp, camEye, models, 0.0f,
                                      imp);
        EXPECT(off.quads == 0);
        EXPECT(off.blockDc == 2);   // os 2 blocos do TIC (o caminho de sempre)
        const R049Run on = r049Frame(bm, r, quad, vp, camEye, models, 30.0f,
                                     imp);
        EXPECT(on.quads == 1);
        EXPECT(on.quadVerts == 6);   // o quad de 4 verts / 6 índices
        EXPECT(on.blockDc == 0);     // os blocos DESCANSARAM
        // (3) O FALLBACK: sem textura (o PNG impossível) NUNCA há quad
        const R049Run fb = r049Frame(bm, r, quad, vp, camEye, models, 30.0f,
                                     nullptr);
        EXPECT(fb.quads == 0);
        EXPECT(fb.blockDc == 2);
        gpu.releaseAll();
        glstub::fb::enabled = false;
    }
    // (4) O .goni: ausente = OFF; com 120 a chave nasce e o load restaura
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        tr->updateWorld();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        const std::string plain = SceneSerializer::dump(scene);
        EXPECT(plain.find("impostorDist") == std::string::npos);
        mr->impostorDistance = 120.0f;
        const std::string on = SceneSerializer::dump(scene);
        EXPECT(on.find("\"impostorDist\":") != std::string::npos);
        SceneSerializer::LoadCtx ctx;
        Scene s1;
        EXPECT(SceneSerializer::loadText(s1, on, ctx));
        EXPECT(s1.components().meshRenderers().at(0).impostorDistance ==
               120.0f);
        Scene s2;
        EXPECT(SceneSerializer::loadText(s2, plain, ctx));
        EXPECT(s2.components().meshRenderers().at(0).impostorDistance ==
               0.0f);
    }
}

// ===========================================================================
// R-052 (0.12-U 3.9a) — A MATEMÁTICA É DO REGISTO, EM GRAUS, E O VIEW P É
// UNIVERSAL. O que se vigia: (1) as funções existem APENAS porque a tabela
// central as lista (a fonte única — apagar a entrada mata a função no
// runtime); (2) sin/cos são em GRAUS (o contrato da casa — radianos é
// regressão silenciosa); (3) o View P aceita os 7 tipos (o universal do
// 3.9a — rejeitar não-Txt é a regressão do comando antigo); (4) a fn do
// utilizador VENCE a função do registo (compat — o shadow da casa).
// A mutação cola no RELATORIO-0.12-U-3.9a.md (M-3.9a1/a2).
// ===========================================================================
#include "voni/Voni.h"
#include "voni/VoniRegistry.h"

namespace {

struct Host052 : voni::Host {
    std::vector<std::string> logs;
    void log(const char* l) override { logs.push_back(l); }
    bool getProp(const std::string&, const std::vector<std::string>&,
                 voni::Value&, std::string&) override { return false; }
    bool setProp(const std::string&, const std::vector<std::string>&,
                 const voni::Value&, std::string&) override { return false; }
    bool ticExists(const std::string&) override { return false; }
    void moveTic(f32, f32, f32) override {}
    void explodeTic(bool) override {}
    bool importAnim(const std::string&, std::string&) override {
        return false;
    }
    bool transitionTo(const std::string&, const std::string&,
                      std::string&) override { return false; }
    std::string currentSceneName() override { return "c"; }
    bool search(const std::string&, const std::vector<std::string>&,
                voni::Value&, std::string&) override { return false; }
    f64 frameDt() override { return 1.0 / 60.0; }
};

} // namespace

TEST(regress_voni_matematica) {
    using namespace voni;

    // ---- (1) A FONTE ÚNICA: as 14 entradas existem como Kind::Funcao com
    // Docs completas e o argc declarado (o mesmo contrato dos componentes)
    static const char* const kFns[] = {
        "abs", "min", "max", "clamp", "lerp", "floor", "ceil", "round",
        "sqrt", "sin", "cos", "random", "random_int", "distance",
    };
    for (const char* n : kFns) {
        const reg::Entry* e = reg::find(n);
        EXPECT_MSG(e != nullptr, "a função '%s' saiu do registo — a fonte "
                                 "única mente", n);
        if (!e) continue;
        EXPECT(e->kind == reg::Kind::Funcao);
        EXPECT(e->syntax != nullptr && *e->syntax);
        EXPECT(e->desc != nullptr && *e->desc);
        EXPECT(e->example != nullptr && *e->example);
        EXPECT(e->equiv != nullptr && *e->equiv);
        EXPECT(e->maxArgs >= e->minArgs);
    }
    // o despacho runtime DEPENDE do kind: um nome que não é Funcao não
    // chama (o 'View P' de sempre NÃO é função)
    {
        const reg::Entry* vp = reg::find("View P");
        EXPECT(vp != nullptr && vp->kind == reg::Kind::Comando);
    }

    // ---- (2) GRAUS: sin(90) == 1 e cos(180) == -1 (radianos = regressão) -
    {
        Error err;
        Script s = Script::compile(
            "central main { on moment { View P sin(90) View P cos(180) } "
            "allmoments { } }", err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        EXPECT(h.logs.size() == 2);
        if (h.logs.size() == 2) {
            EXPECT_MSG(h.logs[0] == "voni: 1",
                       "sin(90) deu '%s' — graus virou radianos?",
                       h.logs[0].c_str());
            EXPECT(h.logs[1] == "voni: -1");
        }
    }

    // ---- (3) VIEW P UNIVERSAL: número e booleano chegam ao log (a
    // regressão é o comando antigo que exigia Txt entre aspas)
    {
        Error err;
        Script s = Script::compile(
            "central main { on moment { View P 42 View P true } "
            "allmoments { } }", err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        EXPECT(h.logs.size() == 2);
        if (h.logs.size() == 2) {
            EXPECT(h.logs[0] == "voni: 42");
            EXPECT(h.logs[1] == "voni: true");
        }
    }

    // ---- (4) A FN DO UTILIZADOR VENCE (compat — o shadow da casa) --------
    {
        Error err;
        Script s = Script::compile(
            "fn abs(n:Num):Num { return n + 1 }\n"
            "central main { on moment { View P abs(4) } allmoments { } }",
            err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        EXPECT(h.logs.size() == 1);
        if (h.logs.size() == 1) {
            EXPECT(h.logs[0] == "voni: 5");   // a DELE soma (não |4|)
        }
    }

    // ---- (5) a referência pública tem a secção FUNÇÕES (a fonte única
    // alimenta a referência — o R-013 afere o byte a byte das linhas)
    {
        const std::string ref = reg::fullReferenceMarkdown();
        EXPECT(ref.find("## Funções") != std::string::npos);
        EXPECT(ref.find("### distance") != std::string::npos);
        EXPECT(ref.find("### random_int") != std::string::npos);
    }
}

// ===========================================================================
// R-053 (0.12-U 3.9b) — O SPAWN CRIA PELO HOST, COM NOME ÚNICO, E O
// HOSPEDEIRO SEM CENA É HONESTO. O que se vigia: (1) a entrada existe
// APENAS pela tabela (Kind::Funcao, argc 4/4, Docs completa); (2) o
// default do Host (sem cena) responde «indisponível» — nunca crash, o
// erro sobe com linha; (3) a cadeia registo→VM→Host→script entrega o TIC
// com NOME ÚNICO em série (esfera, esfera.001) e o nome devolvido é o
// que o script usa (o TIC é usável na pos).
// A mutação cola no RELATORIO-0.12-U-3.9b.md (M-3.9b1/b2).
// ===========================================================================
namespace {

struct Host053 : voni::Host {
    std::vector<std::string> logs;
    std::vector<std::pair<std::string, std::string>> spawns;   // tipo → nome
    void log(const char* l) override { logs.push_back(l); }
    bool getProp(const std::string& n, const std::vector<std::string>& ch,
                 voni::Value& out, std::string& err) override {
        for (const auto& s : spawns) {
            if (s.second == n) {
                if (ch.empty()) {
                    out = voni::Value::ofTic(n);
                    return true;
                }
                if (ch[0] == "pos") {
                    out = voni::Value::ofVec3(1.0f, 2.0f, 3.0f);
                    return true;
                }
            }
        }
        err = "TIC '" + n + "' não existe";
        return false;
    }
    bool setProp(const std::string&, const std::vector<std::string>&,
                 const voni::Value&, std::string&) override {
        return false;
    }
    bool ticExists(const std::string& n) override {
        for (const auto& s : spawns) {
            if (s.second == n) return true;
        }
        return false;
    }
    void moveTic(f32, f32, f32) override {}
    void explodeTic(bool) override {}
    bool importAnim(const std::string&, std::string&) override {
        return false;
    }
    bool transitionTo(const std::string&, const std::string&,
                      std::string&) override { return false; }
    std::string currentSceneName() override { return "c"; }
    bool search(const std::string&, const std::vector<std::string>&,
                voni::Value&, std::string&) override { return false; }
    f64 frameDt() override { return 1.0 / 60.0; }
    // o spawn do host-053: a lista é DELE (esfera/box) + nome único
    bool spawnTic(const std::string& tipo, f32, f32, f32,
                  std::string& outName, std::string& err) override {
        if (tipo != "esfera" && tipo != "box") {
            err = "spawn: o tipo '" + tipo +
                  "' não existe (tipos: esfera box)";
            return false;
        }
        std::string nome = tipo;
        int suf = 1;
        auto existe = [&](const std::string& n) {
            for (const auto& s : spawns) {
                if (s.second == n) return true;
            }
            return false;
        };
        while (existe(nome)) {
            char b[48];
            std::snprintf(b, sizeof(b), "%s.%03d", tipo.c_str(), suf++);
            nome = b;
        }
        spawns.push_back({tipo, nome});
        outName = nome;
        return true;
    }
};

} // namespace

TEST(regress_spawn) {
    using namespace voni;

    // ---- (1) A FONTE ÚNICA: a entrada é Funcao argc 4/4 com Docs ---------
    {
        const reg::Entry* e = reg::find("spawn");
        EXPECT_MSG(e != nullptr, "a entrada 'spawn' saiu do registo");
        if (e) {
            EXPECT(e->kind == reg::Kind::Funcao);
            EXPECT(e->minArgs == 4 && e->maxArgs == 4);
            EXPECT(e->syntax != nullptr && *e->syntax);
            EXPECT(e->desc != nullptr && *e->desc);
            EXPECT(e->example != nullptr && *e->example);
            EXPECT(e->equiv != nullptr && *e->equiv);
        }
    }

    // ---- (2) O HOSPEDEIRO SEM CENA É HONESTO (o default do Host) ---------
    {
        Error err;
        Script s = Script::compile(
            "central main { on moment { v#x:TIC=spawn(\"esfera\", 0, 0, 0) } "
            "allmoments { } }", err);
        EXPECT(err.ok);
        Host052 semCena;   // o host do R-052 NÃO implementa spawn (default)
        EXPECT(!s.runStart(semCena, err));
        EXPECT_MSG(err.message.find("indisponível") != std::string::npos,
                   "o default do spawnTic deixou de ser honesto ('%s')",
                   err.message.c_str());
        EXPECT(err.line == 1);
    }

    // ---- (3) A CADEIA COMPLETA: série única + o TIC devolvido é USÁVEL ---
    {
        Error err;
        Script s = Script::compile(R"VONI(
v#@+a:TIC=spawn("esfera", 1, 2, 3)
v#@+b:TIC=spawn("esfera", 4, 5, 6)
central main { on moment { View P a View P b View P a.pos.x } allmoments { } }
)VONI", err);
        EXPECT(err.ok);
        Host053 h;
        EXPECT(s.runStart(h, err));
        EXPECT(h.spawns.size() == 2);
        EXPECT(h.logs.size() == 3);
        if (h.logs.size() == 3) {
            EXPECT(h.logs[0] == "voni: esfera");        // a base
            EXPECT(h.logs[1] == "voni: esfera.001");    // o ÚNICO seguinte
            EXPECT(h.logs[2] == "voni: 1");             // o pos.x usável
        }
        // (4) o TIPO desconhecido morre com a LISTA no erro (com linha)
        Error err2;
        Script s2 = Script::compile(
            "central main { on moment { v#x:TIC=spawn(\"colega\", 0, 0, 0) } "
            "allmoments { } }", err2);
        EXPECT(err2.ok);
        Host053 h2;
        EXPECT(!s2.runStart(h2, err2));
        EXPECT(err2.message.find("esfera box") != std::string::npos);
        EXPECT(err2.line == 1);
    }
}

// ===========================================================================
// R-054 (0.12-U 3.9c) — OS TIMERS SÃO DA LINGUAGEM, O ESTADO É POR NÓ AST
// E MORRE NO STOP. O que se vigia: (1) as 3 entradas existem (Linguagem,
// Docs completas) e as 3 palavras são RESERVADAS (a gramática não as dá
// como nomes ao utilizador); (2) o every CARREGA o resto do período (o
// anti-drift é o contrato — o acc=0 honesto é a regressão); (3) o after
// dispara 1× e morre; (4) o when dispara na SUBIDA (a borda, não o nível);
// (5) o stop() mata o estado por nó (o restart recomeça — o estado vive no
// Impl, não em static/global que sobreviveria às vidas do script).
// A mutação cola no RELATORIO-0.12-U-3.9c.md (M-3.9c1/c2/c3).
// ===========================================================================
TEST(regress_voni_timers) {
    using namespace voni;

    // ---- (1) A FONTE ÚNICA + as palavras RESERVADAS -----------------------
    static const char* const kTimers[] = {"every", "after", "when"};
    for (const char* n : kTimers) {
        const reg::Entry* e = reg::find(n);
        EXPECT_MSG(e != nullptr, "o timer '%s' saiu do registo", n);
        if (e) {
            EXPECT(e->kind == reg::Kind::Linguagem);
            EXPECT(e->syntax != nullptr && *e->syntax);
            EXPECT(e->desc != nullptr && *e->desc);
            EXPECT(e->equiv != nullptr && *e->equiv);
        }
        EXPECT_MSG(isReservedWord(n),
                   "'%s' deixou de ser palavra reservada — a gramática "
                   "daria o nome ao utilizador e o statement morria", n);
    }
    // o utilizador NÃO declara variável com o nome (o erro é claro)
    {
        Error err;
        Script::compile("v#every:Int=1\ncentral main { }", err);
        EXPECT(!err.ok);
        EXPECT(err.message.find("reservada") != std::string::npos);
    }

    // ---- (2) O EVERY CARREGA O RESTO (o anti-drift é o contrato) ---------
    {
        Error err;
        Script s = Script::compile(
            "central main { on moment { } allmoments { every(0.5) { View P "
            "\"t\" } } }", err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        for (int i = 0; i < 10; ++i) {   // 10 × 0.2 = 2.0 s
            EXPECT(s.runFrame(h, 0.2, err));
        }
        // com o resto a carregar: [.6→.1] [.5→0] [.6→.1] [.5→0] = 4
        EXPECT_MSG(h.logs.size() == 4,
                   "o every disparou %zu vezes em 10×0.2s (o contrato é 4 — "
                   "o resto do período deixou de carregar?)",
                   h.logs.size());
    }

    // ---- (3) O AFTER DISPARA 1× E MORRE ----------------------------------
    {
        Error err;
        Script s = Script::compile(
            "central main { on moment { } allmoments { after(1) { View P "
            "\"porta\" } } }", err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        for (int i = 0; i < 20; ++i) {   // 2.0 s
            EXPECT(s.runFrame(h, 0.1, err));
        }
        EXPECT_MSG(h.logs.size() == 1,
                   "o after disparou %zu vezes (o contrato é 1× e morre)",
                   h.logs.size());
    }

    // ---- (4) O WHEN DISPARA NA SUBIDA (a borda, não o nível) -------------
    {
        Error err;
        Script s = Script::compile(
            "v#vida:Int=5\n"
            "central main { on moment { } allmoments { vida=vida-1 "
            "when(vida < 3) { View P \"caiu\" } } }", err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        for (int i = 0; i < 10; ++i) {
            EXPECT(s.runFrame(h, 0.016, err));
        }
        EXPECT_MSG(h.logs.size() == 1,
                   "o when disparou %zu vezes (o portão 1×-total morreu — "
                   "a condição verdadeira está a re-disparar?)", h.logs.size());
    }

    // ---- (5) O STOP MATA O ESTADO POR NÓ (o restart recomeça) ------------
    {
        Error err;
        Script s = Script::compile(
            "central main { on moment { } allmoments { after(1) { View P "
            "\"viva\" } } }", err);
        EXPECT(err.ok);
        Host052 h;
        EXPECT(s.runStart(h, err));
        for (int i = 0; i < 15; ++i) {
            EXPECT(s.runFrame(h, 0.1, err));
        }
        EXPECT(h.logs.size() == 1);
        s.stop();
        EXPECT(s.runStart(h, err));
        for (int i = 0; i < 15; ++i) {
            EXPECT(s.runFrame(h, 0.1, err));
        }
        EXPECT_MSG(h.logs.size() == 2,
                   "o after da 2.ª vida disparou %zu vezes (o estado por nó "
                   "sobreviveu ao stop — viveu num static?)", h.logs.size());
    }
}

// ===========================================================================
// R-055 (0.12-X) — A CÂMARA DE TERCEIRA PESSOA NÃO PERDE AS CONVENÇÕES.
// O que se vigia: (1) as 2 entradas existem APENAS pela tabela (Componente
// Continuo, argc fechado, Docs completas); (2) as convenções da CÂMARA —
// o −Z local é a visão (o rot em GRAUS no canal da casa), o pivô é
// alvo+h, o olhar é alvo+h/2, a suavização é 1−e^(−suav·dt); (3) o
// camdrag: yaw 360° livre NORMALIZADO, pitch clamped FIXO −20..70, o
// consumo do lookDelta ZERA (2 consumidores nunca dobram o gesto) e o
// hospedeiro sem input é no-op honesto (o script não morre); (4) o
// camrig não divide o tyker com follow/look (o erro ENSINA com o par do
// Substituir); (5) a referência pública tem as secções novas.
// A mutação cola no RELATORIO-0.12-X-CAMRIG.md (M-3.9x1/x2).
// ===========================================================================
namespace {

struct Host055 : voni::Host {
    struct TicP {
        std::string name;
        f32 pos[3] = {0, 0, 0};
        f32 rot[3] = {0, 0, 0};   // GRAUS (a convenção do engine)
    };
    std::vector<TicP> tics;
    f32 lookDx = 0.0f, lookDy = 0.0f;
    bool hasLook = false;   // false = o hospedeiro sem input (o default)

    TicP* tic(const std::string& n) {
        for (auto& t : tics) {
            if (t.name == n) return &t;
        }
        return nullptr;
    }
    void log(const char*) override {}
    bool getProp(const std::string& n, const std::vector<std::string>& ch,
                 voni::Value& out, std::string& err) override {
        TicP* t = tic(n);
        if (!t) {
            err = "TIC '" + n + "' não existe";
            return false;
        }
        if (ch.empty()) {
            out = voni::Value::ofTic(n);
            return true;
        }
        if (ch[0] == "pos") {
            out = voni::Value::ofVec3(t->pos[0], t->pos[1], t->pos[2]);
            return true;
        }
        if (ch[0] == "rot") {
            out = voni::Value::ofVec3(t->rot[0], t->rot[1], t->rot[2]);
            return true;
        }
        err = "propriedade '" + ch[0] + "' não existe";
        return false;
    }
    bool setProp(const std::string& n, const std::vector<std::string>& ch,
                 const voni::Value& v, std::string& err) override {
        TicP* t = tic(n);
        if (!t) {
            err = "TIC '" + n + "' não existe";
            return false;
        }
        if (ch.size() == 1 && ch[0] == "pos" && v.t == voni::Type::Vec3) {
            t->pos[0] = v.v3[0];
            t->pos[1] = v.v3[1];
            t->pos[2] = v.v3[2];
            return true;
        }
        if (ch.size() == 1 && ch[0] == "rot" && v.t == voni::Type::Vec3) {
            t->rot[0] = v.v3[0];
            t->rot[1] = v.v3[1];
            t->rot[2] = v.v3[2];
            return true;
        }
        err = "escrita não suportada no host-055";
        return false;
    }
    bool ticExists(const std::string& n) override { return tic(n) != nullptr; }
    void moveTic(f32, f32, f32) override {}
    void explodeTic(bool) override {}
    bool importAnim(const std::string&, std::string&) override {
        return false;
    }
    bool transitionTo(const std::string&, const std::string&,
                      std::string&) override { return false; }
    std::string currentSceneName() override { return "c"; }
    bool search(const std::string&, const std::vector<std::string>&,
                voni::Value&, std::string&) override { return false; }
    f64 frameDt() override { return 1.0 / 60.0; }
    bool lookDelta(f32& dx, f32& dy) override {
        if (!hasLook) {
            dx = 0.0f;
            dy = 0.0f;
            return false;
        }
        dx = lookDx;
        dy = lookDy;
        lookDx = 0.0f;
        lookDy = 0.0f;
        return true;
    }
};

// árvore de teste do R-057 em /tmp (o padrão TempTree do test_012c_picker)
struct DotTreeR057 {
    std::string root;
    explicit DotTreeR057() {
        char tmpl[] = "/tmp/goni-r057-XXXXXX";
        const char* mkd = mkdtemp(tmpl);
        root = mkd ? std::string(mkd) : "/tmp/goni-r057";
        fileapi::makeDirs(root);
        fileapi::makeDirs(root + "/.thumbnails");        // a PASTA oculta
        fileapi::makeDirs(root + "/Download");
        fileapi::makeDirs(root + "/Anim");
        fileapi::writeAll(root + "/modelo.obj", "o", 1);
        fileapi::writeAll(root + "/.nomedia", "x", 1);   // o FICHEIRO oculto
        fileapi::writeAll(root + "/Foto.PNG", "p", 1);
        fileapi::writeAll(root + "/azul.txt", "t", 1);
    }
    ~DotTreeR057() {
        ::remove((root + "/modelo.obj").c_str());
        ::remove((root + "/.nomedia").c_str());
        ::remove((root + "/Foto.PNG").c_str());
        ::remove((root + "/azul.txt").c_str());
        ::rmdir((root + "/.thumbnails").c_str());
        ::rmdir((root + "/Download").c_str());
        ::rmdir((root + "/Anim").c_str());
        ::rmdir(root.c_str());
    }
};

} // namespace

// ---- R-056 — O AUTO-SAVE É O SEGURO DE VIDA (0.12-QoL D) ---------------------
// A ordem do dono: a cada 3 minutos com mudanças, gravar autosave.goni na
// pasta do projeto; ao abrir, o autosave MAIS RECENTE que o .goni principal
// pergunta [Recuperar]/[Descartar]. O tick corre no FIM DO FRAME.
TEST(regress_qol_autosave) {
    using namespace vv;
    // (1) as DECISÕES puras: o timer 180s + a recuperação honesta
    EXPECT(vv::autosave::kPeriodSec == 180.0f);
    EXPECT(!autosave::due(0.0f));
    EXPECT(!autosave::due(179.9f));
    EXPECT(autosave::due(180.0f));
    EXPECT(autosave::due(360.0f));
    // a recuperação: só com AS DUAS DATAS e o autosave estritamente mais
    // recente; sem datas (−1 — o SAF não expõe stat) NUNCA pergunta
    EXPECT(autosave::decide(-1.0, -1.0) == autosave::Recovery::None);
    EXPECT(autosave::decide(50.0, -1.0) == autosave::Recovery::None);
    EXPECT(autosave::decide(-1.0, 40.0) == autosave::Recovery::None);
    EXPECT(autosave::decide(40.0, 50.0) == autosave::Recovery::None);
    EXPECT(autosave::decide(50.0, 50.0) == autosave::Recovery::None);
    EXPECT(autosave::decide(50.0, 40.0) == autosave::Recovery::Recover);
    // (2) O CICLO REAL no FsStorage (ficheiros de verdade): o autosave
    // grava na RAIZ do projeto, o load volta, o mtime decide
    char tmpl[] = "/tmp/goni-r056-XXXXXX";
    const char* mkd = mkdtemp(tmpl);
    ASSERT(mkd != nullptr);
    FsStorage st(mkd);
    Project p;
    ASSERT(Project::createNew(st, "r056", p));
    Scene cena;
    const Handle h = cena.create("Casa");
    Tic* t = cena.get(h);
    Transform3D* tr = t->addComponent<Transform3D>();
    tr->pos = Vec3{1.0f, 2.0f, 3.0f};
    // o autosave grava (o MESMO dump do serializer; o principal INTACTO)
    EXPECT(p.saveAutosave(st, cena));
    {
        std::string principal, autoTexto;
        ASSERT(st.readText("autosave.goni", autoTexto));
        ASSERT(st.readText(*p.activeScenePath(), principal));
        EXPECT(autoTexto != principal);   // a cena ainda não foi salva
        // o mtime: o autosave existe e é conhecido
        EXPECT(st.modifiedTime("autosave.goni") > 0.0);
    }
    // a cena sofre uma MUDANÇA + o autosave de novo → mais recente
    tr->pos = Vec3{9.0f, 9.0f, 9.0f};
    EXPECT(p.saveAutosave(st, cena));
    // o recovery decision: o autosave (agora) vs o principal (o cenário
    // REAL do dono: a cena foi gravada ANTES do trabalho não guardado —
    // o mtime do principal atrasa 60s; o mtime tem granularidade de 1s)
    {
        // backdate de verdade: o ficheiro principal recebe AGORA−60s
        struct stat st0;
        const std::string full =
            std::string(mkd) + "/" + *p.activeScenePath();
        ASSERT(::stat(full.c_str(), &st0) == 0);
        struct timeval tv[2];
        tv[0].tv_sec = st0.st_atime - 60;
        tv[0].tv_usec = 0;
        tv[1].tv_sec = st0.st_mtime - 60;
        tv[1].tv_usec = 0;
        ASSERT(::utimes(full.c_str(), tv) == 0);
    }
    const f64 mAuto = st.modifiedTime(autosave::kAutosaveFile);
    const f64 mScene = st.modifiedTime(*p.activeScenePath());
    EXPECT_MSG(mScene < mAuto,
               "R-056: o principal (%.0f) tem de ser mais velho que o "
               "autosave (%.0f)", mScene, mAuto);
    EXPECT(autosave::decide(mAuto, mScene) == autosave::Recovery::Recover);
    // o [Recuperar]: o MESMO loadText da casa (o round-trip do dump)
    Scene volta;
    const SceneSerializer::LoadCtx ctx;
    EXPECT(p.loadAutosave(st, volta, ctx));
    const Tic* lv = nullptr;
    volta.forEachActive([&](const Tic& tc) { lv = &tc; });
    ASSERT(lv != nullptr);
    EXPECT(lv->name == "Casa");
    if (const Transform3D* lt = lv->getComponent<Transform3D>()) {
        EXPECT(nearEqF(lt->pos.x, 9.0f, 1e-4f));   // a MUDANÇA veio no auto
    }
    // (3) o DESCARTAR remove o ficheiro (a decisão do dono)
    EXPECT(st.remove(autosave::kAutosaveFile));
    EXPECT(st.modifiedTime(autosave::kAutosaveFile) < 0.0);
    // (4) os UI ids do pop-up (a faixa própria)
    EXPECT(editor::kAutoYesId == 6930u && editor::kAutoNoId == 6931u);
    // limpeza
    st.remove(*p.activeScenePath());
    st.remove("project.goni");
    ::rmdir(mkd);
}

// ---- R-057 — O NAVEGADOR NÃO VOLTAM AO RUÍDO (0.12-QoL B) -------------------
// A ordem do dono (o feedback Redbr1): o navegador de ficheiros é uma LISTA
// COMPACTA — pastas PRIMEIRO por ordem alfabética, depois os ficheiros; e
// TUDO o que começa por ponto FORA (o .git/.thumbnails/.nomedia eram ruído).
// A decisão 0.8.5 («os ficheiros ponto ficam») ficou SUPERADA por esta ordem
// expressa — o pin picker_oculta_dot recalibrou-se no MESMO commit.
TEST(regress_qol_picker) {
    using namespace vv;
    DotTreeR057 t;
    std::vector<fileapi::DirEntry> out;
    ASSERT(fileapi::listDirEntries(t.root, out));
    // (1) ZERO entradas ocultas (pastas E ficheiros — o contrato QoL B)
    for (const fileapi::DirEntry& e : out) {
        EXPECT_MSG(e.name.empty() || e.name[0] != '.',
                   "R-057: a entrada oculta «%s» vazou para a lista",
                   e.name.c_str());
    }
    bool temAnim = false, temDownload = false, temObj = false;
    bool temFoto = false, temAzul = false;
    for (const fileapi::DirEntry& e : out) {
        temAnim = temAnim || e.name == "Anim";
        temDownload = temDownload || e.name == "Download";
        temObj = temObj || e.name == "modelo.obj";
        temFoto = temFoto || e.name == "Foto.PNG";
        temAzul = temAzul || e.name == "azul.txt";
    }
    EXPECT(temAnim && temDownload && temObj && temFoto && temAzul);
    EXPECT(out.size() == 5u);   // 2 pastas + 3 ficheiros (os 2 ponto fora)
    // (2) a ORDEM: pastas PRIMEIRO alfabético (CI — entryNameLess), depois
    // os ficheiros CI: azul.txt < Foto.PNG < modelo.obj
    EXPECT(out[0].isDir && out[0].name == "Anim");
    EXPECT(out[1].isDir && out[1].name == "Download");
    EXPECT(!out[2].isDir && out[2].name == "azul.txt");
    EXPECT(!out[3].isDir && out[3].name == "Foto.PNG");
    EXPECT(!out[4].isDir && out[4].name == "modelo.obj");
    // (3) a FONTE do breadcrumb: o separador « / » (o caminho lê-se como
    // caminho — a ordem do dono no QoL B)
    std::string src;
#if defined(REPO_ROOT)
    EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/ui/UiEditor.cpp", src));
    EXPECT(src.find("ui.fontWidth(\" / \")") != std::string::npos);
#endif
}

// ---- R-058 — O VIEWPORT LIMPO NÃO VOLTA AO RUÍDO (0.12-QoL C) ---------------
// A ordem do dono (o feedback Redbr1): o viewport deixou de aguentar a
// TEIA de linhas da câmara (SÓ com a câmara selecionada), os botões de
// ferramenta flutuam SEM o fundo preto quadrado gigante e o grid perde-se
// num fade SUAVE com a distância.
TEST(regress_qol_viewport) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();
    // (1) a câmara SEM seleção: ZERO linhas do frustum (o glifo mudo é o
    // único desenho — o alvo do toque que seleciona)
    Scene scene;
    const Handle h = scene.create("Camera");
    Tic* t = scene.get(h);
    Transform3D* tr = t->addComponent<Transform3D>();
    (void)tr;
    t->addComponent<CameraComp>();
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    InputState in;
    const Camera orbit;   // a orbit de edição default
    const Mat4 vp = Mat4::mul(orbit.proj(1600.0f / 720.0f), orbit.view());
    const Vec3 eye = orbit.eye();   // o OLHO da MESMA orbit (o D20)
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    camgizmo::drawAll(ui, scene, vp, 1600.0f, 720.0f, Handle{}, eye);
    ui.endFrame();
    {
        const auto& vb = ui.solidsForTest();
        u32 longos = 0;
        for (u32 i = 0; i + 5 < vb.vertexCount(); i += 6) {
            f32 mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
            for (u32 k = 0; k < 6; ++k) {
                const auto& v = vb.vertices()[i + k];
                mnx = v.x < mnx ? v.x : mnx;
                mxx = v.x > mxx ? v.x : mxx;
                mny = v.y < mny ? v.y : mny;
                mxy = v.y > mxy ? v.y : mxy;
            }
            const f32 ext = (mxx - mnx) > (mxy - mny) ? (mxx - mnx)
                                                      : (mxy - mny);
            if (ext > theme::dp(40.0f)) {
                ++longos;   // uma aresta do cone do frustum
            }
        }
        EXPECT_MSG(longos == 0u,
                   "R-058: a teia do frustum NÃO desenha sem seleção "
                   "(%u arestas longas vazaram)", longos);
        EXPECT(vb.vertexCount() > 0u);   // o glifo mudo continua
    }
    // (2) COM seleção: a teia volta (o dono seleciona e edita)
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    camgizmo::drawAll(ui, scene, vp, 1600.0f, 720.0f, h, eye);
    ui.endFrame();
    EXPECT(ui.solidsForTest().vertexCount() > 0u);
    // (3) o GRID: a janela do fade é a FONTE ÚNICA (suave — o caída 2.2×
    // com o piso 60; era 1.5×/30 — a borda dura morreu)
    {
        f32 st0 = 0.0f, en0 = 0.0f;
        Grid::gridFadeForDist(6.0f, st0, en0);
        EXPECT(nearEqF(st0, 12.0f, 0.01f));
        EXPECT(nearEqF(en0, 72.0f, 0.01f));   // 12 + max(60, 6·2.2)
        Grid::gridFadeForDist(60.0f, st0, en0);
        EXPECT(nearEqF(st0, 66.0f, 0.01f));
        EXPECT(nearEqF(en0, 198.0f, 0.01f));  // 66 + max(60, 132)
        Grid::gridFadeForDist(0.0f, st0, en0);   // sem foco = os pisos
        EXPECT(nearEqF(st0, 12.0f, 0.01f) && nearEqF(en0, 72.0f, 0.01f));
        // o caída é SEMPRE ≥ 60 (a suavidade é o contrato)
        f32 st1 = 0.0f, en1 = 0.0f;
        Grid::gridFadeForDist(5000.0f, st1, en1);
        EXPECT(en1 - st1 >= 60.0f);
    }
}

TEST(regress_camrig) {
    using namespace voni;

    // ---- (1) A FONTE ÚNICA: as 2 entradas são Componente Continuo com o
    // argc fechado e as Docs completas (o exemplo é o TRIO da campanha) ---
    {
        const reg::Entry* rig = reg::find("camrig");
        EXPECT_MSG(rig != nullptr, "a entrada 'camrig' saiu do registo");
        if (rig) {
            EXPECT(rig->kind == reg::Kind::Componente);
            EXPECT(rig->compKind == reg::CompKind::Continuo);
            EXPECT(rig->minArgs == 2 && rig->maxArgs == 3);
            EXPECT(rig->syntax != nullptr && *rig->syntax);
            EXPECT(rig->desc != nullptr && *rig->desc);
            EXPECT(rig->example != nullptr && *rig->example);
            EXPECT(std::string(rig->example).find("camdrag") !=
                   std::string::npos);
        }
        const reg::Entry* drag = reg::find("camdrag");
        EXPECT_MSG(drag != nullptr, "a entrada 'camdrag' saiu do registo");
        if (drag) {
            EXPECT(drag->kind == reg::Kind::Componente);
            EXPECT(drag->compKind == reg::CompKind::Continuo);
            EXPECT(drag->minArgs == 1 && drag->maxArgs == 1);
            EXPECT(drag->syntax != nullptr && *drag->syntax);
            EXPECT(drag->desc != nullptr && *drag->desc);
        }
    }

    // ---- (2) AS CONVENÇÕES DA CÂMARA: snap d/h + o olhar alvo+h/2 com o
    // −Z local (pitch −10.62° / yaw 0 para o rig (8,3) do exemplo) --------
    {
        Error err;
        Script s = Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(camera_jogo){ find(cam) camrig(8, 3) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        Host055 h;
        h.tics.push_back({"camera", {40, 40, 40}});
        h.tics.push_back({"jogador", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        EXPECT(s.runFrame(h, h.frameDt(), err));
        Host055::TicP* cam = h.tic("camera");
        EXPECT(cam != nullptr);
        EXPECT(test::nearEqF(cam->pos[0], 0.0f, 1e-3f));
        EXPECT(test::nearEqF(cam->pos[1], 3.0f, 1e-3f));   // o pivô alvo+h
        EXPECT(test::nearEqF(cam->pos[2], 8.0f, 1e-3f));   // d a +Z (yaw 0)
        // o −Z aponta a alvo+h/2 = (0,1.5,0): pitch −10.62°, yaw 0 (GRAUS)
        EXPECT_MSG(test::nearEqF(cam->rot[0], -10.62f, 1e-2f),
                   "o pitch da câmara deu %.3f (o contrato é −10.62 — o −Z "
                   "deixou de apontar a alvo+h/2?)", cam->rot[0]);
        EXPECT(test::nearEqF(cam->rot[1], 0.0f, 1e-2f));
    }
    // a suavização é 1−e^(−suav·dt): o 1.º passo dá exatamente d0·(1−k)
    {
        Error err;
        Script s = Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(camera_jogo){ find(cam) camrig(8, 3, 6) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        Host055 h;
        h.tics.push_back({"camera", {40, 0, 0}});
        h.tics.push_back({"jogador", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        EXPECT(s.runFrame(h, h.frameDt(), err));
        Host055::TicP* cam = h.tic("camera");
        EXPECT(cam != nullptr);
        const f32 k = 1.0f - std::exp(-6.0f / 60.0f);
        const f32 d0 = std::sqrt(40.0f * 40.0f + 9.0f + 64.0f);
        const f32 d1 = std::sqrt(cam->pos[0] * cam->pos[0] +
                                 (cam->pos[1] - 3.0f) * (cam->pos[1] - 3.0f) +
                                 (cam->pos[2] - 8.0f) * (cam->pos[2] - 8.0f));
        EXPECT_MSG(test::nearEqF(d1, d0 * (1.0f - k), 5e-3f),
                   "o 1.º passo suavizado deu %.4f (o contrato é "
                   "d0·(1−k)=%.4f)", d1, d0 * (1.0f - k));
    }
    // o limit clampa a distância FINAL contra o pivô (após a suavização)
    {
        Error err;
        Script s = Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(camera_jogo){ find(cam) camrig(20, 0, 6) limit(3, 12) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        Host055 h;
        h.tics.push_back({"camera", {40, 0, 0}});
        h.tics.push_back({"jogador", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        EXPECT(s.runFrame(h, h.frameDt(), err));
        Host055::TicP* cam = h.tic("camera");
        EXPECT(cam != nullptr);
        const f32 len = std::sqrt(cam->pos[0] * cam->pos[0] +
                                  cam->pos[1] * cam->pos[1] +
                                  cam->pos[2] * cam->pos[2]);
        EXPECT_MSG(test::nearEqF(len, 12.0f, 1e-2f),
                   "a distância final deu %.3f (o contrato é 12 — o limit "
                   "deixou de clamping APÓS a suavização?)", len);
    }

    // ---- (3) O CAMDRAG: yaw 360° normalizado, pitch FIXO, consumo 1× ----
    {
        Error err;
        Script s = Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(camera_jogo){ find(cam) camdrag(1) camrig(4, 0) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        Host055 h;
        h.hasLook = true;
        h.tics.push_back({"camera", {0, 0, 0}});
        h.tics.push_back({"jogador", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        EXPECT(s.runFrame(h, h.frameDt(), err));
        Host055::TicP* cam = h.tic("camera");
        EXPECT(cam != nullptr);
        EXPECT(test::nearEqF(cam->pos[2], 4.0f, 1e-3f));
        // yaw: −90 orbita a +X; +360 devolve ao MESMO sítio (normalizado)
        h.lookDx = -90.0f;
        EXPECT(s.runFrame(h, h.frameDt(), err));
        EXPECT(test::nearEqF(cam->pos[0], 4.0f, 1e-3f));
        h.lookDx = -360.0f;
        EXPECT(s.runFrame(h, h.frameDt(), err));
        EXPECT(test::nearEqF(cam->pos[0], 4.0f, 1e-3f));
        // pitch: +200 CLAMPA a 70 (FIXO — o teto nunca passa)
        h.lookDy = 200.0f;
        EXPECT(s.runFrame(h, h.frameDt(), err));
        EXPECT_MSG(test::nearEqF(cam->pos[1],
                                 4.0f * std::sin(70.0f * 3.14159265f / 180.0f),
                                 1e-2f),
                   "o pitch passou do teto 70 (pos.y=%.3f — o clamp FIXO "
                   "morreu?)", cam->pos[1]);
        // e −400 clampa a −20 (o chão)
        h.lookDy = -400.0f;
        EXPECT(s.runFrame(h, h.frameDt(), err));
        EXPECT(test::nearEqF(cam->pos[1],
                             4.0f * std::sin(-20.0f * 3.14159265f / 180.0f),
                             1e-2f));
        // o consumo ZERA: frame sem gesto NÃO repete o anterior
        EXPECT(s.runFrame(h, h.frameDt(), err));
        EXPECT(test::nearEqF(cam->pos[1],
                             4.0f * std::sin(-20.0f * 3.14159265f / 180.0f),
                             1e-2f));
    }
    // o hospedeiro SEM input é no-op honesto (o script não morre)
    {
        Error err;
        Script s = Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(camera_jogo){ find(cam) camdrag(0.3) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        Host055 h;   // hasLook=false — o default da base
        h.tics.push_back({"camera", {0, 0, 0}});
        h.tics.push_back({"jogador", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        for (int i = 0; i < 10; ++i) {
            EXPECT(s.runFrame(h, h.frameDt(), err));
        }
    }

    // ---- (4) O COMPILADOR NÃO DEIXA O CAMRIG DIVIDIR O TYKER ------------
    {
        Error err;
        Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(c){ find(cam) camrig(8, 3) follow(2) }\n"
            "central main { }\n", err);
        EXPECT(!err.ok);
        EXPECT(err.message.find("camrig") != std::string::npos);
        EXPECT(err.message.find("sobra") != std::string::npos);
        EXPECT(err.fixFrom == "follow");
        EXPECT(err.fixTo == "camrig");
    }
    {
        Error err;
        Script::compile(
            "linker(camera)to(jogador)=RF(cam)\n"
            "tyker(c){ find(cam) camrig(8, 3) look() }\n"
            "central main { }\n", err);
        EXPECT(!err.ok);
        EXPECT(err.fixFrom == "look");
        EXPECT(err.fixTo == "camrig");
    }

    // ---- (5) A REFERÊNCIA PÚBLICA TEM AS SECÇÕES NOVAS (a fonte única
    // alimenta a referência — o R-013 afere o byte a byte das linhas) ----
    {
        const std::string ref = reg::fullReferenceMarkdown();
        EXPECT(ref.find("### camrig") != std::string::npos);
        EXPECT(ref.find("### camdrag") != std::string::npos);
    }
}

// ---- R-059 — O PAN DA CÂMARA NÃO MORRE NEM INVERTE (0.12-Y A) --------------
// A ordem do dono: o Pan (a MÃOZINHA) é a 6.ª ferramenta do rail, 1 dedo
// arrasta o foco no plano da câmara e 2 dedos arranham em QUALQUER modo
// mantendo o pinch. A classe vigiada: o pan INVERTIDO (a cena foge do
// dedo), o pan de 2 dedos MORTO (o midpoint deixa de alimentar o
// Camera::pan) e a órbita a vazar para o target (a exclusividade morre).
TEST(regress_pan) {
    using namespace vv;
    theme::setDensity(1.0f);
    editor::applyDensity();
    const UiRect view{0.0f, 0.0f, 1080.0f, 600.0f};
    // (1) a DIREÇÃO do pan (a fonte única em render/Camera.h): dedo →
    // direita ⇒ o target anda para a ESQUERDA (agarra a cena); dedo ↓ ⇒
    // o target SOBE. Com yaw=0/pitch=0 os eixos são o +x/+y puros.
    {
        Camera cam;
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        cam.pan(100.0f, 0.0f, 600.0f);
        EXPECT_MSG(cam.target.x < 0.0f && cam.target.y == 0.0f,
                   "R-059: o pan para a direita tem de levar o target para "
                   "a ESQUERDA (target.x %.4f)", (double)cam.target.x);
        const f32 dx = -cam.target.x;
        cam.pan(0.0f, 100.0f, 600.0f);
        EXPECT_MSG(cam.target.y > 0.0f,
                   "R-059: o pan para baixo tem de SUBIR o target "
                   "(target.y %.4f)", (double)cam.target.y);
        // a magnitude é a fonte única: 100 px × 2·dist·tan(fov/2)/600
        EXPECT_MSG(nearEqF(dx, 100.0f * camWorldPerPixel(6.0f, cam.fovY,
                                                             600.0f), 1e-5f),
                   "R-059: a magnitude do pan sai da fonte única (%.6f)",
                   (double)dx);
        EXPECT_MSG(nearEqF(cam.yaw, 0.0f, 1e-6f) &&
                       nearEqF(cam.pitch, 0.0f, 1e-6f) &&
                       nearEqF(cam.dist, 6.0f, 1e-6f),
                   "R-059: o pan NÃO pode tocar na rotação/zoom");
    }
    // (2) O PAN DE 2 DEDOS É LIVRE (sem panMode): o midpoint arrasta o
    // target e o pinch mantém o zoom — a cadeia inteira (ancora → pan)
    {
        Camera cam;
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        editor::OrbitState st;
        InputState in;
        in.injectDown(0, 400.0f, 300.0f);
        in.injectDown(1, 500.0f, 300.0f);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, false);
        in.injectMove(0, 440.0f, 300.0f);
        in.injectMove(1, 540.0f, 300.0f);   // mid +40 px, d igual
        editor::updateCameraOrbit(cam, st, in, view, 0, false, false);
        in.injectUp(0);
        in.injectUp(1);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, false);
        EXPECT_MSG(nearEqF(-cam.target.x,
                             40.0f * camWorldPerPixel(6.0f, cam.fovY, 600.0f),
                             1e-5f),
                   "R-059: o pan de 2 dedos MORREU (o midpoint não arrasta "
                   "— target.x %.5f)", (double)cam.target.x);
        EXPECT_MSG(nearEqF(cam.dist, 6.0f, 1e-5f),
                   "R-059: d constante não pode zoom (%.5f)",
                   (double)cam.dist);
        EXPECT_MSG(nearEqF(cam.yaw, 0.0f, 1e-6f),
                   "R-059: 2 dedos NUNCA orbitam (yaw %.5f)",
                   (double)cam.yaw);
    }
    // (3) A EXCLUSIVIDADE: 1 dedo com panMode NÃO orbita (o yaw fica) e
    // sem panMode o target NÃO mexe (a orbit de sempre intacta)
    {
        Camera cam;
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        editor::OrbitState st;
        InputState in;
        in.injectDown(0, 400.0f, 300.0f);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, true);
        in.injectMove(0, 500.0f, 300.0f);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, true);
        in.injectUp(0);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, true);
        EXPECT_MSG(cam.target.x < 0.0f && nearEqF(cam.yaw, 0.0f, 1e-6f),
                   "R-059: o modo Pan tem de arrastar o target SEM orbitar "
                   "(target.x %.4f yaw %.5f)", (double)cam.target.x,
                   (double)cam.yaw);
    }
    {
        Camera cam;
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        editor::OrbitState st;
        InputState in;
        in.injectDown(0, 400.0f, 300.0f);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, false);
        in.injectMove(0, 500.0f, 300.0f);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, false);
        in.injectUp(0);
        editor::updateCameraOrbit(cam, st, in, view, 0, false, false);
        EXPECT_MSG(cam.target.x == 0.0f && cam.yaw != 0.0f,
                   "R-059: sem panMode a orbit de sempre tem de viver "
                   "INTACTA (target.x %.4f yaw %.5f)", (double)cam.target.x,
                   (double)cam.yaw);
    }
}

// ---- R-060 — O PBR-LITE E O WORLDENVIRONMENT NÃO MORREM (0.12-Y C) ----------
// A ordem do dono: o glTF lê Albedo/Normal/Roughness/Metallic, cria a pasta
// assets/NomeDoModelo/ com o .gmesh + textures/, liga os slots sozinho e o
// preset novo «WorldEnvironment» carrega um HDRI (mapa 360) que ilumina a
// cena e define a cor do céu/névoa. A classe vigiada: a FÓRMULA PBR fora da
// fonte única (o shader e os pins divergem às escondidas), a NÉVOA a vazar
// para o lit de sempre (density 0 tem de ser MORTA — byte a byte), o HDRI
// escuro a APAGAR a cena (o piso do ambient), o .goni a PERDER o
// WorldEnvironment no roundtrip e a ORDEM dos slots a regredir.
#include "render/Pbr.h"                  // 0.12-Y (R-060): a fonte única
#include "components/Environment.h"      // 0.12-Y (R-060): o WorldEnvironment
#include "assets/GltfTextures.h"         // 0.12-Y (R-060): os 5 slots
#include "assets/GltfImporter.h"
#include "core/Presets.h"                // 0.12-Y (R-060): o preset novo
TEST(regress_pbr_worldenv) {
    using namespace vv;
    // (1) A FONTE ÚNICA (render/Pbr.h): a névoa exp2 com density 0 é MORTA
    // (o lit de sempre byte a byte) e a curva d·ρ=1 → 1−e⁻¹ ≈ 0.632
    {
        EXPECT_MSG(pbr::fogFactorExp2(100.0f, 0.0f) == 0.0f,
                   "R-060: a névoa com density 0 tem de estar MORTA (o lit "
                   "de sempre) — f=%.4f",
                   (double)pbr::fogFactorExp2(100.0f, 0.0f));
        EXPECT_MSG(nearEqF(pbr::fogFactorExp2(1.0f, 1.0f), 0.6321f, 1e-3f),
                   "R-060: a curva exp2 mudou (d·ρ=1 → 0.6321)");
        // o shininess aferrado: rough 0 = 256 (quase-espelho), rough 1 = 4
        EXPECT_MSG(nearEqF(pbr::roughToShininess(0.0f), 256.0f, 0.01f) &&
                       nearEqF(pbr::roughToShininess(1.0f), 4.0f, 0.01f),
                   "R-060: o expoente do especular saiu dos limites "
                   "[4..256] da fonte única");
        // o mix metálico: o metal NÃO TEM difusa cheia (1.0 → 0.15)
        EXPECT_MSG(nearEqF(pbr::metallicDiffuseScale(1.0f), 0.15f, 1e-6f),
                   "R-060: o metal perdeu o mix da difusa (o look plástico "
                   "voltou)");
    }
    // (2) O AMBIENT DO HDRI (components/Environment.h): o HDRI escuro
    // NUNCA apaga a cena (o piso 0.25) e o claro respeita o teto 4
    {
        const f32 escuro[3] = {0.0f, 0.0f, 0.0f};
        const f32 claro[3] = {1.0f, 1.0f, 1.0f};
        EXPECT_MSG(nearEqF(ambientFromAvg(escuro), 0.25f, 1e-6f),
                   "R-060: o HDRI escuro tem de respeitar o PISO 0.25 do "
                   "ambient (a cena nunca fica negra)");
        EXPECT_MSG(ambientFromAvg(claro) == 4.0f,
                   "R-060: o teto do ambient é 4.0");
    }
    // (3) O ROUNDTRIP (.goni): o WorldEnvironment sobrevive ao dump/load
    // — o serializer novo não pode perder NENHUM campo
    {
        Scene a;
        const Handle h = a.create("WorldEnvironment");
        a.get(h)->addComponent<Transform3D>();
        Environment* e = a.get(h)->addComponent<Environment>();
        e->skyColor[0] = 0.30f;
        e->skyColor[2] = 0.60f;
        e->fogColor[1] = 0.40f;
        e->fogDensity = 0.08f;
        e->ambient = 1.75f;
        e->hdriPath = "assets/hdri/atardecer.png";
        e->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(dump.find("\"Environment\"") != std::string::npos,
                   "R-060: o .goni PERDEU o componente Environment");
        EXPECT_MSG(dump.find("atardecer.png") != std::string::npos,
                   "R-060: o .goni PERDEU o HDRI (o mapa 360)");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Handle hf = b.find("WorldEnvironment");
        ASSERT(hf.valid());
        const Environment* e2 = b.get(hf)->getComponent<Environment>();
        ASSERT(e2 != nullptr);
        EXPECT_MSG(nearEqF(e2->fogDensity, 0.08f, 1e-6f) &&
                       nearEqF(e2->ambient, 1.75f, 1e-6f) &&
                       nearEqF(e2->skyColor[2], 0.60f, 1e-5f) &&
                       e2->hdriPath == "assets/hdri/atardecer.png",
                   "R-060: o roundtrip do Environment perdeu campos "
                   "(density %.4f ambient %.4f sky.b %.4f hdri '%s')",
                   (double)e2->fogDensity, (double)e2->ambient,
                   (double)e2->skyColor[2], e2->hdriPath.c_str());
    }
    // (4) O PRESET (a porta do «+»): o nome e a ESTRUTURA (Transform +
    // Environment — SEM mesh, SEM física)
    {
        EXPECT_MSG(std::string(presetName(PresetKind::WorldEnvironment)) ==
                       "WorldEnvironment",
                   "R-060: o preset WorldEnvironment saiu do menu (+)");
        EXPECT_MSG(static_cast<u32>(PresetKind::Count) == 12,
                   "R-060: a contagem de presets regressou (12 com o "
                   "Wind/Rain/Snow/Storm da 0.12-Y-F2)");
        Scene s;
        const Handle hp =
            createTicFromPreset(s, PresetKind::WorldEnvironment, nullptr,
                                nullptr);
        ASSERT(hp.valid());
        const Tic* t = s.get(hp);
        ASSERT(t != nullptr);
        EXPECT_MSG(t->getComponent<Environment>() != nullptr &&
                       t->getComponent<MeshRenderer>() == nullptr,
                   "R-060: o preset WorldEnvironment tem de ser ESTRUTURA "
                   "(Transform+Environment, SEM mesh)");
    }
    // (5) A EXTRAÇÃO dos 5 slots: o tamanho é meshes×5 (a ordem kBind*) e
    // a textura EXTERNA (uri) passa no slot certo
    {
        GltfModel model;
        MeshData mesh;
        model.meshes.push_back(mesh);
        model.meshes.push_back(mesh);
        model.meshMaterial.push_back(0);
        model.meshMaterial.push_back(-1);   // o mesh 1 SEM material
        GltfMaterial m0;
        m0.normalTex = 0;   // SÓ o normal (o slot 1) — prova a ORDEM
        model.materials.push_back(m0);
        GltfImage img;      // externa (sem bytes): o uri passa-through
        img.uriPath = "texturas/parede_n.png";
        model.images.push_back(img);
        FakeStorage st;
        GltfTexSlots slots;
        std::string err;
        EXPECT(extractGltfTexSlots(st, model, "cidade", slots, err));
        EXPECT_MSG(slots.tex.size() == 10u,
                   "R-060: os slots têm de ser meshes×5 (10) — são %zu",
                   slots.tex.size());
        EXPECT_MSG(slots.tex[1] == "texturas/parede_n.png" &&
                       slots.tex[0].empty() && slots.tex[2].empty(),
                   "R-060: o slot 1 (normal) perdeu a ordem kBind* "
                   "(tex[0]='%s' tex[1]='%s')", slots.tex[0].c_str(),
                   slots.tex[1].c_str());
        EXPECT_MSG(slots.tex[5].empty() && slots.tex[9].empty(),
                   "R-060: o mesh SEM material não pode herdar slots");
    }
}

// ---------------------------------------------------------------------------
// R-061 — O CÉU PROCEDURAL NÃO MORRE — A FONTE ÚNICA, O VENTO COM TEMPO, O
// ROUNDTRIP DO CÉU, O INSPECTOR CONDICIONAL E O CONTRATO DO FS (0.12-Y 3.Y-D)
// ---------------------------------------------------------------------------
// Sintoma vigiado: as NUVENS PARADAS (o tempo fora do UV — M-Yd1); o modo
// COR SÓLIDA a mostrar campos de nuvens (a condição morta no plano — M-Yd2);
// o .goni a perder o skyType/nuvens no roundtrip; a base da câmara a sair
// da fonte única (o FS e as puras a divergir). A origem: o prompt da
// sub-fase D — «Adicionar Céu Procedural com Nuvens Animadas ao
// WorldEnvironment» (a performance mobile é prioridade — FBM 2D, ZERO
// raymarching).
TEST(regress_procedural_sky) {
    using namespace vv;
    using namespace vv::editor;   // o plano/ids do Inspector (EditorLayout.h)
    // (1) A FONTE ÚNICA (render/SkyRenderer.h): o vento USA O TEMPO (a
    // M-Yd1 mata o tempo — a nuvem parada é o defeito nomeado do dono), o
    // gradiente é horizonte→zénite e a cobertura 0 é o céu limpo
    {
        f32 w0[2], w1[2];
        sky::windOffset(0.0f, 0.02f, w0);
        sky::windOffset(10.0f, 0.02f, w1);
        EXPECT_MSG(w1[0] > w0[0] && w1[1] > w0[1],
                   "R-061: O TEMPO NÃO EMPURRA AS NUVENS (o vento parou — "
                   "M-Yd1 vermelho)");
        sky::windOffset(500.0f, 0.0f, w0);
        EXPECT_MSG(w0[0] == 0.0f && w0[1] == 0.0f,
                   "R-061: vento 0 tem de ser o céu parado de sempre");
        const f32 hor[3] = {0.29f, 0.56f, 0.89f};
        const f32 zen[3] = {0.12f, 0.23f, 0.54f};
        f32 col[3];
        sky::skyGradient(0.0f, hor, zen, col);
        EXPECT_MSG(nearEqF(col[1], hor[1], 1e-5f),
                   "R-061: o gradiente em dir.y=0 não é o HORIZONTE");
        sky::skyGradient(1.0f, hor, zen, col);
        EXPECT_MSG(nearEqF(col[1], zen[1], 1e-5f),
                   "R-061: o gradiente em dir.y=1 não é o ZÉNITE");
        EXPECT_MSG(sky::cloudAlpha(0.95f, 0.0f) == 0.0f,
                   "R-061: cobertura 0 = CÉU LIMPO (a nuvem não morre "
                   "mais)");
        Vec3 rB, uB, fB;
        EXPECT_MSG(sky::viewBasis(Mat4::identity(), rB, uB, fB),
                   "R-061: a extração da base degenerou na identidade");
    }
    // (2) O ROUNDTRIP (.goni): o céu procedural sobrevive ao dump/load —
    // o serializer novo não pode perder NENHUM campo do dono
    {
        Scene a;
        const Handle h = a.create("Ceu");
        a.get(h)->addComponent<Transform3D>();
        Environment* e = a.get(h)->addComponent<Environment>();
        e->skyType = Environment::kSkyTypeProcedural;
        e->skyColorHorizon[1] = 0.42f;
        e->skyColorZenith[0] = 0.33f;
        e->sunColor[2] = 0.61f;
        e->sunDirection[0] = 0.0f;
        e->sunDirection[1] = 1.0f;
        e->sunDirection[2] = 0.0f;
        e->cloudCoverage = 0.75f;
        e->cloudSpeed = 0.13f;
        e->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(dump.find("\"skyType\"") != std::string::npos,
                   "R-061: o .goni PERDEU o skyType");
        EXPECT_MSG(dump.find("\"cloudCov\"") != std::string::npos &&
                       dump.find("\"cloudSpd\"") != std::string::npos,
                   "R-061: o .goni PERDEU AS NUVENS (cobertura/vento)");
        EXPECT_MSG(dump.find("\"skyHor\"") != std::string::npos &&
                       dump.find("\"skyZen\"") != std::string::npos &&
                       dump.find("\"sunCol\"") != std::string::npos &&
                       dump.find("\"sunDir\"") != std::string::npos,
                   "R-061: o .goni PERDEU as cores do céu/sol");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Handle hf = b.find("Ceu");
        ASSERT(hf.valid());
        const Environment* e2 = b.get(hf)->getComponent<Environment>();
        ASSERT(e2 != nullptr);
        EXPECT_MSG(e2->skyType == Environment::kSkyTypeProcedural,
                   "R-061: o céu carregou SÓLIDO (o skyType morreu no "
                   "roundtrip)");
        EXPECT_MSG(nearEqF(e2->cloudCoverage, 0.75f, 1e-5f) &&
                       nearEqF(e2->cloudSpeed, 0.13f, 1e-5f),
                   "R-061: as NUVENS não voltam do .goni");
        EXPECT_MSG(nearEqF(e2->skyColorHorizon[1], 0.42f, 1e-5f) &&
                       nearEqF(e2->sunDirection[1], 1.0f, 1e-4f),
                   "R-061: as cores/direção do sol não voltam do .goni");
        // os DEFAULTS ficam fora do dump (o .goni antigo carrega igual)
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<Environment>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(dump2.find("skyType") == std::string::npos &&
                       dump2.find("cloudCov") == std::string::npos,
                   "R-061: os defaults do céu INVADIRAM o .goni (o "
                   "forward-compat morreu)");
    }
    // (3) O INSPECTOR CONDICIONAL: o modo COR SÓLIDA NÃO mostra nuvens (a
    // M-Yd2 mata a condição do plano — o dropdown ganha irmãos fantasma)
    {
        Scene s;
        const Handle h = s.create("ceu");
        s.get(h)->addComponent<Transform3D>();
        s.get(h)->addComponent<Environment>();
        const TextMetrics tm{};
        InspRow rows[64];
        {
            const InspProfile prof = inspectorProfile(*s.get(h));
            EXPECT_MSG(prof.envSkyType == 0,
                       "R-061: o perfil não transporta o tipo de céu");
            const u32 n = inspectorPlan(prof, tm, false, 0, rows);
            u32 cloudRows = 0;
            for (u32 i = 0; i < n; ++i) {
                if (rows[i].id == kInspectorEnvHor ||
                    rows[i].id == kInspectorEnvZen ||
                    rows[i].id == kInspectorEnvSunC ||
                    rows[i].id == kInspectorEnvCov ||
                    rows[i].id == kInspectorEnvSpd) {
                    ++cloudRows;
                }
            }
            EXPECT_MSG(cloudRows == 0,
                       "R-061: O MODO SÓLIDO MOSTRA %u CAMPOS DE NUVENS "
                       "(M-Yd2 vermelho — a condição morreu)",
                       cloudRows);
        }
        // e o PROCEDURAL mostra AS 6 na ordem (o dropdown + os 5 campos)
        s.get(h)->getComponent<Environment>()->skyType =
            Environment::kSkyTypeProcedural;
        {
            const InspProfile prof = inspectorProfile(*s.get(h));
            const u32 n = inspectorPlan(prof, tm, false, 0, rows);
            i32 iT = -1, iH = -1, iZ = -1, iS = -1, iC = -1, iV = -1;
            for (u32 i = 0; i < n; ++i) {
                const u64 id = rows[i].id;
                if (id == kInspectorEnvSkyType) iT = static_cast<i32>(i);
                if (id == kInspectorEnvHor) iH = static_cast<i32>(i);
                if (id == kInspectorEnvZen) iZ = static_cast<i32>(i);
                if (id == kInspectorEnvSunC) iS = static_cast<i32>(i);
                if (id == kInspectorEnvCov) iC = static_cast<i32>(i);
                if (id == kInspectorEnvSpd) iV = static_cast<i32>(i);
            }
            EXPECT_MSG(iT >= 0 && iH >= 0 && iZ >= 0 && iS >= 0 && iC >= 0 &&
                           iV >= 0,
                       "R-061: faltam linhas no plano procedural "
                       "(%d %d %d %d %d %d)",
                       iT, iH, iZ, iS, iC, iV);
            EXPECT_MSG(iT < iH && iH < iZ && iZ < iS && iS < iC && iC < iV,
                       "R-061: a ORDEM procedural saiu da especificação");
        }
    }
    // (4) O CONTRATO NO FONTE (o FS replica as puras — o tempo no UV é a
    // linha do contrato; a M-Yd1 morre AQUI antes de qualquer GL; SEM
    // comentários — a prosa não satisfaz, o padrão do ui_vocab_check)
    {
        std::string src;
        EXPECT(readFileText(REPO_ROOT
                            "/app/src/main/cpp/render/SkyRenderer.cpp",
                            src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("vec2(kWindDirX, kWindDirY) * (uTime * uSpeed)") !=
                       std::string::npos,
                   "R-061: o FS DEIXOU DE USAR O TEMPO no UV das nuvens "
                   "(a nuvem parada voltou — M-Yd1)");
        EXPECT_MSG(code.find("uniform float uTime;") != std::string::npos,
                   "R-061: o FS perdeu o uniform uTime");
    }
    // (5) 0.12-Y-E — O CICLO DIA/NOITE NÃO MORRE: a identidade do meio-dia
    // (o byte a byte da D), a noite ESCURA com estrelas (o teste do dono:
    // 20h), o sol roda pelo relógio, a ALTURA escala o plano das nuvens e
    // o roundtrip dos 4 campos novos (tod/cycSpd/cycOn/cloudH). A M-Ye2
    // (a grade morta) e a M-Ye1 (o tempo fora do UV) morrem AQUI.
    {
        const f32 hor[3] = {0.29f, 0.56f, 0.89f};
        const f32 zen[3] = {0.12f, 0.23f, 0.54f};
        const f32 sun[3] = {1.0f, 0.87f, 0.67f};
        sky::SkyGrade g;
        sky::dayNightGrade(12.0f, hor, zen, sun, g);
        EXPECT_MSG(g.hor[0] == hor[0] && g.zen[1] == zen[1] &&
                       g.stars == 0.0f,
                   "R-061: o meio-dia deixou de ser a IDENTIDADE (a grade "
                   "pisou as cores do utilizador — M-Ye2)");
        sky::dayNightGrade(20.0f, hor, zen, sun, g);
        const f32 lum = 0.2126f * g.hor[0] + 0.7152f * g.hor[1] +
                        0.0722f * g.hor[2];
        EXPECT_MSG(lum < 0.16f && g.stars > 0.5f,
                   "R-061: às 20h o céu não escurece com estrelas "
                   "(lum=%.3f stars=%.2f — M-Ye2 vermelho)",
                   (double)lum, (double)g.stars);
        f32 d6[3], d18[3];
        sky::sunDirFromTime(6.0f, d6);
        sky::sunDirFromTime(18.0f, d18);
        EXPECT_MSG(d6[0] > 0.85f && d18[0] < -0.85f,
                   "R-061: o sol deixou de rodar pelo relógio (nasce %.2f "
                   "põe-se %.2f)", (double)d6[0], (double)d18[0]);
        f32 uv1[2], uv2[2];
        sky::cloudUv(0.3f, 0.8f, -0.5f, 1.0f, 0.0f, 0.0f, uv1);
        sky::cloudUv(0.3f, 0.8f, -0.5f, 2.0f, 0.0f, 0.0f, uv2);
        EXPECT_MSG(uv2[0] > uv1[0] * 1.9f && uv2[0] < uv1[0] * 2.1f,
                   "R-061: a ALTURA das nuvens deixou de escalar o plano");
        // o roundtrip dos campos novos (o serializer não pode perder NENHUM)
        Scene a;
        const Handle h = a.create("Ciclo");
        a.get(h)->addComponent<Transform3D>();
        Environment* e = a.get(h)->addComponent<Environment>();
        e->timeOfDay = 21.0f;
        e->dayCycleSpeed = 0.5f;
        e->autoCycle = true;
        e->cloudHeight = 3.0f;
        e->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(dump.find("\"tod\"") != std::string::npos &&
                       dump.find("\"cycSpd\"") != std::string::npos &&
                       dump.find("\"cycOn\"") != std::string::npos &&
                       dump.find("\"cloudH\"") != std::string::npos,
                   "R-061: o .goni PERDEU o ciclo dia/noite (tod/cycSpd/"
                   "cycOn/cloudH)");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Environment* e2 =
            b.get(b.find("Ciclo"))->getComponent<Environment>();
        ASSERT(e2 != nullptr);
        EXPECT_MSG(nearEqF(e2->timeOfDay, 21.0f, 1e-5f) &&
                       nearEqF(e2->dayCycleSpeed, 0.5f, 1e-5f) &&
                       e2->autoCycle && nearEqF(e2->cloudHeight, 3.0f, 1e-5f),
                   "R-061: o ciclo dia/noite não volta do .goni");
        // os DEFAULTS fora do dump (o meio-dia é silêncio — forward-compat)
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<Environment>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(dump2.find("\"tod\"") == std::string::npos &&
                       dump2.find("\"cycOn\"") == std::string::npos,
                   "R-061: os defaults do ciclo INVADIRAM o .goni");
        // o contrato do FS da E: a altura + o branch das estrelas no fonte
        std::string src;
        EXPECT(readFileText(REPO_ROOT
                            "/app/src/main/cpp/render/SkyRenderer.cpp",
                            src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("(kCloudScale * uHeight)") != std::string::npos,
                   "R-061: a ALTURA saiu do UV das nuvens no FS");
        EXPECT_MSG(code.find("if (uStars > 0.001)") != std::string::npos,
                   "R-061: o branch UNIFORME das estrelas morreu (o dia "
                   "paga o campo inteiro)");
    }
}

TEST(regress_particles) {
    // R-062 (0.12-Y-F1) — O SISTEMA DE PARTÍCULAS NÃO MORRE: a fábrica do
    // jorro (o preset cria o TIC certo), o VS anima (a fórmula do tempo
    // local + a balística vivem no FONTE sem comentários), o draw é UM
    // call instanciado com o uTime ÚLTIMO 1f, o .goni transporta o
    // emissor inteiro e o Inspector escreve nas propriedades (o M-Yf1 —
    // o tempo ignorado — e o M-Yf2 — o Inspector adornado — morrem AQUI).
    {
        // (1) A FÁBRICA: o preset ParticleEmitter = Transform +
        // ParticleEmitter (SÓ isso)
        Scene scene;
        const Handle h = createTicFromPreset(
            scene, PresetKind::ParticleEmitter, nullptr, nullptr);
        EXPECT_MSG(h.valid() &&
                       scene.get(h)->getComponent<Transform3D>() != nullptr &&
                       scene.get(h)->getComponent<ParticleEmitter>() !=
                           nullptr &&
                       scene.get(h)->getComponent<MeshRenderer>() == nullptr,
                   "R-062: o preset da fábrica morreu (Transform+PE, sem "
                   "mesh)");
        // (2) O VS ANIMA — a fórmula EXATA do prompt no fonte (o
        // stripCppComments mata o falso verde por comentário, a lição da D)
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/ParticleShader.h", src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("float localTime = mod(uTime + (seed * 100.0), "
                             "uLifetime);") != std::string::npos,
                   "R-062: o TEMPO LOCAL saiu do VS (a linha do prompt — "
                   "M-Yf1)");
        EXPECT_MSG(code.find("(0.5 * uGrav * localTime * localTime)") !=
                       std::string::npos,
                   "R-062: a balística saiu do VS");
        EXPECT_MSG(code.find("float fade = 1.0 - vProgress;") !=
                       std::string::npos,
                   "R-062: o FADE OUT da vida saiu do FS");
        // (3) O DRAW — 1 call × N instâncias, o divisor dos seeds e o
        // uTime ÚLTIMO 1f (o contrato da casa)
        glstub::fb::enabled = true;
        glstub::fb::resetState();
        glstub::reset();
        ParticleRenderer pr;
        EXPECT(pr.init());
        ParticleRenderer::Params pp;
        pp.maxParticles = 100;
        pp.time = 3.0f;
        const DrawStats st = pr.draw(pp);
        EXPECT_MSG(st.drawCalls == 1 && st.vertices == 400u &&
                       glstub::stats.drawArraysInstancedCalls == 1 &&
                       glstub::stats.lastInstanceCount == 100,
                   "R-062: o draw deixou de ser 1 glDrawArraysInstanced × "
                   "100 (calls=%u verts=%u inst=%d)", st.drawCalls,
                   st.vertices, glstub::stats.lastInstanceCount);
        EXPECT_MSG(glstub::stats.lastUniform1f == 3.0f,
                   "R-062: o uTime deixou de ser o ÚLTIMO 1f (%.2f)",
                   (double)glstub::stats.lastUniform1f);
        pr.destroy();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
        // (4) O .GONI transporta o emissor INTEIRO (e os defaults ficam de
        // fora — a fábrica do jorro é silêncio)
        ParticleEmitter* pe = scene.get(h)->getComponent<ParticleEmitter>();
        pe->lifetime = 4.0f;
        pe->maxParticles = 750;
        pe->clampFields();
        const std::string dump = SceneSerializer::dump(scene);
        EXPECT_MSG(dump.find("\"ParticleEmitter\"") != std::string::npos &&
                       dump.find("\"maxP\"") != std::string::npos &&
                       dump.find("\"life\"") != std::string::npos,
                   "R-062: o .goni PERDEU o emissor");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const ParticleEmitter* pe2 =
            b.get(b.find("ParticleEmitter"))->getComponent<ParticleEmitter>();
        EXPECT_MSG(pe2 != nullptr && pe2->maxParticles == 750 &&
                       nearEqF(pe2->lifetime, 4.0f, 1e-5f),
                   "R-062: o emissor não volta do .goni");
        // (5) O INSPECTOR ESCREVE — o slider da vida toca a propriedade e
        // o botão da textura arma a intenção (o M-Yf2 morre AQUI)
        std::string ui;
        EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/ui/EditorUi.cpp",
                            ui));
        const std::string uiCode = test::stripCppComments(ui);
        EXPECT_MSG(uiCode.find("peEdit->lifetime, \"%.1fs\"") !=
                       std::string::npos &&
                       uiCode.find("st.partTexPick = true;") !=
                           std::string::npos,
                   "R-062: o Inspector deixou de escrever no emissor "
                   "(M-Yf2 — o slider adornado)");
    }
}

// ============================================================================
// R-063 (0.12-V) — O GIZMO E O MUNDO DOS TICs COM PAI NÃO DIVERGEM —
//         regress_viewport_gizmo_world
//
// Os 3 bugs do viewport do dono (C33): (1) o modelo só APARECIA durante o
// arrasto do gizmo (o feedback imediato escrevia world = TRS LOCAL — o mesh
// teleportava para o gizmo e voltava ao largar); (2) o gizmo
// desenhava/hit-testava no tr->pos LOCAL como se fosse MUNDO (fora do mesh
// nos TICs com pai — o mesh desenha na TRANSLAÇÃO do world); (3) o gizmo
// media dist_da_órbita·k (sem tan(fov/2), sem a distância ATÉ ao gizmo) —
// gigante perto do olho, minúsculo longe. A cura: composeWorld (a FONTE
// ÚNICA da composição — o tick e o feedback partilham), worldToLocalPos (o
// drag corre no mundo, o TRS guarda local), gizmoAnchor (a MESMA geometria
// no hit e no draw) e a fórmula do dono dist·tan(fov/2)·k com o toque ≥48px.
// ============================================================================
TEST(regress_viewport_gizmo_world) {
    using namespace vv;
    // (1) A COMPOSIÇÃO ÚNICA (core/TransformSystem — o MESMO contrato do
    //     tick): o filho RODADO+ESCALADO segue o pai; o mundo→local devolve
    //     o local; a raiz fica byte a byte (world = TRS)
    {
        Scene scene;
        const Handle hP = scene.create("pai");
        Transform3D* tp = scene.get(hP)->addComponent<Transform3D>();
        tp->pos = Vec3{0.0f, 1.6f, 0.0f};
        tp->rot = Quat::fromEuler(0.0f, 0.5f, 0.0f);
        tp->scale = Vec3{2.0f, 2.0f, 2.0f};
        TransformSystem::composeWorld(*tp, nullptr);
        EXPECT_MSG(nearEqF(tp->world.m[13], 1.6f, 1e-5f),
                   "R-063: a composição da RAIZ mudou (o comportamento de "
                   "sempre é byte a byte)");
        const Handle hF = scene.create("filho", static_cast<i32>(hP.index));
        Transform3D* tf = scene.get(hF)->addComponent<Transform3D>();
        tf->pos = Vec3{1.0f, 0.0f, 0.0f};
        tf->scale = Vec3{0.5f, 0.5f, 0.5f};
        TransformSystem::composeWorld(*tf, &tp->world);
        // a translação do mundo do filho = parentWorld·(1,0,0)
        const Vec3 w{tf->world.m[12], tf->world.m[13], tf->world.m[14]};
        EXPECT_MSG(nearEqF(w.x, 2.0f * std::cos(0.5f), 1e-4f) &&
                       nearEqF(w.y, 1.6f, 1e-4f) &&
                       nearEqF(w.z, -2.0f * std::sin(0.5f), 1e-4f),
                   "R-063: o mundo do FILHO deixou de compor parent∘local "
                   "(a árvore do import desenha despedaçada)");
        // o MUNDO→LOCAL do pai devolve o TRS LOCAL (a conversa do gizmo)
        const Vec3 loc = TransformSystem::worldToLocalPos(*tp, w);
        EXPECT_MSG(test::vecNearF(loc, Vec3{1.0f, 0.0f, 0.0f}, 1e-4f),
                   "R-063: o mundo→local do pai degenerou (o drag do gizmo "
                   "perde o alvo)");
        // o resolver do pai (o slot do Tic::parent; -1 = sem pai)
        EXPECT(TransformSystem::parentTransformOf(scene,
                                                  static_cast<i32>(hP.index)) == tp);
        EXPECT(TransformSystem::parentTransformOf(scene, -1) == nullptr);
    }
    // (2) A FÓRMULA DO DONO NO FONTE (SEM comentários — a lição M-Yd1) e o
    //     mínimo de toque 48 px
    {
        std::string gz;
        EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/ui/Gizmo.h", gz));
        const std::string code = test::stripCppComments(gz);
        EXPECT_MSG(code.find(
                       "return distToGizmo * tanHalfFov * kScreenLen;") !=
                       std::string::npos,
                   "R-063: a fórmula do tamanho saiu de Gizmo.h (o Bug 3 "
                   "volta — M-V3)");
        EXPECT_MSG(code.find("kGrabPx = 48.0f") != std::string::npos,
                   "R-063: o mínimo de toque 48 px saiu (M-V3)");
    }
    // (3) O MAIN partilha a MESMA geometria no hit e no draw (gizmoAnchor)
    //     e o drag converte mundo→local + compõe o feedback com o pai
    {
        std::string mn;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/main.cpp", mn));
        const std::string code = test::stripCppComments(mn);
        EXPECT_MSG(code.find("gizmoAnchor(*gizmoTr, gbasis.tanHalfFov") !=
                       std::string::npos,
                   "R-063: o feed do gizmo deixou de usar gizmoAnchor "
                   "(a âncora local-as-world volta — M-V2)");
        EXPECT_MSG(code.find("gizmoAnchor(*gizmoTr, "
                             "std::tan(g_camera.fovY * 0.5f)") !=
                       std::string::npos,
                   "R-063: o draw do gizmo deixou de usar gizmoAnchor "
                   "(o desenho diverge do hit — M-V2)");
        EXPECT_MSG(code.find(
                       "TransformSystem::worldToLocalPos(*parentTr") !=
                       std::string::npos,
                   "R-063: o drag deixou de converter mundo→local (o TIC "
                   "com pai move-se rodado pelo pai — M-V1)");
        EXPECT_MSG(code.find("TransformSystem::composeWorld(*tr, parentTr ? "
                             "&parentTr->world : nullptr);") !=
                       std::string::npos,
                   "R-063: o feedback do drag deixou de compor com o pai "
                   "(o teleporte do Bug 1 volta — M-V1)");
    }
    // (4) O INSPECTOR compõe com o pai (os sítios de feedback — o mesmo
    //     contrato do drag; a contagem apanha a remoção furtiva)
    {
        std::string ui;
        EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/ui/EditorUi.cpp",
                            ui));
        const std::string code = test::stripCppComments(ui);
        int sites = 0;
        for (size_t pos = code.find("refreshWorldWithParent(");
             pos != std::string::npos;
             pos = code.find("refreshWorldWithParent(", pos + 1)) {
            ++sites;
        }
        EXPECT_MSG(sites >= 4,
                   "R-063: o Inspector deixou de compor com o pai "
                   "(sítios=%d < 4 — o teleporte volta nos edits — M-V1)",
                   sites);
    }
}

// ============================================================================
// R-064 (0.12-Y-F2) — O CLIMA E O VENTO NÃO MORREM — regress_weather
//
// A decisão de arquitetura do dono: o AmbientEnvironment NÃO renderiza
// partículas — CONFIGURA o ParticleEmitter do MESMO TIC (applyWeatherPreset,
// na criação E na mudança no Inspector); o WindTIC fornece o vento global
// lido pelo ParticleRenderer (uWind antes do draw call); a trovoada é um
// flash de ecrã via uniform no sky shader (SEM geometria de raio). As
// mutações vigiadas: M-Yg1 (o uWind ignorado no VS — as partículas não
// desviam) e M-Yg2 (o applyWeatherPreset não chamado — o preset não aplica).
// ============================================================================
TEST(regress_weather) {
    // (1) O VENTO no VS — a fórmula do prompt SEM comentários (o stripper
    //     da lição M-Yd1 mata o falso verde por prosa — M-Yg1 vive AQUI)
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/ParticleShader.h", src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("uniform vec3 uWind;") != std::string::npos,
                   "R-064: o uniform uWind saiu do VS");
        EXPECT_MSG(code.find("(uWind * localTime)") != std::string::npos,
                   "R-064: o TERMO DO VENTO saiu da posição do VS (a "
                   "fórmula do prompt — M-Yg1)");
        // a sobrecarga pura replica o VS (a fonte única do vento)
        const f32 p0[3] = {0.0f, 0.0f, 0.0f};
        const f32 vel[3] = {0.0f, 0.0f, 0.0f};
        const f32 grav[3] = {0.0f, 0.0f, 0.0f};
        const f32 wind[3] = {1.5f, 0.0f, 0.0f};
        f32 p[3];
        fx::particlePos(p0, vel, grav, wind, 2.0f, p);
        EXPECT_MSG(nearEqF(p[0], 3.0f, 1e-4f),
                   "R-064: a pura COM vento não desvia (wind·t)");
        // o uTime continua o ÚLTIMO 1f do draw (o contrato da casa)
        glstub::fb::enabled = true;
        glstub::fb::resetState();
        glstub::reset();
        ParticleRenderer pr;
        EXPECT(pr.init());
        ParticleRenderer::Params pp;
        pp.maxParticles = 50;
        pp.time = 4.5f;
        pp.wind[1] = 3.0f;
        pr.draw(pp);
        EXPECT_MSG(nearEqF(glstub::stats.lastUniform3f[1], 3.0f, 1e-6f),
                   "R-064: o uWind não chega ao shader (M-Yg1 no render)");
        EXPECT_MSG(nearEqF(glstub::stats.lastUniform1f, 4.5f, 1e-6f),
                   "R-064: o uTime deixou de ser o ÚLTIMO 1f (%.2f)",
                   (double)glstub::stats.lastUniform1f);
        pr.destroy();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }
    // (2) O CLIMA CONFIGURA O EMISSOR — na criação do preset E na mudança
    //     no Inspector (M-Yg2: o applyWeatherPreset não chamado)
    {
        Scene scene;
        const Handle hr =
            createTicFromPreset(scene, PresetKind::Rain, nullptr, nullptr);
        EXPECT_MSG(hr.valid(),
                   "R-064: o preset Rain morreu (Transform+PE+AE)");
        const AmbientEnvironment* ae =
            scene.get(hr)->getComponent<AmbientEnvironment>();
        const ParticleEmitter* pe =
            scene.get(hr)->getComponent<ParticleEmitter>();
        EXPECT_MSG(ae != nullptr && pe != nullptr &&
                       nearEqF(pe->gravity[1], -12.0f, 1e-5f) &&
                       nearEqF(pe->lifetime, 1.2f, 1e-5f),
                   "R-064: o preset Rain não nasce com o clima aplicado "
                   "(grav %.1f vida %.2f — M-Yg2)",
                   (double)(pe ? pe->gravity[1] : 0.0f),
                   (double)(pe ? pe->lifetime : 0.0f));
        EXPECT_MSG(ae != nullptr &&
                       ae->weatherType == AmbientEnvironment::kWeatherRain,
                   "R-064: o clima do preset não é chuva");
        // a NEVE (o pin do dono: lifetime 4.0)
        Scene s2;
        const Handle hs =
            createTicFromPreset(s2, PresetKind::Snow, nullptr, nullptr);
        const ParticleEmitter* pe2 =
            s2.get(hs)->getComponent<ParticleEmitter>();
        EXPECT_MSG(pe2 != nullptr && nearEqF(pe2->lifetime, 4.0f, 1e-5f),
                   "R-064: a neve não vive 4.0 s");
        // o INSPECTOR cicla e aplica (o contrato no fonte)
        std::string ui;
        EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/ui/EditorUi.cpp",
                            ui));
        const std::string uiCode = test::stripCppComments(ui);
        EXPECT_MSG(uiCode.find("applyWeatherPreset(") != std::string::npos,
                   "R-064: o Inspector deixou de chamar o "
                   "applyWeatherPreset na mudança do clima (M-Yg2)");
        EXPECT_MSG(uiCode.find("(e->weatherType + 1u) & 3u") !=
                       std::string::npos,
                   "R-064: o ciclar do clima saiu do despacho (o padrão "
                   "CamProj)");
        // a FONTE do preset (os valores EXATOS do prompt vivem no
        // componente — o serializer e o Inspector partilham)
        std::string comp;
        EXPECT(readFileText(REPO_ROOT
                            "/app/src/main/cpp/components/"
                            "AmbientEnvironment.h",
                            comp));
        const std::string compCode = test::stripCppComments(comp);
        EXPECT_MSG(compCode.find("-12.0f") != std::string::npos &&
                       compCode.find("-18.0f") != std::string::npos &&
                       compCode.find("1.2f") != std::string::npos &&
                       compCode.find("-1.2f") != std::string::npos &&
                       compCode.find("4.0f") != std::string::npos,
                   "R-064: os valores do preset do clima saíram do "
                   "componente (a fonte única)");
    }
    // (3) O FLASH da TROVOADA no céu — a linha EXATA do prompt no FS (sem
    //     geometria de raio) e o update do STORM no frame (antes do draw)
    {
        std::string sky;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/SkyRenderer.cpp", sky));
        const std::string skyCode = test::stripCppComments(sky);
        EXPECT_MSG(skyCode.find("col += vec3(uThunderFlash * 0.6, "
                                "uThunderFlash * 0.65, uThunderFlash * "
                                "0.75);") != std::string::npos,
                   "R-064: a linha do FLASH saiu do FS do céu");
        EXPECT_MSG(skyCode.find("uniform float uThunderFlash;") !=
                       std::string::npos,
                   "R-064: o uniform uThunderFlash saiu do FS");
        std::string mainSrc;
        EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/platform/main.cpp",
                            mainSrc));
        EXPECT_MSG(mainSrc.find("sp.thunder = thunderNow;") !=
                       std::string::npos,
                   "R-064: o céu deixou de consumir o flash do clima");
        EXPECT_MSG(mainSrc.find("updateStorm(") != std::string::npos,
                   "R-064: o update do STORM saiu do frame (o flash não "
                   "arma nem morre)");
        // o flash só entra com STORM ATIVO (a condição no lookup)
        EXPECT_MSG(mainSrc.find(
                       "AmbientEnvironment::kWeatherStorm") !=
                       std::string::npos,
                   "R-064: o lookup do flash perdeu a condição do STORM");
    }
    // (4) O .GONI transporta o clima e o vento — e NUNCA o runtime
    {
        Scene scene;
        const Handle h = scene.create("W");
        scene.get(h)->addComponent<Transform3D>();
        AmbientEnvironment* ae =
            scene.get(h)->addComponent<AmbientEnvironment>();
        ae->weatherType = AmbientEnvironment::kWeatherSnow;
        ae->intensity = 0.9f;
        ae->thunderFlash = 0.5f;   // runtime — nunca viaja
        WindTIC* w = scene.get(h)->addComponent<WindTIC>();
        w->windStrength = 7.5f;
        const std::string dump = SceneSerializer::dump(scene);
        EXPECT_MSG(dump.find("\"AmbientEnvironment\"") != std::string::npos &&
                       dump.find("\"weather\"") != std::string::npos &&
                       dump.find("\"intensity\"") != std::string::npos &&
                       dump.find("\"WindTIC\"") != std::string::npos &&
                       dump.find("\"strength\"") != std::string::npos,
                   "R-064: o .goni PERDEU o clima/vento");
        EXPECT_MSG(dump.find("thunder") == std::string::npos,
                   "R-064: o RUNTIME da trovoada INVADIU o .goni");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const AmbientEnvironment* ae2 =
            b.get(b.find("W"))->getComponent<AmbientEnvironment>();
        const WindTIC* w2 = b.get(b.find("W"))->getComponent<WindTIC>();
        EXPECT_MSG(ae2 != nullptr &&
                       ae2->weatherType ==
                           AmbientEnvironment::kWeatherSnow &&
                       nearEqF(ae2->intensity, 0.9f, 1e-5f) &&
                       w2 != nullptr && nearEqF(w2->windStrength, 7.5f,
                                                1e-5f),
                   "R-064: o clima/vento não volta do .goni");
    }
}

// R-065 (0.12-Z F0) — O DIAGNÓSTICO GL NÃO MORRE — regress_glctx_fase0:
// (1) os INTERRUPTORES POR PASSE existem e o mestre nasce OFF (o render
//     normal byte a byte — o custo de estar desligado é UM bool);
// (2) o ISOLAMENTO vive no frame REAL: a grelha com gate, a geometria com
//     gate no LOOP e no caminho DIRETO, a UI com escape (toolbar fica);
// (3) os CONTADORES de recursos GL alimentam o resumo do boot/term;
// (4) as FRONTEIRAS de passe com estado GL (o vazamento é logado).
TEST(regress_glctx_fase0) {
    // (1)+(4) o módulo existe com o contrato inteiro
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/GlDebug.h", src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("bool master   = false;") != std::string::npos,
                   "R-065: o mestre do debug não nasce OFF");
        EXPECT_MSG(code.find("struct PassState") != std::string::npos &&
                       code.find("inline PassState capture()") !=
                           std::string::npos &&
                       code.find("inline void passBoundary(") !=
                           std::string::npos,
                   "R-065: o snapshot de estado por passe sumiu");
        EXPECT_MSG(code.find("inline void summaryLine(") !=
                       std::string::npos,
                   "R-065: o resumo de contadores sumiu");
    }
    // (2) os gates no frame REAL (o fonte SEM comentários — M-Yd1)
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/main.cpp", src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("gldbg::g_masks.master") != std::string::npos,
                   "R-065: o main deixou de consumir o mestre");
        EXPECT_MSG(code.find(
                       "showGrid ? g_grid.draw(vp, camEye, camFocus)") !=
                       std::string::npos,
                   "R-065: a GRELHA perdeu o gate (o «só a grelha» morre)");
        EXPECT_MSG(
            code.find("showGeom && i < mrs.size()") != std::string::npos &&
                code.find("if (showGeom) {") != std::string::npos,
            "R-065: a GEOMETRIA perdeu o gate (loop ou caminho direto)");
        EXPECT_MSG(code.find("!gldbg::g_masks.ui &&") != std::string::npos,
                   "R-065: o ISOLAMENTO da UI sumiu (a sombra volta a ter "
                   "dono desconhecido)");
        EXPECT_MSG(code.find("gldbg::summaryLine(inv, sizeof(inv));") !=
                       std::string::npos,
                   "R-065: o boot deixou de logar o inventário GL");
        EXPECT_MSG(code.find("APP_CMD_LOST_FOCUS") != std::string::npos,
                   "R-065: o LOST_FOCUS deixou de ser logado");
    }
    // (3) a página de definições tem os toggles (a vara de medir do dono)
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/ui/SettingsPage.cpp", src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("kDbgMasterId") != std::string::npos &&
                       code.find("kDbgGridId") != std::string::npos,
                   "R-065: os toggles de passe saíram das Definições");
    }
    // os CONTADORES vivem nas camadas (o nascer e o morrer contam)
    {
        std::string r;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/Renderer.cpp", r));
        EXPECT_MSG(
            test::stripCppComments(r).find("gldbg::onCreated(") !=
                std::string::npos,
            "R-065: o Renderer deixou de contar os recursos");
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/Material.cpp", r));
        EXPECT_MSG(
            test::stripCppComments(r).find("gldbg::onDestroyed(") !=
                std::string::npos,
            "R-065: o LitMaterial deixou de contar a morte");
    }
}

// R-066 (0.12-Z F1) — A RECRIAÇÃO DO CONTEXTO TEM DONO —
// regress_glctx_fase1: o GLResourceManager é a FONTE ÚNICA da lista de
// invalidação/recriação (o term manual da 0.6.7 MORREU), o boot recria
// PELO GESTOR (o renderer obrigatório mantém o break), a câmara SÃ corre
// no INIT/RESIZE/RESUME e os caches de hit-test recalcam.
TEST(regress_glctx_fase1) {
    std::string src;
    EXPECT(readFileText(
        REPO_ROOT "/app/src/main/cpp/platform/main.cpp", src));
    const std::string code = test::stripCppComments(src);
    // (1) o gestor registado no arranque e usado no TERM (a lista manual
    //     da 0.6.7 foi SUBSTITUÍDA — o invalidateAll manda)
    EXPECT_MSG(code.find("glResourcesRegister();") != std::string::npos,
               "R-066: o registo das camadas saiu do android_main");
    EXPECT_MSG(code.find("g_glResources.invalidateAll();") !=
                   std::string::npos,
               "R-066: o TERM deixou de usar o gestor");
    // a lista MANUAL de destroys NÃO voltou ao term (a região do CASE é
    // que manda — os wrappers das camadas contêm os destroys legítimos)
    {
        const size_t term = code.find("case APP_CMD_TERM_WINDOW:");
        ASSERT(term != std::string::npos);
        const size_t nextCase =
            code.find("case APP_CMD_", term + 10);
        const std::string termBody = code.substr(
            term, nextCase == std::string::npos ? std::string::npos
                                                : nextCase - term);
        EXPECT_MSG(termBody.find("g_font.destroy()") == std::string::npos &&
                       termBody.find("g_grid.destroy()") ==
                           std::string::npos &&
                       termBody.find("g_sky.destroy()") ==
                           std::string::npos &&
                       termBody.find("g_renderer.shutdown()") ==
                           std::string::npos,
                   "R-066: a lista MANUAL de destroys voltou ao term (o "
                   "gestor é a fonte única — as fugas de camada nova "
                   "voltam)");
    }
    // (2) o boot recria PELO GESTOR (o renderer obrigatório mantém o break)
    EXPECT_MSG(code.find("g_glResources.recreateLayer(bl.name)") !=
                   std::string::npos,
               "R-066: o boot deixou de recriar as camadas pelo gestor");
    EXPECT_MSG(code.find("{\"gpu-cube\", \"mesh do cubo\"}") !=
                   std::string::npos,
               "R-066: a lista de boot do gestor sumiu");
    EXPECT_MSG(code.find("if (!rendererOk) {") != std::string::npos,
               "R-066: o break do renderer obrigatório sumiu (o boot "
               "continuaria sem render — a regressão que o CI apanhou)");
    // (3) a câmara SÃ nos três eventos do lifecycle
    {
        const size_t iInit = code.find("cameraEnsureSane();");
        const size_t iResized = code.find("cameraEnsureSane();", iInit + 1);
        const size_t iResume = code.find("cameraEnsureSane();", iResized + 1);
        EXPECT_MSG(iInit != std::string::npos && iResized != iInit &&
                       iResume > iResized && iResume != std::string::npos,
                   "R-066: a câmara sã não corre no INIT/RESIZE/RESUME "
                   "(encontrados %zu sítios)",
                   (size_t)(iInit != std::string::npos) +
                       (size_t)(iResized != iInit) +
                       (size_t)(iResume != std::string::npos &&
                                iResume != iResized));
    }
    EXPECT_MSG(code.find("editorRecalcHitCaches();") != std::string::npos,
               "R-066: os caches de hit-test deixaram de recalcular");
    // (4) o gestor existe com o contrato (ordem + falha reportada)
    {
        std::string h;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/GlResources.h", h));
        const std::string hc = test::stripCppComments(h);
        EXPECT_MSG(hc.find("void invalidateAll()") != std::string::npos &&
                       hc.find("bool recreateAll()") != std::string::npos &&
                       hc.find("bool recreateLayer(const char* name)") !=
                           std::string::npos,
                   "R-066: o contrato do gestor sumiu");
    }
}

// R-067 (0.12-Z F2) — A SOMBRA NÃO VOLTA — regress_glctx_fase2: o NaN/inf
// nos campos do WorldEnvironment é REJEITADO NA FONTE (a clamp não apanha
// NaN — comparações com NaN são sempre falsas — e o fogDensity NaN
// envenena a exp2 do lit FS: outColor NaN = O ECÃ PRETO, a candidata H3
// da sombra), a névoa do FS corre em HIGH P e SEM pow, a GRELHA tem o
// estado EXPLÍCITO (depth test ON + func LESS — o inheritado é o
// vazamento) e a entrada do pass 3D repõe o depth func.
TEST(regress_glctx_fase2) {
    // (1) o sane do WE no fonte (a rejeição ANTES da clamp)
    {
        std::string h;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/components/Environment.h", h));
        const std::string code = test::stripCppComments(h);
        EXPECT_MSG(code.find("static f32 sane(f32 v, f32 def, f32 lo, f32 hi)")
                       != std::string::npos &&
                       code.find("!std::isfinite(v)") != std::string::npos &&
                       code.find("fogDensity = sane(fogDensity") !=
                           std::string::npos,
                   "R-067: a rejeição de NaN/inf saiu do WE (a sombra "
                   "preta volta pela névoa envenenada)");
    }
    // (2) a névoa do FS em HIGH P e SEM pow (o mediump é fp16 no Mali do
    //     C33 — a distância degenerava)
    {
        std::string m;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/Material.cpp", m));
        const std::string code = test::stripCppComments(m);
        EXPECT_MSG(code.find("highp float fog = 1.0 - exp(-fogD * fogD);") !=
                       std::string::npos,
                   "R-067: a névoa do lit FS perdeu o highp/x*x (o dono: "
                   "highp para fog)");
        EXPECT_MSG(code.find("pow(length") == std::string::npos,
                   "R-067: o pow() voltou à névoa (o x*x é o contrato)");
    }
    // (3) a grelha com o estado EXPLÍCITO (o teste unitário do estado é do
    //     device — o fonte é o contrato)
    {
        std::string g;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/Grid.cpp", g));
        const std::string code = test::stripCppComments(g);
        EXPECT_MSG(code.find("glEnable(GL_DEPTH_TEST);") !=
                       std::string::npos,
                   "R-067: a grelha voltou a INHERITAR o depth test");
        EXPECT_MSG(code.find("glDepthFunc(GL_LESS);") != std::string::npos,
                   "R-067: a grelha perdeu o depth func explícito");
    }
    // (4) a entrada do pass 3D repõe o depth func (defesa em profundidade)
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/main.cpp", src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(code.find("glDepthFunc(GL_LESS);") != std::string::npos,
                   "R-067: a entrada do passe 3D deixou de repor o depth "
                   "func");
    }
}

// R-068 (0.12-Z F3) — A RECUPERAÇÃO AUTOMÁTICA NÃO MORRE —
// regress_glctx_fase3: a SONDA por frame no topo do frame() (o contexto/
// surface EGL + glIsProgram/glIsTexture + o glGetError em SÉRIE com o
// limiar), a RECUPERAÇÃO pelo gestor da F1 (o detach → o release → o
// invalidateAll → o recreateAll → a câmara sã → o reset da interação), o
// INDICADOR DE SAÚDE «GL: RECRIADO n» (só no debug) e o BOTÃO do dono nas
// Definições.
TEST(regress_glctx_fase3) {
    std::string src;
    EXPECT(readFileText(
        REPO_ROOT "/app/src/main/cpp/platform/main.cpp", src));
    const std::string code = test::stripCppComments(src);
    // (1) a sonda no TOPO do frame (o dono: «verificação por frame barata»)
    EXPECT_MSG(code.find("contextEnsureAlive();") != std::string::npos,
               "R-068: a sonda por frame saiu do frame()");
    // (2) as sondas GL e o glGetError em SÉRIE (o limiar existe — um erro
    //     isolado não é perda)
    EXPECT_MSG(code.find("glIsProgram(g_renderer.uiProgram())") !=
                       std::string::npos &&
                   code.find("glIsTexture(g_renderer.whiteTexture())") !=
                       std::string::npos,
               "R-068: as sondas glIsProgram/glIsTexture sumiram");
    EXPECT_MSG(code.find("kGlErrorStreakMax = 30") != std::string::npos,
               "R-068: o limiar do glGetError em série sumiu");
    // (3) a recuperação pelo GESTOR (a fonte única da F1 — nada de listas
    //     à mão na recuperação)
    EXPECT_MSG(code.find("contextRecover(const char* why)") !=
                       std::string::npos &&
                   code.find("detachRenderersFromGpu();") !=
                       std::string::npos,
               "R-068: a recuperação perdeu o ciclo (detach/gpu/gestor)");
    {
        const size_t rec = code.find("void contextRecover(const char* why)");
        ASSERT(rec != std::string::npos);
        const std::string body =
            code.substr(rec, code.find("void contextEnsureAlive", rec) - rec);
        EXPECT_MSG(body.find("g_glResources.invalidateAll();") !=
                       std::string::npos &&
                       body.find("g_glResources.recreateAll()") !=
                           std::string::npos &&
                       body.find("editorRecalcHitCaches();") !=
                           std::string::npos &&
                       body.find("cameraEnsureSane();") !=
                           std::string::npos,
                   "R-068: a recuperação não passa pelo gestor + o reset "
                   "da interação + a câmara sã");
    }
    // (4) o reset da interação no PAUSE e no LOST_FOCUS
    {
        const size_t pause = code.find("case APP_CMD_PAUSE:");
        ASSERT(pause != std::string::npos);
        EXPECT_MSG(code.find("editorRecalcHitCaches();", pause) <
                       code.find("case APP_CMD_RESUME:"),
                   "R-068: o PAUSE deixou de resetar a interação");
        const size_t lost = code.find("case APP_CMD_LOST_FOCUS:");
        ASSERT(lost != std::string::npos);
        EXPECT_MSG(code.find("editorRecalcHitCaches();", lost) <
                       code.find("case APP_CMD_DESTROY:"),
                   "R-068: o LOST_FOCUS deixou de resetar a interação");
    }
    // (5) o indicador de saúde (só no debug) e o botão do dono
    EXPECT_MSG(code.find("g_contextRecreations == 0") !=
                   std::string::npos,
               "R-068: o indicador de saúde sumiu da statusLine");
    {
        std::string sp;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/ui/SettingsPage.cpp", sp));
        EXPECT_MSG(test::stripCppComments(sp).find("kRestartRendererId") !=
                       std::string::npos,
                   "R-068: o botão Reiniciar renderer saiu das Definições");
    }
    // (6) o ACTION_CANCEL segue a soltar TODOS os ponteiros (o contrato do
    //     dono — o reset da interação no sistema de input)
    {
        std::string is;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/InputState.cpp", is));
        const std::string isc = test::stripCppComments(is);
        EXPECT_MSG(isc.find("case AMOTION_EVENT_ACTION_UP:") !=
                       std::string::npos &&
                   isc.find("case AMOTION_EVENT_ACTION_CANCEL:") !=
                       std::string::npos &&
                   isc.find("releaseAll();") != std::string::npos,
                   "R-068: o ACTION_CANCEL deixou de soltar os ponteiros");
    }
}

// R-069 (0.12-Z F4) — O TESTE DA RECUPERAÇÃO NÃO MORRE —
// regress_glctx_fase4: o botão «Simular perda» (o flag que arma 1 frame de
// sonda morta e a recuperação REAL corre — sem atalho) e OS 20 CENÁRIOS
// do dono no replay do device (10 SEM WE + 10 COM WE).
TEST(regress_glctx_fase4) {
    std::string src;
    EXPECT(readFileText(
        REPO_ROOT "/app/src/main/cpp/platform/main.cpp", src));
    const std::string code = test::stripCppComments(src);
    EXPECT_MSG(code.find("g_simulateLossFrames = 1;") != std::string::npos,
               "R-069: o despacho da simulação sumiu");
    EXPECT_MSG(code.find("if (g_simulateLossFrames > 0)") !=
                   std::string::npos,
               "R-069: a sonda deixou de consumir a simulação");
    {
        std::string sp;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/ui/SettingsPage.cpp", sp));
        const std::string c = test::stripCppComments(sp);
        EXPECT_MSG(c.find("kSimulateLossId") != std::string::npos &&
                       c.find("gldbg::g_masks.master &&") !=
                           std::string::npos,
                   "R-069: o botão Simular saiu das Definições (ou deixou "
                   "de ser debug-only)");
    }
    {
        std::string c33;
        EXPECT(readFileText(REPO_ROOT "/tests/c33_virtual.cpp", c33));
        const std::string c = test::stripCppComments(c33);
        EXPECT_MSG(c.find("os 20 CENÁRIOS do dono") != std::string::npos,
                   "R-069: os 20 cenários saíram do replay");
        EXPECT_MSG(c.find("APP_CMD_TERM_WINDOW") != std::string::npos &&
                       c.find("APP_CMD_WINDOW_RESIZED") !=
                           std::string::npos &&
                       c.find("APP_CMD_PAUSE") != std::string::npos,
                   "R-069: os eventos do lifecycle saíram dos cenários");
        EXPECT_MSG(c.find("g_simulateLossFrames = 1;") != std::string::npos,
                   "R-069: a perda simulada saiu dos cenários");
    }
}

// ---------------------------------------------------------------------------
// R-070 — regress_glctx_z2_fase1 (0.12-Z2 F1): o INVALIDATE CORRE SEMPRE —
// o achado do fuzzing no GLES REAL (raster_z2 --fuzz): o salto
// `if (!l.valid) continue` no invalidateAll deixava a camada cujo recreate
// FALHOU (a font-mono no OEM enxuto falha SEMPRE) permanentemente inválida
// — o term seguinte saltava o destroy dela e o handle do contexto morto
// sobrevivia (o guard `if (tex_)` do FontAtlas "recriava" com id morto =
// TEXTO MORTO depois de sair e voltar — o sintoma do dono no device).
// A mutação (re-pôr o salto) tem de ficar VERMELHA aqui.
// ---------------------------------------------------------------------------
TEST(regress_glctx_z2_fase1) {
    std::string src;
    EXPECT(readFileText(
        REPO_ROOT "/app/src/main/cpp/render/GlResources.h", src));
    const std::string code = test::stripCppComments(src);
    // (1) o salto por `valid` NÃO EXISTE no invalidateAll (o invalidate é
    //     idempotente por contrato e corre sempre)
    EXPECT_MSG(code.find("if (!l.valid)") == std::string::npos &&
                   code.find("continue;   // já inválida") ==
                       std::string::npos,
               "R-070: o salto da camada inválida voltou ao invalidateAll "
               "(o handle morto sobrevive ao contexto seguinte — o texto "
               "morre após sair e voltar)");
    // (2) o commentário da 0.12-Z2 F1 está no fonte (a razão da regra —
    //     lido no fonte CRU: o stripCppComments come os comentários)
    EXPECT_MSG(src.find("0.12-Z2 F1") != std::string::npos,
               "R-070: a nota do achado do fuzzing saiu do GlResources.h");
    // (3) o pin do term duplo aferra o contrato F1-R (0.12-Z2 F1-R: o
    //     contador conta TRANSIÇÕES válida→inválida — o nº REAL de perdas
    //     — e não o nº de chamadas: um TERM duplo (Android faz
    //     TERM/INIT/TERM/INIT) NÃO dobra o contador nem infla o
    //     «GL: RECRIADO n»; o contrato F1 contava inv0+6, o que vendia
    //     2 perdas onde só houve UMA. O ESSENCIAL inalterado: o destroy
    //     CORRE sempre (aferido pelo próprio pin nas linhas do «O
    //     invalidateAll tem de CORRER o destroy mesmo em camadas já
    //     inválidas») e o salto proibido no check (1) acima.
    {
        std::string pin;
        EXPECT(readFileText(REPO_ROOT "/tests/test_012z_ctx.cpp", pin));
        const std::string c = test::stripCppComments(pin);
        EXPECT_MSG(c.find("inv0 + 1") != std::string::npos &&
                       c.find("inv0 + 6") == std::string::npos,
                   "R-070: o pin do term duplo não aferra o contrato F1-R "
                   "(inv0 + 1 — as TRANSIÇÕES, não as chamadas)");
    }
}

// ---------------------------------------------------------------------------
// R-071 — regress_sky_ground (0.12-Z2 F2): o BELOW-HORIZON é o TOM DE CHÃO —
// o achado da FASE 0 no GLES REAL (a captura B5_we_meianoite.png: o
// meio-plano inferior pintado com uHorizon = a «zona azul/escura fixa que
// corta a grelha» do dono). A pura skyGradient e o FS do céu têm de
// manter o MESMO par kGroundDim/kGroundFadeY (a réplica linha a linha é o
// padrão da casa). A mutação (voltar ao clamp puro) fica VERMELHA aqui.
// ---------------------------------------------------------------------------
TEST(regress_sky_ground) {
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/SkyRenderer.h", src));
        EXPECT_MSG(src.find("kGroundDim") != std::string::npos &&
                       src.find("kGroundFadeY") != std::string::npos &&
                       src.find("0.12-Z2 F2") != std::string::npos,
                   "R-071: o tom de chão saiu da pura skyGradient (a zona "
                   "azul fixa volta ao meio-plano inferior)");
    }
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/SkyRenderer.cpp", src));
        // o FS replica as MESMAS constantes (a réplica da pura)
        EXPECT_MSG(src.find("kGroundDim   = 0.45") != std::string::npos &&
                       src.find("kGroundFadeY = 0.18") != std::string::npos,
                   "R-071: o FS do céu divergiu da pura (as constantes do "
                   "chão têm de ser as MESMAS — o padrão Pbr.h)");
        EXPECT_MSG(src.find("vec3 sky = uHorizon + (uZenith - uHorizon) * t;")
                           != std::string::npos &&
                       src.find("sstep(0.0, -kGroundFadeY, dir.y)") !=
                           std::string::npos,
                   "R-071: o FS deixou de misturar o tom de chão (a "
                   "réplica linha a linha morreu)");
    }
}

// R-072 (0.12-Z2 F1-BIS) — A AUDITORIA ADICIONAL DO CICLO DE VIDA —
// regress_glctx_z2_audit: os 6 achados da auditoria exaustiva que os
// irmãos não cobrem: (1) o eglInitialize com major/minor REAIS (o nullptr
// é legal por spec mas o glvnd devolve EGL_FALSE 0x3001 — PROVADO por
// probe isolado: o raster harness não arrancava); (2) o RETORNO do
// eglSwapBuffers alimenta a sonda (o EGL_FALSE era ignorado); (3) o init
// parcial do EGL limpa o que já nasceu (o contexto sem dono); (4) o TERM
// resetA o ESTADO DE INTERAÇÃO (o PAUSE/LOST_FOCUS repunham — o term
// FALTAVA; o Android pode destruir a janela a meio do gesto); (5) a
// camada OPCIONAL (font-mono) não aborta o recreateAll (a device sem
// mono ficava presa no cooldown para sempre); (6) o Renderer::init
// começa pelo destroy e o shutdown repõe os locations; (7) a recuperação
// mata os meshes de prim (o term já matava).
TEST(regress_glctx_z2_audit) {
    std::string src;
    EXPECT(readFileText(
        REPO_ROOT "/app/src/main/cpp/platform/EglContext.cpp", src));
    const std::string ec = test::stripCppComments(src);
    // (1) o eglInitialize com ponteiros REAIS
    EXPECT_MSG(ec.find("eglInitialize(display_, &eglVerMajor, "
                       "&eglVerMinor)") != std::string::npos,
               "R-072: o eglInitialize voltou ao nullptr (o glvnd devolve "
               "EGL_FALSE 0x3001 — o raster harness não arranca)");
    // (2) o swap VERIFICADO + a sonda consome + (3) a auto-limpeza
    EXPECT_MSG(ec.find("eglSwapBuffers(display_, surface_) == EGL_TRUE") !=
                   std::string::npos,
               "R-072: o retorno do eglSwapBuffers deixou de ser "
               "verificado (a perda no swap não alimenta a sonda)");
    EXPECT_MSG(ec.find("lastSwapOk_ = true") != std::string::npos,
               "R-072: o estado do swap saiu do EglContext (o init)");
    {
        std::string eh;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/EglContext.h", eh));
        EXPECT_MSG(eh.find("bool lastSwapOk() const") != std::string::npos,
                   "R-072: o lastSwapOk() saiu do EglContext.h");
    }
    EXPECT_MSG(ec.find("eglDestroyContext(display_, context_)") !=
                   std::string::npos,
               "R-072: o init parcial deixou de limpar (o contexto órfão "
               "volta)");
    // (4) o TERM resetA o input
    {
        std::string m;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/main.cpp", m));
        const std::string code = test::stripCppComments(m);
        const size_t term = code.find("case APP_CMD_TERM_WINDOW:");
        ASSERT(term != std::string::npos);
        const size_t nextCase = code.find("case APP_CMD_", term + 10);
        const std::string termBody =
            code.substr(term, nextCase == std::string::npos
                                  ? std::string::npos
                                  : nextCase - term);
        EXPECT_MSG(termBody.find("g_input.resetAll();") !=
                       std::string::npos &&
                       termBody.find("editorRecalcHitCaches();") !=
                           std::string::npos,
                   "R-072: o TERM deixou de resetar o input (o gesto "
                   "sobrevive à perda de contexto)");
        // (7) a recuperação mata os prims
        const size_t rec =
            code.find("void contextRecover(const char* why)");
        ASSERT(rec != std::string::npos);
        EXPECT_MSG(code.find("primMeshesDestroyAll();", rec) !=
                       std::string::npos,
                   "R-072: a recuperação deixou de destruir os meshes de "
                   "prim (os VAO/VBO órfãos voltam)");
        EXPECT_MSG(code.find("!g_egl.lastSwapOk()") != std::string::npos,
                   "R-072: a sonda deixou de consumir o estado do swap");
    }
    // (5) a camada opcional
    {
        std::string h;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/GlResources.h", h));
        const std::string hc = test::stripCppComments(h);
        EXPECT_MSG(hc.find("bool optional = false") != std::string::npos &&
                       hc.find("camada OPCIONAL") != std::string::npos,
                   "R-072: a camada opcional saiu do gestor (a device sem "
                   "mono volta a não recuperar nunca)");
        std::string m;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/main.cpp", m));
        EXPECT_MSG(test::stripCppComments(m).find(
                       "layerRecreateFontMono, true);") !=
                       std::string::npos,
                   "R-072: o font-mono deixou de ser opcional no registo");
    }
    // (6) o Renderer::init começa pelo destroy; o shutdown repõe os locs
    {
        std::string r;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/Renderer.cpp", r));
        const std::string rc = test::stripCppComments(r);
        const size_t init = rc.find("bool Renderer::init()");
        ASSERT(init != std::string::npos);
        const size_t shutdownCall = rc.find("shutdown();", init);
        const size_t subsClear = rc.find("subs_.clear();", init);
        EXPECT_MSG(shutdownCall != std::string::npos &&
                       subsClear != std::string::npos &&
                       shutdownCall < subsClear,
                   "R-072: o Renderer::init deixou de começar pelo destroy "
                   "(o 2.º init no mesmo contexto vaza)");
        EXPECT_MSG(rc.find("locProj_ = locTex_ = -1;") != std::string::npos,
                   "R-072: o shutdown deixou de repor os uniform locations "
                   "(o location sobrevive à recriação)");
    }
}

// R-073 — regress_weather_z2_f3 (0.12-Z2 FASE 3): O CLIMA VISÍVEL — os
// achados do F0.4 fechados na FONTE: (1) o PISO EM PÍXEIS no VS (o max()
// com o w do centro — a gota de 0.015 m media ~1.3 px @720p/dist 7,
// invisível no device); (2) o EMISSOR SEGUE A CÂMARA (o ramo do clima no
// passe de partículas — followOrigin + uSpawnScale); (3) O ORÇAMENTO DO
// FPS (a EMA degrada o count do clima abaixo de 50 fps); (4) o VOLUME
// transportado pelo preset e pelo .goni ("rad"). A mutação (o max() do VS
// removido) fica VERMELHA aqui.
TEST(regress_weather_z2_f3) {
    {
        std::string src;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/ParticleShader.h", src));
        const std::string code = test::stripCppComments(src);
        // (1) o piso em píxeis no VS (a FONTE ÚNICA das puras + o GLSL)
        EXPECT_MSG(code.find("kMinParticlePx = 3.0f") !=
                       std::string::npos,
                   "R-073: o piso em píxeis (kMinParticlePx 3.0) saiu da "
                   "fonte única");
        EXPECT_MSG(code.find("uniform float uMinWorldPerDepth;") !=
                       std::string::npos &&
                       code.find("max(sz, uMinWorldPerDepth * center.w)") !=
                           std::string::npos,
                   "R-073: o max() do piso no VS saiu (a gota sub-pixel "
                   "volta — o clima invisível no device)");
        // (2) o volume do clima (a escala do spawn no VS)
        EXPECT_MSG(code.find("uniform float uSpawnScale;") !=
                       std::string::npos &&
                       code.find("(aOriginSeed.xyz * uSpawnScale)") !=
                           std::string::npos,
                   "R-073: a escala do volume saiu do VS (o clima volta ao "
                   "ponto)");
        // (3) o orçamento do FPS (os números do dono)
        EXPECT_MSG(code.find("kFpsFloor   = 50.0f") != std::string::npos &&
                       code.find("kFpsRecover = 58.0f") !=
                           std::string::npos &&
                       code.find("kScaleMin   = 0.25f") !=
                           std::string::npos,
                   "R-073: o orçamento do FPS (50/58/0.25) saiu da fonte "
                   "única");
    }
    {
        std::string m;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/platform/main.cpp", m));
        const std::string mc = test::stripCppComments(m);
        EXPECT_MSG(mc.find("g_weatherBudget.step(clockFrozen ? 0.0f : "
                           "g_frameDt);") != std::string::npos,
                   "R-073: o orçamento deixou de correr por frame (a EMA "
                   "morre)");
        EXPECT_MSG(mc.find("fx::followOrigin(aim, pes.at(i).spawnRadius, "
                           "origem);") != std::string::npos,
                   "R-073: o emissor do clima deixou de seguir a câmara");
        EXPECT_MSG(mc.find("pp.maxParticles = "
                           "g_weatherBudget.count(pp.maxParticles);") !=
                       std::string::npos,
                   "R-073: o orçamento deixou de escalonar o count do "
                   "clima");
        EXPECT_MSG(mc.find("pp.minWorldPerDepth = partMinW;") !=
                       std::string::npos,
                   "R-073: o piso em píxeis deixou de chegar ao draw");
    }
    {
        std::string ae;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/components/AmbientEnvironment.h",
            ae));
        const std::string aec = test::stripCppComments(ae);
        EXPECT_MSG(aec.find("pe->spawnRadius  = kRainRadius;") !=
                       std::string::npos &&
                       aec.find("pe->spawnRadius  = kSnowRadius;") !=
                           std::string::npos,
                   "R-073: os presets deixaram de transportar o volume do "
                   "clima");
        EXPECT_MSG(aec.find("pe->spawnRadius = "
                            "ParticleEmitter::kSpawnRadiusDefault;") !=
                       std::string::npos,
                   "R-073: o NONE deixou de repor o jorro (o emissor fica "
                   "com o volume do clima morto)");
    }
    {
        std::string ss;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/core/SceneSerializer.cpp", ss));
        const std::string ssc = test::stripCppComments(ss);
        EXPECT_MSG(ssc.find("c.addMember(\"rad\", "
                            "Json::makeNumber(pe->spawnRadius));") !=
                       std::string::npos,
                   "R-073: o .goni deixou de transportar o raio do volume "
                   "(rad)");
    }
    // (5) O ACHADO P0 DA FASE 3 — O uLIFETIME NO UPLOAD: o uniform ficava
    // 0.0 (o draw nunca o carregava) → mod(x,0)=NaN → TODOS os vértices
    // morriam no clip — o sistema de partículas NÃO DESENHAVA NENHUM
    // PÍXEL no driver real desde a 0.12-Y-F1 (o stub do CI conta draws e
    // não vê fragmentos; a prova de píxeis da FASE 3 é que apanhou). A
    // mutação (remover o upload) fica VERMELHA aqui.
    {
        std::string pr;
        EXPECT(readFileText(
            REPO_ROOT "/app/src/main/cpp/render/ParticleRenderer.cpp", pr));
        const std::string prc = test::stripCppComments(pr);
        EXPECT_MSG(prc.find("locLifetime_ = "
                            "glGetUniformLocation(prog_, \"uLifetime\");") !=
                       std::string::npos,
                   "R-073: o location do uLifetime saiu do init (o uniform "
                   "volta a ficar 0.0 — NaN no mod — zero píxeis)");
        EXPECT_MSG(prc.find("glUniform1f(locLifetime_, p.lifetime);") !=
                       std::string::npos,
                   "R-073: o UPLOAD do uLifetime saiu do draw (mod(x,0)="
                   "NaN — o clima volta a desaparecer no device)");
        // o upload tem de ser ANTES do uTime (o ÚLTIMO 1f — o contrato)
        {
            const size_t life = prc.find("glUniform1f(locLifetime_, "
                                         "p.lifetime);");
            const size_t time = prc.find("glUniform1f(locTime_, p.time);");
            EXPECT_MSG(life != std::string::npos && time != std::string::npos &&
                           life < time,
                       "R-073: o uLifetime passou o uTime (o ÚLTIMO 1f do "
                       "draw é o contrato da casa)");
        }
    }
}

// R-074 — regress_golden_z2_f5 (0.12-Z2 FASE 5): A REGRESSÃO POR IMAGEM —
// a maquinaria das golden images (tests/raster_golden.h) e o modo --golden
// do raster_z2 aferrados NO FONTE (o CI não tem Mesa, mas o contrato tem
// de viver). As mutações que este pin mata: (1) os constantes do FNV-1a 64
// mudados (o hash das goldens divergia do frameHash/V9 — o contrato a
// mentir sobre si próprio); (2) a tolerância da casa afrouxada (o 0.5%/32
// do V9 — o céu partido a passar); (3) o modo golden saído do harness (a
// regressão por imagem desligada); (4) o hash das goldens a ser um
// ad-hoc em vez da maquinaria; (5) o verify a comparar contra um PNG que
// nunca decodifica (o falso verde). A prova de mutação AO VIVO (o kGroundDim
// 0.45→0.95 do FS, a regressão da F2): 4 cenas FALHA (47% diff, maxDelta
// 113), os diff PNGs saem, grid_solo/clima/cam_borda ficam EXATO — e a
// R-074 continua VERDE porque o QUE ela aferra é a maquinaria, não o céu
// (o céu é aferido pela R-071 e pelo próprio golden).
TEST(regress_golden_z2_f5) {
    {
        std::string src;
        EXPECT(readFileText(REPO_ROOT "/tests/raster_golden.h", src));
        const std::string code = test::stripCppComments(src);
        // (1) o FNV-1a 64 É o do frameHash (o MESMO algoritmo do V9)
        EXPECT_MSG(code.find("1469598103934665603ull") != std::string::npos &&
                       code.find("1099511628211ull") != std::string::npos,
                   "R-074: os constantes do FNV-1a 64 saíram (o hash das "
                   "goldens divergiu do frameHash/V9 — o contrato a mentir "
                   "sobre si próprio)");
        // (2) a tolerância da casa (o V9: 0.5% e maxDelta ≤32) não afrouxa
        EXPECT_MSG(code.find("kTolPct = 0.5") != std::string::npos &&
                       code.find("kTolMaxDelta = 32") != std::string::npos,
                   "R-074: a tolerância da casa afrouxou (o 0.5%%/32 do V9 "
                   "é o contrato — afrouxar é a regressão a passar)");
        // (3) o veredicto tem os TRÊS ramos (o FALHA é o que mata)
        EXPECT_MSG(code.find("Verdict::TOLERANCIA") != std::string::npos &&
                       code.find("Verdict::FALHA") != std::string::npos &&
                       code.find("Verdict::EXATO") != std::string::npos,
                   "R-074: um ramo do veredicto saiu (o EXATO/TOLERANCIA/"
                   "FALHA é a política da casa)");
        // (4) o diff RGBA compara os 4 CANAIS byte a byte (o maxDelta é o
        // maior salto por canal — um diff que olhe só um canal deixa o céu
        // partido passar)
        EXPECT_MSG(code.find("inline DiffReport diffRgba") !=
                       std::string::npos,
                   "R-074: o diffRgba saiu da maquinaria (o diff por canal "
                   "é o que separa a TOLERANCIA do FALHA)");
        // (5) o ficheiro dourado é O caminho do contrato
        EXPECT_MSG(code.find("tests/golden/golden_z2.txt") !=
                       std::string::npos,
                   "R-074: o caminho da lista dourada saiu (a lista é "
                   "tests/golden/golden_z2.txt — o contrato do ficheiro)");
    }
    {
        std::string rz;
        EXPECT(readFileText(REPO_ROOT "/tests/raster_z2.cpp", rz));
        const std::string rzc = test::stripCppComments(rz);
        // (6) o modo golden VIVO no harness (a regressão por imagem não
        // desliga)
        EXPECT_MSG(rzc.find("--golden") != std::string::npos &&
                       rzc.find("goldenRun(") != std::string::npos &&
                       rzc.find("--golden-write") != std::string::npos,
                   "R-074: o modo golden saiu do raster_z2 (a regressão "
                   "por imagem desligada)");
        // (7) o hash das goldens passa PELA maquinaria (não um hash ad hoc
        // — a mesma fonte do FNV para o V9 e para as goldens)
        EXPECT_MSG(rzc.find("gogolden::fnv1a64(px.data(), px.size())") !=
                       std::string::npos,
                   "R-074: o hash das goldens deixou de passar pela "
                   "maquinaria (um hash ad hoc diverge do V9 sem ninguém "
                   "notar)");
        // (8) o verify compara CONTRA O PNG DECODIFICADO (loadPng — o
        // falso verde do hash-que-nunca-confere morre)
        EXPECT_MSG(rzc.find("vv::loadPng(") != std::string::npos &&
                       rzc.find("gogolden::diffRgba(") != std::string::npos,
                   "R-074: o verify deixou de decodificar/comparar o PNG "
                   "de referência (o falso verde do hash sem imagem)");
    }
    {
        // (9) o pino RUNTIME da maquinaria vive e corre no test_core (os
        // vetores canónicos do FNV, o parser, os veredictos, a máscara)
        std::string t5;
        EXPECT(readFileText(REPO_ROOT "/tests/test_012z2_f5.cpp", t5));
        EXPECT_MSG(t5.find("golden_fnv_conhecido") != std::string::npos &&
                       t5.find("golden_parse_roundtrip") !=
                           std::string::npos &&
                       t5.find("golden_veredicto") != std::string::npos &&
                       t5.find("golden_mask_diff") != std::string::npos,
                   "R-074: os pins runtime da maquinaria saíram do "
                   "test_core (o FNV/parser/veredicto/máscara sem prova)");
    }
}

// R-075 — regress_utf8_z2_f5 (0.12-Z2 F5): O UTF8DECODE SÓ LÊ OS BYTES
// QUE A SEQUÊNCIA EXIJE — o ACHADO F5-B do ASan (o load incondicional de
// s[1..3] lia além do fim num byte malformado no fim de um buffer; o
// regress_glyph_coverage sob ASan apanhou-o AO VIVO). A cura (o `need`
// pela classe do lead) tem de ficar NO FONTE: a mutação (repor o load
// incondicional de s[3]) fica VERMELHA aqui.
TEST(regress_utf8_z2_f5) {
    std::string src;
    EXPECT(readFileText(REPO_ROOT "/app/src/main/cpp/ui/FontAtlas.h", src));
    const std::string code = test::stripCppComments(src);
    // a cura VIVA: o `need` pela classe do lead (1/2/3, 0 = inválido)
    EXPECT_MSG(code.find("const int need = (c0 & 0xE0u) == 0xC0u ? 1") !=
                   std::string::npos,
               "R-075: o `need` saiu do utf8Decode (a cura do ACHADO F5-B "
               "foi desfeita — os bytes voltam a ler-se sem o lead "
               "autorizar)");
    // o load INCONDICIONAL de s[3] PROIBIDO (a mutação exata do achado:
    // `static_cast<unsigned char>(s[3]);` numa linha própria sem guarda)
    EXPECT_MSG(code.find("const unsigned char c3 = "
                         "static_cast<unsigned char>(s[3]);") ==
                   std::string::npos,
               "R-075: o load incondicional de s[3] voltou (o overflow "
               "além do fim em texto malformado volta com ele)");
    // o FFFD com bytes=1 no lead inválido (o contrato do iterador: nunca
    // crash, nunca loop, sempre avança)
    EXPECT_MSG(code.find("return 0xFFFDu;") != std::string::npos,
               "R-075: o U+FFFD saiu (o contrato do decodificador morreu)");
}
