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
    // FASE 9 (G0-1/G0-2): o contrato NOVO — abrir SEM fonte guardada
    // carrega o ESQUELETO base (entry point da spec §3) com o cursor NO
    // INTERIOR (dentro de "allmoments { }"); digitar insere NO CARET.
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    // esqueleto presente, cursor no interior (antes do '}' de allmoments)
    EXPECT(e.st.scriptWin.buf == vv::editor::scriptwin::kSkeleton);
    EXPECT(e.st.scriptWin.caret == vv::editor::scriptwin::kSkeletonCaret);
    {
        // o caret aponta para DENTRO do bloco allmoments (o '}' à frente
        // fecha allmoments — não é o fim do esqueleto)
        EXPECT(e.st.scriptWin.buf[e.st.scriptWin.caret] == '}');
        const std::string before = e.st.scriptWin.buf.substr(
            0, e.st.scriptWin.caret);
        EXPECT(before.find("allmoments {") != std::string::npos);
    }
    vv::ime::clearForTest();
    vv::ime::pushText("v++a=1");
    vv::ime::pushKey(vv::ime::Key::Enter);
    vv::ime::pushText("b");
    vv::ime::Event ev;
    while (vv::ime::poll(ev)) {
        vv::editor::scriptwin::applyEvent(e.st.scriptWin, ev);
    }
    // o texto entrou NO CARET (interior do allmoments), não no fim
    EXPECT(e.st.scriptWin.buf ==
           "central main {\n"
           "  on moment { }\n"
           "  allmoments { v++a=1\nb}\n"
           "}\n");
    // DEL apaga o code point ANTES do caret
    vv::ime::pushKey(vv::ime::Key::Del);
    vv::ime::poll(ev);
    vv::editor::scriptwin::applyEvent(e.st.scriptWin, ev);
    EXPECT(e.st.scriptWin.buf ==
           "central main {\n"
           "  on moment { }\n"
           "  allmoments { v++a=1\n}\n"
           "}\n");
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_caret_move_setas_e_del_no_meio) {
    // FASE 9 (G0-1): o caret MOVE-SE (Left/Right/Up/Down) e DEL apaga onde
    // o caret está (o modelo antigo era append-only no fim)
    UiEnv e;
    e.scene.get(e.tic)->getComponent<ScriptComp>()->source = "abc\ndef";
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    EXPECT(e.st.scriptWin.caret == 7);   // fim (fonte guardada = intacta)
    vv::ime::Event ev;
    auto key = [&](vv::ime::Key k) {
        ev.isText = false;
        ev.key = k;
        vv::editor::scriptwin::applyEvent(e.st.scriptWin, ev);
    };
    key(vv::ime::Key::Up);      // mesma coluna (3) na linha anterior
    EXPECT(e.st.scriptWin.caret == 3);
    key(vv::ime::Key::Left);    // um code point atrás
    EXPECT(e.st.scriptWin.caret == 2);
    key(vv::ime::Key::Right);
    EXPECT(e.st.scriptWin.caret == 3);
    key(vv::ime::Key::Down);    // volta para o fim da 2ª linha
    EXPECT(e.st.scriptWin.caret == 7);
    key(vv::ime::Key::Up);
    key(vv::ime::Key::Del);     // apaga o 'c' (col 2 da 1ª linha)
    EXPECT(e.st.scriptWin.buf == "ab\ndef");
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_fonte_existente_abre_intacta) {
    // FASE 9 (G0-2): fonte guardada abre INTACTA (o esqueleto só entra
    // quando NÃO há fonte)
    UiEnv e;
    const char* kSrc = "v++x=1\nView P \"ola\"\n";
    e.scene.get(e.tic)->getComponent<ScriptComp>()->source = kSrc;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    EXPECT(e.st.scriptWin.buf == kSrc);
    EXPECT(e.st.scriptWin.caret == e.st.scriptWin.buf.size());
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_teclado_in_app_digitavel) {
    // FASE 9 (G0-1 — o coração do fix): o teclado IN-APP desenhado pelo
    // editor emite pelo MESMO applyEvent do IME; 20 teclas, editor ABERTO,
    // texto PRESENTE, zero crash. A página de símbolos tem { } " = (a
    // linguagem precisa deles).
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.kbOpen = true;
    // 20 eventos "teclas do teclado in-app" (letras + espaço + enter +
    // símbolos) — mesmos eventos que o drawKeyboard emite (20 EXATAS:
    // o array é [20] — a 1ª versão listava 19 e o nullptr final crashava
    // o teste em kKeys[19][0])
    const char* kKeys[20] = {"v", "+", "+", "a", " ", "=", " ", "1", "\n",
                             "{", " ", "}", "\n", "b", "c", "d", "e", "f",
                             "g", "h"};
    for (int i = 0; i < 20; ++i) {
        vv::ime::Event ev;
        if (kKeys[i][0] == '\n') {
            ev.isText = false;
            ev.key = vv::ime::Key::Enter;
        } else {
            ev.isText = true;
            ev.text = kKeys[i];
        }
        vv::editor::scriptwin::applyEvent(e.st.scriptWin, ev);
        // O EDITOR CONTINUA ABERTO a cada tecla (não fecha ao digitar)
        EXPECT(e.st.scriptWin.open);
    }
    EXPECT(e.st.scriptWin.buf.size() > 20);   // esqueleto + 20 teclas
    // o desenho com o teclado aberto não crasha (portrait 720×1536)
    e.frame(720.0f, 1536.0f);
    EXPECT(e.st.scriptWin.open);
    EXPECT(e.st.scriptWin.kbOpen);
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_toque_no_corpo_abre_teclado_e_pede_ime) {
    // FASE 9 (G0-1): toque PARADO no corpo → result 5 (o main re-pede o
    // IME) e o teclado in-app abre (o caminho do device sem IME visível)
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    // tap no MEIO do corpo (não em back/run/stop/lupa)
    int r = 0;
    {
        e.input.injectDown(0, 360.0f, 400.0f);
        e.ui.beginFrame(nullptr, &e.input, 720.0f, 1536.0f);
        r = vv::editor::scriptwin::draw(e.ui, e.input, e.st.scriptWin,
                                        720.0f, 1536.0f, 0.0f);
        e.ui.endFrame();
        e.input.injectUp(0);
        e.ui.beginFrame(nullptr, &e.input, 720.0f, 1536.0f);
        r = vv::editor::scriptwin::draw(e.ui, e.input, e.st.scriptWin,
                                        720.0f, 1536.0f, 0.0f);
        e.ui.endFrame();
        e.input.clearEdges();
    }
    EXPECT(r == 5);                 // o main re-pede o IME
    // 0.9.6 (G3): a POLÍTICA DE COEXISTÊNCIA — o teclado próprio CEDA ao
    // IME do sistema (nunca os dois); abre pelo BOTÃO do cabeçalho
    EXPECT(!e.st.scriptWin.kbOpen);
    EXPECT(e.st.scriptWin.open);    // digitar/toque NÃO fecha
    vv::editor::scriptwin::close(e.st.scriptWin);
}

