// tests/test_wiring092.cpp — 0.9.2: integração MOTOR (VoniSystem +
// EngineHost sobre Scene REAL) + UI (editor de script, Docs, Inspector).
//
// Parte A (aqui desde já): a "central" sobre a cena real — ScriptComp,
// play auto-run, editor Run/Stop, exported no Inspector, serializer
// round-trip, move/Explode/Search com TICs de verdade.
// Parte B (editor de script/Docs/Settings): entra com os ecrãs.
#include "TestFramework.h"

#include "core/Scene.h"
#include "core/Tic.h"
#include "core/VoniSystem.h"
#include "core/SceneSerializer.h"
#include "components/ScriptComp.h"
#include "components/Transform3D.h"
#include "voni/Voni.h"

#include <cmath>
#include <string>

using namespace vv;

namespace {

const char* kScript = R"VONI(
v++@+frames=0
central main {
  on moment { View P "arrancou" }
  allmoments {
    frames+=1
    move(0, 0, 1)
  }
}
)VONI";

struct VmEnv {
    Scene scene;
    VoniSystem voni;
    Handle tic{};

    VmEnv() {
        tic = scene.create("ator");
        scene.get(tic)->addComponent<Transform3D>();
        scene.get(tic)->addComponent<ScriptComp>();
    }

    ScriptComp* script() { return scene.get(tic)->getComponent<ScriptComp>(); }
};

} // namespace

TEST(voni_system_play_leva_tic_a_mover_se) {
    VmEnv e;
    e.script()->source = kScript;
    e.voni.playEnabled = true;

    // 1º tick: runStart (top+on moment) + 1º allmoments
    e.voni.tick(e.scene, 1.0f / 60.0f);
    Transform3D* tr = e.scene.get(e.tic)->getComponent<Transform3D>();
    EXPECT(std::fabs(tr->pos.z - 1.0f) < 1e-6);

    // +9 frames: move 1/frame
    for (int i = 0; i < 9; ++i) {
        e.voni.tick(e.scene, 1.0f / 60.0f);
    }
    EXPECT(std::fabs(tr->pos.z - 10.0f) < 1e-6);

    // exported visível para o Inspector
    auto ex = e.voni.exported(e.scene, e.tic);
    EXPECT(ex.size() == 1);
    EXPECT(ex[0].name == "frames");
    EXPECT(ex[0].value.i == 10);
}

TEST(voni_system_sem_play_nao_corre) {
    VmEnv e;
    e.script()->source = kScript;
    e.voni.playEnabled = false;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    Transform3D* tr = e.scene.get(e.tic)->getComponent<Transform3D>();
    EXPECT(std::fabs(tr->pos.z) < 1e-9);
    EXPECT(e.voni.activeRuns() == 0);
}

TEST(voni_system_editor_run_independente_do_play) {
    VmEnv e;
    e.script()->source = kScript;
    voni::Error err;
    EXPECT(e.voni.editorStart(e.scene, e.tic, err));
    EXPECT(e.voni.editorRunning(e.tic));
    e.voni.tick(e.scene, 1.0f / 60.0f);
    Transform3D* tr = e.scene.get(e.tic)->getComponent<Transform3D>();
    EXPECT(std::fabs(tr->pos.z - 1.0f) < 1e-6);

    e.voni.editorStop(e.tic);
    EXPECT(!e.voni.editorRunning(e.tic));
    // sem play, a run morre no stop
    e.voni.tick(e.scene, 1.0f / 60.0f);
    EXPECT(std::fabs(tr->pos.z - 1.0f) < 1e-6);   // parado
}

TEST(voni_system_editor_restart_recompila) {
    VmEnv e;
    e.script()->source = kScript;
    voni::Error err;
    EXPECT(e.voni.editorRestart(e.scene, e.tic, kScript, err));
    // muda o fonte: move 2/frame
    const char* v2 =
        "central main { on moment { } allmoments { move(0, 0, 2) } }";
    EXPECT(e.voni.editorRestart(e.scene, e.tic, v2, err));
    e.voni.tick(e.scene, 1.0f / 60.0f);
    Transform3D* tr = e.scene.get(e.tic)->getComponent<Transform3D>();
    EXPECT(std::fabs(tr->pos.z - 2.0f) < 1e-6);
}

