// tests/test_scenes.cpp — 0.7.1: CENAS MÚLTIPLAS + TRANSIÇÕES.
//
// Aferimos o CONTRATO da sub-fase:
//   • criar/trocar/persistir cenas ao nível do PROJETO (FakeStorage — cada
//     cena é um .goni próprio no manifesto; a troca GUARDA a atual antes de
//     carregar a nova; o round-trip completo A→B→A devolve cada cena com os
//     seus TICs; o manifesto persiste a ativa);
//   • TRANSIÇÕES puras (ui/SceneFx.h): transitionStep devolve true UMA vez
//     no PONTO MÉDIO (o swap), nunca antes/depois; cover em rampa 0→1→0;
//     inativa = custo zero;
//   • transitionDraw: fade = quad fullscreen com alpha=cover; slide = quad
//     que cobre da esquerda (ida) e sai pela direita (volta) — aferido nos
//     batches de CPU;
//   • overlay CENAS: lista as cenas do manifesto (a ativa marcada), tap
//     troca, "+ Nova cena" (pick 1), toque fora fecha;
//   • ações declarativas: Scene.Load = Instant; Scene.Transition = Fade
//     (default) ou Slide (param) — o callback recebe nome+estilo; o
//     round-trip .goni preserva act "trans" + param;
//   • teclado in-app com propósito 1 (nome de cena) fica pronto para o
//     main (buffer/alvo no EditorState).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "FakeStorage.h"
#include "components/UiCanvas.h"
#include "core/Presets.h"
#include "core/Project.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/SceneFx.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {
constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
} // namespace

// ---- 1. projeto: criar/trocar/persistir cenas (o fluxo do main) ---------------

TEST(cenas_criar_trocar_persistir_e2e) {
    FakeStorage st;
    Project p;
    ASSERT_OK:;
    if (!Project::openOrCreate(st, "teste", p)) {
        EXPECT(!"openOrCreate falhou");
        return;
    }
    EXPECT(p.scenes.size() == 1u);   // "scenes/main.goni"
    EXPECT(p.activeScene == 0u);

    // cena A (main): um TIC com nome único
    Scene a;
    a.create("CuboA");
    EXPECT(p.saveActiveScene(st, a));

    // ---- criar a cena B (a SEQUÊNCIA do main: save atual → addScene →
    // clear → save vazia → manifesto)
    EXPECT(p.addScene("nivel2"));
    EXPECT(p.scenes.size() == 2u);
    EXPECT(p.activeScene == 1u);
    // duplicado: SÓ ATIVA (sem criar outra) — o contrato do addScene
    EXPECT(p.addScene("nivel2"));
    EXPECT(p.scenes.size() == 2u);   // nenhuma duplicada
    EXPECT(p.activeScene == 1u);
    Scene b;   // vazia
    EXPECT(p.saveActiveScene(st, b));
    EXPECT(p.saveManifest(st));

    // a cena B em memória ganha conteúdo próprio
    b.create("CuboB");
    b.create("LuzB");
    EXPECT(p.saveActiveScene(st, b));

    // ---- trocar B → A (guarda B, carrega A)
    p.activeScene = 0;
    EXPECT(p.saveManifest(st));
    Scene loaded;
    SceneSerializer::LoadCtx ctx;
    EXPECT(p.loadActiveScene(st, loaded, ctx));
    EXPECT(loaded.count() == 1u);
    EXPECT(loaded.find("CuboA").valid());
    EXPECT(loaded.find("CuboB") == Handle::invalid());   // cenas separadas

    // ---- trocar A → B de volta
    p.activeScene = 1;
    EXPECT(p.saveManifest(st));
    Scene loaded2;
    EXPECT(p.loadActiveScene(st, loaded2, ctx));
    EXPECT(loaded2.count() == 2u);
    EXPECT(loaded2.find("CuboB").valid());
    EXPECT(loaded2.find("LuzB").valid());
    EXPECT(loaded2.find("CuboA") == Handle::invalid());

    // ---- REOPEN: o manifesto persiste a ativa (B) e as duas cenas existem
    Project p2;
    EXPECT(Project::open(st, p2));
    EXPECT(p2.scenes.size() == 2u);
    EXPECT(p2.activeScene == 1u);
    Scene reopened;
    EXPECT(p2.loadActiveScene(st, reopened, ctx));
    EXPECT(reopened.count() == 2u);   // a B certa veio do disco
    EXPECT(reopened.find("CuboB").valid());

    // a cena A continua no ficheiro DELA (a troca não a tocou)
    p2.activeScene = 0;
    Scene aAgain;
    EXPECT(p2.loadActiveScene(st, aAgain, ctx));
    EXPECT(aAgain.count() == 1u);
    EXPECT(aAgain.find("CuboA").valid());
}

