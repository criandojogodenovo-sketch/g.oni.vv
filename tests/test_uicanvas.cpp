// tests/test_uicanvas.cpp — 0.7.0: RUNTIME da UI criável (F6).
//
// Aferimos o CONTRATO do ui/UiRuntime + components/UiCanvas:
//   • ancoragens 3×3 em 2 RESOLUÇÕES (elementRect — a fonte única):
//     esquerda/topo fixam à esquerda/topo, centro/meio seguem o centro,
//     direita/fundo fixam à direita/fundo (distâncias preservadas);
//   • setAnchor PRESERVA a posição absoluta (o elemento não salta);
//   • hit-test: o TOPO ganha (z-order), só Button/Menu são interativos,
//     elementos/TICs invisíveis não hit-testam;
//   • ações declarativas: Show/Hide/Toggle por NOME (em QUALQUER canvas),
//     Spawn (preset válido/inválido), LoadScene honesta (sem o callback da
//     0.7.1 NÃO finge sucesso; alvo inexistente → toast claro);
//   • Menu: linhas "label>alvo" (o alvo da linha sobrepõe);
//   • draw: elementos desenhados nos rects esperados (sem sobreposição),
//     invisíveis não desenham;
//   • serialização round-trip: UiCanvas (elementos completos) + Tic
//     visible + MeshRenderer tint via dump/loadText.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "components/MeshRenderer.h"
#include "components/UiCanvas.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "ui/UiRuntime.h"

using namespace vv;
using namespace vv::ui;
using ::test::nearEqF;

namespace {
constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kSW2 = 1280.0f;   // 2ª resolução (aferição de âncoras)
} // namespace

// ---- 1. âncoras em 2 resoluções -------------------------------------------------

TEST(uicanvas_ancoras_em_2_resolucoes) {
    const safe::Insets ins{};   // sem insets: o espaço puro

    // esquerda: distância à borda ESQUERDA preservada
    UiElement e;
    e.anchorH = UiElement::AnchorH::Left;
    e.ox = 40.0f;
    e.w = 200.0f;
    e.anchorV = UiElement::AnchorV::Top;
    e.oy = 30.0f;
    e.h = 60.0f;
    UiRect r1 = elementRect(e, kSW, kSH, ins);
    UiRect r2 = elementRect(e, kSW2, kSH, ins);
    EXPECT(nearEqF(r1.x, 40.0f));
    EXPECT(nearEqF(r2.x, 40.0f));   // continua a 40px da esquerda
    EXPECT(nearEqF(r1.y, 30.0f));
    EXPECT(nearEqF(r2.y, 30.0f));

    // direita: colado à borda DIREITA nas duas resoluções
    e.anchorH = UiElement::AnchorH::Right;
    e.ox = 0.0f;
    r1 = elementRect(e, kSW, kSH, ins);
    r2 = elementRect(e, kSW2, kSH, ins);
    EXPECT(nearEqF(r1.x + r1.w, kSW));
    EXPECT(nearEqF(r2.x + r2.w, kSW2));
    EXPECT(nearEqF(r1.x + r1.w - r2.x - r2.w, kSW - kSW2));   // delta = 320

    // centro horizontal: o CENTRO do ecrã nas duas resoluções
    e.anchorH = UiElement::AnchorH::Center;
    e.ox = -100.0f;   // 100px à esquerda do centro
    r1 = elementRect(e, kSW, kSH, ins);
    r2 = elementRect(e, kSW2, kSH, ins);
    EXPECT(nearEqF(r1.x, kSW * 0.5f - 100.0f));
    EXPECT(nearEqF(r2.x, kSW2 * 0.5f - 100.0f));

    // fundo: colado à borda inferior
    e.anchorV = UiElement::AnchorV::Bottom;
    e.oy = 0.0f;
    e.h = 80.0f;
    r1 = elementRect(e, kSW, kSH, ins);
    r2 = elementRect(e, kSW2, kSH, ins);
    EXPECT(nearEqF(r1.y + r1.h, kSH));
    EXPECT(nearEqF(r2.y + r2.h, kSH));

    // meio vertical: o TOPO do elemento fica no centro (a semântica do
    // offset — o canto superior-esquerdo conta do ponto de âncora)
    e.anchorV = UiElement::AnchorV::Middle;
    e.oy = 0.0f;
    r1 = elementRect(e, kSW, kSH, ins);
    EXPECT(nearEqF(r1.y, kSH * 0.5f));

    // safe-area: âncora esquerda respeita o inset (nada atrás da nav bar)
    const safe::Insets ins2{24.0f, 40.0f, 8.0f, 32.0f};
    e.anchorH = UiElement::AnchorH::Left;
    e.ox = 0.0f;
    const UiRect r3 = elementRect(e, kSW, kSH, ins2);
    EXPECT(nearEqF(r3.x, 24.0f));   // dentro do contentRect
    e.anchorV = UiElement::AnchorV::Bottom;
    e.oy = 0.0f;
    e.h = 80.0f;
    const UiRect r4 = elementRect(e, kSW, kSH, ins2);
    EXPECT(nearEqF(r4.y + r4.h, kSH - 32.0f));
}