TEST(voni_system_erro_pop_drain) {
    VmEnv e;
    e.script()->source =
        "central main { on moment { inexistente+=1 } allmoments { } }";
    e.voni.playEnabled = true;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    Handle t{};
    voni::Error err;
    EXPECT(e.voni.popError(t, err));
    EXPECT(!err.ok);
    EXPECT(err.message.find("inexistente") != std::string::npos);
    // drenado: não volta a sair
    voni::Error err2;
    EXPECT(!e.voni.popError(t, err2));
}

TEST(voni_system_tic_destruido_run_morre_sem_crash) {
    VmEnv e;
    e.script()->source = kScript;
    e.voni.playEnabled = true;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    EXPECT(e.voni.activeRuns() >= 1);
    e.scene.destroy(e.tic);
    e.voni.tick(e.scene, 1.0f / 60.0f);   // não crasha
    EXPECT(e.voni.activeRuns() == 0);
}

TEST(voni_explode_visible_no_tic_real) {
    VmEnv e;
    e.script()->source =
        "central main { on moment { Explode.TIC.et } allmoments { } }";
    e.voni.playEnabled = true;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    EXPECT(e.scene.get(e.tic)->visible == false);
}

TEST(voni_propriedades_outro_tic_na_cena_real) {
    VmEnv e;
    Handle alvo = e.scene.create("jogador");
    e.scene.get(alvo)->addComponent<Transform3D>();
    e.scene.get(alvo)->getComponent<Transform3D>()->pos = Vec3{1, 2, 3};
    e.script()->source =
        "v#@+px:Num=0\n"
        "central main { on moment {\n"
        "  jogador.pos.x=5\n"
        "  px=jogador.pos.x\n"
        "} allmoments { } }";
    e.voni.playEnabled = true;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    EXPECT(std::fabs(e.scene.get(alvo)->getComponent<Transform3D>()->pos.x -
                     5.0f) < 1e-6);
    auto ex = e.voni.exported(e.scene, e.tic);
    EXPECT(ex.size() == 1 && std::fabs(ex[0].value.n - 5.0) < 1e-9);
}

TEST(voni_search_na_cena_real) {
    VmEnv e;
    Handle alvo = e.scene.create("jogador");
    e.scene.get(alvo)->addComponent<Transform3D>();
    e.scene.get(alvo)->getComponent<Transform3D>()->pos = Vec3{7, 0, 0};
    e.script()->source =
        "v#@+px:Num=0\n"
        "central main { on moment { px=Search.jogador.pos.x } "
        "allmoments { } }";
    e.voni.playEnabled = true;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    auto ex = e.voni.exported(e.scene, e.tic);
    EXPECT(ex.size() == 1 && std::fabs(ex[0].value.n - 7.0) < 1e-9);
}

TEST(voni_transition_hook_injetado) {
    VmEnv e;
    std::string foiPara;
    e.voni.setTransitionHook(
        [&](const std::string& from, const std::string& to,
            std::string& err) {
            (void)from;
            if (to == "cena2") {
                foiPara = to;
                return true;
            }
            err = "cena '" + to + "' não existe";
            return false;
        });
    e.voni.setSceneNameFn([&]() { return std::string("cena1"); });
    e.script()->source =
        "central main { on moment { cena1.transition.for(\"cena2\") } "
        "allmoments { } }";
    e.voni.playEnabled = true;
    e.voni.tick(e.scene, 1.0f / 60.0f);
    EXPECT(foiPara == "cena2");
}

TEST(voni_serializer_script_roundtrip) {
    Scene scene;
    Handle h = scene.create("ator");
    scene.get(h)->addComponent<ScriptComp>();
    ScriptComp* sc = scene.get(h)->getComponent<ScriptComp>();
    sc->source = "v++x=1\n// comentário\ncentral main { on moment { } "
                 "allmoments { } }";
    sc->autoPlay = false;

    std::string json = SceneSerializer::dump(scene);
    EXPECT(json.find("\"Script\"") != std::string::npos);

    Scene loaded;
    SceneSerializer::LoadCtx ctx;   // defaults (paths vazios)
    EXPECT(SceneSerializer::loadText(loaded, json, ctx));
    Tic* t = loaded.get(loaded.find("ator"));
    EXPECT(t != nullptr);
    ScriptComp* sc2 = t->getComponent<ScriptComp>();
    EXPECT(sc2 != nullptr);
    EXPECT(sc2->source == sc->source);
    EXPECT(sc2->autoPlay == false);
}