// ---- 2. transições puras (SceneFx) ----------------------------------------------

TEST(cenas_transicao_swap_uma_vez_no_ponto_medio) {
    ui::SceneTransition tr;
    tr.active = true;
    tr.style = ui::SceneTransition::Style::Fade;
    tr.dur = 0.6f;
    tr.target = "nivel2";

    // inativa → nada (o frame comum tem custo zero)
    ui::SceneTransition off;
    EXPECT(!ui::transitionStep(off, 0.1f));
    EXPECT(ui::transitionCover(off) == 0.0f);

    // passos de 0.1s: o swap ao chegar ao MEIO (t=0.3), UMA única vez
    bool swapped = false;
    u32 swaps = 0;
    for (int i = 0; i < 12; ++i) {
        if (ui::transitionStep(tr, 0.1f)) {
            ++swaps;
            swapped = true;
            // no frame do swap o ecrã está TAPADO (cover = 1)
            EXPECT(nearEqF(ui::transitionCover(tr), 1.0f));
        }
    }
    EXPECT(swapped);
    EXPECT(swaps == 1u);      // nunca duas vezes
    EXPECT(!tr.active);       // acabou (12×0.1 = 1.2s > 0.6s)
    EXPECT(nearEqF(tr.t, 0.6f));
    EXPECT(!ui::transitionStep(tr, 0.1f));   // morta continua morta

    // cover em rampa: subida 0→1 até ao meio, descida 1→0 até ao fim
    ui::SceneTransition t2;
    t2.active = true;
    t2.dur = 1.0f;
    t2.t = 0.0f;
    EXPECT(nearEqF(ui::transitionCover(t2), 0.0f));
    t2.t = 0.25f;
    EXPECT(nearEqF(ui::transitionCover(t2), 0.5f));
    t2.t = 0.5f;
    EXPECT(nearEqF(ui::transitionCover(t2), 1.0f));
    t2.t = 0.75f;
    EXPECT(nearEqF(ui::transitionCover(t2), 0.5f));
    t2.t = 1.0f;
    EXPECT(nearEqF(ui::transitionCover(t2), 0.0f));
}

