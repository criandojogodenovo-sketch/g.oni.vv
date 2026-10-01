// tests/test_browser.cpp — 0.7.2: IMPORT ROBUSTO (navegador + galeria +
// aplicar-após-import).
//
// Aferimos o CONTRATO da sub-fase:
//   • FileApi::listDirEntries: diretorias PRIMEIRO (ordenadas), ficheiros
//     só os suportados (.obj/.gltf/.glb/.png case-insensitive), caminhos
//     absolutos; opendir falho → false (mensagem COM O CAMINHO, não cega);
//   • parentPath: subir para o pai; raiz fica na raiz;
//   • raízes navegáveis: a GALERIA (DCIM/Camera e Pictures) está incluída;
//   • o CAMINHO é VISÍVEL: browserPathLabel devolve o caminho inteiro
//     quando cabe e corta o INÍCIO quando não (guarda o FIM — é onde o
//     dono está); browserEmptyMessage traz o caminho (nunca toast cego);
//   • o overlay NAVEGADOR: raízes 1..5 / subir 6 / entradas 7+; a lista
//     desenha as entradas (diretorias com "/", ficheiros "mesh:"/"tex:");
//     pasta vazia mostra a mensagem COM O CAMINHO; fora fecha;
//   • APLICAR-APÓS-IMPORT: o diálogo aparece com o nome do ficheiro e do
//     TIC; Sim aplica via applyAssetPick (o MESMO caminho do seletor —
//     textura liga texture+texPath com os resolvers); Nao não mexe;
//   • e2e do import: readAll → writeBytes no storage → catálogo → apply
//     (o fluxo do browserImportFile do main, peça a peça).
#include "TestFramework.h"
#include <cstdio>
#include <string>
#include <vector>
#include "FakeStorage.h"
#include "core/Presets.h"
#include "core/Project.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/FileApi.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {
constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

// árvore de teste em /tmp (criada/limpa por caso)
struct TempTree {
    std::string root;
    explicit TempTree(const char* sub) {
        char tmpl[] = "/tmp/goni-browser-XXXXXX";
        const char* mkd = mkdtemp(tmpl);
        root = mkd ? std::string(mkd) + "/" + sub : "/tmp/goni-browser";
        fileapi::makeDirs(root);
        fileapi::makeDirs(root + "/zdir");
        fileapi::makeDirs(root + "/adir/sub");
        fileapi::writeAll(root + "/modelo.obj", "o 1", 3);
        fileapi::writeAll(root + "/Foto.PNG", "png", 3);   // case-insensitive
        fileapi::writeAll(root + "/notas.txt", "x", 1);    // não suportado
        fileapi::writeAll(root + "/adir/sub/casa.gltf", "g", 1);
        fileapi::writeAll(root + "/zdir/textura.png", "t", 1);
    }
    ~TempTree() {
        std::remove((root + "/adir/sub/casa.gltf").c_str());
        std::remove((root + "/zdir/textura.png").c_str());
        std::remove((root + "/adir/sub").c_str());
        std::remove((root + "/adir").c_str());
        std::remove((root + "/zdir").c_str());
        std::remove((root + "/modelo.obj").c_str());
        std::remove((root + "/Foto.PNG").c_str());
        std::remove((root + "/notas.txt").c_str());
        std::remove(root.c_str());
        std::remove(root.substr(0, root.size() - 6).c_str());   // /tmp/goni-browser-XXXXXX
    }
};
} // namespace

// ---- 1. FileApi: listagem do navegador ------------------------------------------

