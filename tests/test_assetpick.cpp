// tests/test_assetpick.cpp — F6: WIRING do seletor de textura do Inspector.
//
// REGRESSÃO DO C33 (0.6.9): após importar um PNG (import OK, ficheiro em
// textures/ do projeto) e tocar "tex:" no Inspector escolhendo a imagem,
// NADA acontecia — o estado permanecia "tex: none", o cubo ficava sem
// textura e o engine.log não registava linha de aplicação NEM de erro.
//
// CAUSA RAIZ (diagnóstico por leitura de código): drawAssetMenu fecha o
// seletor NO CLIQUE (st.assetMenu = 0 antes do return) e o dispatch do
// main.cpp lia g_editor.assetMenu DEPOIS da chamada → sempre 0 → o bloco
// `if (pick > 0)` era CÓDIGO MORTO desde a F5-E (0.5.0). A escolha nunca
// chegava ao MeshRenderer. O mesmo no seletor de mesh.
//
// Aqui aferimos o CONTRATO novo:
//   • escolher textura muda o ESTADO (texture + texPath) e devolve a linha
//     de log "material: textura aplicada <ref>" + toast;
//   • o warn do gate (texturas >2K) chega ao toast;
//   • carga que falha mantém o estado ANTERIOR intacto (sem estragar);
//   • remover (pick 1) liberta a referência + "material: textura removida";
//   • A SEQUÊNCIA DO MAIN (o fix): menuKind capturado ANTES do drawAssetMenu
//     com um clique REAL injetado (InputState) — o pick chega ao componente;
//   • mesh pick aplica mesh/material + textura embutida do glTF;
//   • TIC morto / sem MeshRenderer / pick fora do catálogo: sem crash;
//   • round-trip .goni preserva a referência aplicada (dump → loadText);
//   • stub de render confirma o BIND da textura (glBindTexture + uHasTex=1;
//     nullptr → uHasTex=0 SEM bind) — os contadores F6 do glstub.
#include "TestFramework.h"
#include "ui/EditorUi.h"
#include "ui/EditorLayout.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "platform/InputState.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "components/MeshRenderer.h"
#include "render/Cube.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/Texture.h"
#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats — F6: binds)

#include <cstdio>
#include <cstring>
#include <string>

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kFontPx = 28.0f;   // o MESMO heightPx do main.cpp no device

// sentinelas de estado (o padrão do test_serializer — identidade de
// ponteiro basta; o GL só entra no teste do BIND no fim do ficheiro)
Mesh* const kMeshStub  = reinterpret_cast<Mesh*>(0x10);
Mesh* const kCubeStub  = reinterpret_cast<Mesh*>(0x11);
Texture* const kTexA   = reinterpret_cast<Texture*>(0x30);
Texture* const kTexB   = reinterpret_cast<Texture*>(0x31);
LitMaterial* const kMatStub = reinterpret_cast<LitMaterial*>(0x20);

// ambiente: cena com o TIC do caso C33 (preset Player = Transform + MR +
// InputMap + Body) e catálogo com o PNG importado
struct PickEnv {
    Scene       scene;
    Handle      sel{};
    AssetCatalog cat;
    AssetResolvers res;

    PickEnv() {
        sel = createTicFromPreset(scene, PresetKind::PlayerBody3D, nullptr, nullptr);
        EXPECT(sel.valid());
    }

    MeshRenderer* mr() {
        Tic* t = scene.get(sel);
        return t ? t->getComponent<MeshRenderer>() : nullptr;
    }
};

} // namespace

// ---- 1. escolher textura aplica o ESTADO + log/toast -------------------------

TEST(assetpick_escolher_textura_aplica_estado_e_log) {
    PickEnv e;
    e.cat.textures = {"textures/wood.png"};
    e.res.texture = [](const std::string& ref, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return ref == "textures/wood.png" ? kTexA : nullptr;
    };

    const AssetPickOutcome out =
        applyAssetPick(e.scene, e.sel, /*menuKind=*/2, /*pick=*/2, e.cat, e.res);

    EXPECT(out.applied);
    EXPECT(e.mr() != nullptr);
    EXPECT(e.mr()->texture == kTexA);                        // referência ligada
    EXPECT(e.mr()->texPath == "textures/wood.png");         // ref relativa no material
    EXPECT(std::strcmp(out.log, "material: textura aplicada textures/wood.png") == 0);
    EXPECT(std::strcmp(out.toast, "textura aplicada") == 0);
}