TEST(scriptwin_lupa_abre_docs_por_cima) {
    // FASE 9 (G0-3): a lupa na toolbar do editor devolve 4 (o main abre
    // as Docs por cima — pesquisa filtra + mostra o exemplo)
    UiEnv e;
    vv::editor::scriptwin::open(e.st.scriptWin, e.scene, e.tic);
    const f32 docsX = 720.0f - 72.0f * 2.0f - 16.0f - 8.0f - 48.0f;
    int r = 0;
    e.input.injectDown(0, docsX + 24.0f, 28.0f);
    e.ui.beginFrame(nullptr, &e.input, 720.0f, 1536.0f);
    r = vv::editor::scriptwin::draw(e.ui, e.input, e.st.scriptWin, 720.0f,
                                    1536.0f, 0.0f);
    e.ui.endFrame();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, 720.0f, 1536.0f);
    r = vv::editor::scriptwin::draw(e.ui, e.input, e.st.scriptWin, 720.0f,
                                    1536.0f, 0.0f);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(r == 4);
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
    // FASE 9 (G0-3 — o fix): a linha "Ver docs da V.ONI" era MORTA no
    // device (o walk do scrollTap só avançava o cursor — o toque nunca
    // voltava re-despachado). O tap na linha tem de devolver kOpenDocs.
    UiEnv e;
    e.st.settingsMenu = true;
    // colapsa Geral/Audio/Permissoes/Diagnóstico para a linha Docs subir
    // para dentro do viewport 1600×720 (o mesmo atalho do teste do
    // kOpenTextWindow do wiring091)
    e.st.settingsCollapsed =
        vv::editor::settings::kBitGeral | vv::editor::settings::kBitAudio |
        vv::editor::settings::kBitPerm | vv::editor::settings::kBitDiag;
    // um frame para o layout estabilizar (slots de scroll do UiContext)
    e.ui.beginFrame(nullptr, &e.input, 1600.0f, 720.0f);
    e.ui.endFrame();
    e.input.clearEdges();

    // y da linha: 8 (pad) + 4 headers colapsados ×48 + header Docs ×48 +
    // meia linha; a linha INTEIRA é o alvo (o mesmo hit-test do draw)
    // 0.9.6 (G1): Settings ECRÃ CHEIO — sem a banda kToolbarH do overlayArea
    const f32 yRow = 56.0f + 8.0f + 4.0f * 48.0f +
                     48.0f + 24.0f;
    vv::editor::settings::Ctx ctx;
    ctx.version = "0.9.4 (vc 47)";
    e.input.injectDown(0, 800.0f, yRow);
    e.ui.beginFrame(nullptr, &e.input, 1600.0f, 720.0f);
    vv::editor::settings::draw(e.ui, e.input, e.st, ctx);
    e.ui.endFrame();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, 1600.0f, 720.0f);
    const vv::editor::settings::Result r =
        vv::editor::settings::draw(e.ui, e.input, e.st, ctx);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(r == vv::editor::settings::kOpenDocs);
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