TEST(uicanvas_setanchor_preserva_posicao) {
    UiCanvas c;
    UiElement e;
    e.anchorH = UiElement::AnchorH::Left;
    e.anchorV = UiElement::AnchorV::Top;
    e.ox = 100.0f;
    e.oy = 50.0f;
    e.w = 200.0f;
    e.h = 100.0f;
    c.elements.push_back(e);

    // posição absoluta ANTES (design 1600x720, sem insets)
    const UiRect before = elementRect(c.elements[0], kSW, kSH, safe::Insets{});
    // muda para direita/fundo — o elemento FICA no mesmo sítio
    c.setAnchor(0, UiElement::AnchorH::Right, UiElement::AnchorV::Bottom,
                kSW, kSH);
    const UiRect after = elementRect(c.elements[0], kSW, kSH, safe::Insets{});
    EXPECT(nearEqF(before.x, after.x));
    EXPECT(nearEqF(before.y, after.y));
    EXPECT(c.elements[0].anchorH == UiElement::AnchorH::Right);
    EXPECT(c.elements[0].anchorV == UiElement::AnchorV::Bottom);
    // e a 2ª resolução: a DISTÂNCIA à borda direita mantém-se (o contrato
    // da âncora — o elemento estava a 1300px da direita em 1600, continua a
    // 1300px em 1280, mesmo que saia do ecrã: é o comportamento da âncora)
    const UiRect r2 = elementRect(c.elements[0], kSW2, kSH, safe::Insets{});
    EXPECT(nearEqF((kSW - (after.x + after.w)) - (kSW2 - (r2.x + r2.w)),
                   0.0f));
}

// ---- 2. criação de elementos ----------------------------------------------------

TEST(uicanvas_add_element_defaults_e_nome_unico) {
    UiCanvas c;
    const i32 i0 = c.addElement(UiElement::Kind::Panel, kSW, kSH);
    const i32 i1 = c.addElement(UiElement::Kind::Panel, kSW, kSH);
    const i32 i2 = c.addElement(UiElement::Kind::Button, kSW, kSH);
    EXPECT(i0 == 0 && i1 == 1 && i2 == 2);
    // nomes únicos Godot-style
    EXPECT(c.elements[0].name == "panel");
    EXPECT(c.elements[1].name == "panel.001");
    EXPECT(c.elements[2].name == "button");
    // o panel nasce CENTRADO no espaço de design
    const UiRect r = elementRect(c.elements[0], kSW, kSH, safe::Insets{});
    EXPECT(nearEqF(r.x + r.w * 0.5f, kSW * 0.5f, 0.5f));
    EXPECT(nearEqF(r.y + r.h * 0.5f, kSH * 0.5f, 0.5f));
    // botão nasce com texto default
    EXPECT(!c.elements[2].text.empty());
    // findElement acha por nome
    EXPECT(c.findElement("panel.001") == 1);
    EXPECT(c.findElement("nao-existe") == -1);
}

// ---- 3. hit-test ------------------------------------------------------------------