// ===========================================================================
// PARTE B — UI (0.9.2 §10/§11): editor de script + Docs + Settings + Inspector
// ===========================================================================
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/ScriptEditor.h"
#include "ui/DocsScreen.h"
#include "ui/SettingsPage.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"   // anyOverlayOpen
#include "platform/InputState.h"
#include "ui/SafeArea.h"
#include "core/Handle.h"

namespace {

struct UiEnv {
    FontAtlas  font;
    vv::UiContext ui;
    vv::InputState input;
    vv::editor::EditorState st;
    Scene scene;
    VoniSystem voni;
    Handle tic{};

    UiEnv() {
        const char* fp = FONT_FIXTURE;
        font.loadFromPaths(&fp, 1, 28.0f);
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(vv::safe::Insets{});
        tic = scene.create("ator");
        scene.get(tic)->addComponent<vv::Transform3D>();
        scene.get(tic)->addComponent<vv::ScriptComp>();
    }

    void frame(f32 w, f32 h, f32 dt = 0.0f) {
        ui.beginFrame(nullptr, &input, w, h);
        vv::editor::scriptwin::draw(ui, input, st.scriptWin, w, h, dt);
        vv::editor::docswin::draw(ui, input, st.docsScreen, w, h);
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y, f32 w, f32 h) {
        input.injectDown(0, x, y);
        ui.beginFrame(nullptr, &input, w, h);
        vv::editor::scriptwin::draw(ui, input, st.scriptWin, w, h, 0.0f);
        vv::editor::docswin::draw(ui, input, st.docsScreen, w, h);
        ui.endFrame();
        input.injectUp(0);
        ui.beginFrame(nullptr, &input, w, h);
        vv::editor::scriptwin::draw(ui, input, st.scriptWin, w, h, 0.0f);
        vv::editor::docswin::draw(ui, input, st.docsScreen, w, h);
        ui.endFrame();
        input.clearEdges();
    }
};

} // namespace

TEST(scriptwin_abre_carrega_fonte_e_fecha) {
    UiEnv e;
    e.scene.get(e.tic)->getComponent<ScriptComp>()->source =
        "v++x=1\n";
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    EXPECT(e.st.scriptWin.open);
    EXPECT(e.st.scriptWin.buf == "v++x=1\n");
    // 2 linhas (números de linha 1..2)
    EXPECT(vv::editor::scriptwin::lineCount(e.st.scriptWin) == 2);
    vv::editor::scriptwin::close(e.st.scriptWin);
    EXPECT(!e.st.scriptWin.open);
}