// o nome REAL do C33 (screenshot-… importado) passa íntegro pela ref
TEST(assetpick_textura_screenshot_do_c33) {
    PickEnv e;
    e.cat.textures = {"textures/screenshot-20260930-1010.png"};
    e.res.texture = [](const std::string&, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return kTexA;
    };

    const AssetPickOutcome out =
        applyAssetPick(e.scene, e.sel, 2, 2, e.cat, e.res);

    EXPECT(out.applied);
    EXPECT(e.mr()->texPath == "textures/screenshot-20260930-1010.png");
    EXPECT(std::strcmp(out.log,
           "material: textura aplicada textures/screenshot-20260930-1010.png") == 0);
}

// ---- 2. warn do gate (texturas >2K) chega ao toast ---------------------------

TEST(assetpick_textura_com_aviso_do_gate_mostra_o_aviso) {
    PickEnv e;
    e.cat.textures = {"textures/big.png"};
    e.res.texture = [](const std::string&, std::string* warn) -> const Texture* {
        if (warn) { *warn = "textura >2K: sem compressao"; }
        return kTexA;
    };

    const AssetPickOutcome out =
        applyAssetPick(e.scene, e.sel, 2, 2, e.cat, e.res);

    EXPECT(out.applied);   // aplicou MESMO com aviso (comportamento F5.1-A)
    EXPECT(e.mr()->texture == kTexA);
    EXPECT(std::strcmp(out.toast, "textura >2K: sem compressao") == 0);
    EXPECT(std::strcmp(out.log, "material: textura aplicada textures/big.png") == 0);
}

// ---- 3. carga que falha mantém o estado ANTERIOR ----------------------------

TEST(assetpick_textura_que_falha_mantem_estado_anterior) {
    PickEnv e;
    e.cat.textures = {"textures/wood.png", "textures/corrompida.png"};
    // o TIC JÁ tinha uma textura aplicada
    e.mr()->texture = kTexB;
    e.mr()->texPath = "textures/old.png";
    e.res.texture = [](const std::string& ref, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return ref == "textures/old.png" ? kTexB : nullptr;   // nova falha
    };

    const AssetPickOutcome out =
        applyAssetPick(e.scene, e.sel, 2, 3 /*corrompida.png*/, e.cat, e.res);

    EXPECT(!out.applied);                              // a escolha não vingou
    EXPECT(e.mr()->texture == kTexB);                  // estado ANTERIOR intacto
    EXPECT(e.mr()->texPath == "textures/old.png");
    EXPECT(std::strcmp(out.toast, "falha ao carregar textura") == 0);
    EXPECT(std::strstr(out.log, "FALHOU") != nullptr); // causa no engine.log
    EXPECT(std::strstr(out.log, "corrompida.png") != nullptr);
}

// ---- 4. remover (pick 1) liberta a referência → "tex: none" ------------------

TEST(assetpick_remover_textura_volta_a_none) {
    PickEnv e;
    e.cat.textures = {"textures/wood.png"};
    e.res.texture = [](const std::string&, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return kTexA;
    };

    // aplica primeiro (o caminho feliz)
    EXPECT(applyAssetPick(e.scene, e.sel, 2, 2, e.cat, e.res).applied);
    EXPECT(e.mr()->texture == kTexA);

    // remover → liberta a referência
    const AssetPickOutcome out = applyAssetPick(e.scene, e.sel, 2, 1, e.cat, e.res);
    EXPECT(out.applied);
    EXPECT(e.mr()->texture == nullptr);        // referência libertada
    EXPECT(e.mr()->texPath.empty());          // Inspector volta a "tex: none"
    EXPECT(std::strcmp(out.toast, "tex: none") == 0);
    EXPECT(std::strcmp(out.log, "material: textura removida") == 0);

    // e aplicar de NOVO volta a funcionar (o ciclo não fica preso)
    const AssetPickOutcome again = applyAssetPick(e.scene, e.sel, 2, 2, e.cat, e.res);
    EXPECT(again.applied);
    EXPECT(e.mr()->texture == kTexA);
    EXPECT(e.mr()->texPath == "textures/wood.png");
}

// ---- 5. A SEQUÊNCIA DO MAIN — menuKind ANTES do drawAssetMenu ----------------
// (o fix: um clique REAL injetado fecha o seletor e o dispatch APLICA — o
// protocolo exato do main.cpp; antes do fix a escolha morria no caminho)