namespace {
// cena de hit-test: 2 TICs com canvas — o TIC2 (depois) fica por CIMA
Scene makeHitScene() {
    Scene s;
    Handle h1 = s.create("UI1");
    h1 = h1;   // (silencia unused em builds de release)
    Tic* t1 = s.get(h1);
    UiCanvas* c1 = t1->addComponent<UiCanvas>();
    UiElement panel;
    panel.name = "fundo";
    panel.kind = UiElement::Kind::Panel;
    panel.ox = 100.0f; panel.oy = 100.0f; panel.w = 600.0f; panel.h = 400.0f;
    c1->elements.push_back(panel);
    UiElement btn;
    btn.name = "btn1";
    btn.kind = UiElement::Kind::Button;
    btn.ox = 150.0f; btn.oy = 150.0f; btn.w = 200.0f; btn.h = 80.0f;
    c1->elements.push_back(btn);

    Handle h2 = s.create("UI2");
    Tic* t2 = s.get(h2);
    UiCanvas* c2 = t2->addComponent<UiCanvas>();
    UiElement btn2;
    btn2.name = "btn2";
    btn2.kind = UiElement::Kind::Button;
    btn2.ox = 150.0f; btn2.oy = 150.0f; btn2.w = 200.0f; btn2.h = 80.0f;
    c2->elements.push_back(btn2);
    return s;
}
} // namespace

TEST(uicanvas_hittest_topo_ganha_e_soh_interativos) {
    Scene s = makeHitScene();
    const safe::Insets ins{};

    // dentro de btn1/btn2 (mesmo rect, canvases diferentes): o TOPO (TIC2)
    CanvasHit hit = hitTestCanvas(s, 200.0f, 180.0f, kSW, kSH, ins);
    EXPECT(hit.valid);
    EXPECT(s.get(hit.tic)->name == "UI2");   // o canvas de cima
    EXPECT(hit.element == 0);

    // panel NÃO é interativo (só Button/Menu hit-testam)
    hit = hitTestCanvas(s, 500.0f, 400.0f, kSW, kSH, ins);   // só o panel
    EXPECT(!hit.valid);

    // fora de tudo
    hit = hitTestCanvas(s, 900.0f, 600.0f, kSW, kSH, ins);
    EXPECT(!hit.valid);

    // elemento invisível não hit-testa: esconder btn2 → btn1 (de baixo) ganha
    s.components().uiCanvases().find(s.find("UI2"))->elements[0].visible = false;
    hit = hitTestCanvas(s, 200.0f, 180.0f, kSW, kSH, ins);
    EXPECT(hit.valid);
    EXPECT(s.get(hit.tic)->name == "UI1");

    // TIC invisível: o canvas INTEIRO não hit-testa
    Tic* t1 = s.get(s.find("UI1"));
    t1->visible = false;
    hit = hitTestCanvas(s, 200.0f, 180.0f, kSW, kSH, ins);
    EXPECT(!hit.valid);
}