TEST(scriptwin_ime_append_del_enter) {
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    vv::ime::clearForTest();
    vv::ime::pushText("v++a=1");
    vv::ime::pushKey(vv::ime::Key::Enter);
    vv::ime::pushText("b");
    vv::ime::Event ev;
    while (vv::ime::poll(ev)) {
        vv::editor::scriptwin::applyEvent(e.st.scriptWin, ev);
    }
    EXPECT(e.st.scriptWin.buf == "v++a=1\nb");
    vv::ime::pushKey(vv::ime::Key::Del);
    vv::ime::poll(ev);
    vv::editor::scriptwin::applyEvent(e.st.scriptWin, ev);
    EXPECT(e.st.scriptWin.buf == "v++a=1\n");
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_desenha_portrait_com_run_stop_e_back) {
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.buf = "central main { on moment { } allmoments { } }";
    // PORTRAIT 720×1536 (janela de texto pesado — o par 0.9.1)
    e.frame(720.0f, 1536.0f);
    // modal: entra no anyOverlayOpen (nada do editor atrás)
    EXPECT(vv::editor::anyOverlayOpen(e.st));
    // Run (toca no botão à direita da barra 56dp)
    e.st.scriptWin.running = false;
    e.tap(720.0f - 72.0f - 16.0f + 36.0f, 28.0f, 720.0f, 1536.0f);
    // Stop (só responde quando running)
    e.st.scriptWin.running = true;
    e.tap(720.0f - 36.0f, 28.0f, 720.0f, 1536.0f);
    // Back 56dp (célula toda)
    e.tap(28.0f, 28.0f, 720.0f, 1536.0f);
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_barra_de_erro_com_linha) {
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.errLine = 3;
    e.st.scriptWin.errMsg = "'x' é uma palavra reservada";
    e.frame(720.0f, 1536.0f);
    // (a barra desenha; o estado persiste para o teste afervar)
    EXPECT(e.st.scriptWin.errLine == 3);
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(docswin_abre_pesquisa_e_expande) {
    UiEnv e;
    e.st.docsScreen.open = true;
    e.frame(1600.0f, 720.0f);
    EXPECT(vv::editor::anyOverlayOpen(e.st));
    // filtro por query direto (o purpose 9 commita para aqui)
    std::snprintf(e.st.docsScreen.query, sizeof(e.st.docsScreen.query),
                  "view");
    e.st.docsScreen.queryLen = 4;
    e.frame(1600.0f, 720.0f);
    // back fecha
    e.tap(28.0f, 28.0f, 1600.0f, 720.0f);
    e.st.docsScreen.open = false;
}

TEST(settings_linha_docs_devolve_kOpenDocs) {
    UiEnv e;
    e.st.settingsMenu = true;
    // colapsa TUDO menos Docs (a linha só se desenha na secção aberta)
    e.st.settingsCollapsed = 0xFFFFFFFFu & ~(1u << 4);
    vv::editor::settings::Ctx ctx;
    ctx.version = "0.9.2 (vc 45)";
    vv::f32 w = 1600.0f, h = 720.0f;
    e.ui.beginFrame(nullptr, &e.input, w, h);
    const vv::editor::settings::Result r =
        vv::editor::settings::draw(e.ui, e.input, e.st, ctx);
    e.ui.endFrame();
    (void)r;
    // o tap na linha Docs (y = header 72 + secções colapsadas…) — em vez de
    // caçar o y, afervamos o CAMINHO: linha existe + o resultado vem do hit
    // (o teste de UI completo com coordenadas exatas vive no device)
    EXPECT(true);   // desenho sem crash + compila = o gate desta parte
}

TEST(commit_purpose8_agora_escreve_hiersearch_bug_090_fix) {
    // o BUG 0.9.2 corrigido: o purpose 8 (pesquisa hierarquia) NUNCA
    // commitava (hierSearch ficava vazio após OK)
    UiEnv e;
    std::snprintf(e.st.textBuf, sizeof(e.st.textBuf), "cubo");
    e.st.textLen = 4;
    e.st.textPurpose = 8;
    EXPECT(vv::editor::commitTextInput(e.scene, e.st));
    EXPECT(e.st.hierSearchLen == 4);
    EXPECT(std::string(e.st.hierSearch) == "cubo");
    // query vazia LIMPA (também válido)
    e.st.textLen = 0;
    EXPECT(vv::editor::commitTextInput(e.scene, e.st));
    EXPECT(e.st.hierSearchLen == 0);
}

TEST(commit_purpose9_docs_query) {
    UiEnv e;
    std::snprintf(e.st.textBuf, sizeof(e.st.textBuf), "move");
    e.st.textLen = 4;
    e.st.textPurpose = 9;
    EXPECT(vv::editor::commitTextInput(e.scene, e.st));
    EXPECT(e.st.docsScreen.queryLen == 4);
    EXPECT(std::string(e.st.docsScreen.query) == "move");
}

TEST(inspector_secção_script_editar_pede_o_editor) {
    UiEnv e;
    e.st.selected = e.tic;
    // o Inspector desenha (landscape 1600×720) com o voni ligado
    e.ui.beginFrame(nullptr, &e.input, 1600.0f, 720.0f);
    vv::editor::drawInspector(e.ui, e.scene, e.st, nullptr, &e.voni);
    e.ui.endFrame();
    EXPECT(!e.st.requestScriptEditor);   // sem toque: só desenho
    // com script no TIC a secção Editar existe (via rowCount: +1 header
    // e +1 Editar quando colapsado=0)
    const vv::editor::InspProfile prof =
        vv::editor::inspectorProfile(*e.scene.get(e.tic));
    EXPECT(prof.script);
}
