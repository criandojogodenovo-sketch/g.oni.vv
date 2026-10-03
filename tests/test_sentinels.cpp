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
        std::fread(buf, 1, sizeof(buf) - 1, f);
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