TEST(uicanvas_hittest_menu_por_linha) {
    Scene s;
    Tic* t = s.get(s.create("menu"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement m;
    m.kind = UiElement::Kind::Menu;
    m.text = "Jogar>cena2\nSair";
    m.ox = 100.0f; m.oy = 100.0f; m.w = 300.0f; m.h = 160.0f;
    c->elements.push_back(m);

    // 2 linhas de 80px: tocar na 1ª → menuItem 0; na 2ª → menuItem 1
    CanvasHit hit = hitTestCanvas(s, 150.0f, 120.0f, kSW, kSH, safe::Insets{});
    EXPECT(hit.valid && hit.menuItem == 0);
    hit = hitTestCanvas(s, 150.0f, 220.0f, kSW, kSH, safe::Insets{});
    EXPECT(hit.valid && hit.menuItem == 1);
    // parsing das linhas: label>alvo (sem '>' → alvo = label)
    std::string label, tgt;
    EXPECT(menuLineAt(c->elements[0], 0, label, tgt));
    EXPECT(label == "Jogar" && tgt == "cena2");
    EXPECT(menuLineAt(c->elements[0], 1, label, tgt));
    EXPECT(label == "Sair" && tgt == "Sair");
    EXPECT(menuLineCount(c->elements[0]) == 2u);
}

// ---- 4. ações declarativas ----------------------------------------------------------

TEST(uicanvas_acoes_show_hide_toggle_por_nome) {
    Scene s = makeHitScene();
    UiActionCtx ctx;   // sem callbacks — panel actions não precisam

    // TOGGLE "fundo" (está no canvas UI1; a ação corre no botão de UI2)
    UiElement& btn2 =
        s.components().uiCanvases().find(s.find("UI2"))->elements[0];
    btn2.action = UiElement::Action::TogglePanel;
    btn2.target = "fundo";
    UiElement& fundo =
        s.components().uiCanvases().find(s.find("UI1"))->elements[0];
    EXPECT(fundo.visible);
    UiActionResult out = applyUiAction(s, btn2, ctx);
    EXPECT(out.acted);
    EXPECT(!fundo.visible);
    out = applyUiAction(s, btn2, ctx);
    EXPECT(fundo.visible);

    // SHOW num elemento escondido; HIDE num visível
    fundo.visible = false;
    btn2.action = UiElement::Action::ShowPanel;
    applyUiAction(s, btn2, ctx);
    EXPECT(fundo.visible);
    btn2.action = UiElement::Action::HidePanel;
    applyUiAction(s, btn2, ctx);
    EXPECT(!fundo.visible);

    // alvo inexistente: HONESTO (toast claro, sem acted)
    btn2.target = "ghost";
    out = applyUiAction(s, btn2, ctx);
    EXPECT(!out.acted);
    EXPECT(out.wantToast);
    EXPECT(std::string(out.toast).find("ghost") != std::string::npos);
}

TEST(uicanvas_acao_spawn_e_cena_honesta) {
    Scene s;
    Handle spawned = Handle::invalid();
    UiActionCtx ctx;
    ctx.user = &spawned;
    ctx.spawnPreset = [](PresetKind kind, void* user) -> Handle {
        Scene* sp = nullptr;   // (o callback real fecha sobre g_scene; aqui
        (void)sp;              //  só registamos o kind pedido)
        *static_cast<Handle*>(user) = Handle{7, 1};
        (void)kind;
        return *static_cast<Handle*>(user);
    };
    UiElement e;
    e.kind = UiElement::Kind::Button;
    e.action = UiElement::Action::Spawn;
    e.target = "RigidBody3D";
    UiActionResult out = applyUiAction(s, e, ctx);
    EXPECT(out.acted && spawned.valid());
    EXPECT(std::string(out.toast).find("RigidBody3D") != std::string::npos);

    // preset inválido → toast honesto, sem acted
    e.target = "Player";
    out = applyUiAction(s, e, ctx);
    EXPECT(!out.acted && out.wantToast);

    // LoadScene SEM o callback do carregador: NÃO finge sucesso
    bool asked = false;
    ctx.sceneExists = [](const std::string&, void* u) -> bool {
        *static_cast<bool*>(u) = true;
        return true;
    };
    ctx.user = &asked;
    ctx.loadScene = nullptr;
    e.action = UiElement::Action::LoadScene;
    e.target = "cena2";
    out = applyUiAction(s, e, ctx);
    EXPECT(!out.acted);   // sem carregador nada carrega
    EXPECT(out.wantToast);
    EXPECT(std::string(out.toast).find("sem carregador") != std::string::npos);
    EXPECT(asked);   // mas a EXISTÊNCIA foi consultada

    // cena que NÃO existe → toast "nao existe" (sem chegar ao load)
    ctx.sceneExists = [](const std::string&, void*) -> bool { return false; };
    out = applyUiAction(s, e, ctx);
    EXPECT(!out.acted);
    EXPECT(std::string(out.toast).find("nao existe") != std::string::npos);

    // COM o callback (o contrato da 0.7.1): acted + alvo/estilo passados
    std::string loaded;
    int gotStyle = -1;
    ctx.loadScene = [](const std::string& name, vv::ui::SceneSwap style,
                      void* user) {
        auto* got = static_cast<std::pair<std::string, int>*>(user);
        got->first = name;
        got->second = static_cast<int>(style);
    };
    std::pair<std::string, int> got{"", -1};
    ctx.user = &got;
    ctx.sceneExists = [](const std::string&, void*) -> bool { return true; };
    out = applyUiAction(s, e, ctx);
    EXPECT(out.acted);
    EXPECT(got.first == "cena2");
    // LoadScene = instantâneo; TransitionScene (0.7.1) = fade/slide
    EXPECT(got.second == static_cast<int>(vv::ui::SceneSwap::Instant));
    e.action = UiElement::Action::TransitionScene;   // 0.7.1
    e.param = "slide";
    out = applyUiAction(s, e, ctx);
    EXPECT(out.acted);
    EXPECT(got.second == static_cast<int>(vv::ui::SceneSwap::Slide));
    e.param.clear();   // default = fade
    out = applyUiAction(s, e, ctx);
    EXPECT(got.second == static_cast<int>(vv::ui::SceneSwap::Fade));
}

// ---- 5. desenho (rects reais emitidos) ----------------------------------------------

namespace {
struct Rect { f32 x0, y0, x1, y1; };
void collectRects(const QuadBatch& b, std::vector<Rect>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}
bool rectsOverlap(const Rect& a, const Rect& b, f32 eps = 0.01f) {
    return std::min(a.x1, b.x1) - std::max(a.x0, b.x0) > eps &&
           std::min(a.y1, b.y1) - std::max(a.y0, b.y0) > eps;
}
} // namespace

TEST(uicanvas_draw_elementos_sem_sobreposicao) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.beginFrame(nullptr, nullptr, kSW, kSH);

    UiCanvas c;
    UiElement a;
    a.kind = UiElement::Kind::Panel;
    a.ox = 100.0f; a.oy = 100.0f; a.w = 300.0f; a.h = 200.0f;
    a.name = "a";
    c.elements.push_back(a);
    UiElement b;
    b.kind = UiElement::Kind::Button;
    b.text = "OK";
    b.ox = 500.0f; b.oy = 100.0f; b.w = 200.0f; b.h = 80.0f;
    b.name = "b";
    c.elements.push_back(b);
    UiElement hidden;
    hidden.kind = UiElement::Kind::Panel;
    hidden.ox = 550.0f; hidden.oy = 300.0f; hidden.w = 100.0f; hidden.h = 100.0f;
    hidden.visible = false;
    hidden.name = "hidden";
    c.elements.push_back(hidden);

    const u32 drawn = drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(drawn == 2u);   // o invisível NÃO desenha (mas também não conta)

    std::vector<Rect> rects;
    collectRects(ui.solidsForTest(), rects);
    EXPECT(rects.size() >= 2u);
    // os rects dos dois elementos visíveis existem e NÃO se sobrepõem
    Rect ra{100.0f, 100.0f, 400.0f, 300.0f};
    Rect rb{500.0f, 100.0f, 700.0f, 180.0f};
    bool foundA = false, foundB = false;
    for (const Rect& r : rects) {
        if (std::fabs(r.x0 - ra.x0) < 0.5f && std::fabs(r.y0 - ra.y0) < 0.5f &&
            std::fabs(r.x1 - ra.x1) < 0.5f && std::fabs(r.y1 - ra.y1) < 0.5f) {
            foundA = true;
        }
        if (std::fabs(r.x0 - rb.x0) < 0.5f && std::fabs(r.y0 - rb.y0) < 0.5f &&
            std::fabs(r.x1 - rb.x1) < 0.5f && std::fabs(r.y1 - rb.y1) < 0.5f) {
            foundB = true;
        }
    }
    EXPECT(foundA);
    EXPECT(foundB);
    EXPECT(!rectsOverlap(ra, rb));
    // nada desenhado no rect do elemento INVISÍVEL
    Rect rh{550.0f, 300.0f, 650.0f, 400.0f};
    for (const Rect& r : rects) {
        EXPECT(!rectsOverlap(r, rh));
    }
    ui.endFrame();
}

// ---- 6. serialização round-trip ------------------------------------------------------

TEST(uicanvas_serializacao_roundtrip) {
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement panel;
    panel.kind = UiElement::Kind::Panel;
    panel.name = "fundo";
    panel.ox = 24.0f; panel.oy = 48.5f; panel.w = 320.0f; panel.h = 180.0f;
    panel.color[0] = 0.25f; panel.color[1] = 0.5f;
    panel.color[2] = 0.75f; panel.color[3] = 0.9f;
    panel.anchorH = UiElement::AnchorH::Right;
    panel.anchorV = UiElement::AnchorV::Bottom;
    panel.visible = false;
    c->elements.push_back(panel);
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.name = "btn";
    btn.text = "JOGAR";
    btn.action = UiElement::Action::LoadScene;
    btn.target = "cena2";
    btn.ox = -100.0f; btn.oy = 20.0f; btn.w = 240.0f; btn.h = 72.0f;
    c->elements.push_back(btn);
    t->visible = false;   // TIC escondido também serializa

    // MeshRenderer com tint (cor por TIC)
    Tic* t2 = s.get(s.create("Cubo"));
    MeshRenderer* mr = t2->addComponent<MeshRenderer>();
    mr->tint[0] = 1.0f; mr->tint[1] = 0.5f; mr->tint[2] = 0.25f;

    const std::string text = SceneSerializer::dump(s);

    // round-trip numa cena nova
    Scene s2;
    SceneSerializer::LoadCtx ctx;   // sem resolvers — refs ficam, ponteiros null
    EXPECT(SceneSerializer::loadText(s2, text, ctx));

    Tic* hud = s2.get(s2.find("HUD"));
    EXPECT(hud != nullptr);
    EXPECT(!hud->visible);   // visibilidade do TIC preservada
    const UiCanvas* c2 = hud->getComponent<UiCanvas>();
    EXPECT(c2 != nullptr && c2->elements.size() == 2);

    const UiElement& p2 = c2->elements[0];
    EXPECT(p2.kind == UiElement::Kind::Panel);
    EXPECT(p2.name == "fundo");
    EXPECT(nearEqF(p2.ox, 24.0f));
    EXPECT(nearEqF(p2.oy, 48.5f));
    EXPECT(nearEqF(p2.w, 320.0f));
    EXPECT(nearEqF(p2.h, 180.0f));
    EXPECT(nearEqF(p2.color[0], 0.25f));
    EXPECT(nearEqF(p2.color[3], 0.9f));
    EXPECT(p2.anchorH == UiElement::AnchorH::Right);
    EXPECT(p2.anchorV == UiElement::AnchorV::Bottom);
    EXPECT(!p2.visible);

    const UiElement& b2 = c2->elements[1];
    EXPECT(b2.text == "JOGAR");
    EXPECT(b2.action == UiElement::Action::LoadScene);
    EXPECT(b2.target == "cena2");
    EXPECT(b2.visible);

    // tint preservado
    Tic* cubo = s2.get(s2.find("Cubo"));
    const MeshRenderer* mr2 = cubo ? cubo->getComponent<MeshRenderer>() : nullptr;
    EXPECT(mr2 != nullptr);
    EXPECT(nearEqF(mr2->tint[0], 1.0f));
    EXPECT(nearEqF(mr2->tint[1], 0.5f));
    EXPECT(nearEqF(mr2->tint[2], 0.25f));

    // dump da cena carregada preserva tudo (2º round-trip textual)
    const std::string text2 = SceneSerializer::dump(s2);
    EXPECT(text2.find("\"target\":\"cena2\"") != std::string::npos);
    EXPECT(text2.find("\"visible\":false") != std::string::npos);
    EXPECT(text2.find("\"tint\"") != std::string::npos);
}

TEST(uicanvas_cenas_antigas_abrem_sem_uicanvas) {
    // .goni de uma versão 0.6.x (sem visible/tint/UiCanvas) abre com os
    // defaults: TIC visível, tint branco, sem canvas — forward-compat
    Scene s;
    SceneSerializer::LoadCtx ctx;
    const char* old = R"({"version":1,"tics":[
        {"id":0,"name":"PlayerBody3D","active":true,"parent":-1,
         "components":[{"type":"Transform3D","pos":[0,0.5,0],"rot":[0,0,0,1],"scale":[1,1,1]},
                       {"type":"MeshRenderer","mesh":"cube"},
                       {"type":"InputMap"}]}]})";
    EXPECT(SceneSerializer::loadText(s, old, ctx));
    Tic* t = s.get(s.find("PlayerBody3D"));
    EXPECT(t != nullptr);
    EXPECT(t->visible);   // default true
    const MeshRenderer* mr = t->getComponent<MeshRenderer>();
    EXPECT(mr != nullptr);
    EXPECT(nearEqF(mr->tint[0], 1.0f));   // branco
    EXPECT(nearEqF(mr->tint[1], 1.0f));
    EXPECT(nearEqF(mr->tint[2], 1.0f));
    EXPECT(t->getComponent<UiCanvas>() == nullptr);
}