TEST(assetpick_sequencia_do_main_menukind_antes_do_draw) {
    FontAtlas font;
    const char* fp = FONT_FIXTURE;
    if (!font.loadFromPaths(&fp, 1, kFontPx)) {
        EXPECT(!"fonte fixture ausente");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState input;

    PickEnv e;
    e.cat.textures = {"textures/wood.png"};
    e.res.texture = [](const std::string&, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return kTexA;
    };

    EditorState st;
    st.selected = e.sel;
    st.assetMenu = 2;                       // seletor de TEXTURAS aberto
    const int menuKind = st.assetMenu;      // ← o FIX: capturado ANTES

    // geometria do seletor — as MESMAS fórmulas do drawAssetMenu (kMenuW
    // 340, header 48, linhas de 48 com botões de 40, cap 5)
    const f32 w = kMenuW;
    const f32 h = kHeaderH + (1.0f + 1.0f) * 48.0f + kPad;
    const f32 x = (kSW - w) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;
    const f32 bx = x + kPad + (w - 2.0f * kPad) * 0.5f;          // botão do ficheiro
    const f32 by = y + kHeaderH + 1.0f * 48.0f + 20.0f;          // (i=0 → centro)

    // frame 1: press dentro do botão do ficheiro (active_)
    input.injectDown(0, bx, by);
    ui.beginFrame(nullptr, &input, kSW, kSH);
    drawAssetMenu(ui, input, kSW, kSH, st, e.cat);
    ui.endFrame();
    input.clearEdges();
    EXPECT(st.assetMenu == 2);              // press não fecha (pressedOutside=fora)

    // frame 2: release → o clique vinga, o seletor fecha-se a si próprio
    input.injectUp(0);
    ui.beginFrame(nullptr, &input, kSW, kSH);
    const int pick = drawAssetMenu(ui, input, kSW, kSH, st, e.cat);
    ui.endFrame();
    input.clearEdges();

    EXPECT(pick == 2);                      // ficheiro 0 escolhido (1-based)
    EXPECT(st.assetMenu == 0);              // fechado no clique (o comportamento)

    // o dispatch corre DEPOIS do draw com o menuKind de ANTES — e chega
    // ao componente (o bloco antigo morria aqui: lia assetMenu == 0)
    const AssetPickOutcome out =
        applyAssetPick(e.scene, e.sel, menuKind, pick, e.cat, e.res);
    EXPECT(out.applied);
    EXPECT(e.mr() != nullptr);
    EXPECT(e.mr()->texture == kTexA);
    EXPECT(e.mr()->texPath == "textures/wood.png");
    EXPECT(std::strcmp(out.log, "material: textura aplicada textures/wood.png") == 0);
}

// ---- 6. mesh pick: aplica mesh/material + textura embutida do glTF ----------
TEST(assetpick_escolher_mesh_aplica_e_textura_embutida) {
    PickEnv e;
    e.cat.meshes = {"meshes/quad.gltf"};
    e.res.mesh = [](const std::string& ref) -> Mesh* {
        return ref == "meshes/quad.gltf" ? kMeshStub : nullptr;
    };
    e.res.meshTextureFor = [](const std::string& ref) -> std::string {
        return ref == "meshes/quad.gltf" ? std::string("textures/quad.png")
                                         : std::string();
    };
    e.res.texture = [](const std::string& ref, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return ref == "textures/quad.png" ? kTexA : nullptr;
    };
    e.res.material = kMatStub;

    const AssetPickOutcome out = applyAssetPick(e.scene, e.sel, 1, 3, e.cat, e.res);

    EXPECT(out.applied);
    EXPECT(e.mr()->mesh == kMeshStub);
    EXPECT(e.mr()->meshPath == "meshes/quad.gltf");
    EXPECT(e.mr()->material == kMatStub);
    EXPECT(e.mr()->texPath == "textures/quad.png");   // embutida aplicada logo
    EXPECT(e.mr()->texture == kTexA);
    EXPECT(std::strcmp(out.toast, "mesh aplicado (+textura)") == 0);
    EXPECT(std::strcmp(out.log,
           "editor: mesh meshes/quad.gltf aplicado com textura textures/quad.png") == 0);
}

TEST(assetpick_mesh_cube_procedural_limpa_a_ref) {
    PickEnv e;
    e.mr()->mesh = kMeshStub;
    e.mr()->meshPath = "meshes/quad.obj";   // tinha um asset importado
    e.res.cubeMesh = kCubeStub;
    e.res.material = kMatStub;

    // 0.8.12: cube é o pick 2 (none ocupa o 1º lugar do picker)
    const AssetPickOutcome out = applyAssetPick(e.scene, e.sel, 1, 2, e.cat, e.res);

    EXPECT(out.applied);
    EXPECT(e.mr()->mesh == kCubeStub);      // cubo procedural
    EXPECT(e.mr()->meshPath.empty());      // ref libertada
    EXPECT(e.mr()->material == kMatStub);
    EXPECT(std::strcmp(out.toast, "mesh: cube") == 0);
}

// ---- 0.8.12 — none DE PRIMEIRA CLASSE no picker de MESH ---------------------
TEST(assetpick_mesh_none_limpa_o_slot_com_deferred_free) {
    PickEnv e;
    e.mr()->mesh = kMeshStub;
    e.mr()->meshPath = "meshes/quad.obj";
    e.mr()->primRetire = nullptr;

    // pick 1 = none: o slot LIMPA (TIC deixa de renderizar mesh) e a
    // posse antiga vai para primRetire (deferred free no ponto seguro do
    // frame seguinte — o MESMO caminho seguro das trocas)
    const AssetPickOutcome out = applyAssetPick(e.scene, e.sel, 1, 1, e.cat, e.res);

    EXPECT(out.applied);
    EXPECT(e.mr()->mesh == nullptr);           // slot limpo
    EXPECT(e.mr()->material == nullptr);       // sem material
    EXPECT(e.mr()->meshPath.empty());          // ref libertada
    EXPECT(e.mr()->primOn == false);
    EXPECT(e.mr()->primPending == false);
    EXPECT(e.mr()->primRetire == kMeshStub);   // posse p/ cova (deferred free)
    EXPECT(std::strcmp(out.toast, "mesh: none") == 0);
    EXPECT(std::strstr(out.log, "mesh none") != nullptr);
    EXPECT(std::strstr(out.log, "deferred free") != nullptr);
}

TEST(assetpick_mesh_none_x_none_repetido_sem_crash) {
    PickEnv e;
    e.res.cubeMesh = kCubeStub;
    e.res.material = kMatStub;
    e.mr()->mesh = kCubeStub;   // estado inicial: cube

    // none→X→none ×5 (o ciclo do prompt: sem crash, sem leak, estado vazio
    // consistente no fim de cada none)
    for (int i = 0; i < 5; ++i) {
        AssetPickOutcome o1 = applyAssetPick(e.scene, e.sel, 1, 1, e.cat, e.res);   // none
        EXPECT(o1.applied);
        EXPECT(e.mr()->mesh == nullptr);
        EXPECT(e.mr()->primRetire == kCubeStub);   // posse p/ cova (enterra só o que é nosso)
        AssetPickOutcome o2 = applyAssetPick(e.scene, e.sel, 1, 2, e.cat, e.res);   // cube
        EXPECT(o2.applied);
        EXPECT(e.mr()->mesh == kCubeStub);
        AssetPickOutcome o3 = applyAssetPick(e.scene, e.sel, 1, 1, e.cat, e.res);   // none
        EXPECT(o3.applied);
        EXPECT(e.mr()->mesh == nullptr);
        EXPECT(e.mr()->meshPath.empty());
    }
    // estágio final: vazio (o ciclo termina em none; a posse pendente é o
    // ponteiro anterior — a COVA decide o que é nosso no ponto seguro)
    EXPECT(e.mr()->primRetire == kCubeStub);
}

// ---- 7. alvos inválidos: sem crash, sem ação ---------------------------------

TEST(assetpick_alvos_invalidos_sao_ignorados_sem_crash) {
    PickEnv e;
    e.cat.textures = {"textures/wood.png"};
    e.res.texture = [](const std::string&, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return kTexA;
    };

    // handle morto
    const Handle invalid{};
    AssetPickOutcome out = applyAssetPick(e.scene, invalid, 2, 2, e.cat, e.res);
    EXPECT(!out.applied);
    EXPECT(out.toast[0] == '\0' && out.log[0] == '\0');

    // pick inválido (nenhum neste frame)
    out = applyAssetPick(e.scene, e.sel, 2, 0, e.cat, e.res);
    EXPECT(!out.applied);

    // TIC SEM MeshRenderer (criado nu, sem preset)
    Scene s2;
    const Handle h2 = s2.create("sem-mr");
    out = applyAssetPick(s2, h2, 2, 2, e.cat, e.res);
    EXPECT(!out.applied);

    // pick fora do catálogo (índice além do fim; catálogo de 1 → 3 é demais)
    out = applyAssetPick(e.scene, e.sel, 2, 3, e.cat, e.res);
    EXPECT(!out.applied);
    EXPECT(e.mr()->texPath.empty());        // nada mudou
    EXPECT(e.mr()->texture == nullptr);

    // menuKind desconhecido: sem ação, sem crash
    out = applyAssetPick(e.scene, e.sel, 7, 2, e.cat, e.res);
    EXPECT(!out.applied);
}

// ---- 8. round-trip .goni preserva a referência aplicada ----------------------

TEST(assetpick_roundtrip_goni_preserva_a_referencia) {
    PickEnv e;
    e.cat.textures = {"textures/wood.png"};
    e.res.texture = [](const std::string&, std::string* warn) -> const Texture* {
        if (warn) { warn->clear(); }
        return kTexA;
    };

    // escolher → estado
    EXPECT(applyAssetPick(e.scene, e.sel, 2, 2, e.cat, e.res).applied);
    EXPECT(e.mr()->texPath == "textures/wood.png");

    // Save → Load (dump/loadText — o caminho do .goni)
    const std::string text = SceneSerializer::dump(e.scene);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ctx.cubeMesh = kCubeStub;
    ctx.material = kMatStub;
    ctx.resolveTex = [](const std::string& ref) -> const Texture* {
        return ref == "textures/wood.png" ? kTexB : nullptr;
    };
    if (!SceneSerializer::loadText(s2, text, ctx)) {
        EXPECT(!"loadText falhou");
        return;
    }

    const Handle h2 = s2.find("PlayerBody3D");
    if (!h2.valid()) {
        EXPECT(!"TIC não recarregado");
        return;
    }
    MeshRenderer* mr2 = s2.get(h2)->getComponent<MeshRenderer>();
    EXPECT(mr2 != nullptr);
    EXPECT(mr2->texPath == "textures/wood.png");   // ref preservada no .goni
    EXPECT(mr2->texture == kTexB);                 // re-ligada pelo resolver do load
}

// ---- 9. stub de render confirma o BIND da textura aplicada ---------------------

TEST(assetpick_render_binda_a_textura_aplicada) {
    glstub::reset();
    Renderer r;
    if (!r.init()) {
        EXPECT(!"Renderer::init falhou (stub)");
        return;
    }

    Texture tex;
    const u8 rgba[2 * 2 * 4] = {
        0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
        0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,
    };
    if (!tex.createFromRGBA(rgba, 2, 2)) {
        EXPECT(!"upload da textura falhou (stub)");
        return;
    }

    Mesh mesh;
    {
        const CubeMeshData cube = makeCube(1.0f);
        if (!mesh.create(cube.vertices.data(),
                        static_cast<u32>(cube.vertices.size()),
                        cube.indices.data(),
                        static_cast<u32>(cube.indices.size()))) {
            EXPECT(!"upload do mesh falhou (stub)");
            return;
        }
    }

    // o draw do TIC: drawMesh(mesh, model, vp, mr->texture) — com a
    // textura APLICADA o material binda o id certo e acende uHasTex
    glstub::reset();   // só o draw interessa (init/create ficam fora)
    r.drawMesh(mesh, Mat4::identity(), Mat4::identity(), &tex);
    EXPECT(glstub::stats.boundTextures >= 1);            // houve bind
    EXPECT(glstub::stats.lastBoundTexture == tex.handle());
    EXPECT(nearEqF(glstub::stats.lastUniform1f, 1.0f));  // uHasTex = 1

    // sem textura (tex: none) → uHasTex = 0 e NENHUM bind novo
    glstub::reset();
    r.drawMesh(mesh, Mat4::identity(), Mat4::identity(), nullptr);
    EXPECT(glstub::stats.boundTextures == 0);             // nada bindado
    EXPECT(nearEqF(glstub::stats.lastUniform1f, 0.0f)); // cubo cinzento
}