// ===========================================================================
// 0.9.5 · METADE 1 — LINKERS & TYKERS NO MOTOR REAL (VoniSystem + EngineHost
// sobre a Scene): o follow move o Transform3D de verdade, o colorpars tinge
// o MeshRenderer de verdade, o ciclo mata a run com o erro legível no
// popError (o caminho que o editor mostra na barra de erro).
// ===========================================================================
TEST(voni_motor_linker_tyker_follow_move_o_transform) {
    VmEnv e;
    // o ALVO: outro TIC com Transform3D em (5,0,0)
    const Handle alvo = e.scene.create("alvo");
    {
        Transform3D* tr = e.scene.get(alvo)->addComponent<Transform3D>();
        tr->pos = Vec3{5.0f, 0.0f, 0.0f};
        tr->updateWorld();
    }
    {
        Transform3D* tr = e.scene.get(e.tic)->getComponent<Transform3D>();
        tr->pos = Vec3{15.0f, 0.0f, 0.0f};
        tr->updateWorld();
    }
    e.script()->source =
        "linker(ator)to(alvo)=RF(principal)\n"
        "tyker(seguelo){ find(principal) follow(2) }\n"
        "central main { }\n";
    voni::Error err;
    EXPECT(e.voni.editorStart(e.scene, e.tic, err));
    e.voni.tick(e.scene, 1.0f / 60.0f);
    Transform3D* tr = e.scene.get(e.tic)->getComponent<Transform3D>();
    // follow(2): o ator fica a 2 do alvo, na direção de onde veio (7,0,0)
    EXPECT(std::fabs(tr->pos.x - 7.0f) < 1e-4f);
    EXPECT(std::fabs(tr->pos.y) < 1e-4f);
    EXPECT(std::fabs(tr->pos.z) < 1e-4f);
}

TEST(voni_motor_colorpars_tinge_o_meshrenderer) {
    VmEnv e;
    e.scene.get(e.tic)->addComponent<MeshRenderer>();
    const Handle alvo = e.scene.create("alvo");
    e.scene.get(alvo)->addComponent<Transform3D>();
    e.script()->source =
        "linker(ator)to(alvo)=RF(p)\n"
        "tyker(pinta){ find(p) colorpars(cor)(#00FF00) }\n"
        "central main { }\n";
    voni::Error err2;
    EXPECT(e.voni.editorStart(e.scene, e.tic, err2));
    e.voni.tick(e.scene, 1.0f / 60.0f);
    MeshRenderer* mr = e.scene.get(e.tic)->getComponent<MeshRenderer>();
    EXPECT(mr != nullptr);
    // o parâmetro 'cor' tinge o material da ORIGEM do link
    EXPECT(std::fabs(mr->tint[0] - 0.0f) < 1e-4f);
    EXPECT(std::fabs(mr->tint[1] - 1.0f) < 1e-4f);
    EXPECT(std::fabs(mr->tint[2] - 0.0f) < 1e-4f);
}