TEST(browser_listdir_entries_ordenado_e_filtrado) {
    TempTree t("b1");
    std::vector<fileapi::DirEntry> out;
    if (!fileapi::listDirEntries(t.root, out)) {
        EXPECT(!"opendir falhou na árvore de teste");
        return;
    }
    // esperado: DIRETORIAS primeiro (adir, zdir — ordenadas), depois TODOS
    // os ficheiros (0.8.5: o não suportado entra com kind 0 — o dono vê o
    // .txt/.fbx e, ao tocar, recebe o erro claro; antes: fora = silêncio)
    ASSERT_SIZE:;
    EXPECT(out.size() == 5u);
    if (out.size() < 5) {
        return;
    }
    EXPECT(out[0].isDir && out[0].name == "adir");
    EXPECT(out[1].isDir && out[1].name == "zdir");
    EXPECT(!out[2].isDir && out[2].kind == 't' && out[2].name == "Foto.PNG");
    EXPECT(!out[3].isDir && out[3].kind == 'm' && out[3].name == "modelo.obj");
    EXPECT(!out[4].isDir && out[4].kind == 0 && out[4].name == "notas.txt");
    // paths absolutos montados
    EXPECT(out[0].path == t.root + "/adir");
    // SUBPASTA: o gltf aparece (o browser desce pastas dentro de pastas)
    std::vector<fileapi::DirEntry> sub;
    EXPECT(fileapi::listDirEntries(t.root + "/adir/sub", sub));
    EXPECT(sub.size() == 1u);
    EXPECT(sub[0].kind == 'm' && sub[0].name == "casa.gltf");
    // pasta INEXISTENTE → false (o chamador mostra o caminho)
    std::vector<fileapi::DirEntry> none;
    EXPECT(!fileapi::listDirEntries("/tmp/nao-existe-goni", none));
    EXPECT(none.empty());
}

TEST(browser_parent_path) {
    EXPECT(fileapi::parentPath("/storage/emulated/0/DCIM/Camera") ==
           "/storage/emulated/0/DCIM");
    EXPECT(fileapi::parentPath("/storage/emulated/0") ==
           "/storage/emulated");
    EXPECT(fileapi::parentPath("/") == "/");   // raiz fica na raiz
    EXPECT(fileapi::parentPath("") == "/");
}

TEST(browser_raizes_incluem_a_galeria) {
    // all-files + Download/Documents + A GALERIA (DCIM/Camera, Pictures)
    EXPECT(fileapi::kBrowserRootCount == 5);
    bool hasCamera = false, hasPictures = false, hasRoot = false;
    for (int i = 0; i < fileapi::kBrowserRootCount; ++i) {
        const std::string p = fileapi::kBrowserRoots[i].path;
        hasCamera = hasCamera || p == "/storage/emulated/0/DCIM/Camera";
        hasPictures = hasPictures || p == "/storage/emulated/0/Pictures";
        hasRoot = hasRoot || p == fileapi::kExternalRoot;
        EXPECT(fileapi::kBrowserRoots[i].label != nullptr);
        EXPECT(!p.empty() && p[0] == '/');
    }
    EXPECT(hasCamera);
    EXPECT(hasPictures);
    EXPECT(hasRoot);
}

// ---- 2. o caminho é VISÍVEL (nunca toast cego) ------------------------------------

TEST(browser_path_label_come_o_inicio_guarda_o_fim) {
    // medida fake determinística: 10 px por char (o suficiente p/ aferir)
    auto measure = [](const std::string& s, void*) -> f32 {
        return static_cast<f32>(s.size() * 10.0f);
    };
    // cabe inteiro
    EXPECT(browserPathLabel("/storage/emulated/0/DCIM/Camera", 500.0f,
                            measure, nullptr) ==
           "/storage/emulated/0/DCIM/Camera");
    // não cabe (270px de texto, 200 disponíveis): corta o INÍCIO, guarda o FIM
    const std::string cut = browserPathLabel(
        "/storage/emulated/0/DCIM/Camera", 200.0f, measure, nullptr);
    EXPECT(cut.size() < 27u);                        // cortou
    EXPECT(cut.rfind("...", 0) == 0);                // com prefixo ...
    EXPECT(cut.find("DCIM/Camera") != std::string::npos);   // o FIM ficou
    // o rótulo nunca excede a largura
    EXPECT(measure(cut, nullptr) <= 200.0f);
    // caminho curto em largura curta: também corta (nunca transborda)
    const std::string cut2 = browserPathLabel("/a/b/c", 20.0f, measure, nullptr);
    EXPECT(measure(cut2, nullptr) <= 20.0f);
}

TEST(browser_mensagem_vazia_tem_o_caminho) {
    const std::string vazio =
        browserEmptyMessage("/storage/emulated/0/Download/pasta", false);
    EXPECT(vazio.find("(vazio)") != std::string::npos);
    EXPECT(vazio.find("/storage/emulated/0/Download/pasta") !=
           std::string::npos);   // O CAMINHO — nunca um toast cego
    const std::string falhou =
        browserEmptyMessage("/storage/emulated/0/X", true);
    EXPECT(falhou.find("(sem acesso)") != std::string::npos);
    EXPECT(falhou.find("/storage/emulated/0/X") != std::string::npos);
}