TEST(cenas_transicao_draw_fade_e_slide_nos_batches) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);

    struct Rect { f32 x0, y0, x1, y1; };
    auto lastQuad = [](const UiContext& u) -> Rect {
        const QuadVertex* v = u.solidsForTest().vertices();
        const u32 n = u.solidsForTest().vertexCount();
        Rect r{0, 0, 0, 0};
        if (n >= 6) {
            const u32 i = n - 6;
            r = {v[i].x, v[i].y, v[i + 2].x, v[i + 2].y};
        }
        return r;
    };

    // FADE no ponto médio: 1 quad FULLSCREEN opaco
    ui::SceneTransition tr;
    tr.active = true;
    tr.style = ui::SceneTransition::Style::Fade;
    tr.dur = 0.6f;
    tr.t = 0.3f;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    ui::transitionDraw(ui, tr, kSW, kSH);
    Rect q = lastQuad(ui);
    EXPECT(nearEqF(q.x0, 0.0f) && nearEqF(q.y0, 0.0f));
    EXPECT(nearEqF(q.x1, kSW) && nearEqF(q.y1, kSH));
    ui.endFrame();

    // FADE a subir (t=0.15 → cover 0.5): fullscreen com alpha — aferido pelo
    // canal a do último vértice (0.5)
    tr.t = 0.15f;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    ui::transitionDraw(ui, tr, kSW, kSH);
    {
        const QuadVertex* v = ui.solidsForTest().vertices();
        const u32 n = ui.solidsForTest().vertexCount();
        EXPECT(n >= 6);
        if (n >= 6) {
            EXPECT(nearEqF(v[n - 6].a, 0.5f, 0.01f));
        }
    }
    ui.endFrame();

    // SLIDE na ida (t=0.15 → cover 0.5): quad da ESQUERDA, meia largura
    tr.style = ui::SceneTransition::Style::Slide;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    ui::transitionDraw(ui, tr, kSW, kSH);
    q = lastQuad(ui);
    EXPECT(nearEqF(q.x0, 0.0f));
    EXPECT(nearEqF(q.x1, kSW * 0.5f, 0.5f));
    EXPECT(nearEqF(q.y1, kSH));
    ui.endFrame();

    // SLIDE na volta (t=0.45 → cover 0.5): o bordo esquerdo JÁ largou a
    // margem (x0 > 0) e o quad sai pela direita
    tr.t = 0.45f;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    ui::transitionDraw(ui, tr, kSW, kSH);
    q = lastQuad(ui);
    EXPECT(q.x0 > 0.0f);
    EXPECT(nearEqF(q.x1, kSW, 0.5f));
    EXPECT(nearEqF(q.x0, kSW * 0.5f, 0.5f));
    ui.endFrame();

    // INATIVA: não emite NADA
    ui::SceneTransition off;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    ui::transitionDraw(ui, off, kSW, kSH);
    EXPECT(ui.solidsForTest().vertexCount() == 0u);
    ui.endFrame();
}

// ---- 3. overlay CENAS (a lista do manifesto) -------------------------------------