TEST(voni_motor_ciclo_erro_no_poperror_do_editor) {
    VmEnv e;
    e.script()->source =
        "linker(ator)to(alvo)=RF(p)\n"
        "linker(alvo)to(ator)=RF(p)\n"
        "central main { }\n";
    e.scene.create("alvo");
    voni::Error err;
    EXPECT(!e.voni.editorStart(e.scene, e.tic, err));
    EXPECT(!err.ok);
    EXPECT(err.message.find("ciclo") != std::string::npos);
    // o erro chega à BARRA DO EDITOR pelo popError (o drain do VoniSystem):
    // como o editorStart FALHOU, a run fica morta — o popError traz o fatal
    vv::Handle errTic{};
    voni::Error popped;
    // (o VoniSystem só põe no pendingError no tick das runs de Play; a run
    // do editor falhou no start — o erro vem no return, como acima. O
    // caminho do popError é coberto pelo caso voni_system_erro_pop_drain.)
    (void)errTic;
    (void)popped;
}

// ===========================================================================
// 0.9.5 · METADE 2 — O EDITOR QUE ENSINA: os esqueletos por Tab, a strip
// fina de ajuda (mini-descrição em tempo real + toque numa palavra), os
// níveis I/N/S e o botão copiar-referência. TUDO alimentado pelo REGISTO
// (a bijeção é a sentinela R-013; aqui afervamos o COMPORTAMENTO do editor).
// ===========================================================================
namespace m2 {

// um evento IME de tecla
vv::ime::Event keyEvent(vv::ime::Key k) {
    vv::ime::Event ev;
    ev.isText = false;
    ev.key = k;
    return ev;
}
vv::ime::Event textEvent(const char* t) {
    vv::ime::Event ev;
    ev.isText = true;
    ev.text = t;
    return ev;
}

} // namespace

TEST(scriptwin_tab_os_esqueletos_da_spec) {
    UiEnv e;
    namespace sw = vv::editor::scriptwin;

    // exist + Tab → "exist(){ } notexist{ }" com o caret NO INTERIOR
    sw::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.buf.clear();
    e.st.scriptWin.caret = 0;
    sw::applyEvent(e.st.scriptWin, m2::textEvent("exist"));
    sw::applyEvent(e.st.scriptWin, m2::keyEvent(vv::ime::Key::Tab));
    EXPECT(e.st.scriptWin.buf == "exist(){ } notexist{ }");
    EXPECT(e.st.scriptWin.caret == 8);   // DENTRO do exist(){ … }

    // option + Tab
    e.st.scriptWin.buf.clear();
    e.st.scriptWin.caret = 0;
    sw::applyEvent(e.st.scriptWin, m2::textEvent("option"));
    sw::applyEvent(e.st.scriptWin, m2::keyEvent(vv::ime::Key::Tab));
    EXPECT(e.st.scriptWin.buf ==
          "option(){ and valor(ação) stopand notoption{ } }");

    // repeat + Tab
    e.st.scriptWin.buf.clear();
    e.st.scriptWin.caret = 0;
    sw::applyEvent(e.st.scriptWin, m2::textEvent("repeat"));
    sw::applyEvent(e.st.scriptWin, m2::keyEvent(vv::ime::Key::Tab));
    EXPECT(e.st.scriptWin.buf == "repeat(n){ }");

    // tyker + Tab (o find(RF) vem NO esqueleto — é obrigatório de 1º)
    e.st.scriptWin.buf.clear();
    e.st.scriptWin.caret = 0;
    sw::applyEvent(e.st.scriptWin, m2::textEvent("tyker"));
    sw::applyEvent(e.st.scriptWin, m2::keyEvent(vv::ime::Key::Tab));
    EXPECT(e.st.scriptWin.buf == "tyker(nome){ find(RF) }");

    // palavra SEM esqueleto + Tab → indenta 2 espaços (o clássico)
    e.st.scriptWin.buf.clear();
    e.st.scriptWin.caret = 0;
    sw::applyEvent(e.st.scriptWin, m2::textEvent("zzz"));
    sw::applyEvent(e.st.scriptWin, m2::keyEvent(vv::ime::Key::Tab));
    EXPECT(e.st.scriptWin.buf == "zzz  ");

    // PREFIXO também casa: "exi" + Tab → o esqueleto do exist
    e.st.scriptWin.buf.clear();
    e.st.scriptWin.caret = 0;
    sw::applyEvent(e.st.scriptWin, m2::textEvent("exi"));
    sw::applyEvent(e.st.scriptWin, m2::keyEvent(vv::ime::Key::Tab));
    EXPECT(e.st.scriptWin.buf == "exist(){ } notexist{ }");
}