// ---- 3. o overlay NAVEGADOR --------------------------------------------------------

TEST(browser_overlay_raizes_subir_e_entradas) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState in;
    EditorState st;

    std::vector<fileapi::DirEntry> entries = {
        {"adir", "/x/adir", true, 0},
        {"modelo.obj", "/x/modelo.obj", false, 'm'},
        {"foto.png", "/x/foto.png", false, 't'},
    };

    auto frame = [&]() {
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int pick =
            drawFileBrowser(ui, in, kSW, kSH, st, "/storage/emulated/0/x",
                            entries, false);
        ui.endFrame();
        in.clearEdges();
        return pick;
    };
    auto tap = [&](f32 x, f32 y) {
        in.injectDown(0, x, y);
        frame();
        in.injectUp(0);
        return frame();
    };

    st.fileBrowser = true;
    EXPECT(frame() == 0);   // aberto, sem toque

    // geometria (a MESMA do draw): painel 92% cap 900, header 48, path 34,
    // raízes 52+12, subir 52, listTop
    const f32 w = 900.0f;
    const f32 h = kHeaderH + 34.0f + 52.0f + 8.0f + 52.0f + 8.0f +
                  3.0f * 48.0f + kPad;
    const f32 x = (kSW - w) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;
    const f32 pathY = y + kHeaderH + 6.0f;
    const f32 rootsY = pathY + 34.0f - 18.0f + 12.0f;
    const f32 upY = rootsY + 52.0f;
    const f32 listTop = upY + 52.0f;

    // raiz 3 (Docs) → pick 3; raiz Camera (4) → pick 4
    const f32 rootW = (w - 2.0f * kPad - 4.0f * 6.0f) / 5.0f;
    EXPECT(tap(x + kPad + 2.0f * (rootW + 6.0f) + rootW * 0.5f,
               rootsY + 20.0f) == 3);
    st.fileBrowser = true;
    EXPECT(tap(x + kPad + 3.0f * (rootW + 6.0f) + rootW * 0.5f,
               rootsY + 20.0f) == 4);

    // subir → 6
    st.fileBrowser = true;
    EXPECT(tap(x + w * 0.5f, upY + 22.0f) == 6);

    // entrada 0 (diretoria adir) → 7; entrada 2 (foto.png) → 9
    st.fileBrowser = true;
    EXPECT(tap(x + w * 0.5f, listTop + 0.0f * 48.0f + 24.0f) == 7);
    st.fileBrowser = true;
    EXPECT(tap(x + w * 0.5f, listTop + 2.0f * 48.0f + 24.0f) == 9);

    // fora fecha SEM escolha
    st.fileBrowser = true;
    EXPECT(tap(60.0f, 60.0f) == 0);
    EXPECT(!st.fileBrowser);

    // pasta VAZIA: a mensagem COM O CAMINHO substitui a lista
    st.fileBrowser = true;
    const std::vector<fileapi::DirEntry> vazio;
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawFileBrowser(ui, in, kSW, kSH, st, "/storage/emulated/0/vazia", vazio,
                    false);
    ui.endFrame();
    // (a mensagem é desenhada dentro; aferimos o TEXTO pelo helper + o
    // overlay não crasha com a lista vazia)
    EXPECT(ui.solidsForTest().vertexCount() > 0);
}

// ---- 4. aplicar-após-import ----------------------------------------------------------