TEST(cenas_menu_lista_marca_ativa_troca_e_nova) {
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

    const std::vector<std::string> scenes = {"scenes/main.goni",
                                             "scenes/nivel2.goni",
                                             "scenes/boss.goni"};

    // nome de exibição (FONTE ÚNICA)
    char name[48];
    EXPECT(std::string(sceneDisplayName("scenes/nivel2.goni", name,
                                        sizeof(name))) == "nivel2");
    EXPECT(std::string(sceneDisplayName("main.goni", name, sizeof(name))) ==
           "main");

    auto frame = [&]() {
        ui.beginFrame(nullptr, &in, kSW, kSH);
        const int pick = drawScenesMenu(ui, in, kSW, kSH, st, scenes, 1u);
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

    st.scenesMenu = true;
    EXPECT(frame() == 0);   // aberto, sem toque

    // geometria 0.9.0 (spec H): sheet 280dp CENTRADO na BANDA DO VIEWPORT
    // (fallback sem âncora; +Nova cena 48dp + separador + lista 48dp)
    const u32 shown = 3u;   // < 6 → sem scroll
    const f32 rowH = 48.0f;
    const f32 sheetW = 280.0f;
    const f32 h = 48.0f + 56.0f + 8.0f + static_cast<f32>(shown) * rowH + 8.0f;
    f32 ox, oy, aw, ah;
    overlayArea(kSW, kSH, safe::Insets{}, ox, oy, aw, ah);
    const f32 x = ox + (aw - sheetW) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;
    const f32 listTop = y + 4.0f + 56.0f + 8.0f;

    // tap na cena 2 (boss — índice 2): pick = 2+2 = 4
    EXPECT(tap(x + sheetW * 0.5f, listTop + 2.0f * rowH + rowH * 0.5f) == 4);
    EXPECT(!st.scenesMenu);   // a escolha fecha o menu

    // "＋ Nova cena" (fill accent, fixo no topo): pick = 1
    st.scenesMenu = true;
    EXPECT(tap(x + sheetW * 0.5f, y + 4.0f + 24.0f) == 1);
    EXPECT(!st.scenesMenu);

    // toque FORA fecha sem escolha
    st.scenesMenu = true;
    EXPECT(tap(60.0f, 360.0f) == 0);
    EXPECT(!st.scenesMenu);

    // o main abre o teclado in-app com o propósito 1 (nome da cena nova)
    openTextInput(st, 1, Handle{}, -1, "");
    EXPECT(st.textInput && st.textPurpose == 1);
}

// ---- 4. ações declarativas Scene.Load / Scene.Transition --------------------------

TEST(cenas_acoes_load_instant_e_transition_fade_slide) {
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.action = UiElement::Action::LoadScene;
    btn.target = "nivel2";
    c->elements.push_back(btn);

    // contexto dos callbacks (UM user partilhado — o padrão dos resolvers)
    struct CbCtx {
        std::string name;
        int style = -1;
        bool exists = true;
    } cb;
    ui::UiActionCtx ctx;
    ctx.user = &cb;
    ctx.sceneExists = [](const std::string&, void* u) -> bool {
        return static_cast<CbCtx*>(u)->exists;
    };
    ctx.loadScene = [](const std::string& name, ui::SceneSwap style, void* u) {
        auto* c = static_cast<CbCtx*>(u);
        c->name = name;
        c->style = static_cast<int>(style);
    };

    // Scene.Load → INSTANTÂNEO
    ui::UiActionResult out = ui::applyUiAction(s, c->elements[0], ctx);
    EXPECT(out.acted);
    EXPECT(cb.name == "nivel2");
    EXPECT(cb.style == static_cast<int>(ui::SceneSwap::Instant));

    // Scene.Transition com param "slide" → SLIDE
    c->elements[0].action = UiElement::Action::TransitionScene;
    c->elements[0].param = "slide";
    out = ui::applyUiAction(s, c->elements[0], ctx);
    EXPECT(out.acted);
    EXPECT(cb.style == static_cast<int>(ui::SceneSwap::Slide));
    EXPECT(std::string(out.toast).find("slide") != std::string::npos);

    // param vazio → FADE (o default)
    c->elements[0].param.clear();
    out = ui::applyUiAction(s, c->elements[0], ctx);
    EXPECT(out.acted);
    EXPECT(cb.style == static_cast<int>(ui::SceneSwap::Fade));

    // cena que NÃO existe → toast honesto, callback NUNCA chamado
    cb.exists = false;
    cb.style = -1;
    out = ui::applyUiAction(s, c->elements[0], ctx);
    EXPECT(!out.acted);
    EXPECT(cb.style == -1);
    EXPECT(std::string(out.toast).find("não existe") != std::string::npos);

    // round-trip .goni preserva act "trans" + param "slide"
    c->elements[0].param = "slide";
    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx lctx;
    EXPECT(SceneSerializer::loadText(s2, text, lctx));
    const UiCanvas* c2 =
        s2.get(s2.find("HUD"))->getComponent<UiCanvas>();
    EXPECT(c2 != nullptr && c2->elements.size() == 1);
    EXPECT(c2->elements[0].action == UiElement::Action::TransitionScene);
    EXPECT(c2->elements[0].param == "slide");
    EXPECT(c2->elements[0].target == "nivel2");
    // .goni de 0.7.0 (sem param) abre com fade default
    const char* old = R"({"version":1,"tics":[
        {"id":0,"name":"HUD","active":true,"parent":-1,
         "components":[{"type":"UiCanvas","elements":[
           {"kind":"button","name":"b","x":10,"y":10,"w":100,"h":50,
            "color":[0.1,0.1,0.1,1],"visible":true,"ah":"left","av":"top",
            "act":"trans","target":"nivel2"}]}]}]})";
    Scene s3;
    EXPECT(SceneSerializer::loadText(s3, old, lctx));
    const UiCanvas* c3 =
        s3.get(s3.find("HUD"))->getComponent<UiCanvas>();
    EXPECT(c3 != nullptr);
    EXPECT(c3->elements[0].action == UiElement::Action::TransitionScene);
    EXPECT(c3->elements[0].param.empty());   // fade default
}