TEST(scriptwin_strip_mini_descricao_desde_a_1_letra) {
    UiEnv e;
    namespace sw = vv::editor::scriptwin;

    sw::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.buf = "repeat(3){ }";
    e.st.scriptWin.caret = 13;   // no fim

    // nível NORMAL (default): 1 linha só, SEM exemplo
    EXPECT(e.st.scriptWin.helpLevel == 1);
    // digitando "ex" (o caret depois de "ex") → a strip acende DESDE A 1ª
    // letra com nome+1-linha do PRIMEIRO casamento por prefixo
    e.st.scriptWin.buf = "ex";
    e.st.scriptWin.caret = 2;
    const std::string l1 = sw::helpStripLine1(e.st.scriptWin);
    EXPECT(!l1.empty());
    EXPECT(l1.find("exist") != std::string::npos);
    EXPECT(sw::helpStripLine2(e.st.scriptWin).empty());   // Normal: 1 linha

    // INICIANTE (0): SEMPRE com o exemplo (2 linhas)
    e.st.scriptWin.helpLevel = 0;
    EXPECT(!sw::helpStripLine1(e.st.scriptWin).empty());
    const std::string l2 = sw::helpStripLine2(e.st.scriptWin);
    EXPECT(l2.find("ex.:") == 0);

    // SILENCIOSO (2): NADA (sem encher o ecrã)
    e.st.scriptWin.helpLevel = 2;
    EXPECT(sw::helpStripLine1(e.st.scriptWin).empty());
    EXPECT(sw::helpStripLine2(e.st.scriptWin).empty());

    // palavra que não casa com nada → a strip APAGA
    e.st.scriptWin.helpLevel = 1;
    e.st.scriptWin.buf = "zzqq";
    e.st.scriptWin.caret = 4;
    EXPECT(sw::helpStripLine1(e.st.scriptWin).empty());
}

TEST(scriptwin_toque_na_palavra_explica_com_exemplo) {
    UiEnv e;
    namespace sw = vv::editor::scriptwin;

    sw::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.buf = "repeat(3){ }\n";
    e.st.scriptWin.caret = 13;

    // a palavra sob o offset 2 é "repeat"
    EXPECT(sw::wordAtOffset(e.st.scriptWin, 2) == "repeat");
    EXPECT(sw::wordAtOffset(e.st.scriptWin, 0) == "repeat");
    EXPECT(sw::wordAtOffset(e.st.scriptWin, 5) == "repeat");
    EXPECT(sw::wordAtOffset(e.st.scriptWin, 6) == "repeat");  // fronteira '('
    EXPECT(sw::wordAtOffset(e.st.scriptWin, 7) == "3");       // dígito é palavra
    EXPECT(sw::wordAtOffset(e.st.scriptWin, 10) == "");       // espaço → nada

    // o TOQUE (estado helpTapped+helpWord): a explicação com EXEMPLO (a
    // spec: "linha de explicação com exemplo, vinda da Docs")
    e.st.scriptWin.helpTapped = true;
    e.st.scriptWin.helpWord = "repeat";
    const std::string l1 = sw::helpStripLine1(e.st.scriptWin);
    EXPECT(l1.find("repeat") != std::string::npos);
    EXPECT(l1.find("Repete o bloco") != std::string::npos);
    // no Normal, o TOQUE traz o exemplo (a 2ª linha acende)
    EXPECT(e.st.scriptWin.helpLevel == 1);
    EXPECT(sw::helpStripLine2(e.st.scriptWin).find("ex.:") == 0);

    // DIGITAR limpa o estado de toque (volta à mini-descrição)
    sw::applyEvent(e.st.scriptWin, m2::textEvent("x"));
    EXPECT(!e.st.scriptWin.helpTapped);
    EXPECT(e.st.scriptWin.helpWord.empty());
}