TEST(browser_apos_import_dialogo_sim_aplica_nao_nao_mexe) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState in;
    EditorState st;

    // cena com o TIC do caso: preset Player (tem MeshRenderer)
    Scene scene;
    AssetCatalog catalog;
    catalog.textures = {"foto.png"};
    const Handle h = createTicFromPreset(scene, PresetKind::PlayerBody3D,
                                         nullptr, nullptr);
    st.selected = h;
    Tic* tic = scene.get(h);
    MeshRenderer* mr = tic ? tic->getComponent<MeshRenderer>() : nullptr;
    EXPECT(mr != nullptr);

    // resolvers de teste (o padrão do test_assetpick — ponteiros puros;
    // a sentinela é uma Texture real do stub)
    Texture texA;
    AssetResolvers res;
    res.texture = [](const std::string& ref,
                    std::string* warn) -> const Texture* {
        (void)ref;
        (void)warn;
        static Texture sentinel;   // estático: o ponteiro-puro precisa
        return &sentinel;
    };
    res.material = reinterpret_cast<LitMaterial*>(0x20);
    const Texture* sentTex = res.texture("textures/foto.png", nullptr);

    auto frame = [&]() {
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int ch = drawApplyDialog(ui, in, kSW, kSH, st, "foto.png",
                                       tic->name.c_str());
        ui.endFrame();
        in.clearEdges();
        return ch;
    };
    auto tap = [&](f32 x, f32 y) {
        in.injectDown(0, x, y);
        frame();
        in.injectUp(0);
        return frame();
    };

    // NAO: o import fica, o MeshRenderer NÃO mexe
    st.applyAsk = true;
    const f32 h2 = storageDialogHeight();
    const UiRect dlg = centeredMenuRect(0.0f, 0.0f, kSW, kSH, h2);
    UiRect yes{}, no{};
    storageDialogButtons(dlg, yes, no);
    EXPECT(tap(no.x + no.w * 0.5f, no.y + no.h * 0.5f) == 2);
    EXPECT(!st.applyAsk);
    EXPECT(mr->texture == nullptr);
    EXPECT(mr->texPath.empty());

    // SIM: o main chama applyAssetPick com o MESMO caminho do seletor —
    // o pick 2 (item 1 do catálogo de texturas) liga texture + texPath
    st.applyAsk = true;
    const AssetPickOutcome out = applyAssetPick(scene, h, 2, 2, catalog, res);
    EXPECT(out.applied);
    EXPECT(mr->texture == sentTex);
    EXPECT(mr->texPath == "textures/foto.png");
    EXPECT(std::string(out.log).find("textures/foto.png") !=
           std::string::npos);
}

// ---- 5. e2e do import: leitura → projeto → catálogo → apply ---------------------------

TEST(browser_import_e2e_do_ficheiro_ao_tic) {
    TempTree t("b2");
    FakeStorage storage;
    Project p;
    if (!Project::openOrCreate(storage, "p", p)) {
        EXPECT(!"openOrCreate falhou");
        return;
    }

    // 1) o navegador lista a pasta com o PNG
    std::vector<fileapi::DirEntry> entries;
    EXPECT(fileapi::listDirEntries(t.root + "/zdir", entries));
    ASSERT_ENTRY:;
    if (entries.size() != 1u) {
        EXPECT(!"esperava 1 entrada (textura.png)");
        return;
    }
    EXPECT(entries[0].kind == 't' && entries[0].name == "textura.png");

    // 2) o IMPORT do main: readAll → writeBytes no projeto
    std::vector<u8> bytes;
    EXPECT(fileapi::readAll(entries[0].path, bytes));
    const std::string rel = "textures/textura.png";
    EXPECT(storage.writeBytes(rel, bytes.data(), bytes.size()));

    // 3) refreshCatalog (listDir + filtro por extensão — como no main)
    AssetCatalog catalog;
    std::vector<std::string> files;
    EXPECT(storage.listDir(Project::kDirTextures, files));
    for (const std::string& f : files) {
        if (fileapi::kindOfExtension(f) == 't') {
            catalog.textures.push_back(f);
        }
    }
    EXPECT(catalog.textures.size() == 1u);
    EXPECT(catalog.textures[0] == "textura.png");

    // 4) aplicar-após-import (Sim): o MESMO applyAssetPick do seletor
    Scene scene;
    const Handle h = createTicFromPreset(scene, PresetKind::StaticBody3D,
                                         nullptr, nullptr);
    Tic* tic = scene.get(h);
    MeshRenderer* mr = tic ? tic->getComponent<MeshRenderer>() : nullptr;
    EXPECT(mr != nullptr);
    AssetResolvers res;
    res.texture = [](const std::string&, std::string*) -> const Texture* {
        static Texture sentinel;
        return &sentinel;
    };
    const Texture* sentTex = res.texture("", nullptr);
    const AssetPickOutcome out = applyAssetPick(scene, h, 2, 2, catalog, res);
    EXPECT(out.applied);
    EXPECT(mr->texPath == rel);
    EXPECT(mr->texture == sentTex);
}