TEST(scriptwin_botao_nivel_cicla_e_o_tab_do_teclado_existe) {
    UiEnv e;
    namespace sw = vv::editor::scriptwin;
    sw::open(e.st.scriptWin, e.scene, e.tic);
    e.st.scriptWin.kbOpen = true;

    // o botão do NÍVEL cicla N→S→I→N (o draw processa o toque)
    EXPECT(e.st.scriptWin.helpLevel == 1);
    // 0.9.6 (G3): 720×1536 portrait — o botão do nível está a docsX-200
    // (docsX=504; o do teclado próprio entrou em docsX-152)
    e.tap(504.0f - 200.0f + 24.0f, 28.0f, 720.0f, 1536.0f);
    EXPECT(e.st.scriptWin.helpLevel == 2);
    e.tap(504.0f - 200.0f + 24.0f, 28.0f, 720.0f, 1536.0f);
    EXPECT(e.st.scriptWin.helpLevel == 0);
    e.tap(504.0f - 200.0f + 24.0f, 28.0f, 720.0f, 1536.0f);
    EXPECT(e.st.scriptWin.helpLevel == 1);

    // 0.9.6 (G3): a linha de baixo do teclado — [<][^][v][>][ESPACO 2u]
    // [TAB 1u][PAG 1u][APAGA 1.5u][ENTER 1.5u][FECHAR 1u] = 12u+9g
    // unit = (720-16-9*6)/12 = 54.2; TAB após 4 setas + espaço 2u
    {
        const f32 unit = (720.0f - 16.0f - 9.0f * 6.0f) / 12.0f;
        const f32 tabX = 8.0f + 4.0f * (unit + 6.0f) + 2.0f * unit + 6.0f +
                         unit * 0.5f;
        const f32 kbTop = 1536.0f - 40.0f - 0.0f -
                          (5.0f * 48.0f + 4.0f * 6.0f + 2.0f * 8.0f);
        const f32 tabY = kbTop + 8.0f + 4.0f * (48.0f + 6.0f) + 24.0f;
        e.st.scriptWin.buf = "exist";
        e.st.scriptWin.caret = 5;
        e.tap(tabX, tabY, 720.0f, 1536.0f);
        EXPECT(e.st.scriptWin.buf == "exist(){ } notexist{ }");   // o TAB
    }
}

TEST(scriptwin_botao_copiar_referencia_devolve_6) {
    UiEnv e;
    namespace sw = vv::editor::scriptwin;
    sw::open(e.st.scriptWin, e.scene, e.tic);

    // o botão 📋 devolve 6 (o main põe no clipboard via JNI) — 720 portrait:
    // docsX=504, copy em docsX-56=448
    int r = 0;
    e.input.injectDown(0, 448.0f + 24.0f, 28.0f);
    e.ui.beginFrame(nullptr, &e.input, 720.0f, 1536.0f);
    r = sw::draw(e.ui, e.input, e.st.scriptWin, 720.0f, 1536.0f, 0.0f);
    e.ui.endFrame();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, 720.0f, 1536.0f);
    r = sw::draw(e.ui, e.input, e.st.scriptWin, 720.0f, 1536.0f, 0.0f);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(r == 6);
}

TEST(voni_erros_que_ensinam_as_palavras_estrangeiras) {
    // quem sabe Python/JS escreve 'if' — o erro ENSINA o equivalente V.ONI
    // (a lista vive no REGISTO — kForeign — a sentinela R-013 afere a bijeção)
    // Uns morrem no COMPILE (if/while/def com blocos); outros parseiam como
    // comandos (break/print à solta) e morrem no RUNTIME — os DOIS ensinam.
    struct Caso { const char* src; const char* pedaco; };
    const Caso casos[] = {
        {"if (vida == 0) { }", "exist"},
        {"while (vida > 0) { }", "last"},
        {"def soma() { }", "fn"},
        {"switch (n) { }", "option"},
        {"for x { }", "repeat"},
        {"else { }", "notexist"},
        {"break", "resume"},
        {"print(\"ola\")", "View P"},
    };
    for (const Caso& c : casos) {
        voni::Error err;
        voni::Script s = voni::Script::compile(c.src, err);
        if (err.ok) {
            // parseou como comando → o RUNTIME ensina (o mesmo registo)
            VmEnv e;
            e.script()->source = c.src;
            voni::Error rerr;
            EXPECT(!e.voni.editorStart(e.scene, e.tic, rerr));
            EXPECT(rerr.message.find(c.pedaco) != std::string::npos);
        } else {
            EXPECT(err.message.find(c.pedaco) != std::string::npos);
            EXPECT(err.line >= 1);   // com linha (o editor acende a gutter)
        }
    }
}
